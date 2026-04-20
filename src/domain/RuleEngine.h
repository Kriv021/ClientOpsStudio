#pragma once

#include "domain/Models.h"

#include <QVector>

class RuleEngine {
public:
    DiagnosisResult diagnose(const HttpResponseData& response,
                             const QVector<LogRecord>& allLogs,
                             int windowSeconds = 10) const;

    DiagnosisResult diagnoseWithCorrelation(const HttpResponseData& response,
                                            const QVector<LogRecord>& correlatedLogs,
                                            const QString& correlationMode,
                                            const QString& correlationEvidence) const;

private:
    QVector<LogRecord> windowLogs(const HttpResponseData& response,
                                  const QVector<LogRecord>& allLogs,
                                  int windowSeconds) const;
};
