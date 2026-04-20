#include "infra/MarkdownExporter.h"

#include <QDateTime>
#include <QTextStream>

QString MarkdownExporter::buildReport(const CaseSession& caseSession,
                                      const HttpRequestData& request,
                                      const HttpResponseData& response,
                                      const QVector<LogRecord>& scopedLogs,
                                      const DiagnosisResult& diagnosis,
                                      const QString& aiAdvice) {
    QString out;
    QTextStream s(&out);

    s << "# ClientOps 排障报告\n\n";

    s << "## 1. Case 信息\n";
    s << "- Case ID: " << caseSession.id << "\n";
    s << "- 标题: " << caseSession.title << "\n";
    s << "- 问题描述: " << caseSession.problemDescription << "\n";
    s << "- 目标接口: " << caseSession.targetEndpoint << "\n";
    s << "- 生成时间: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";

    s << "\n## 2. 请求概览\n";
    s << "- 方法: " << request.method << "\n";
    s << "- URL: " << request.url << "\n";
    s << "- TraceHint: " << request.traceIdHint << "\n";
    s << "- 超时(ms): " << request.timeoutMs << "\n";
    s << "- 重试次数: " << request.retryCount << "\n";
    s << "- 响应状态: " << response.statusCode << "\n";
    s << "- 耗时(ms): " << response.latencyMs << "\n";
    s << "- 成功: " << (response.success ? "是" : "否") << "\n";
    if (!response.errorMessage.isEmpty()) {
        s << "- 错误: " << response.errorMessage << "\n";
    }

    s << "\n## 3. 响应片段\n";
    const QString resp = response.responseBody.left(1200);
    s << "```\n" << resp << "\n```\n";

    s << "\n## 4. 关联信息\n";
    s << "- 关联方式: " << diagnosis.correlationMode << "\n";
    s << "- 关联依据: " << diagnosis.correlationEvidence << "\n";
    s << "- 关联可信度: " << diagnosis.confidence << "\n";
    s << "- 命中日志条数: " << diagnosis.correlatedCount << "\n";

    s << "\n## 5. 关联日志（窗口）\n";
    s << "共 " << scopedLogs.size() << " 行\n\n";
    s << "```\n";
    int shown = 0;
    for (const LogRecord& rec : scopedLogs) {
        s << rec.rawLine << "\n";
        if (++shown >= 60) {
            s << "... (日志过长，已截断)\n";
            break;
        }
    }
    s << "```\n";

    s << "\n## 6. 规则诊断\n";
    s << "- 摘要: " << diagnosis.summary << "\n\n";
    for (const RuleFinding& f : diagnosis.findings) {
        s << "### " << f.title << "\n";
        s << "- 严重级别: " << f.severity << "\n";
        s << "- 证据: " << f.evidence << "\n";
        s << "- 建议: " << f.suggestion << "\n\n";
    }

    if (!aiAdvice.trimmed().isEmpty()) {
        s << "## 7. AI 建议\n";
        s << aiAdvice << "\n";
    }

    return out;
}
