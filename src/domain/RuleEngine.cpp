#include "domain/RuleEngine.h"

#include <QRegularExpression>
#include <QSet>

namespace {

QString firstCaptured(const QString& text, const QRegularExpression& re) {
    const auto match = re.match(text);
    return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

bool containsFinding(const DiagnosisResult& result, const QString& title) {
    for (const auto& item : result.findings) {
        if (item.title == title) {
            return true;
        }
    }
    return false;
}

} // namespace

QVector<LogRecord> RuleEngine::windowLogs(const HttpResponseData& response,
                                          const QVector<LogRecord>& allLogs,
                                          int windowSeconds) const {
    QVector<LogRecord> out;
    if (!response.startedAt.isValid()) {
        return out;
    }
    const QDateTime begin = response.startedAt.addSecs(-windowSeconds);
    const QDateTime end = response.startedAt.addSecs(windowSeconds);

    for (const LogRecord& rec : allLogs) {
        if (!rec.timestamp.isValid()) {
            continue;
        }
        if (rec.timestamp >= begin && rec.timestamp <= end) {
            out.push_back(rec);
        }
    }
    return out;
}

DiagnosisResult RuleEngine::diagnose(const HttpResponseData& response,
                                     const QVector<LogRecord>& allLogs,
                                     int windowSeconds) const {
    const QVector<LogRecord> scopedLogs = windowLogs(response, allLogs, windowSeconds);
    const QString evidence = QString("时间窗口关联 ±%1 秒").arg(windowSeconds);
    return diagnoseWithCorrelation(response, scopedLogs, "time-window", evidence);
}

DiagnosisResult RuleEngine::diagnoseWithCorrelation(const HttpResponseData& response,
                                                    const QVector<LogRecord>& correlatedLogs,
                                                    const QString& correlationMode,
                                                    const QString& correlationEvidence) const {
    DiagnosisResult result;
    result.correlationMode = correlationMode;
    result.correlationEvidence = correlationEvidence;
    result.correlatedCount = correlatedLogs.size();

    if (correlationMode == "trace-id") {
        result.confidence = "高";
    } else if (correlationMode == "incident-time" || correlationMode == "request-time" || correlationMode == "time-window") {
        result.confidence = correlatedLogs.size() >= 3 ? "中" : "低";
    } else if (correlationMode == "full-log-scan") {
        result.confidence = "低";
    } else {
        result.confidence = "低";
    }

    auto addFinding = [&result](int severity, const QString& title, const QString& evidence, const QString& suggestion) {
        if (containsFinding(result, title)) {
            return;
        }
        RuleFinding f;
        f.severity = severity;
        f.title = title;
        f.evidence = evidence;
        f.suggestion = suggestion;
        result.findings.push_back(f);
    };

    int errorCount = 0;
    int parsedStatusCode = 0;
    QSet<QString> reqIds;
    QString path;
    QString errCode;
    QString inventoryTimeoutService;
    QString partnerTimeoutUpstream;
    QString slowQueryCostMs;
    QString deadlockLine;

    for (const LogRecord& rec : correlatedLogs) {
        if (rec.level == "ERROR") {
            ++errorCount;
        }
        if (!rec.reqId.trimmed().isEmpty()) {
            reqIds.insert(rec.reqId.trimmed());
        }
        if (path.isEmpty()) {
            path = firstCaptured(rec.rawLine, QRegularExpression(R"(path=([^\s]+))"));
        }
        if (errCode.isEmpty()) {
            errCode = firstCaptured(rec.rawLine, QRegularExpression(R"(errCode=([A-Z0-9_]+))"));
        }
        if (inventoryTimeoutService.isEmpty()) {
            inventoryTimeoutService = firstCaptured(rec.rawLine, QRegularExpression(R"(Upstream timeout service=([^\s]+))", QRegularExpression::CaseInsensitiveOption));
        }
        if (partnerTimeoutUpstream.isEmpty()) {
            partnerTimeoutUpstream = firstCaptured(rec.rawLine, QRegularExpression(R"(Partner timeout upstream=([^\s]+))", QRegularExpression::CaseInsensitiveOption));
        }
        if (slowQueryCostMs.isEmpty()) {
            slowQueryCostMs = firstCaptured(rec.rawLine, QRegularExpression(R"(Slow query costMs=(\d+))", QRegularExpression::CaseInsensitiveOption));
        }
        if (deadlockLine.isEmpty() && rec.rawLine.contains("deadlock", Qt::CaseInsensitive)) {
            deadlockLine = rec.rawLine;
        }
        if (parsedStatusCode == 0) {
            parsedStatusCode = firstCaptured(rec.rawLine, QRegularExpression(R"(status=(\d{3}))")).toInt();
        }
    }

    if (response.timedOut) {
        addFinding(3, "请求超时", "请求被超时中断", "检查目标服务延迟、网络可达性，适当上调超时阈值。");
    }

    if (response.errorMessage.contains("refused", Qt::CaseInsensitive)) {
        addFinding(3, "连接被拒绝", response.errorMessage, "检查服务是否启动、端口和防火墙策略。");
    }

    if (response.errorMessage.contains("host", Qt::CaseInsensitive) || response.errorMessage.contains("dns", Qt::CaseInsensitive)) {
        addFinding(2, "DNS/主机解析异常", response.errorMessage, "检查域名配置、DNS 解析和网络策略。");
    }

    if (response.statusCode == 401 || response.statusCode == 403) {
        addFinding(2, "鉴权失败", QString("HTTP %1").arg(response.statusCode), "检查 Token 是否过期、权限范围和 Header 格式。");
    }

    if (response.statusCode == 404) {
        addFinding(1, "资源不存在", "HTTP 404", "检查请求路径、版本和网关路由配置。");
    }

    if (response.statusCode == 429) {
        addFinding(2, "触发限流", "HTTP 429", "增加退避重试并检查服务端限流策略。");
    }

    if (response.statusCode >= 500 && response.statusCode <= 599) {
        addFinding(3, "服务端异常", QString("HTTP %1").arg(response.statusCode), "重点检查服务错误栈、下游依赖与数据库状态。");
    }

    if (!inventoryTimeoutService.isEmpty()) {
        addFinding(3, "下游库存服务超时",
                   QString("%1 出现超时，接口=%2，错误码=%3")
                       .arg(inventoryTimeoutService,
                            path.isEmpty() ? "未知" : path,
                            errCode.isEmpty() ? "未提取到" : errCode),
                   "优先检查库存服务 RT、线程池与连接池，再确认网关超时阈值是否过低。");
    }

    if (!deadlockLine.isEmpty()) {
        addFinding(3, "数据库死锁",
                   slowQueryCostMs.isEmpty() ? deadlockLine : QString("%1；慢查询耗时 %2ms").arg(deadlockLine, slowQueryCostMs),
                   "先按 SQL 和事务顺序排查锁竞争，再补充重试与索引优化。");
    }

    if (!partnerTimeoutUpstream.isEmpty()) {
        addFinding(3, "支付回调下游超时",
                   QString("支付回调链路命中 %1 超时，错误码=%2")
                       .arg(partnerTimeoutUpstream,
                            errCode.isEmpty() ? "未提取到" : errCode),
                   "检查第三方回调确认接口、重试策略和幂等落库逻辑。");
    }

    if (!errCode.isEmpty()) {
        addFinding(2, "网关返回业务错误码",
                   QString("错误码 %1，接口=%2").arg(errCode, path.isEmpty() ? "未知" : path),
                   "结合业务错误码与对应服务日志，确认是哪一层先失败。");
    }

    if (response.success && errorCount >= 3) {
        addFinding(2, "成功请求伴随大量错误日志", QString("窗口内 ERROR 数量: %1").arg(errorCount), "存在隐性风险，建议补充告警阈值和熔断策略。");
    }

    if (result.confidence == "低") {
        addFinding(1, "关联可信度较低", result.correlationEvidence, "建议补充 reqId/traceId 或扩大日志时间窗口后重新分析。");
    }

    if (correlationMode == "full-log-scan" && reqIds.size() > 1) {
        addFinding(1, "全量扫描混入多条请求", QString("当前扫描包含 %1 个 reqId").arg(reqIds.size()),
                   "建议补充故障时间或 reqId，避免不同请求互相污染诊断结果。");
    }

    if (result.findings.isEmpty()) {
        addFinding(1, "未命中高风险规则", "未发现明确规则命中", "可扩大日志时间窗口或补充上下文后重试诊断。");
    }

    const int finalStatusCode = response.statusCode > 0 ? response.statusCode : parsedStatusCode;
    int high = 0;
    for (const RuleFinding& f : result.findings) {
        if (f.severity == 3) {
            ++high;
        }
    }

    if (!inventoryTimeoutService.isEmpty()) {
        result.summary = QString("下单链路命中 %1 超时，接口返回 %2，关联可信度：%3。")
                             .arg(inventoryTimeoutService)
                             .arg(finalStatusCode > 0 ? QString::number(finalStatusCode) : "未知状态")
                             .arg(result.confidence);
    } else if (!deadlockLine.isEmpty()) {
        result.summary = QString("查询链路更接近数据库死锁，接口返回 %1，关联可信度：%2。")
                             .arg(finalStatusCode > 0 ? QString::number(finalStatusCode) : "未知状态")
                             .arg(result.confidence);
    } else if (!partnerTimeoutUpstream.isEmpty()) {
        result.summary = QString("支付回调更接近 %1 超时，错误码 %2，关联可信度：%3。")
                             .arg(partnerTimeoutUpstream,
                                  errCode.isEmpty() ? "未知" : errCode,
                                  result.confidence);
    } else if (high > 0) {
        result.summary = QString("高风险诊断 %1 条，关联可信度：%2。").arg(high).arg(result.confidence);
    } else {
        result.summary = QString("共命中 %1 条规则，关联可信度：%2。").arg(result.findings.size()).arg(result.confidence);
    }

    return result;
}
