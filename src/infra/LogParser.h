#pragma once

#include "domain/Models.h"

#include <QObject>
#include <QVector>

class LogParser : public QObject {
    Q_OBJECT
public:
    explicit LogParser(QObject* parent = nullptr);

    void parseFileAsync(const QString& filePath);
    static QVector<LogRecord> parseFile(const QString& filePath, QString* error = nullptr);
    static LogLoadSummary buildLoadSummary(const QString& filePath, const QVector<LogRecord>& records, qint64 elapsedMs);
    static constexpr int kRenderRecordLimit = 5000;

signals:
    void parseCompleted(const QVector<LogRecord>& records, const QString& error);
};
