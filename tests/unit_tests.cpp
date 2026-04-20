#include "domain/RuleEngine.h"
#include "infra/LogParser.h"
#include "infra/SqliteStore.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QVector>
#include <iostream>

namespace {

bool containsTitle(const DiagnosisResult& result, const QString& title) {
    for (const auto& f : result.findings) {
        if (f.title == title) {
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

int testRuleEngine() {
    QString error;
    const QVector<LogRecord> logs = LogParser::parseFile(QString::fromUtf8(TEST_LOG_FILE), &error);
    if (!error.isEmpty()) {
        std::cerr << "[FAIL] 测试前置日志读取失败: " << error.toStdString() << std::endl;
        return 1;
    }

    HttpResponseData response;
    response.statusCode = 500;
    response.success = false;
    response.errorMessage = "Internal Server Error";
    response.startedAt = QDateTime::fromString("2026-04-17 14:21:33.820", "yyyy-MM-dd HH:mm:ss.zzz");

    RuleEngine engine;
    const DiagnosisResult result = engine.diagnoseWithCorrelation(response, logs, "trace-id", "命中 reqId=9f2a1d");

    if (!containsTitle(result, "服务端异常")) {
        std::cerr << "[FAIL] 未命中服务端异常规则" << std::endl;
        return 1;
    }

    if (!containsTitle(result, "下游库存服务超时")) {
        std::cerr << "[FAIL] 未命中库存超时规则" << std::endl;
        return 1;
    }

    if (!containsTitle(result, "网关返回业务错误码")) {
        std::cerr << "[FAIL] 未命中业务错误码规则" << std::endl;
        return 1;
    }

    if (result.correlationMode != "trace-id" || result.confidence != "高") {
        std::cerr << "[FAIL] 关联模式或可信度异常" << std::endl;
        return 1;
    }

    std::cout << "[PASS] testRuleEngine" << std::endl;
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

int testRuleEngineDistinguishesScenarios() {
    QString error;
    RuleEngine engine;

    const QVector<LogRecord> timeoutLogs = LogParser::parseFile(QString::fromUtf8(TEST_TIMEOUT_LOG_FILE), &error);
    if (!error.isEmpty()) {
        std::cerr << "[FAIL] timeout scenario parse failed: " << error.toStdString() << std::endl;
        return 1;
    }

    HttpResponseData timeoutResp;
    timeoutResp.statusCode = 500;
    timeoutResp.errorMessage = "Internal Server Error";
    const DiagnosisResult timeoutResult = engine.diagnoseWithCorrelation(timeoutResp, timeoutLogs, "trace-id", "命中 reqId=9f2a1d");
    if (!containsTitle(timeoutResult, "下游库存服务超时")) {
        std::cerr << "[FAIL] 超时场景未命中库存超时" << std::endl;
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
    const DiagnosisResult deadlockResult = engine.diagnoseWithCorrelation(deadlockResp, deadlockLogs, "trace-id", "命中 reqId=7ab4d0");
    if (!containsTitle(deadlockResult, "数据库死锁")) {
        std::cerr << "[FAIL] 死锁场景未命中数据库死锁" << std::endl;
        return 1;
    }

    const QVector<LogRecord> callbackLogs = LogParser::parseFile(QString::fromUtf8(TEST_CALLBACK_LOG_FILE), &error);
    if (!error.isEmpty()) {
        std::cerr << "[FAIL] callback scenario parse failed: " << error.toStdString() << std::endl;
        return 1;
    }

    const DiagnosisResult callbackResult = engine.diagnoseWithCorrelation(HttpResponseData(), callbackLogs, "incident-time", "按故障时间前后 30 秒关联");
    if (!containsTitle(callbackResult, "支付回调下游超时")) {
        std::cerr << "[FAIL] 回调场景未命中支付回调下游超时" << std::endl;
        return 1;
    }

    if (timeoutResult.summary == deadlockResult.summary || deadlockResult.summary == callbackResult.summary) {
        std::cerr << "[FAIL] 不同场景的诊断摘要不应相同" << std::endl;
        return 1;
    }

    std::cout << "[PASS] testRuleEngineDistinguishesScenarios" << std::endl;
    return 0;
}

int testScenarioLogsContainNoise() {
    QString error;

    const QVector<LogRecord> timeoutLogs = LogParser::parseFile(QString::fromUtf8(TEST_TIMEOUT_LOG_FILE), &error);
    if (!error.isEmpty()) {
        std::cerr << "[FAIL] timeout scenario parse failed: " << error.toStdString() << std::endl;
        return 1;
    }
    if (timeoutLogs.size() < 12 || distinctReqIds(timeoutLogs) < 2) {
        std::cerr << "[FAIL] 超时场景日志过于简单，应至少包含多请求噪声" << std::endl;
        return 1;
    }

    const QVector<LogRecord> deadlockLogs = LogParser::parseFile(QString::fromUtf8(TEST_DEADLOCK_LOG_FILE), &error);
    if (!error.isEmpty()) {
        std::cerr << "[FAIL] deadlock scenario parse failed: " << error.toStdString() << std::endl;
        return 1;
    }
    if (deadlockLogs.size() < 12 || distinctReqIds(deadlockLogs) < 2) {
        std::cerr << "[FAIL] 死锁场景日志过于简单，应至少包含多请求噪声" << std::endl;
        return 1;
    }

    const QVector<LogRecord> callbackLogs = LogParser::parseFile(QString::fromUtf8(TEST_CALLBACK_LOG_FILE), &error);
    if (!error.isEmpty()) {
        std::cerr << "[FAIL] callback scenario parse failed: " << error.toStdString() << std::endl;
        return 1;
    }
    if (callbackLogs.size() < 12) {
        std::cerr << "[FAIL] 回调场景日志过于简单，应至少包含时间窗内噪声" << std::endl;
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
    failures += testRuleEngine();
    failures += testStoreStartsEmpty();
    failures += testRuleEngineDistinguishesScenarios();
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
