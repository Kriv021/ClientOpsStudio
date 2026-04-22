#pragma once

#include "domain/Models.h"

#include <QVector>

class SignalExtractor {
public:
    QVector<DomainSignal> extract(const QVector<LogRecord>& logs) const;
};
