#include "domain/DiagnosisEngine.h"
#include "domain/SignalExtractor.h"
#include "infra/LogParser.h"
#include "infra/SqliteStore.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QTemporaryDir>
#include <QVector>
#include <iostream>

namespace {

bool containsCandidate(const DiagnosisResult& result, const QString& name) {
    for (const auto& item : result.candidates) {
        if (item.name == name) {
            return true;
        }
    }
    return false;
}

bool containsSignal(const QVector<DomainSignal>& signalList, DomainSignalType type) {
    for (const auto& signal : signalList) {
        if (signal.type == type) {
            return true;
        }
    }
    return false;
}

int distinctReqIds(const QVector<LogRecord>& logs) {
    QSet<QString> ids;
    for (const auto& rec : logs) {
        if (!rec.reqId.trimmed().isEmpty()) {
            ids.insert(rec.reqId.trimmed());
        }
    }
    return ids.size();
}

int testLogParser() {
    QString error;
    const QVector<LogRecord> logs = LogParser::parseFile(QString::fromUtf8(TEST_LOG_FILE), &error);

    if (!error.isEmpty()) {
        std::cerr << "[FAIL] 日志解析返回错误: " << error.toStdString() << std::endl;
        return 1;
    }

    if (logs.size() != 5) {
        std::cerr << "[FAIL] 日志行数不符合预期，actual=" << logs.size() << std::endl;
        return 1;
    }

    if (logs[0].level != "INFO" || !logs[0].timestamp.isValid()) {
        std::cerr << "[FAIL] 第一行解析失败" << std::endl;
        return 1;
    }

    if (logs[3].level != "ERROR") {
        std::cerr << "[FAIL] 第四行级别解析失败" << std::endl;
        return 1;
    }

    if (logs[0].reqId != "9f2a1d") {
        std::cerr << "[FAIL] reqId 提取失败" << std::endl;
        return 1;
    }

    std::cout << "[PASS] testLogParser" << std::endl;
    return 0;
}

int testSignalExtractor() {
    QString error;
    const QVector<LogRecord> logs = LogParser::parseFile(QString::fromUtf8(TEST_LOG_FILE), &error);
    if (!error.isEmpty()) {
        std::cerr << "[FAIL] 测试前置日志读取失败: " << error.toStdString() << std::endl;
        return 1;
    }

    SignalExtractor extractor;
    const QVector<DomainSignal> signalList = extractor.extract(logs);

    if (!containsSignal(signalList, DomainSignalType::RequestStarted)) {
        std::cerr << "[FAIL] 未抽取到请求开始信号" << std::endl;
        return 1;
    }
    if (!containsSignal(signalList, DomainSignalType::UpstreamTimeout)) {
        std::cerr << "[FAIL] 未抽取到下游超时信号" << std::endl;
        return 1;
    }
    if (!containsSignal(signalList, DomainSignalType::BusinessErrCodeSeen)) {
        std::cerr << "[FAIL] 未抽取到业务错误码信号" << std::endl;
        return 1;
    }
    if (!containsSignal(signalList, DomainSignalType::RedisLatencyWarn)) {
        std::cerr << "[FAIL] 未抽取到干扰型 Redis 延迟信号" << std::endl;
        return 1;
    }

    std::cout << "[PASS] testSignalExtractor" << std::endl;
    return 0;
}

int testStoreStartsEmpty() {
    QTemporaryDir dir;
    if (!dir.isValid()) {
        std::cerr << "[FAIL] 无法创建临时目录" << std::endl;
        return 1;
    }

    SqliteStore store(dir.path() + "/clientops_test.db");
    QString error;
    if (!store.init(&error)) {
        std::cerr << "[FAIL] store init failed: " << error.toStdString() << std::endl;
        return 1;
    }

    const QVector<CaseSession> cases = store.loadCaseSessions(20, &error);
    if (!error.isEmpty()) {
        std::cerr << "[FAIL] loadCaseSessions failed: " << error.toStdString() << std::endl;
        return 1;
    }
    if (!cases.isEmpty()) {
        std::cerr << "[FAIL] 新库启动后应为空案例列表，actual=" << cases.size() << std::endl;
        return 1;
    }

    std::cout << "[PASS] testStoreStartsEmpty" << std::endl;
    return 0;
}

int testDiagnosisEngineDistinguishesScenarios() {
    QString error;
    SignalExtractor extractor;
    DiagnosisEngine engine;

    const QVector<LogRecord> timeoutLogs = LogParser::parseFile(QString::fromUtf8(TEST_TIMEOUT_LOG_FILE), &error);
    if (!error.isEmpty()) {
        std::cerr << "[FAIL] timeout scenario parse failed: " << error.toStdString() << std::endl;
        return 1;
    }

    HttpResponseData timeoutResp;
    timeoutResp.statusCode = 500;
    timeoutResp.errorMessage = "Internal Server Error";
    const auto timeoutSignals = extractor.extract(timeoutLogs);
    const DiagnosisResult timeoutResult = engine.diagnose(timeoutResp, timeoutSignals, "trace-id", "命中 reqId=9f2a1d");
    if (timeoutResult.rulePrimaryCause != "下游依赖超时") {
        std::cerr << "[FAIL] 超时场景主因不正确" << std::endl;
        return 1;
    }
    if (timeoutResult.candidates.size() < 2) {
        std::cerr << "[FAIL] 超时场景应至少保留两个候选根因" << std::endl;
        return 1;
    }

    const QVector<LogRecord> deadlockLogs = LogParser::parseFile(QString::fromUtf8(TEST_DEADLOCK_LOG_FILE), &error);
    if (!error.isEmpty()) {
        std::cerr << "[FAIL] deadlock scenario parse failed: " << error.toStdString() << std::endl;
        return 1;
    }

    HttpResponseData deadlockResp;
    deadlockResp.statusCode = 500;
    deadlockResp.errorMessage = "Internal Server Error";
    const auto deadlockSignals = extractor.extract(deadlockLogs);
    const DiagnosisResult deadlockResult = engine.diagnose(deadlockResp, deadlockSignals, "trace-id", "命中 reqId=7ab4d0");
    if (deadlockResult.rulePrimaryCause != "数据库死锁/锁竞争") {
        std::cerr << "[FAIL] 死锁场景主因不正确" << std::endl;
        return 1;
    }

    const QVector<LogRecord> callbackLogs = LogParser::parseFile(QString::fromUtf8(TEST_CALLBACK_LOG_FILE), &error);
    if (!error.isEmpty()) {
        std::cerr << "[FAIL] callback scenario parse failed: " << error.toStdString() << std::endl;
        return 1;
    }

    const auto callbackSignals = extractor.extract(callbackLogs);
    const DiagnosisResult callbackResult = engine.diagnose(HttpResponseData(), callbackSignals, "incident-time", "按故障时间前后 30 秒关联");
    if (callbackResult.rulePrimaryCause != "下游依赖超时") {
        std::cerr << "[FAIL] 回调场景主因不正确" << std::endl;
        return 1;
    }
    if (!containsCandidate(callbackResult, "证据不足/未知")) {
        std::cerr << "[FAIL] 回调场景应保留证据不足候选以反映缺 reqId 的不确定性" << std::endl;
        return 1;
    }

    if (timeoutResult.summary == deadlockResult.summary || deadlockResult.summary == callbackResult.summary) {
        std::cerr << "[FAIL] 不同场景的诊断摘要不应相同" << std::endl;
        return 1;
    }

    std::cout << "[PASS] testDiagnosisEngineDistinguishesScenarios" << std::endl;
    return 0;
}

int testDiagnosisResultSupportsAiReview() {
    QString error;
    const QVector<LogRecord> logs = LogParser::parseFile(QString::fromUtf8(TEST_TIMEOUT_LOG_FILE), &error);
    if (!error.isEmpty()) {
        std::cerr << "[FAIL] ai review scenario parse failed: " << error.toStdString() << std::endl;
        return 1;
    }

    SignalExtractor extractor;
    DiagnosisEngine engine;
    const auto signalList = extractor.extract(logs);

    HttpResponseData response;
    response.statusCode = 500;
    response.errorMessage = "Internal Server Error";
    const DiagnosisResult result = engine.diagnose(response, signalList, "trace-id", "命中 reqId=9f2a1d");

    if (result.evidenceItems.isEmpty()) {
        std::cerr << "[FAIL] 诊断结果应包含结构化证据" << std::endl;
        return 1;
    }
    if (result.candidates.size() < 3) {
        std::cerr << "[FAIL] 诊断结果应保留 top3 候选供 AI 复核" << std::endl;
        return 1;
    }
    if (result.finalConclusion.isEmpty()) {
        std::cerr << "[FAIL] 诊断结果应预留最终结论字段" << std::endl;
        return 1;
    }

    std::cout << "[PASS] testDiagnosisResultSupportsAiReview" << std::endl;
    return 0;
}

int testScenarioLogsContainNoise() {
    QString error;

    const QVector<LogRecord> timeoutLogs = LogParser::parseFile(QString::fromUtf8(TEST_TIMEOUT_LOG_FILE), &error);
    if (!error.isEmpty()) {
        std::cerr << "[FAIL] timeout scenario parse failed: " << error.toStdString() << std::endl;
        return 1;
    }
    if (timeoutLogs.size() < 24 || distinctReqIds(timeoutLogs) < 3) {
        std::cerr << "[FAIL] 超时场景日志过于简单，应至少包含多请求噪声与干扰信号" << std::endl;
        return 1;
    }

    const QVector<LogRecord> deadlockLogs = LogParser::parseFile(QString::fromUtf8(TEST_DEADLOCK_LOG_FILE), &error);
    if (!error.isEmpty()) {
        std::cerr << "[FAIL] deadlock scenario parse failed: " << error.toStdString() << std::endl;
        return 1;
    }
    if (deadlockLogs.size() < 22 || distinctReqIds(deadlockLogs) < 3) {
        std::cerr << "[FAIL] 死锁场景日志过于简单，应至少包含多请求噪声与误导慢查询" << std::endl;
        return 1;
    }

    const QVector<LogRecord> callbackLogs = LogParser::parseFile(QString::fromUtf8(TEST_CALLBACK_LOG_FILE), &error);
    if (!error.isEmpty()) {
        std::cerr << "[FAIL] callback scenario parse failed: " << error.toStdString() << std::endl;
        return 1;
    }
    if (callbackLogs.size() < 20) {
        std::cerr << "[FAIL] 回调场景日志过于简单，应至少包含时间窗内噪声与无 reqId 主链路" << std::endl;
        return 1;
    }

    std::cout << "[PASS] testScenarioLogsContainNoise" << std::endl;
    return 0;
}

int testStorePersistsCaseWorkspace() {
    QTemporaryDir dir;
    if (!dir.isValid()) {
        std::cerr << "[FAIL] 无法创建临时目录" << std::endl;
        return 1;
    }

    SqliteStore store(dir.path() + "/clientops_state.db");
    QString error;
    if (!store.init(&error)) {
        std::cerr << "[FAIL] store init failed: " << error.toStdString() << std::endl;
        return 1;
    }

    const int caseId = store.createCaseSession("支付回调排查", "支付回调偶发失败", "/api/v1/payment/callback", &error);
    if (caseId < 0) {
        std::cerr << "[FAIL] createCaseSession failed: " << error.toStdString() << std::endl;
        return 1;
    }

    CaseSession session;
    session.id = caseId;
    session.problemDescription = "支付回调偶发失败";
    session.targetEndpoint = "/api/v1/payment/callback";
    session.analysisMode = "log";
    session.incidentTraceId = "trace-88";
    session.useIncidentTime = true;
    session.incidentTime = QDateTime::fromString("2026-04-18 09:15:12", "yyyy-MM-dd HH:mm:ss");
    session.requestDraft.caseId = caseId;
    session.requestDraft.method = "POST";
    session.requestDraft.url = "https://your-env.example.com/api/v1/payment/callback";
    session.requestDraft.headersText = "Content-Type: application/json";
    session.requestDraft.bodyText = "{\"provider\":\"wxpay\"}";
    session.requestDraft.traceIdHint = "trace-88";
    session.requestDraft.timeoutMs = 3000;
    session.requestDraft.retryCount = 0;
    session.logPath = QString::fromUtf8(TEST_CALLBACK_LOG_FILE);

    if (!store.saveCaseSession(session, &error)) {
        std::cerr << "[FAIL] saveCaseSession failed: " << error.toStdString() << std::endl;
        return 1;
    }

    const QVector<CaseSession> cases = store.loadCaseSessions(20, &error);
    if (!error.isEmpty() || cases.size() != 1) {
        std::cerr << "[FAIL] loadCaseSessions after save failed" << std::endl;
        return 1;
    }

    const CaseSession loaded = cases.first();
    if (loaded.analysisMode != "log" || loaded.incidentTraceId != "trace-88" || !loaded.useIncidentTime) {
        std::cerr << "[FAIL] 案例模式或锚点恢复失败" << std::endl;
        return 1;
    }
    if (loaded.requestDraft.url != session.requestDraft.url || loaded.logPath != session.logPath) {
        std::cerr << "[FAIL] 请求草稿或日志路径恢复失败" << std::endl;
        return 1;
    }

    std::cout << "[PASS] testStorePersistsCaseWorkspace" << std::endl;
    return 0;
}

int testLogLoadSummary() {
    QString error;
    const QString path = QString::fromUtf8(TEST_TIMEOUT_LOG_FILE);
    const QVector<LogRecord> logs = LogParser::parseFile(path, &error);
    if (!error.isEmpty()) {
        std::cerr << "[FAIL] summary scenario parse failed: " << error.toStdString() << std::endl;
        return 1;
    }

    const auto summary = LogParser::buildLoadSummary(path, logs, 37);
    if (summary.totalRecords != logs.size()) {
        std::cerr << "[FAIL] 日志摘要总行数不正确" << std::endl;
        return 1;
    }
    if (summary.distinctReqIds < 2) {
        std::cerr << "[FAIL] 日志摘要应识别多请求噪声" << std::endl;
        return 1;
    }
    if (summary.elapsedMs != 37) {
        std::cerr << "[FAIL] 日志摘要未保留解析耗时" << std::endl;
        return 1;
    }
    if (summary.fileSizeBytes <= 0) {
        std::cerr << "[FAIL] 日志摘要文件大小不正确" << std::endl;
        return 1;
    }
    if (summary.renderedRecords != summary.totalRecords || summary.wasTruncated) {
        std::cerr << "[FAIL] 小样本不应被截断渲染" << std::endl;
        return 1;
    }

    QVector<LogRecord> manyLogs;
    manyLogs.resize(5005);
    const auto truncated = LogParser::buildLoadSummary(path, manyLogs, 12);
    if (!truncated.wasTruncated || truncated.renderedRecords != 5000) {
        std::cerr << "[FAIL] 大样本应标记为截断渲染" << std::endl;
        return 1;
    }

    std::cout << "[PASS] testLogLoadSummary" << std::endl;
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    int failures = 0;
    failures += testLogParser();
    failures += testSignalExtractor();
    failures += testStoreStartsEmpty();
    failures += testDiagnosisEngineDistinguishesScenarios();
    failures += testDiagnosisResultSupportsAiReview();
    failures += testScenarioLogsContainNoise();
    failures += testStorePersistsCaseWorkspace();
    failures += testLogLoadSummary();

    if (failures == 0) {
        std::cout << "All tests passed." << std::endl;
        return 0;
    }

    std::cerr << failures << " tests failed." << std::endl;
    return 1;
}
