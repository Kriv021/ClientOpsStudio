#pragma once

#include "domain/Models.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>

class SqliteStore {
public:
    explicit SqliteStore(const QString& dbPath);
    ~SqliteStore();

    bool init(QString* error = nullptr);

    bool saveRequest(const HttpRequestData& request, QString* error = nullptr);
    QVector<HttpRequestData> loadRecentRequests(int limit = 20, QString* error = nullptr) const;
    QVector<HttpRequestData> loadRecentRequestsByCase(int caseId, int limit = 20, QString* error = nullptr) const;

    int createCaseSession(const QString& title,
                          const QString& problemDescription,
                          const QString& targetEndpoint,
                          QString* error = nullptr);
    bool saveCaseSession(const CaseSession& session, QString* error = nullptr);
    bool updateCaseSession(int caseId,
                           const QString& problemDescription,
                           const QString& targetEndpoint,
                           QString* error = nullptr);
    bool deleteCaseSession(int caseId, QString* error = nullptr);
    QVector<CaseSession> loadCaseSessions(int limit = 200, QString* error = nullptr) const;

    bool setSetting(const QString& key, const QString& value, QString* error = nullptr);
    QString getSetting(const QString& key, const QString& defaultValue = QString()) const;

private:
    bool ensureColumnExists(const QString& table, const QString& columnDef, QString* error) const;
    bool hasColumn(const QString& table, const QString& column) const;

    QString connectionName_;
    QSqlDatabase db_;
};
