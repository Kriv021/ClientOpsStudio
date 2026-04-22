#pragma once

#include <QObject>

#include "app/AiConfigDialog.h"

class QNetworkAccessManager;

class AiAdvisor : public QObject {
    Q_OBJECT
public:
    explicit AiAdvisor(QObject* parent = nullptr);

    bool isConfigured() const;
    void setConfig(const AiConfigInput& cfg);
    AiConfigInput config() const;
    void requestAdvice(const QString& prompt);

signals:
    void adviceReady(const QString& advice);
    void adviceError(const QString& error);

private:
    QNetworkAccessManager* manager_ = nullptr;
    AiConfigInput config_;

    AiConfigInput loadFromEnv() const;
};
