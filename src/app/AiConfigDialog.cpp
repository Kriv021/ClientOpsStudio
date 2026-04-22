#include "app/AiConfigDialog.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QVBoxLayout>

AiConfigDialog::AiConfigDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("AI 接口配置");
    setModal(true);
    resize(650, 300);

    network_ = new QNetworkAccessManager(this);

    auto* root = new QVBoxLayout(this);

    auto* tip = new QLabel("支持 OpenAI 兼容接口。默认仅本次会话使用，勾选后会保存到本地。", this);
    tip->setWordWrap(true);

    auto* form = new QFormLayout();

    apiKeyEdit_ = new QLineEdit(this);
    apiKeyEdit_->setEchoMode(QLineEdit::Password);
    apiKeyEdit_->setPlaceholderText("请输入 API 密钥");

    baseUrlEdit_ = new QLineEdit(this);
    baseUrlEdit_->setPlaceholderText("例如: https://api.openai.com/v1");

    modelEdit_ = new QLineEdit(this);
    modelEdit_->setPlaceholderText("例如: gpt-4o-mini");

    form->addRow("API 密钥", apiKeyEdit_);
    form->addRow("接口地址", baseUrlEdit_);
    form->addRow("模型名称", modelEdit_);

    auto* actionRow = new QHBoxLayout();
    importEnvBtn_ = new QPushButton("从环境变量导入", this);
    importEnvBtn_->setProperty("variant", "secondary");

    testConnBtn_ = new QPushButton("连接测试", this);
    testConnBtn_->setProperty("variant", "secondary");

    persistCheck_ = new QCheckBox("保存到本地（下次自动加载）", this);
    persistCheck_->setChecked(false);

    testStatusLabel_ = new QLabel("未测试", this);
    testStatusLabel_->setStyleSheet("color:#64748B;");

    actionRow->addWidget(importEnvBtn_);
    actionRow->addWidget(testConnBtn_);
    actionRow->addWidget(persistCheck_);
    actionRow->addWidget(testStatusLabel_);
    actionRow->addStretch(1);

    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    if (auto* saveBtn = btnBox->button(QDialogButtonBox::Save)) {
        saveBtn->setText("保存");
    }
    if (auto* cancelBtn = btnBox->button(QDialogButtonBox::Cancel)) {
        cancelBtn->setText("取消");
    }

    root->addWidget(tip);
    root->addLayout(form);
    root->addLayout(actionRow);
    root->addWidget(btnBox);

    connect(importEnvBtn_, &QPushButton::clicked, this, [this]() {
        const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        const QString key = env.value("OPENAI_API_KEY", env.value("DASHSCOPE_API_KEY"));
        const QString base = env.value("OPENAI_BASE_URL", env.value("AI_BASE_URL"));
        const QString model = env.value("OPENAI_MODEL", env.value("AI_MODEL"));

        if (!key.isEmpty()) {
            apiKeyEdit_->setText(key);
        }
        if (!base.isEmpty()) {
            baseUrlEdit_->setText(base);
        }
        if (!model.isEmpty()) {
            modelEdit_->setText(model);
        }
    });

    connect(testConnBtn_, &QPushButton::clicked, this, &AiConfigDialog::runConnectionTest);

    connect(btnBox, &QDialogButtonBox::accepted, this, [this]() {
        if (!validateRequiredFields()) {
            return;
        }
        accept();
    });

    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void AiConfigDialog::setConfig(const AiConfigInput& cfg) {
    apiKeyEdit_->setText(cfg.apiKey);
    baseUrlEdit_->setText(cfg.baseUrl);
    modelEdit_->setText(cfg.model);
    persistCheck_->setChecked(cfg.persistToLocal);
}

AiConfigInput AiConfigDialog::config() const {
    AiConfigInput cfg;
    cfg.apiKey = apiKeyEdit_->text().trimmed();
    cfg.baseUrl = baseUrlEdit_->text().trimmed();
    cfg.model = modelEdit_->text().trimmed();
    cfg.persistToLocal = persistCheck_->isChecked();
    return cfg;
}

bool AiConfigDialog::validateRequiredFields() {
    if (apiKeyEdit_->text().trimmed().isEmpty()) {
        apiKeyEdit_->setFocus();
        testStatusLabel_->setText("请先填写 API Key");
        testStatusLabel_->setStyleSheet("color:#EF4444;");
        return false;
    }

    if (baseUrlEdit_->text().trimmed().isEmpty()) {
        baseUrlEdit_->setFocus();
        testStatusLabel_->setText("请先填写接口地址");
        testStatusLabel_->setStyleSheet("color:#EF4444;");
        return false;
    }

    if (modelEdit_->text().trimmed().isEmpty()) {
        modelEdit_->setFocus();
        testStatusLabel_->setText("请先填写模型名称");
        testStatusLabel_->setStyleSheet("color:#EF4444;");
        return false;
    }

    return true;
}

void AiConfigDialog::runConnectionTest() {
    if (!validateRequiredFields()) {
        return;
    }

    const AiConfigInput cfg = config();
    QUrl url(cfg.baseUrl);
    QString path = url.path();
    if (!path.endsWith('/')) {
        path += '/';
    }
    path += "chat/completions";
    url.setPath(path);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", QString("Bearer %1").arg(cfg.apiKey).toUtf8());

    QJsonObject payload;
    payload.insert("model", cfg.model);
    payload.insert("temperature", 0);
    payload.insert("max_tokens", 8);

    QJsonArray messages;
    QJsonObject user;
    user.insert("role", "user");
    user.insert("content", "reply ok");
    messages.push_back(user);
    payload.insert("messages", messages);

    testConnBtn_->setEnabled(false);
    testStatusLabel_->setText("测试中...");
    testStatusLabel_->setStyleSheet("color:#0EA5E9;");

    QNetworkReply* reply = network_->post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        testConnBtn_->setEnabled(true);
        const QByteArray body = reply->readAll();

        if (reply->error() != QNetworkReply::NoError) {
            QString detail = QString::fromUtf8(body).trimmed();
            if (detail.isEmpty()) {
                detail = reply->errorString();
            }
            if (detail.length() > 100) {
                detail = detail.left(100) + "...";
            }

            testStatusLabel_->setText("失败: " + detail);
            testStatusLabel_->setStyleSheet("color:#EF4444;");
            reply->deleteLater();
            return;
        }

        testStatusLabel_->setText("连接成功");
        testStatusLabel_->setStyleSheet("color:#16A34A;");
        reply->deleteLater();
    });
}
