#include "domain/SignalExtractor.h"

#include <QRegularExpression>

namespace {

QString captureFirst(const QString& text, const QRegularExpression& re) {
    const auto match = re.match(text);
    return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

QString extractModule(const QString& rawLine) {
    return captureFirst(rawLine, QRegularExpression(R"(\[([^\]\s=]+)\])"));
}

QString extractPath(const QString& rawLine) {
    return captureFirst(rawLine, QRegularExpression(R"(path=([^\s]+))"));
}

void enrichCommonAttrs(DomainSignal& signal) {
    const QString status = captureFirst(signal.rawLine, QRegularExpression(R"(status=(\d{3}))", QRegularExpression::CaseInsensitiveOption));
    const QString errCode = captureFirst(signal.rawLine, QRegularExpression(R"(errCode=([A-Z0-9_]+))", QRegularExpression::CaseInsensitiveOption));
    const QString service = captureFirst(signal.rawLine, QRegularExpression(R"(service=([^\s]+))", QRegularExpression::CaseInsensitiveOption));
    const QString upstream = captureFirst(signal.rawLine, QRegularExpression(R"(upstream=([^\s]+))", QRegularExpression::CaseInsensitiveOption));
    const QString costMs = captureFirst(signal.rawLine, QRegularExpression(R"(costMs=(\d+))", QRegularExpression::CaseInsensitiveOption));

    if (!status.isEmpty()) {
        signal.attrs.insert("status", status);
    }
    if (!errCode.isEmpty()) {
        signal.attrs.insert("errCode", errCode);
    }
    if (!service.isEmpty()) {
        signal.attrs.insert("service", service);
    }
    if (!upstream.isEmpty()) {
        signal.attrs.insert("upstream", upstream);
    }
    if (!costMs.isEmpty()) {
        signal.attrs.insert("costMs", costMs);
    }
}

DomainSignal makeSignal(const LogRecord& record, DomainSignalType type, int severity = 1) {
    DomainSignal signal;
    signal.type = type;
    signal.timestamp = record.timestamp;
    signal.module = extractModule(record.rawLine);
    signal.reqId = record.reqId;
    signal.traceId = record.traceId;
    signal.path = extractPath(record.rawLine);
    signal.severity = severity;
    signal.rawLine = record.rawLine;
    enrichCommonAttrs(signal);
    return signal;
}

} // namespace

QVector<DomainSignal> SignalExtractor::extract(const QVector<LogRecord>& logs) const {
    QVector<DomainSignal> out;
    out.reserve(logs.size() * 2);

    for (const auto& record : logs) {
        const QString raw = record.rawLine;

        // 第一版只做单行文本日志的模式抽取：
        // 正则和关键词集中在这里，DiagnosisEngine 只关心标准化信号，不直接碰 rawLine。
        if (raw.contains("Incoming request", Qt::CaseInsensitive)) {
            out.push_back(makeSignal(record, DomainSignalType::RequestStarted));
        }
        if (raw.contains("Request completed", Qt::CaseInsensitive) ||
            raw.contains("Callback handled status=", Qt::CaseInsensitive)) {
            out.push_back(makeSignal(record, DomainSignalType::RequestCompleted));
        }
        if (raw.contains("Request failed", Qt::CaseInsensitive) ||
            raw.contains("Callback failed", Qt::CaseInsensitive)) {
            out.push_back(makeSignal(record, DomainSignalType::RequestFailed, 3));
        }
        if (raw.contains("status=", Qt::CaseInsensitive)) {
            out.push_back(makeSignal(record, DomainSignalType::HttpStatusSeen));
        }
        if (raw.contains("errCode=", Qt::CaseInsensitive)) {
            out.push_back(makeSignal(record, DomainSignalType::BusinessErrCodeSeen, 2));
        }
        if (raw.contains("Upstream timeout", Qt::CaseInsensitive)) {
            out.push_back(makeSignal(record, DomainSignalType::UpstreamTimeout, 3));
        }
        if (raw.contains("Partner timeout", Qt::CaseInsensitive)) {
            out.push_back(makeSignal(record, DomainSignalType::PartnerTimeout, 3));
        }
        if (raw.contains("refused", Qt::CaseInsensitive) ||
            raw.contains("connection reset", Qt::CaseInsensitive) ||
            raw.contains("host unreachable", Qt::CaseInsensitive) ||
            raw.contains("dns", Qt::CaseInsensitive)) {
            out.push_back(makeSignal(record, DomainSignalType::UpstreamRefused, 3));
        }
        if (raw.contains("deadlock", Qt::CaseInsensitive) ||
            raw.contains("lock wait timeout", Qt::CaseInsensitive)) {
            out.push_back(makeSignal(record, DomainSignalType::DbDeadlock, 3));
        }
        if (raw.contains("Slow query", Qt::CaseInsensitive)) {
            out.push_back(makeSignal(record, DomainSignalType::SlowQuery, 2));
        }
        if (raw.contains("Redis latency", Qt::CaseInsensitive)) {
            out.push_back(makeSignal(record, DomainSignalType::RedisLatencyWarn, 1));
        }
        if (raw.contains("pool busy", Qt::CaseInsensitive) ||
            raw.contains("[db-pool]", Qt::CaseInsensitive) ||
            raw.contains("waiters=", Qt::CaseInsensitive)) {
            out.push_back(makeSignal(record, DomainSignalType::ConnectionPoolBusy, 2));
        }
    }

    return out;
}
