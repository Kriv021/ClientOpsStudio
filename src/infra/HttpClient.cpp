#include "infra/HttpClient.h"

#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

HttpClient::HttpClient(QObject* parent) : QObject(parent) {}

void HttpClient::sendRequest(const HttpRequestData& request) {
    const QDateTime startTime = QDateTime::currentDateTime();
    sendAttempt(request, request.retryCount, startTime);
}

QList<QPair<QByteArray, QByteArray>> HttpClient::parseHeaders(const QString& headersText) {
    QList<QPair<QByteArray, QByteArray>> headers;
    const QStringList lines = headersText.split('\n', Qt::SkipEmptyParts);
    for (const QString& rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const int pos = line.indexOf(':');
        if (pos <= 0) {
            continue;
        }
        const QByteArray key = line.left(pos).trimmed().toUtf8();
        const QByteArray value = line.mid(pos + 1).trimmed().toUtf8();
        headers.push_back({key, value});
    }
    return headers;
}

void HttpClient::sendAttempt(const HttpRequestData& request, int attemptLeft, const QDateTime& startTime) {
    QUrl url(request.url.trimmed());
    HttpResponseData response;
    response.startedAt = startTime;

    if (!url.isValid() || url.scheme().isEmpty()) {
        response.errorMessage = "URL 无效，请包含协议头（例如 http:// 或 https://）";
        response.finishedAt = QDateTime::currentDateTime();
        response.latencyMs = static_cast<int>(startTime.msecsTo(response.finishedAt));
        emit requestFinished(response);
        return;
    }

    QNetworkRequest netReq(url);
    for (const auto& [k, v] : parseHeaders(request.headersText)) {
        netReq.setRawHeader(k, v);
    }

    QNetworkReply* reply = nullptr;
    const QString method = request.method.trimmed().toUpper();
    if (method == "POST") {
        reply = manager_.post(netReq, request.bodyText.toUtf8());
    } else {
        reply = manager_.get(netReq);
    }

    QTimer* timer = new QTimer(reply);
    timer->setSingleShot(true);
    timer->start(request.timeoutMs);

    QObject::connect(timer, &QTimer::timeout, reply, [reply]() {
        if (reply->isRunning()) {
            reply->setProperty("timedOut", true);
            reply->abort();
        }
    });

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request, attemptLeft, startTime]() {
        HttpResponseData out;
        out.startedAt = startTime;
        out.finishedAt = QDateTime::currentDateTime();
        out.latencyMs = static_cast<int>(startTime.msecsTo(out.finishedAt));

        const bool timedOut = reply->property("timedOut").toBool();
        out.timedOut = timedOut;

        const QVariant statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        out.statusCode = statusCode.isValid() ? statusCode.toInt() : 0;
        out.responseBody = QString::fromUtf8(reply->readAll());

        const auto err = reply->error();
        if (err == QNetworkReply::NoError) {
            out.success = (out.statusCode >= 200 && out.statusCode < 300);
        } else {
            out.success = false;
            if (timedOut) {
                out.errorMessage = "请求超时";
            } else {
                out.errorMessage = reply->errorString();
            }
        }

        const bool canRetry = !out.success && attemptLeft > 0 &&
                              (timedOut || err == QNetworkReply::ConnectionRefusedError ||
                               err == QNetworkReply::TimeoutError || err == QNetworkReply::TemporaryNetworkFailureError ||
                               err == QNetworkReply::HostNotFoundError);

        reply->deleteLater();

        if (canRetry) {
            sendAttempt(request, attemptLeft - 1, startTime);
            return;
        }

        emit requestFinished(out);
    });
}
