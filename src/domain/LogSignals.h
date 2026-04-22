#pragma once

#include <QDateTime>
#include <QMap>
#include <QString>

enum class DomainSignalType {
    RequestStarted,
    RequestCompleted,
    RequestFailed,
    HttpStatusSeen,
    BusinessErrCodeSeen,
    UpstreamTimeout,
    UpstreamRefused,
    PartnerTimeout,
    DbDeadlock,
    SlowQuery,
    RedisLatencyWarn,
    ConnectionPoolBusy
};

struct DomainSignal {
    DomainSignalType type = DomainSignalType::RequestStarted;
    QDateTime timestamp;
    QString module;
    QString reqId;
    QString traceId;
    QString path;
    int severity = 1;
    QString rawLine;
    QMap<QString, QString> attrs;
};

inline QString signalTypeName(DomainSignalType type) {
    switch (type) {
    case DomainSignalType::RequestStarted:
        return "RequestStarted";
    case DomainSignalType::RequestCompleted:
        return "RequestCompleted";
    case DomainSignalType::RequestFailed:
        return "RequestFailed";
    case DomainSignalType::HttpStatusSeen:
        return "HttpStatusSeen";
    case DomainSignalType::BusinessErrCodeSeen:
        return "BusinessErrCodeSeen";
    case DomainSignalType::UpstreamTimeout:
        return "UpstreamTimeout";
    case DomainSignalType::UpstreamRefused:
        return "UpstreamRefused";
    case DomainSignalType::PartnerTimeout:
        return "PartnerTimeout";
    case DomainSignalType::DbDeadlock:
        return "DbDeadlock";
    case DomainSignalType::SlowQuery:
        return "SlowQuery";
    case DomainSignalType::RedisLatencyWarn:
        return "RedisLatencyWarn";
    case DomainSignalType::ConnectionPoolBusy:
        return "ConnectionPoolBusy";
    }
    return "Unknown";
}
