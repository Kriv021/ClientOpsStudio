#include "domain/DiagnosisEngine.h"

#include <algorithm>
#include <QHash>
#include <QSet>

namespace {

struct CandidateState {
    RootCauseCandidate candidate;
};

EvidenceItem makeEvidence(const DomainSignal& signal, const QString& title, int weight, bool primary) {
    EvidenceItem item;
    item.title = title;
    item.snippet = signal.rawLine;
    item.module = signal.module;
    item.timestamp = signal.timestamp;
    item.signalType = signal.type;
    item.weight = weight;
    item.isPrimary = primary;
    return item;
}

void addEvidence(CandidateState& state, const DomainSignal& signal, const QString& title, int weight, bool primary) {
    EvidenceItem item = makeEvidence(signal, title, weight, primary);
    if (primary) {
        state.candidate.primaryEvidence.push_back(item);
    } else {
        state.candidate.secondaryEvidence.push_back(item);
    }
}

void score(CandidateState& state,
           int delta,
           const DomainSignal& signal,
           const QString& title,
           bool primary = true) {
    state.candidate.score += delta;
    addEvidence(state, signal, title, delta, primary);
}

QString confidenceFrom(const QString& correlationMode, int topScore, int secondScore) {
    if (correlationMode == "trace-id" && topScore >= 7 && topScore - secondScore >= 3) {
        return "高";
    }
    if ((correlationMode == "incident-time" || correlationMode == "request-time") && topScore >= 6) {
        return "中";
    }
    if (topScore >= 8 && topScore - secondScore >= 4) {
        return "中";
    }
    return "低";
}

QString candidateSummary(const QString& name, const RootCauseCandidate& candidate) {
    return QString("%1（得分 %2）").arg(name).arg(candidate.score);
}

QString candidateNextSteps(const QString& name) {
    if (name == "下游依赖超时") {
        return "优先检查下游服务 RT、超时配置、线程池与连接池。";
    }
    if (name == "下游依赖连接失败") {
        return "检查目标服务可达性、DNS、端口与网络策略。";
    }
    if (name == "数据库死锁/锁竞争") {
        return "按事务顺序与 SQL 锁竞争路径排查，并补充重试策略。";
    }
    if (name == "数据库慢查询/数据库压力") {
        return "检查慢 SQL、连接池配置和高峰期数据库负载。";
    }
    if (name == "认证/权限失败") {
        return "检查 token、签名、权限范围与网关鉴权配置。";
    }
    if (name == "路由/资源不存在") {
        return "核对路径、版本、路由映射与 handler 发布状态。";
    }
    if (name == "限流/流控") {
        return "检查网关限流、熔断、下游降级与重试退避策略。";
    }
    return "补充 reqId/traceId 或故障时间，并扩大日志时间窗后重试。";
}

void pushFinding(DiagnosisResult& result, const RootCauseCandidate& candidate, int severity) {
    RuleFinding finding;
    finding.title = candidate.name;
    finding.evidence = candidate.primaryEvidence.isEmpty() ? candidate.summary : candidate.primaryEvidence.first().snippet;
    finding.suggestion = candidate.nextSteps;
    finding.severity = severity;
    result.findings.push_back(finding);
}

} // namespace

