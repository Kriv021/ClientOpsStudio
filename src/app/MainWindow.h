#pragma once

#include "app/AiConfigDialog.h"
#include "domain/Models.h"
#include "domain/RuleEngine.h"
#include "infra/AiAdvisor.h"
#include "infra/HttpClient.h"
#include "infra/LogParser.h"
#include "infra/SqliteStore.h"

#include <QElapsedTimer>
#include <QMainWindow>
#include <memory>

class QCheckBox;
class QComboBox;
class QDateTimeEdit;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QSplitter;
class QTabWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    struct CorrelationOutcome {
        QVector<LogRecord> logs;
        QString mode;
        QString evidence;
    };

    void setupUi();
    void applyTheme();
    void wireSignals();
    void initStore();
    void loadCaseSessions();
    void loadHistoryForCurrentCase();
    void loadAiConfig();
    void saveAiConfig(const AiConfigInput& cfg);
    void restoreSplitterState();
    void persistSplitterState();

    int selectedCaseId() const;
    bool ensureCaseReadyForRequest();
    bool createCaseSession();
    bool deleteCurrentCase();
    const CaseSession* findCaseSession(int caseId) const;
    CaseSession buildCaseSessionFromUi(int caseId) const;
    void saveCaseState(int caseId);
    void applyCaseSession(const CaseSession& session);
    void refreshCaseSummary();
    void refreshLogSummaryUi();
    void resetWorkspaceState(bool clearCaseMeta);
    bool loadLogFileSync(const QString& filePath);
    void onCaseChanged();

    HttpRequestData collectRequestFromUi() const;
    CorrelationOutcome correlateLogs(int windowSeconds) const;
    void updateResponseUi(const HttpResponseData& response);
    void renderLogs(const QVector<LogRecord>& logs);
    void applyLogFilters();
    void runDiagnosis();
    QString buildAiPrompt() const;
    void renderDiagnosis();
    void openAiConfigDialog();
    void fillSampleRequestAndLog();
    void copyResponseText();
    void setEmptyStates();

private:
    HttpClient httpClient_;
    LogParser logParser_;
    RuleEngine ruleEngine_;
    AiAdvisor aiAdvisor_;
    std::unique_ptr<SqliteStore> store_;

    HttpRequestData lastRequest_;
    HttpResponseData lastResponse_;
    QVector<LogRecord> allLogs_;
    QVector<LogRecord> filteredLogs_;
    QVector<LogRecord> scopedLogs_;
    DiagnosisResult lastDiagnosis_;
    QString lastAiAdvice_;
    QVector<HttpRequestData> historyRequests_;
    QVector<CaseSession> caseSessions_;
    QString currentLogPath_;
    LogLoadSummary currentLogSummary_;
    QString pendingLogPath_;
    int activeCaseId_ = -1;
    int pendingLogCaseId_ = -1;
    QElapsedTimer pendingLogTimer_;
    bool suppressCaseSync_ = false;

    QWidget* central_ = nullptr;
    QSplitter* verticalSplitter_ = nullptr;
    QSplitter* topSplitter_ = nullptr;
    QSplitter* bottomSplitter_ = nullptr;

    QLabel* pageTitle_ = nullptr;
    QLabel* pageDesc_ = nullptr;
    QLabel* currentCaseTitleLabel_ = nullptr;
    QLabel* currentCaseMetaLabel_ = nullptr;
    QTabWidget* workTabs_ = nullptr;
    QLineEdit* caseSearchEdit_ = nullptr;
    QListWidget* caseList_ = nullptr;
    QComboBox* modeBox_ = nullptr;
    QCheckBox* useIncidentTimeCheck_ = nullptr;
    QDateTimeEdit* incidentTimeEdit_ = nullptr;
    QLineEdit* incidentTraceEdit_ = nullptr;

    QComboBox* caseBox_ = nullptr;
    QPushButton* newCaseBtn_ = nullptr;
    QPushButton* deleteCaseBtn_ = nullptr;
    QLineEdit* caseProblemEdit_ = nullptr;
    QLineEdit* caseTargetEdit_ = nullptr;

    QComboBox* methodBox_ = nullptr;
    QLineEdit* urlEdit_ = nullptr;
    QLineEdit* traceHintEdit_ = nullptr;
    QSpinBox* timeoutSpin_ = nullptr;
    QSpinBox* retrySpin_ = nullptr;
    QPlainTextEdit* headersEdit_ = nullptr;
    QPlainTextEdit* bodyEdit_ = nullptr;
    QPushButton* sendBtn_ = nullptr;
    QPushButton* sampleRequestBtn_ = nullptr;

    QLabel* statusLabel_ = nullptr;
    QLabel* latencyLabel_ = nullptr;
    QLabel* errorLabel_ = nullptr;
    QPlainTextEdit* responseEdit_ = nullptr;
    QPushButton* copyResponseBtn_ = nullptr;

    QPushButton* importLogBtn_ = nullptr;
    QComboBox* levelFilterBox_ = nullptr;
    QLineEdit* keywordEdit_ = nullptr;
    QSpinBox* windowSpin_ = nullptr;
    QPushButton* applyFilterBtn_ = nullptr;
    QLabel* logFileSummaryLabel_ = nullptr;
    QLabel* logStatsSummaryLabel_ = nullptr;
    QLabel* logRenderHintLabel_ = nullptr;
    QPlainTextEdit* logView_ = nullptr;

    QPushButton* diagnoseBtn_ = nullptr;
    QCheckBox* aiCheck_ = nullptr;
    QPushButton* aiConfigBtn_ = nullptr;
    QPlainTextEdit* diagnosisView_ = nullptr;
    QPushButton* exportBtn_ = nullptr;

    QComboBox* historyBox_ = nullptr;
    QPushButton* loadHistoryBtn_ = nullptr;

    AiConfigInput aiConfig_;
};
