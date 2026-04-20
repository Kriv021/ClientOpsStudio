#include "infra/LogParser.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QStringConverter>
#include <QTextStream>
#include <QtConcurrent>

LogParser::LogParser(QObject* parent) : QObject(parent) {}

QVector<LogRecord> LogParser::parseFile(const QString& filePath, QString* error) {
    QVector<LogRecord> records;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = QString("无法打开日志文件: %1").arg(filePath);
        }
        return records;
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    // 格式示例: 2026-04-17 14:21:33.799 ERROR [module] [reqId=xxx] [traceId=yyy] ...
    const QRegularExpression re(
        R"(^\s*(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}(?:\.\d{3})?)\s+(INFO|WARN|ERROR|DEBUG)\s+(.*)$)");
    const QRegularExpression reqIdRe(R"(\[(?:reqId|requestId)\s*=\s*([^\]\s]+)\])", QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression traceIdRe(R"(\[(?:traceId|traceID)\s*=\s*([^\]\s]+)\])", QRegularExpression::CaseInsensitiveOption);

    while (!in.atEnd()) {
        const QString line = in.readLine();
        LogRecord rec;
        rec.rawLine = line;

        const QRegularExpressionMatch m = re.match(line);
            if (m.hasMatch()) {
                const QString ts = m.captured(1);
                rec.level = m.captured(2);
                rec.message = m.captured(3);

            QDateTime t = QDateTime::fromString(ts, "yyyy-MM-dd HH:mm:ss.zzz");
            if (!t.isValid()) {
                t = QDateTime::fromString(ts, "yyyy-MM-dd HH:mm:ss");
                }
                rec.timestamp = t;

                const QRegularExpressionMatch reqMatch = reqIdRe.match(line);
                if (reqMatch.hasMatch()) {
                    rec.reqId = reqMatch.captured(1).trimmed();
                }
                const QRegularExpressionMatch traceMatch = traceIdRe.match(line);
                if (traceMatch.hasMatch()) {
                    rec.traceId = traceMatch.captured(1).trimmed();
                }
            } else {
            rec.level = "RAW";
            rec.message = line;
        }
        records.push_back(rec);
    }

    if (error) {
        error->clear();
    }
    return records;
}

LogLoadSummary LogParser::buildLoadSummary(const QString& filePath, const QVector<LogRecord>& records, qint64 elapsedMs) {
    LogLoadSummary summary;
    const QFileInfo info(filePath);
    summary.filePath = filePath;
    summary.fileName = info.fileName();
    summary.fileSizeBytes = info.exists() ? info.size() : 0;
    summary.totalRecords = records.size();
    summary.renderedRecords = qMin(summary.totalRecords, kRenderRecordLimit);
    summary.elapsedMs = elapsedMs;
    summary.wasTruncated = summary.totalRecords > kRenderRecordLimit;

    QSet<QString> reqIds;
    for (const auto& rec : records) {
        if (!rec.reqId.trimmed().isEmpty()) {
            reqIds.insert(rec.reqId.trimmed());
        }
    }
    summary.distinctReqIds = reqIds.size();
    return summary;
}

void LogParser::parseFileAsync(const QString& filePath) {
    (void)QtConcurrent::run([this, filePath]() {
        QString error;
        const QVector<LogRecord> records = LogParser::parseFile(filePath, &error);
        emit parseCompleted(records, error);
    });
}
