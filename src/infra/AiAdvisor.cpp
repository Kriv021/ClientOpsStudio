#include "infra/AiAdvisor.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcessEnvironment>

AiAdvisor::AiAdvisor(QObject* parent) : QObject(parent) {
    manager_ = new QNetworkAccessManager(this);
}

AiConfigInput AiAdvisor::loadFromEnv() const {
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    AiConfigInput cfg;
    cfg.apiKey = env.value("OPENAI_API_KEY", env.value("DASHSCOPE_API_KEY"));
    cfg.baseUrl = env.value("OPENAI_BASE_URL", "https://api.openai.com/v1");
    cfg.model = env.value("OPENAI_MODEL", "gpt-4o-mini");
    return cfg;
}

void AiAdvisor::setConfig(const AiConfigInput& cfg) {
    config_ = cfg;
}

AiConfigInput AiAdvisor::config() const {
    return config_;
}

bool AiAdvisor::isConfigured() const {
    if (!config_.apiKey.isEmpty() && !config_.baseUrl.isEmpty() && !config_.model.isEmpty()) {
        return true;
    }
    const AiConfigInput envCfg = loadFromEnv();
    return !envCfg.apiKey.isEmpty() && !envCfg.baseUrl.isEmpty() && !envCfg.model.isEmpty();
}

void AiAdvisor::requestAdvice(const QString& prompt) {
    AiConfigInput useCfg = config_;
    if (useCfg.apiKey.isEmpty() || useCfg.baseUrl.isEmpty() || useCfg.model.isEmpty()) {
        useCfg = loadFromEnv();
    }

    if (useCfg.apiKey.isEmpty()) {
        emit adviceError("未配置 AI API Key");
        return;
    }

    const QUrl url(useCfg.baseUrl + "/chat/completions");

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", QString("Bearer %1").arg(useCfg.apiKey).toUtf8());

    QJsonObject payload;
    payload.insert("model", useCfg.model);

    QJsonArray messages;
    QJsonObject sys;
    sys.insert("role", "system");
    sys.insert("content", "你是客户端排障助手，请输出简洁、可执行建议。请按：根因猜测/验证步骤/修复建议 三段输出。");
    messages.push_back(sys);

    QJsonObject user;
    user.insert("role", "user");
    user.insert("content", prompt);
    messages.push_back(user);

    payload.insert("messages", messages);
    payload.insert("temperature", 0.2);
    payload.insert("max_tokens", 380);

    QNetworkReply* reply = manager_->post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            emit adviceError(QString("AI 请求失败: %1\n%2").arg(reply->errorString(), QString::fromUtf8(body)));
            reply->deleteLater();
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(body);
        const QJsonObject obj = doc.object();
        const QJsonArray choices = obj.value("choices").toArray();
        if (choices.isEmpty()) {
            emit adviceError("AI 返回为空");
            reply->deleteLater();
            return;
        }

        const QJsonObject msg = choices.at(0).toObject().value("message").toObject();
        const QString content = msg.value("content").toString();
        if (content.isEmpty()) {
            emit adviceError("AI 返回内容为空");
            reply->deleteLater();
            return;
        }

        emit adviceReady(content);
        reply->deleteLater();
    });
}