DiagnosisResult DiagnosisEngine::diagnose(const HttpResponseData& response,
                                          const QVector<DomainSignal>& signalList,
                                          const QString& correlationMode,
                                          const QString& correlationEvidence) const {
    DiagnosisResult result;
    result.correlationMode = correlationMode;
    result.correlationEvidence = correlationEvidence;
    result.correlatedCount = signalList.size();

    // 诊断层不再直接匹配原始日志，而是对候选根因做加减分。
    // 这样后续换日志文案时，优先改 SignalExtractor 即可。
    QHash<QString, CandidateState> states;
    const QStringList categories = {
        "下游依赖超时",
        "下游依赖连接失败",
        "数据库死锁/锁竞争",
        "数据库慢查询/数据库压力",
        "认证/权限失败",
        "路由/资源不存在",
        "限流/流控",
        "证据不足/未知"
    };
    for (const auto& name : categories) {
        CandidateState state;
        state.candidate.name = name;
        state.candidate.nextSteps = candidateNextSteps(name);
        states.insert(name, state);
    }

    const auto hasFailureResponse = response.statusCode >= 400 || !response.errorMessage.trimmed().isEmpty() || !response.success;
    if (correlationMode == "incident-time" || correlationMode == "full-log-scan") {
        states["证据不足/未知"].candidate.score += 3;
    }

    QSet<QString> seenIds;
    for (const auto& signal : signalList) {
        if (!signal.reqId.isEmpty()) {
            seenIds.insert(signal.reqId);
        }

        switch (signal.type) {
        case DomainSignalType::UpstreamTimeout:
        case DomainSignalType::PartnerTimeout:
            score(states["下游依赖超时"], 5, signal, "下游超时主信号");
            break;
        case DomainSignalType::UpstreamRefused:
            score(states["下游依赖连接失败"], 5, signal, "下游连接失败主信号");
            break;
        case DomainSignalType::DbDeadlock:
            score(states["数据库死锁/锁竞争"], 6, signal, "数据库死锁主信号");
            break;
        case DomainSignalType::SlowQuery:
            score(states["数据库慢查询/数据库压力"], 3, signal, "慢查询信号");
            score(states["数据库死锁/锁竞争"], 1, signal, "死锁辅助慢查询", false);
            break;
        case DomainSignalType::RedisLatencyWarn:
            score(states["证据不足/未知"], 1, signal, "伴随噪声信号", false);
            break;
        case DomainSignalType::ConnectionPoolBusy:
            score(states["数据库慢查询/数据库压力"], 3, signal, "连接池压力信号");
            break;
        case DomainSignalType::RequestFailed:
            score(states["下游依赖超时"], 2, signal, "请求失败收口", false);
            score(states["下游依赖连接失败"], 2, signal, "请求失败收口", false);
            score(states["数据库死锁/锁竞争"], 2, signal, "请求失败收口", false);
            score(states["数据库慢查询/数据库压力"], 1, signal, "请求失败收口", false);
            break;
        case DomainSignalType::HttpStatusSeen: {
            const int status = signal.attrs.value("status").toInt();
            if (status >= 500) {
                score(states["下游依赖超时"], 2, signal, "5xx 收口", false);
                score(states["下游依赖连接失败"], 1, signal, "5xx 收口", false);
                score(states["数据库死锁/锁竞争"], 2, signal, "5xx 收口", false);
                score(states["数据库慢查询/数据库压力"], 1, signal, "5xx 收口", false);
            } else if (status == 401 || status == 403) {
                score(states["认证/权限失败"], 6, signal, "HTTP 认证失败");
            } else if (status == 404) {
                score(states["路由/资源不存在"], 6, signal, "HTTP 404");
            } else if (status == 429) {
                score(states["限流/流控"], 6, signal, "HTTP 429");
            }
            break;
        }
        case DomainSignalType::BusinessErrCodeSeen: {
            const QString errCode = signal.attrs.value("errCode");
            if (errCode.contains("TIMEOUT", Qt::CaseInsensitive) ||
                errCode.contains("RESERVE_FAIL", Qt::CaseInsensitive) ||
                errCode.contains("CALLBACK_CONFIRM_TIMEOUT", Qt::CaseInsensitive)) {
                score(states["下游依赖超时"], 3, signal, "业务错误码收口", false);
            }
            if (errCode.contains("DEADLOCK", Qt::CaseInsensitive)) {
                score(states["数据库死锁/锁竞争"], 3, signal, "业务错误码收口", false);
            }
            if (errCode.contains("AUTH", Qt::CaseInsensitive) || errCode.contains("TOKEN", Qt::CaseInsensitive)) {
                score(states["认证/权限失败"], 3, signal, "业务错误码收口", false);
            }
            break;
        }
        case DomainSignalType::RequestStarted:
        case DomainSignalType::RequestCompleted:
            break;
        }
    }

    if (!hasFailureResponse) {
        states["证据不足/未知"].candidate.score += 2;
    }
    if (seenIds.size() > 2 && correlationMode == "full-log-scan") {
        states["证据不足/未知"].candidate.score += 2;
    }

    QVector<RootCauseCandidate> ranked;
    ranked.reserve(states.size());
    for (auto it = states.begin(); it != states.end(); ++it) {
        auto candidate = it.value().candidate;
        if (candidate.score <= 0 && candidate.name != "证据不足/未知") {
            continue;
        }
        candidate.summary = candidateSummary(candidate.name, candidate);
        ranked.push_back(candidate);
    }

    std::sort(ranked.begin(), ranked.end(), [](const RootCauseCandidate& lhs, const RootCauseCandidate& rhs) {
        if (lhs.score != rhs.score) {
            return lhs.score > rhs.score;
        }
        return lhs.name < rhs.name;
    });

    if (ranked.isEmpty()) {
        RootCauseCandidate unknown;
        unknown.name = "证据不足/未知";
        unknown.score = 1;
        unknown.summary = candidateSummary(unknown.name, unknown);
        unknown.nextSteps = candidateNextSteps(unknown.name);
        ranked.push_back(unknown);
    }

    // 内部可以保留更多候选，但对外和给 AI 复核时只保留 top3，
    // 否则信息噪声会明显上升。
    if (ranked.size() > 3) {
        ranked.resize(3);
    }

    if (correlationMode != "trace-id") {
        bool hasUnknown = false;
        for (const auto& candidate : ranked) {
            if (candidate.name == "证据不足/未知") {
                hasUnknown = true;
                break;
            }
        }
        if (!hasUnknown) {
            auto unknown = states["证据不足/未知"].candidate;
            if (unknown.score <= 0) {
                unknown.score = 1;
            }
            unknown.summary = candidateSummary(unknown.name, unknown);
            unknown.nextSteps = candidateNextSteps(unknown.name);
            if (ranked.size() == 3) {
                ranked[2] = unknown;
            } else {
                ranked.push_back(unknown);
            }
        }
    }

    result.candidates = ranked;
    result.rulePrimaryCause = ranked.first().name;
    result.finalConclusion = result.rulePrimaryCause;

    const int topScore = ranked.first().score;
    const int secondScore = ranked.size() > 1 ? ranked.at(1).score : 0;
    result.confidence = confidenceFrom(correlationMode, topScore, secondScore);

    for (const auto& candidate : ranked) {
        for (const auto& item : candidate.primaryEvidence) {
            result.evidenceItems.push_back(item);
        }
    }

    if (result.evidenceItems.isEmpty() && !signalList.isEmpty()) {
        const auto& first = signalList.first();
        result.evidenceItems.push_back(makeEvidence(first, "基础关联证据", 1, true));
    }

    for (const auto& candidate : ranked) {
        pushFinding(result, candidate, candidate.name == result.rulePrimaryCause ? 3 : 2);
    }

    if (result.rulePrimaryCause == "下游依赖超时") {
        result.summary = QString("更接近下游依赖超时，候选已按证据强度排序，关联可信度：%1。").arg(result.confidence);
    } else if (result.rulePrimaryCause == "数据库死锁/锁竞争") {
        result.summary = QString("更接近数据库死锁或锁竞争，已结合失败收口与辅助慢查询信号，关联可信度：%1。").arg(result.confidence);
    } else if (result.rulePrimaryCause == "证据不足/未知") {
        result.summary = QString("当前证据不足以稳定指向单一根因，建议补充 reqId/traceId 或扩大时间窗。");
    } else {
        result.summary = QString("当前最可能根因为“%1”，关联可信度：%2。").arg(result.rulePrimaryCause, result.confidence);
    }

    return result;
}
