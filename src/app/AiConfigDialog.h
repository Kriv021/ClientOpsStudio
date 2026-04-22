#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;
class QPushButton;
class QLabel;
class QCheckBox;
class QNetworkAccessManager;

struct AiConfigInput {
    QString apiKey;
    QString baseUrl;
    QString model;
    bool persistToLocal = false;
};

class AiConfigDialog : public QDialog {
    Q_OBJECT
public:
    explicit AiConfigDialog(QWidget* parent = nullptr);

    void setConfig(const AiConfigInput& cfg);
    AiConfigInput config() const;

private:
    void runConnectionTest();
    bool validateRequiredFields();

    QLineEdit* apiKeyEdit_ = nullptr;
    QLineEdit* baseUrlEdit_ = nullptr;
    QLineEdit* modelEdit_ = nullptr;
    QPushButton* importEnvBtn_ = nullptr;
    QPushButton* testConnBtn_ = nullptr;
    QLabel* testStatusLabel_ = nullptr;
    QCheckBox* persistCheck_ = nullptr;
    QNetworkAccessManager* network_ = nullptr;
};
