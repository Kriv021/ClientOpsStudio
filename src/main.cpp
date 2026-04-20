#include "app/MainWindow.h"
#include "domain/Models.h"

#include <QApplication>
#include <QMetaType>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    qRegisterMetaType<HttpResponseData>("HttpResponseData");
    qRegisterMetaType<LogRecord>("LogRecord");
    qRegisterMetaType<QVector<LogRecord>>("QVector<LogRecord>");

    MainWindow w;
    w.show();
    return app.exec();
}
