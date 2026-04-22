#pragma once

#include "domain/Models.h"

#include <QVector>

class DiagnosisEngine {
public:
    DiagnosisResult diagnose(const HttpResponseData& response,
                             const QVector<DomainSignal>& signalList,
                             const QString& correlationMode,
                             const QString& correlationEvidence) const;
};
