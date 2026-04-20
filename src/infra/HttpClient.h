#pragma once

#include "domain/Models.h"

#include <QNetworkAccessManager>
#include <QObject>

class HttpClient : public QObject {
    Q_OBJECT
public:
    explicit HttpClient(QObject* parent = nullptr);

    void sendRequest(const HttpRequestData& request);

signals:
    void requestFinished(const HttpResponseData& response);

private:
    QNetworkAccessManager manager_;

    static QList<QPair<QByteArray, QByteArray>> parseHeaders(const QString& headersText);
    void sendAttempt(const HttpRequestData& request, int attemptLeft, const QDateTime& startTime);
};
