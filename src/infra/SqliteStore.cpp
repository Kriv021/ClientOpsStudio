#include "infra/SqliteStore.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

SqliteStore::SqliteStore(const QString& dbPath) {
    connectionName_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    db_ = QSqlDatabase::addDatabase("QSQLITE", connectionName_);
    db_.setDatabaseName(dbPath);
}

SqliteStore::~SqliteStore() {
    if (db_.isOpen()) {
        db_.close();
    }
    db_ = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName_);
}

bool SqliteStore::hasColumn(const QString& table, const QString& column) const {
    QSqlQuery q(db_);
    if (!q.exec(QString("PRAGMA table_info(%1)").arg(table))) {
        return false;
    }

    while (q.next()) {
        if (q.value(1).toString().compare(column, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

bool SqliteStore::ensureColumnExists(const QString& table, const QString& columnDef, QString* error) const {
    const QString column = columnDef.split(' ', Qt::SkipEmptyParts).value(0).trimmed();
    if (column.isEmpty()) {
        if (error) {
            *error = "无效列定义";
        }
        return false;
    }

    if (hasColumn(table, column)) {
        return true;
    }

    QSqlQuery q(db_);
    if (!q.exec(QString("ALTER TABLE %1 ADD COLUMN %2").arg(table, columnDef))) {
        if (error) {
            *error = q.lastError().text();
        }
        return false;
    }
    return true;
}

bool SqliteStore::init(QString* error) {
    if (!db_.open()) {
        if (error) {
            *error = db_.lastError().text();
        }
        return false;
    }

    QSqlQuery q(db_);
    const bool ok1 = q.exec(
        "CREATE TABLE IF NOT EXISTS request_history ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "case_id INTEGER DEFAULT -1,"
        "method TEXT NOT NULL,"
        "url TEXT NOT NULL,"
        "headers_text TEXT,"
        "body_text TEXT,"
        "trace_id_hint TEXT,"
        "started_at TEXT,"
        "timeout_ms INTEGER,"
        "retry_count INTEGER,"
        "created_at TEXT NOT NULL"
        ")");

    const bool ok2 = q.exec(
        "CREATE TABLE IF NOT EXISTS app_settings ("
        "key TEXT PRIMARY KEY,"
        "value TEXT NOT NULL,"
        "updated_at TEXT NOT NULL"
        ")");

    const bool ok3 = q.exec(
        "CREATE TABLE IF NOT EXISTS case_session ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "title TEXT NOT NULL,"
        "problem_description TEXT NOT NULL,"
        "target_endpoint TEXT,"
        "analysis_mode TEXT DEFAULT 'log',"
        "incident_trace_id TEXT,"
        "use_incident_time INTEGER DEFAULT 0,"
        "incident_time TEXT,"
        "request_method TEXT DEFAULT 'GET',"
        "request_url TEXT,"
        "request_headers TEXT,"
        "request_body TEXT,"
        "request_trace_hint TEXT,"
        "request_timeout_ms INTEGER DEFAULT 5000,"
        "request_retry_count INTEGER DEFAULT 1,"
        "log_path TEXT,"
        "created_at TEXT NOT NULL,"
        "updated_at TEXT NOT NULL"
        ")");

    if (!ok1 || !ok2 || !ok3) {
        if (error) {
            *error = q.lastError().text();
        }
        return false;
    }

    if (!ensureColumnExists("request_history", "case_id INTEGER DEFAULT -1", error)) {
        return false;
    }
    if (!ensureColumnExists("request_history", "trace_id_hint TEXT", error)) {
        return false;
    }
    if (!ensureColumnExists("request_history", "started_at TEXT", error)) {
        return false;
    }
    if (!ensureColumnExists("case_session", "analysis_mode TEXT DEFAULT 'log'", error)) {
        return false;
    }
    if (!ensureColumnExists("case_session", "incident_trace_id TEXT", error)) {
        return false;
    }
    if (!ensureColumnExists("case_session", "use_incident_time INTEGER DEFAULT 0", error)) {
        return false;
    }
    if (!ensureColumnExists("case_session", "incident_time TEXT", error)) {
        return false;
    }
    if (!ensureColumnExists("case_session", "request_method TEXT DEFAULT 'GET'", error)) {
        return false;
    }
    if (!ensureColumnExists("case_session", "request_url TEXT", error)) {
        return false;
    }
    if (!ensureColumnExists("case_session", "request_headers TEXT", error)) {
        return false;
    }
    if (!ensureColumnExists("case_session", "request_body TEXT", error)) {
        return false;
    }
    if (!ensureColumnExists("case_session", "request_trace_hint TEXT", error)) {
        return false;
    }
    if (!ensureColumnExists("case_session", "request_timeout_ms INTEGER DEFAULT 5000", error)) {
        return false;
    }
    if (!ensureColumnExists("case_session", "request_retry_count INTEGER DEFAULT 1", error)) {
        return false;
    }
    if (!ensureColumnExists("case_session", "log_path TEXT", error)) {
        return false;
    }

    return true;
}

bool SqliteStore::saveRequest(const HttpRequestData& request, QString* error) {
    QSqlQuery q(db_);
    q.prepare(
        "INSERT INTO request_history(case_id,method,url,headers_text,body_text,trace_id_hint,started_at,timeout_ms,retry_count,created_at) "
        "VALUES(?,?,?,?,?,?,?,?,?,?)");
    q.addBindValue(request.caseId);
    q.addBindValue(request.method);
    q.addBindValue(request.url);
    q.addBindValue(request.headersText);
    q.addBindValue(request.bodyText);
    q.addBindValue(request.traceIdHint);
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    q.addBindValue(request.timeoutMs);
    q.addBindValue(request.retryCount);
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));

    if (!q.exec()) {
        if (error) {
            *error = q.lastError().text();
        }
        return false;
    }
    return true;
}

QVector<HttpRequestData> SqliteStore::loadRecentRequests(int limit, QString* error) const {
    QVector<HttpRequestData> out;
    QSqlQuery q(db_);
    q.prepare(
        "SELECT case_id,method,url,headers_text,body_text,trace_id_hint,timeout_ms,retry_count "
        "FROM request_history ORDER BY id DESC LIMIT ?");
    q.addBindValue(limit);

    if (!q.exec()) {
        if (error) {
            *error = q.lastError().text();
        }
        return out;
    }

    while (q.next()) {
        HttpRequestData req;
        req.caseId = q.value(0).toInt();
        req.method = q.value(1).toString();
        req.url = q.value(2).toString();
        req.headersText = q.value(3).toString();
        req.bodyText = q.value(4).toString();
        req.traceIdHint = q.value(5).toString();
        req.timeoutMs = q.value(6).toInt();
        req.retryCount = q.value(7).toInt();
        out.push_back(req);
    }

    return out;
}

QVector<HttpRequestData> SqliteStore::loadRecentRequestsByCase(int caseId, int limit, QString* error) const {
    QVector<HttpRequestData> out;
    QSqlQuery q(db_);
    q.prepare(
        "SELECT case_id,method,url,headers_text,body_text,trace_id_hint,timeout_ms,retry_count "
        "FROM request_history WHERE case_id=? ORDER BY id DESC LIMIT ?");
    q.addBindValue(caseId);
    q.addBindValue(limit);

    if (!q.exec()) {
        if (error) {
            *error = q.lastError().text();
        }
        return out;
    }

    while (q.next()) {
        HttpRequestData req;
        req.caseId = q.value(0).toInt();
        req.method = q.value(1).toString();
        req.url = q.value(2).toString();
        req.headersText = q.value(3).toString();
        req.bodyText = q.value(4).toString();
        req.traceIdHint = q.value(5).toString();
        req.timeoutMs = q.value(6).toInt();
        req.retryCount = q.value(7).toInt();
        out.push_back(req);
    }

    return out;
}

int SqliteStore::createCaseSession(const QString& title,
                                   const QString& problemDescription,
                                   const QString& targetEndpoint,
                                   QString* error) {
    QSqlQuery q(db_);
    q.prepare(
        "INSERT INTO case_session("
        "title,problem_description,target_endpoint,analysis_mode,incident_trace_id,use_incident_time,incident_time,"
        "request_method,request_url,request_headers,request_body,request_trace_hint,request_timeout_ms,request_retry_count,log_path,"
        "created_at,updated_at"
        ") VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    q.addBindValue(title);
    q.addBindValue(problemDescription);
    q.addBindValue(targetEndpoint);
    q.addBindValue("log");
    q.addBindValue(QString());
    q.addBindValue(0);
    q.addBindValue(QString());
    q.addBindValue("GET");
    q.addBindValue(QString());
    q.addBindValue(QString());
    q.addBindValue(QString());
    q.addBindValue(QString());
    q.addBindValue(5000);
    q.addBindValue(1);
    q.addBindValue(QString());
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    q.addBindValue(now);
    q.addBindValue(now);

    if (!q.exec()) {
        if (error) {
            *error = q.lastError().text();
        }
        return -1;
    }
    return q.lastInsertId().toInt();
}

bool SqliteStore::saveCaseSession(const CaseSession& session, QString* error) {
    QSqlQuery q(db_);
    q.prepare(
        "UPDATE case_session SET "
        "problem_description=?, target_endpoint=?, analysis_mode=?, incident_trace_id=?, use_incident_time=?, incident_time=?, "
        "request_method=?, request_url=?, request_headers=?, request_body=?, request_trace_hint=?, request_timeout_ms=?, request_retry_count=?, log_path=?, updated_at=? "
        "WHERE id=?");
    q.addBindValue(session.problemDescription);
    q.addBindValue(session.targetEndpoint);
    q.addBindValue(session.analysisMode);
    q.addBindValue(session.incidentTraceId);
    q.addBindValue(session.useIncidentTime ? 1 : 0);
    q.addBindValue(session.useIncidentTime && session.incidentTime.isValid() ? session.incidentTime.toString(Qt::ISODate) : QString());
    q.addBindValue(session.requestDraft.method.isEmpty() ? "GET" : session.requestDraft.method);
    q.addBindValue(session.requestDraft.url);
    q.addBindValue(session.requestDraft.headersText);
    q.addBindValue(session.requestDraft.bodyText);
    q.addBindValue(session.requestDraft.traceIdHint);
    q.addBindValue(session.requestDraft.timeoutMs > 0 ? session.requestDraft.timeoutMs : 5000);
    q.addBindValue(session.requestDraft.retryCount > 0 ? session.requestDraft.retryCount : 1);
    q.addBindValue(session.logPath);
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    q.addBindValue(session.id);

    if (!q.exec()) {
        if (error) {
            *error = q.lastError().text();
        }
        return false;
    }
    return true;
}

bool SqliteStore::updateCaseSession(int caseId,
                                    const QString& problemDescription,
                                    const QString& targetEndpoint,
                                    QString* error) {
    QSqlQuery q(db_);
    q.prepare(
        "UPDATE case_session SET problem_description=?, target_endpoint=?, updated_at=? WHERE id=?");
    q.addBindValue(problemDescription);
    q.addBindValue(targetEndpoint);
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    q.addBindValue(caseId);

    if (!q.exec()) {
        if (error) {
            *error = q.lastError().text();
        }
        return false;
    }
    return true;
}

bool SqliteStore::deleteCaseSession(int caseId, QString* error) {
    if (!db_.transaction()) {
        if (error) {
            *error = db_.lastError().text();
        }
        return false;
    }

    QSqlQuery q1(db_);
    q1.prepare("DELETE FROM request_history WHERE case_id=?");
    q1.addBindValue(caseId);
    if (!q1.exec()) {
        db_.rollback();
        if (error) {
            *error = q1.lastError().text();
        }
        return false;
    }

    QSqlQuery q2(db_);
    q2.prepare("DELETE FROM case_session WHERE id=?");
    q2.addBindValue(caseId);
    if (!q2.exec()) {
        db_.rollback();
        if (error) {
            *error = q2.lastError().text();
        }
        return false;
    }

    if (!db_.commit()) {
        db_.rollback();
        if (error) {
            *error = db_.lastError().text();
        }
        return false;
    }
    return true;
}

QVector<CaseSession> SqliteStore::loadCaseSessions(int limit, QString* error) const {
    QVector<CaseSession> out;
    QSqlQuery q(db_);
    q.prepare(
        "SELECT id,title,problem_description,target_endpoint,analysis_mode,incident_trace_id,use_incident_time,incident_time,"
        "request_method,request_url,request_headers,request_body,request_trace_hint,request_timeout_ms,request_retry_count,log_path,"
        "created_at,updated_at "
        "FROM case_session ORDER BY updated_at DESC LIMIT ?");
    q.addBindValue(limit);

    if (!q.exec()) {
        if (error) {
            *error = q.lastError().text();
        }
        return out;
    }

    while (q.next()) {
        CaseSession c;
        c.id = q.value(0).toInt();
        c.title = q.value(1).toString();
        c.problemDescription = q.value(2).toString();
        c.targetEndpoint = q.value(3).toString();
        c.analysisMode = q.value(4).toString().trimmed().isEmpty() ? "log" : q.value(4).toString().trimmed();
        c.incidentTraceId = q.value(5).toString();
        c.useIncidentTime = q.value(6).toInt() != 0;
        c.incidentTime = QDateTime::fromString(q.value(7).toString(), Qt::ISODate);
        c.requestDraft.caseId = c.id;
        c.requestDraft.method = q.value(8).toString().trimmed().isEmpty() ? "GET" : q.value(8).toString().trimmed();
        c.requestDraft.url = q.value(9).toString();
        c.requestDraft.headersText = q.value(10).toString();
        c.requestDraft.bodyText = q.value(11).toString();
        c.requestDraft.traceIdHint = q.value(12).toString();
        c.requestDraft.timeoutMs = q.value(13).toInt() > 0 ? q.value(13).toInt() : 5000;
        c.requestDraft.retryCount = q.value(14).toInt() > 0 ? q.value(14).toInt() : 1;
        c.logPath = q.value(15).toString();
        c.createdAt = QDateTime::fromString(q.value(16).toString(), Qt::ISODate);
        c.updatedAt = QDateTime::fromString(q.value(17).toString(), Qt::ISODate);
        out.push_back(c);
    }

    return out;
}

bool SqliteStore::setSetting(const QString& key, const QString& value, QString* error) {
    QSqlQuery q(db_);
    q.prepare(
        "INSERT INTO app_settings(key,value,updated_at) VALUES(?,?,?) "
        "ON CONFLICT(key) DO UPDATE SET value=excluded.value, updated_at=excluded.updated_at");
    q.addBindValue(key);
    q.addBindValue(value);
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));

    if (!q.exec()) {
        if (error) {
            *error = q.lastError().text();
        }
        return false;
    }
    return true;
}

QString SqliteStore::getSetting(const QString& key, const QString& defaultValue) const {
    QSqlQuery q(db_);
    q.prepare("SELECT value FROM app_settings WHERE key=? LIMIT 1");
    q.addBindValue(key);
    if (!q.exec()) {
        return defaultValue;
    }
    if (!q.next()) {
        return defaultValue;
    }
    return q.value(0).toString();
}
