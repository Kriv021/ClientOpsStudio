#pragma once

#include "domain/Models.h"

#include <QString>
#include <QVector>

class MarkdownExporter {
public:
    static QString buildReport(const CaseSession& caseSession,
                               const HttpRequestData& request,
                               const HttpResponseData& response,
                               const QVector<LogRecord>& scopedLogs,
                               const DiagnosisResult& diagnosis,
                               const QString& aiAdvice);
};
