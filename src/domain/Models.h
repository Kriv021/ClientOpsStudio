#pragma once

#include "domain/LogSignals.h"

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QVector>

struct HttpRequestData {
    int caseId = -1;
    QString method;
    QString url;
    QString headersText;
    QString bodyText;
    QString traceIdHint;
    int timeoutMs = 5000;
    int retryCount = 1;
};

struct HttpResponseData {
    int statusCode = 0;
    QString responseBody;
    QString errorMessage;
    int latencyMs = 0;
    bool success = false;
    bool timedOut = false;
    QDateTime startedAt;
    QDateTime finishedAt;
};

struct CaseSession {
    int id = -1;
    QString title;
    QString problemDescription;
    QString targetEndpoint;
    QString analysisMode = "log";
    QString incidentTraceId;
    bool useIncidentTime = false;
    QDateTime incidentTime;
    HttpRequestData requestDraft;
    QString logPath;
    QDateTime createdAt;
    QDateTime updatedAt;
};

struct LogRecord {
    QDateTime timestamp;
    QString level;
    QString message;
    QString rawLine;
    QString reqId;
    QString traceId;
};

struct LogLoadSummary {
    QString filePath;
    QString fileName;
    qint64 fileSizeBytes = 0;
    int totalRecords = 0;
    int renderedRecords = 0;
    int distinctReqIds = 0;
    qint64 elapsedMs = 0;
    bool wasTruncated = false;
};

struct RuleFinding {
    QString title;
    QString evidence;
    QString suggestion;
    int severity = 1; // 1=low,2=medium,3=high
};

struct EvidenceItem {
    QString title;
    QString snippet;
    QString module;
    QDateTime timestamp;
    DomainSignalType signalType = DomainSignalType::RequestStarted;
    int weight = 0;
    bool isPrimary = false;
};

struct RootCauseCandidate {
    QString name;
    int score = 0;
    QString summary;
    QString nextSteps;
    QVector<EvidenceItem> primaryEvidence;
    QVector<EvidenceItem> secondaryEvidence;
};

struct DiagnosisResult {
    QVector<RuleFinding> findings;
    QString summary;
    QString correlationMode;
    QString correlationEvidence;
    QString confidence;
    int correlatedCount = 0;
    QString rulePrimaryCause;
    QString aiReviewedCause;
    QString aiReviewAction;
    QString aiReviewNote;
    QString finalConclusion;
    QVector<EvidenceItem> evidenceItems;
    QVector<RootCauseCandidate> candidates;
};

Q_DECLARE_METATYPE(HttpRequestData)
Q_DECLARE_METATYPE(HttpResponseData)
Q_DECLARE_METATYPE(CaseSession)
Q_DECLARE_METATYPE(LogRecord)
Q_DECLARE_METATYPE(LogLoadSummary)
Q_DECLARE_METATYPE(DomainSignal)
Q_DECLARE_METATYPE(QVector<LogRecord>)
