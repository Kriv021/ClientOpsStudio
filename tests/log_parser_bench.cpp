#include "infra/LogParser.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <iostream>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    if (argc < 2) {
        std::cerr << "Usage: LogParserBench <log-file-path>" << std::endl;
        return 1;
    }

    const QString filePath = QString::fromLocal8Bit(argv[1]);
    QFileInfo fi(filePath);
    if (!fi.exists()) {
        std::cerr << "file_not_found: " << filePath.toStdString() << std::endl;
        return 1;
    }

    QElapsedTimer timer;
    timer.start();
    QString error;
    const QVector<LogRecord> logs = LogParser::parseFile(filePath, &error);
    const qint64 elapsed = timer.elapsed();

    if (!error.isEmpty()) {
        std::cerr << "parse_error: " << error.toStdString() << std::endl;
        return 1;
    }

    std::cout << "file=" << filePath.toStdString() << "\n";
    std::cout << "size_bytes=" << fi.size() << "\n";
    std::cout << "records=" << logs.size() << "\n";
    std::cout << "elapsed_ms=" << elapsed << "\n";
    return 0;
}
