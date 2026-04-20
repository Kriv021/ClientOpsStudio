
#include "app/MainWindow.h"

#include "infra/MarkdownExporter.h"

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QAction>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStringConverter>
#include <QTabWidget>
#include <QTextStream>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setupUi();
    applyTheme();
    initStore();
    loadCaseSessions();
    loadAiConfig();
    wireSignals();
    restoreSplitterState();
    setEmptyStates();
    onCaseChanged();
}

void MainWindow::setupUi() {
    setWindowTitle("ClientOps Studio");
    resize(1480, 920);
    setMinimumSize(1220, 760);

    auto* fileMenu = menuBar()->addMenu("文件");
    auto* exportAction = fileMenu->addAction("导出报告");
    fileMenu->addSeparator();
    auto* exitAction = fileMenu->addAction("退出");

    auto* viewMenu = menuBar()->addMenu("视图");
    auto* resetLayoutAction = viewMenu->addAction("重置布局");

    auto* helpMenu = menuBar()->addMenu("帮助");
    auto* helpAction = helpMenu->addAction("使用说明");

    central_ = new QWidget(this);
    setCentralWidget(central_);

    auto* root = new QVBoxLayout(central_);
    root->setContentsMargins(18, 18, 18, 14);
    root->setSpacing(16);

    verticalSplitter_ = new QSplitter(Qt::Horizontal, central_);
    verticalSplitter_->setHandleWidth(8);
    verticalSplitter_->setChildrenCollapsible(false);

    auto* sidebar = new QFrame(verticalSplitter_);
    sidebar->setObjectName("sidebar");
    sidebar->setMinimumWidth(248);
    sidebar->setMaximumWidth(304);
    auto* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(18, 18, 18, 18);
    sidebarLayout->setSpacing(12);

    pageTitle_ = new QLabel("ClientOps", sidebar);
    pageTitle_->setObjectName("pageTitle");
    pageDesc_ = new QLabel("故障排查工作台", sidebar);
    pageDesc_->setObjectName("pageDesc");

    caseSearchEdit_ = new QLineEdit(sidebar);
    caseSearchEdit_->setPlaceholderText("搜索案例");
    caseSearchEdit_->setClearButtonEnabled(true);

    caseList_ = new QListWidget(sidebar);
    caseList_->setObjectName("caseList");
    caseList_->setAlternatingRowColors(false);
    caseList_->setSpacing(4);

    caseBox_ = new QComboBox(sidebar);
    caseBox_->hide();

    newCaseBtn_ = new QPushButton(QIcon(":/icons/plus.svg"), "新建案例", sidebar);
    newCaseBtn_->setProperty("variant", "secondary");
    deleteCaseBtn_ = new QPushButton(QIcon(":/icons/delete.svg"), "删除案例", sidebar);
    deleteCaseBtn_->setProperty("variant", "danger");
    aiConfigBtn_ = new QPushButton(QIcon(":/icons/settings.svg"), "AI API 配置", sidebar);
    aiConfigBtn_->setProperty("variant", "secondary");
    auto* helpBtn = new QPushButton(QIcon(":/icons/help.svg"), "使用帮助", sidebar);
    helpBtn->setProperty("variant", "secondary");

    sidebarLayout->addWidget(pageTitle_);
    sidebarLayout->addWidget(pageDesc_);
    sidebarLayout->addSpacing(6);
    sidebarLayout->addWidget(caseSearchEdit_);
    sidebarLayout->addWidget(caseList_, 1);
    sidebarLayout->addWidget(newCaseBtn_);
    sidebarLayout->addWidget(aiConfigBtn_);
    sidebarLayout->addWidget(helpBtn);
    sidebarLayout->addWidget(deleteCaseBtn_);

    auto* workspace = new QFrame(verticalSplitter_);
    workspace->setObjectName("workspace");
    auto* workspaceLayout = new QVBoxLayout(workspace);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->setSpacing(14);

    auto* summaryCard = new QFrame(workspace);
    summaryCard->setObjectName("summaryCard");
    auto* summaryLayout = new QVBoxLayout(summaryCard);
    summaryLayout->setContentsMargins(20, 18, 20, 18);
    summaryLayout->setSpacing(14);

    currentCaseTitleLabel_ = new QLabel("未选择案例", summaryCard);
    currentCaseTitleLabel_->setObjectName("caseHeaderTitle");
    currentCaseMetaLabel_ = new QLabel("先在左侧创建案例，或载入示例场景。", summaryCard);
    currentCaseMetaLabel_->setObjectName("caseHeaderMeta");

    caseProblemEdit_ = new QLineEdit(summaryCard);
    caseProblemEdit_->setPlaceholderText("例如：用户反馈下单接口偶发 500，需确认根因");
    caseProblemEdit_->setClearButtonEnabled(true);
    caseTargetEdit_ = new QLineEdit(summaryCard);
    caseTargetEdit_->setPlaceholderText("例如：/api/v1/orders");
    caseTargetEdit_->setClearButtonEnabled(true);

    auto* summaryFields = new QGridLayout();
    summaryFields->setHorizontalSpacing(16);
    summaryFields->setVerticalSpacing(8);
    auto* problemLabel = new QLabel("问题描述", summaryCard);
    problemLabel->setObjectName("fieldLabel");
    auto* targetLabel = new QLabel("目标接口", summaryCard);
    targetLabel->setObjectName("fieldLabel");
    summaryFields->addWidget(problemLabel, 0, 0);
    summaryFields->addWidget(targetLabel, 0, 1);
    summaryFields->addWidget(caseProblemEdit_, 1, 0);
    summaryFields->addWidget(caseTargetEdit_, 1, 1);
    summaryFields->setColumnStretch(0, 3);
    summaryFields->setColumnStretch(1, 2);

    summaryLayout->addWidget(currentCaseTitleLabel_);
    summaryLayout->addWidget(currentCaseMetaLabel_);
    summaryLayout->addLayout(summaryFields);

    workTabs_ = new QTabWidget(workspace);
    workTabs_->setObjectName("workTabs");
    workTabs_->setDocumentMode(true);

    auto* requestTab = new QWidget(workTabs_);
    auto* requestTabLayout = new QVBoxLayout(requestTab);
    requestTabLayout->setContentsMargins(0, 10, 0, 0);
    requestTabLayout->setSpacing(12);

    topSplitter_ = new QSplitter(Qt::Horizontal, requestTab);
    topSplitter_->setHandleWidth(8);
    topSplitter_->setChildrenCollapsible(false);

    auto* requestScroll = new QScrollArea(topSplitter_);
    requestScroll->setWidgetResizable(true);
    requestScroll->setFrameShape(QFrame::NoFrame);
    requestScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* requestPane = new QFrame(requestScroll);
    requestPane->setObjectName("panel");
    auto* reqLayout = new QVBoxLayout(requestPane);
    reqLayout->setContentsMargins(18, 18, 18, 18);
    reqLayout->setSpacing(12);

    auto* requestTitleLabel = new QLabel("请求验证", requestPane);
    requestTitleLabel->setObjectName("sectionTitle");
    auto* requestDescLabel = new QLabel("仅在需要验证修复、补发相同请求或观察响应差异时使用。", requestPane);
    requestDescLabel->setObjectName("sectionHint");

    historyBox_ = new QComboBox(requestPane);
    historyBox_->setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);
    loadHistoryBtn_ = new QPushButton("填充历史请求", requestPane);
    loadHistoryBtn_->setProperty("variant", "secondary");

    auto* historyLabel = new QLabel("历史请求", requestPane);
    historyLabel->setObjectName("fieldLabel");
    auto* historyRow = new QHBoxLayout();
    historyRow->setSpacing(10);
    historyRow->addWidget(historyBox_, 1);
    historyRow->addWidget(loadHistoryBtn_);

    methodBox_ = new QComboBox(requestPane);
    methodBox_->addItems({"GET", "POST"});
    urlEdit_ = new QLineEdit(requestPane);
    urlEdit_->setPlaceholderText("https://httpbin.org/anything/api/v1/orders");
    traceHintEdit_ = new QLineEdit(requestPane);
    traceHintEdit_->setPlaceholderText("可选：请求级 reqId / traceId");
    timeoutSpin_ = new QSpinBox(requestPane);
    timeoutSpin_->setRange(100, 60000);
    timeoutSpin_->setValue(5000);
    timeoutSpin_->setSuffix(" ms");
    retrySpin_ = new QSpinBox(requestPane);
    retrySpin_->setRange(0, 3);
    retrySpin_->setValue(1);

    headersEdit_ = new QPlainTextEdit(requestPane);
    headersEdit_->setPlaceholderText("Content-Type: application/json\nAuthorization: Bearer <token>");
    headersEdit_->setMinimumHeight(110);
    bodyEdit_ = new QPlainTextEdit(requestPane);
    bodyEdit_->setPlaceholderText("{\n  \"id\": 1001\n}");
    bodyEdit_->setMinimumHeight(160);

    auto* methodLabel = new QLabel("方法", requestPane);
    methodLabel->setObjectName("fieldLabel");
    auto* urlLabel = new QLabel("请求地址", requestPane);
    urlLabel->setObjectName("fieldLabel");
    auto* traceLabel = new QLabel("请求关联 ID", requestPane);
    traceLabel->setObjectName("fieldLabel");
    auto* timeoutLabel = new QLabel("超时", requestPane);
    timeoutLabel->setObjectName("fieldLabel");
    auto* retryLabel = new QLabel("重试", requestPane);
    retryLabel->setObjectName("fieldLabel");
    auto* headersLabel = new QLabel("请求头", requestPane);
    headersLabel->setObjectName("fieldLabel");
    auto* bodyLabel = new QLabel("请求体", requestPane);
    bodyLabel->setObjectName("fieldLabel");

    auto* requestMetaGrid = new QGridLayout();
    requestMetaGrid->setHorizontalSpacing(12);
    requestMetaGrid->setVerticalSpacing(8);
    requestMetaGrid->addWidget(methodLabel, 0, 0);
    requestMetaGrid->addWidget(timeoutLabel, 0, 1);
    requestMetaGrid->addWidget(retryLabel, 0, 2);
    requestMetaGrid->addWidget(methodBox_, 1, 0);
    requestMetaGrid->addWidget(timeoutSpin_, 1, 1);
    requestMetaGrid->addWidget(retrySpin_, 1, 2);

    auto* requestBodyGrid = new QGridLayout();
    requestBodyGrid->setHorizontalSpacing(12);
    requestBodyGrid->setVerticalSpacing(8);
    requestBodyGrid->addWidget(headersLabel, 0, 0);
    requestBodyGrid->addWidget(bodyLabel, 0, 1);
    requestBodyGrid->addWidget(headersEdit_, 1, 0);
    requestBodyGrid->addWidget(bodyEdit_, 1, 1);
    requestBodyGrid->setColumnStretch(0, 1);
    requestBodyGrid->setColumnStretch(1, 1);

    auto* reqActions = new QHBoxLayout();
    reqActions->setSpacing(10);
    sendBtn_ = new QPushButton(QIcon(":/icons/request.svg"), "发送请求", requestPane);
    sendBtn_->setProperty("variant", "primary");
    sampleRequestBtn_ = new QPushButton("载入示例场景", requestPane);
    sampleRequestBtn_->setProperty("variant", "secondary");
    reqActions->addWidget(sampleRequestBtn_);
    reqActions->addStretch(1);
    reqActions->addWidget(sendBtn_);

    reqLayout->addWidget(requestTitleLabel);
    reqLayout->addWidget(requestDescLabel);
    reqLayout->addWidget(historyLabel);
    reqLayout->addLayout(historyRow);
    reqLayout->addWidget(urlLabel);
    reqLayout->addWidget(urlEdit_);
    reqLayout->addLayout(requestMetaGrid);
    reqLayout->addWidget(traceLabel);
    reqLayout->addWidget(traceHintEdit_);
    reqLayout->addLayout(requestBodyGrid);
    reqLayout->addStretch(1);
    reqLayout->addLayout(reqActions);
    requestScroll->setWidget(requestPane);

    auto* responsePane = new QFrame(topSplitter_);
    responsePane->setObjectName("panel");
    auto* respLayout = new QVBoxLayout(responsePane);
    respLayout->setContentsMargins(18, 18, 18, 18);
    respLayout->setSpacing(12);

    auto* responseTitle = new QLabel("响应结果", responsePane);
    responseTitle->setObjectName("sectionTitle");
    auto* responseHint = new QLabel("这里用于观察补发请求后的状态码、错误信息和返回正文。", responsePane);
    responseHint->setObjectName("sectionHint");

    auto* statRow = new QHBoxLayout();
    statRow->setSpacing(12);
    statusLabel_ = new QLabel("状态: 未执行", responsePane);
    latencyLabel_ = new QLabel("耗时: -", responsePane);
    errorLabel_ = new QLabel("错误: -", responsePane);
    copyResponseBtn_ = new QPushButton("复制响应", responsePane);
    copyResponseBtn_->setProperty("variant", "secondary");
    statRow->addWidget(statusLabel_);
    statRow->addWidget(latencyLabel_);
    statRow->addWidget(errorLabel_);
    statRow->addStretch(1);
    statRow->addWidget(copyResponseBtn_);

    responseEdit_ = new QPlainTextEdit(responsePane);
    responseEdit_->setReadOnly(true);
    responseEdit_->setProperty("role", "output");
    responseEdit_->setLineWrapMode(QPlainTextEdit::NoWrap);

    respLayout->addWidget(responseTitle);
    respLayout->addWidget(responseHint);
    respLayout->addLayout(statRow);
    respLayout->addWidget(responseEdit_);
    topSplitter_->setSizes({580, 740});
    requestTabLayout->addWidget(topSplitter_, 1);

    auto* logTab = new QWidget(workTabs_);
    auto* logTabLayout = new QVBoxLayout(logTab);
    logTabLayout->setContentsMargins(0, 10, 0, 0);
    logTabLayout->setSpacing(12);

    bottomSplitter_ = new QSplitter(Qt::Horizontal, logTab);
    bottomSplitter_->setHandleWidth(8);
    bottomSplitter_->setChildrenCollapsible(false);

    auto* logPane = new QFrame(bottomSplitter_);
    logPane->setObjectName("panel");
    auto* logLayout = new QVBoxLayout(logPane);
    logLayout->setContentsMargins(18, 18, 18, 18);
    logLayout->setSpacing(12);

    auto* logTitle = new QLabel("日志排查", logPane);
    logTitle->setObjectName("sectionTitle");
    auto* logHint = new QLabel("先导入故障时段日志，再通过时间窗、关键字和锚点缩小范围。", logPane);
    logHint->setObjectName("sectionHint");

    auto* logSummaryCard = new QFrame(logPane);
    logSummaryCard->setObjectName("summaryCard");
    auto* logSummaryLayout = new QVBoxLayout(logSummaryCard);
    logSummaryLayout->setContentsMargins(14, 12, 14, 12);
    logSummaryLayout->setSpacing(4);
    logFileSummaryLabel_ = new QLabel("文件：未导入", logSummaryCard);
    logFileSummaryLabel_->setObjectName("helperText");
    logStatsSummaryLabel_ = new QLabel("统计：-", logSummaryCard);
    logStatsSummaryLabel_->setObjectName("helperText");
    logRenderHintLabel_ = new QLabel("显示：导入后会展示解析耗时、reqId 数量和渲染截断信息。", logSummaryCard);
    logRenderHintLabel_->setObjectName("helperText");
    logRenderHintLabel_->setWordWrap(true);
    logSummaryLayout->addWidget(logFileSummaryLabel_);
    logSummaryLayout->addWidget(logStatsSummaryLabel_);
    logSummaryLayout->addWidget(logRenderHintLabel_);

    auto* filterRow = new QHBoxLayout();
    filterRow->setSpacing(10);
    importLogBtn_ = new QPushButton(QIcon(":/icons/log.svg"), "导入日志", logPane);
    importLogBtn_->setProperty("variant", "secondary");
    levelFilterBox_ = new QComboBox(logPane);
    levelFilterBox_->addItems({"ALL", "INFO", "WARN", "ERROR", "RAW"});
    keywordEdit_ = new QLineEdit(logPane);
    keywordEdit_->setPlaceholderText("关键字");
    windowSpin_ = new QSpinBox(logPane);
    windowSpin_->setRange(5, 300);
    windowSpin_->setValue(30);
    windowSpin_->setSuffix(" 秒");
    applyFilterBtn_ = new QPushButton("应用过滤", logPane);
    applyFilterBtn_->setProperty("variant", "secondary");

    filterRow->addWidget(importLogBtn_);
    filterRow->addWidget(new QLabel("级别", logPane));
    filterRow->addWidget(levelFilterBox_);
    filterRow->addWidget(new QLabel("关键字", logPane));
    filterRow->addWidget(keywordEdit_, 1);
    filterRow->addWidget(new QLabel("时间窗", logPane));
    filterRow->addWidget(windowSpin_);
    filterRow->addWidget(applyFilterBtn_);

    logView_ = new QPlainTextEdit(logPane);
    logView_->setReadOnly(true);
    logView_->setProperty("role", "output");
    logView_->setLineWrapMode(QPlainTextEdit::NoWrap);

    logLayout->addWidget(logTitle);
    logLayout->addWidget(logHint);
    logLayout->addWidget(logSummaryCard);
    logLayout->addLayout(filterRow);
    logLayout->addWidget(logView_, 1);

    auto* explainPane = new QFrame(bottomSplitter_);
    explainPane->setObjectName("sidePanel");
    auto* explainLayout = new QVBoxLayout(explainPane);
    explainLayout->setContentsMargins(18, 18, 18, 18);
    explainLayout->setSpacing(10);

    auto* anchorTitle = new QLabel("排查锚点", explainPane);
    anchorTitle->setObjectName("sectionTitle");
    auto* anchorHint = new QLabel("优先填写 reqId/traceId；没有时再用故障时间。", explainPane);
    anchorHint->setObjectName("sectionHint");

    modeBox_ = new QComboBox(explainPane);
    modeBox_->addItems({"补发请求验证", "按日志排查"});
    modeBox_->setCurrentIndex(1);
    modeBox_->setToolTip("补发请求验证：先发请求再诊断；按日志排查：可不发请求，直接按日志排查。");

    incidentTraceEdit_ = new QLineEdit(explainPane);
    incidentTraceEdit_->setPlaceholderText("reqId / traceId");
    incidentTraceEdit_->setClearButtonEnabled(true);

    useIncidentTimeCheck_ = new QCheckBox("使用故障时间", explainPane);
    incidentTimeEdit_ = new QDateTimeEdit(QDateTime::currentDateTime(), explainPane);
    incidentTimeEdit_->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    incidentTimeEdit_->setCalendarPopup(true);
    incidentTimeEdit_->setEnabled(false);

    auto* modeLabel = new QLabel("排查方式", explainPane);
    modeLabel->setObjectName("fieldLabel");
    auto* incidentIdLabel = new QLabel("故障关联 ID", explainPane);
    incidentIdLabel->setObjectName("fieldLabel");
    auto* incidentTimeLabel = new QLabel("故障时间", explainPane);
    incidentTimeLabel->setObjectName("fieldLabel");

    auto* explainText = new QLabel(
        "关联规则\n"
        "1. 已知 reqId/traceId 时优先精确命中\n"
        "2. 只知道故障时间时，按时间窗缩小日志\n"
        "3. 两者都没有时，补发请求才有意义；否则只能全量扫描",
        explainPane);
    explainText->setObjectName("helperText");
    explainText->setWordWrap(true);

    explainLayout->addWidget(anchorTitle);
    explainLayout->addWidget(anchorHint);
    explainLayout->addWidget(modeLabel);
    explainLayout->addWidget(modeBox_);
    explainLayout->addWidget(incidentIdLabel);
    explainLayout->addWidget(incidentTraceEdit_);
    explainLayout->addWidget(useIncidentTimeCheck_);
    explainLayout->addWidget(incidentTimeLabel);
    explainLayout->addWidget(incidentTimeEdit_);
    explainLayout->addSpacing(8);
    explainLayout->addWidget(explainText);
    explainLayout->addStretch(1);
    bottomSplitter_->setSizes({940, 320});

    logTabLayout->addWidget(bottomSplitter_, 1);

    auto* diagnosisTab = new QWidget(workTabs_);
    auto* diagnosisLayout = new QVBoxLayout(diagnosisTab);
    diagnosisLayout->setContentsMargins(0, 10, 0, 0);
    diagnosisLayout->setSpacing(12);

    auto* diagnosisHero = new QFrame(diagnosisTab);
    diagnosisHero->setObjectName("summaryCard");
    auto* diagnosisHeroLayout = new QVBoxLayout(diagnosisHero);
    diagnosisHeroLayout->setContentsMargins(18, 16, 18, 16);
    diagnosisHeroLayout->setSpacing(6);
    auto* diagnosisTitle = new QLabel("诊断报告", diagnosisHero);
    diagnosisTitle->setObjectName("sectionTitle");
    auto* diagnosisHint = new QLabel("把关联证据、根因候选和下一步验证动作组织成可阅读的结论。", diagnosisHero);
    diagnosisHint->setObjectName("sectionHint");
    diagnosisHeroLayout->addWidget(diagnosisTitle);
    diagnosisHeroLayout->addWidget(diagnosisHint);

    auto* actionRow = new QHBoxLayout();
    actionRow->setSpacing(10);
    diagnoseBtn_ = new QPushButton(QIcon(":/icons/diagnosis.svg"), "生成诊断", diagnosisTab);
    diagnoseBtn_->setProperty("variant", "primary");
    aiCheck_ = new QCheckBox("启用 AI 建议", diagnosisTab);
    exportBtn_ = new QPushButton("导出报告", diagnosisTab);
    exportBtn_->setProperty("variant", "primary");
    actionRow->addWidget(diagnoseBtn_);
    actionRow->addWidget(aiCheck_);
    actionRow->addStretch(1);
    actionRow->addWidget(exportBtn_);

    diagnosisView_ = new QPlainTextEdit(diagnosisTab);
    diagnosisView_->setReadOnly(true);
    diagnosisView_->setProperty("role", "output");
    diagnosisView_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    diagnosisView_->setObjectName("diagnosisView");
    diagnosisLayout->addWidget(diagnosisHero);
    diagnosisLayout->addLayout(actionRow);
    diagnosisLayout->addWidget(diagnosisView_, 1);

    workTabs_->addTab(logTab, QIcon(":/icons/log.svg"), "日志排查");
    workTabs_->addTab(requestTab, QIcon(":/icons/request.svg"), "请求验证");
    workTabs_->addTab(diagnosisTab, QIcon(":/icons/diagnosis.svg"), "诊断报告");
    workTabs_->setCurrentIndex(0);

    workspaceLayout->addWidget(summaryCard);
    workspaceLayout->addWidget(workTabs_, 1);

    verticalSplitter_->addWidget(sidebar);
    verticalSplitter_->addWidget(workspace);
    verticalSplitter_->setSizes({276, 1184});

    root->addWidget(verticalSplitter_, 1);

    const auto showHelp = [this]() {
        QMessageBox::information(this, "使用说明",
            "ClientOps Studio 使用流程：\n\n"
            "1. 在左侧创建或选择案例，先描述问题与目标接口\n"
            "2. 优先导入故障时段日志；若有 reqId/traceId 或故障时间，一并填写\n"
            "3. 默认先在“日志排查”页缩小范围，需要时再到“请求验证”补发请求\n"
            "4. 在“诊断报告”页查看证据链、根因候选与验证动作");
    };

    connect(helpBtn, &QPushButton::clicked, this, showHelp);
    connect(helpAction, &QAction::triggered, this, showHelp);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    connect(exportAction, &QAction::triggered, this, [this]() { exportBtn_->click(); });
    connect(resetLayoutAction, &QAction::triggered, this, [this]() {
        verticalSplitter_->setSizes({276, 1184});
        topSplitter_->setSizes({580, 740});
        bottomSplitter_->setSizes({940, 320});
        workTabs_->setCurrentIndex(0);
    });

    statusBar()->showMessage("就绪");
}

void MainWindow::applyTheme() {
    setStyleSheet(R"(
* {
    font-family: "Microsoft YaHei UI", "Segoe UI", sans-serif;
    font-size: 13px;
}
QMainWindow {
    background: #F5F7FA;
}
QMenuBar {
    background: #F8FAFC;
    border-bottom: 1px solid #E2E8F0;
    color: #0F172A;
}
QMenuBar::item {
    padding: 6px 10px;
    border-radius: 6px;
    margin: 2px 4px;
}
QMenuBar::item:selected {
    background: #E2E8F0;
}
QMenu {
    background: #FFFFFF;
    border: 1px solid #E2E8F0;
    padding: 4px;
}
QMenu::item {
    padding: 6px 18px;
    border-radius: 6px;
}
QMenu::item:selected {
    background: #EEF2F7;
}
#sidebar {
    border: 1px solid #E2E8F0;
    border-radius: 22px;
    background: #FBFCFD;
}
#workspace {
    background: transparent;
}
#summaryCard {
    border: 1px solid #E2E8F0;
    border-radius: 22px;
    background: #FFFFFF;
}
#panel, #sidePanel {
    border: 1px solid #E2E8F0;
    border-radius: 20px;
    background: #FFFFFF;
}
QLabel {
    color: #0F172A;
}
QLabel#pageTitle {
    color: #0F172A;
    font-size: 18px;
    font-weight: 800;
}
QLabel#pageDesc {
    color: #64748B;
    font-size: 12px;
}
QLabel#sectionTitle {
    color: #0F172A;
    font-size: 16px;
    font-weight: 700;
}
QLabel#sectionHint, QLabel#caseHeaderMeta {
    color: #64748B;
    font-size: 12px;
}
QLabel#caseHeaderTitle {
    color: #0F172A;
    font-size: 24px;
    font-weight: 800;
}
QLabel#fieldLabel {
    color: #334155;
    font-size: 12px;
    font-weight: 700;
}
QLabel#helperText {
    color: #475569;
    font-size: 12px;
    background: #F8FAFC;
    border: 1px solid #E2E8F0;
    border-radius: 14px;
    padding: 12px;
}
QLineEdit, QPlainTextEdit, QComboBox, QSpinBox, QDateTimeEdit {
    border: 1px solid #CBD5E1;
    border-radius: 12px;
    background: #FFFFFF;
    color: #0F172A;
}
QLineEdit, QDateTimeEdit {
    padding: 7px 12px;
}
QComboBox, QSpinBox {
    padding: 0 10px;
}
QLineEdit, QComboBox, QSpinBox, QDateTimeEdit {
    min-height: 38px;
}
QPlainTextEdit {
    padding: 10px 12px;
    selection-background-color: #DBEAFE;
}
QLineEdit:focus, QPlainTextEdit:focus, QComboBox:focus, QSpinBox:focus, QDateTimeEdit:focus {
    border: 1px solid #2563EB;
}
QComboBox::drop-down {
    subcontrol-origin: padding;
    subcontrol-position: top right;
    width: 28px;
    border: none;
}
QPlainTextEdit[role="output"] {
    background: #FCFDFE;
    color: #111827;
    border: 1px solid #E2E8F0;
    selection-background-color: #DBEAFE;
}
#diagnosisView {
    font-size: 13px;
}
QPushButton {
    border-radius: 12px;
    padding: 8px 16px;
    border: 1px solid transparent;
    font-weight: 600;
    min-height: 38px;
}
QPushButton[variant="primary"] {
    background: #0F172A;
    color: #FFFFFF;
}
QPushButton[variant="primary"]:hover { background: #1E293B; }
QPushButton[variant="primary"]:pressed { background: #334155; }
QPushButton[variant="secondary"] {
    background: #FFFFFF;
    color: #0F172A;
    border-color: #CBD5E1;
}
QPushButton[variant="secondary"]:hover { background: #F8FAFC; }
QPushButton[variant="danger"] {
    background: #FEF2F2;
    color: #991B1B;
    border-color: #FECACA;
}
QPushButton[variant="danger"]:hover { background: #FEE2E2; }
QPushButton[variant="danger"]:pressed { background: #FECACA; }
QTabWidget::pane {
    border: none;
    background: transparent;
}
QTabBar::tab {
    background: transparent;
    border: none;
    border-radius: 12px;
    padding: 10px 16px;
    margin-right: 8px;
    color: #64748B;
}
QTabBar::tab:selected {
    background: #FFFFFF;
    color: #0F172A;
    font-weight: 700;
    border: 1px solid #E2E8F0;
}
QListWidget#caseList {
    border: 1px solid #E2E8F0;
    border-radius: 18px;
    background: #FFFFFF;
    outline: none;
    padding: 8px;
}
QListWidget#caseList::item {
    border-radius: 14px;
    padding: 12px 12px;
    margin: 4px 2px;
}
QListWidget#caseList::item:selected {
    background: #F1F5F9;
    color: #0F172A;
}
QStatusBar {
    background: #F8FAFC;
    color: #475569;
    border-top: 1px solid #E2E8F0;
}
QCheckBox {
    spacing: 6px;
    color: #334155;
}
QSplitter::handle {
    background: #E2E8F0;
    border-radius: 4px;
    margin: 1px;
}
QSplitter::handle:hover {
    background: #CBD5E1;
}
QSplitter::handle:horizontal { width: 8px; }
QSplitter::handle:vertical { height: 8px; }
)");
}
void MainWindow::initStore() {
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    const QString dbPath = appData + "/clientops_studio.db";

    store_ = std::make_unique<SqliteStore>(dbPath);
    QString err;
    if (!store_->init(&err)) {
        QMessageBox::warning(this, "SQLite 初始化失败", err);
    }
}

void MainWindow::loadCaseSessions() {
    if (!store_) {
        return;
    }

    QString err;
    caseSessions_ = store_->loadCaseSessions(200, &err);
    caseBox_->clear();
    caseList_->clear();

    if (!err.isEmpty()) {
        statusBar()->showMessage("案例加载失败: " + err, 5000);
        return;
    }

    for (const auto& c : caseSessions_) {
        caseBox_->addItem(QString("#%1 %2").arg(c.id).arg(c.title), c.id);
        QString line = QString("%1  %2").arg(c.id).arg(c.title);
        if (!c.targetEndpoint.trimmed().isEmpty()) {
            line += "\n" + c.targetEndpoint.trimmed();
        }
        auto* item = new QListWidgetItem(line, caseList_);
        item->setData(Qt::UserRole, c.id);
        item->setToolTip(c.problemDescription);
    }

    const int defaultId = store_->getSetting("current_case_id", "-1").toInt();
    int targetIndex = -1;
    for (int i = 0; i < caseBox_->count(); ++i) {
        if (caseBox_->itemData(i).toInt() == defaultId) {
            targetIndex = i;
            break;
        }
    }
    if (targetIndex < 0 && caseBox_->count() > 0) {
        targetIndex = 0;
    }
    if (targetIndex >= 0) {
        caseBox_->setCurrentIndex(targetIndex);
        const QSignalBlocker blocker(caseList_);
        caseList_->setCurrentRow(targetIndex);
    } else {
        caseBox_->setCurrentIndex(-1);
        store_->setSetting("current_case_id", "-1", &err);
        activeCaseId_ = -1;
        resetWorkspaceState(true);
    }
}

void MainWindow::loadHistoryForCurrentCase() {
    if (!store_) {
        return;
    }

    QString err;
    const int caseId = selectedCaseId();
    if (caseId >= 0) {
        historyRequests_ = store_->loadRecentRequestsByCase(caseId, 20, &err);
    } else {
        historyRequests_.clear();
    }
    historyBox_->clear();

    if (!err.isEmpty()) {
        statusBar()->showMessage("历史加载失败: " + err, 5000);
        return;
    }

    for (const auto& req : historyRequests_) {
        historyBox_->addItem(QString("%1 %2").arg(req.method, req.url));
    }
}

void MainWindow::loadAiConfig() {
    if (!store_) {
        return;
    }

    const bool persisted = store_->getSetting("ai_persist", "0") == "1";
    if (!persisted) {
        aiConfig_.persistToLocal = false;
        return;
    }

    aiConfig_.apiKey = store_->getSetting("ai_api_key", "");
    aiConfig_.baseUrl = store_->getSetting("ai_base_url", "");
    aiConfig_.model = store_->getSetting("ai_model", "");
    aiConfig_.persistToLocal = true;

    if (!aiConfig_.apiKey.isEmpty() && !aiConfig_.baseUrl.isEmpty() && !aiConfig_.model.isEmpty()) {
        aiAdvisor_.setConfig(aiConfig_);
        aiCheck_->setChecked(true);
        statusBar()->showMessage("已加载本地 AI 配置", 2500);
    }
}

void MainWindow::saveAiConfig(const AiConfigInput& cfg) {
    aiConfig_ = cfg;
    aiAdvisor_.setConfig(aiConfig_);
    aiCheck_->setChecked(true);

    if (!store_) {
        return;
    }

    QString err;
    if (cfg.persistToLocal) {
        store_->setSetting("ai_persist", "1", &err);
        store_->setSetting("ai_api_key", cfg.apiKey, &err);
        store_->setSetting("ai_base_url", cfg.baseUrl, &err);
        store_->setSetting("ai_model", cfg.model, &err);
        statusBar()->showMessage("AI 配置已保存到本地", 3000);
    } else {
        store_->setSetting("ai_persist", "0", &err);
        store_->setSetting("ai_api_key", "", &err);
        store_->setSetting("ai_base_url", "", &err);
        store_->setSetting("ai_model", "", &err);
        statusBar()->showMessage("AI 配置仅用于本次会话", 3000);
    }

    if (!err.isEmpty()) {
        statusBar()->showMessage("AI 配置保存失败: " + err, 5000);
    }
}

void MainWindow::restoreSplitterState() {
    const auto applyDefaultSizes = [this]() {
        verticalSplitter_->setSizes({276, 1184});
        topSplitter_->setSizes({580, 740});
        bottomSplitter_->setSizes({940, 320});
    };

    const auto isCollapsed = [](QSplitter* splitter) {
        const auto sizes = splitter->sizes();
        for (const int s : sizes) {
            if (s < 80) {
                return true;
            }
        }
        return false;
    };

    if (!store_) {
        applyDefaultSizes();
        return;
    }

    const auto restore = [this, &isCollapsed](const QString& key, QSplitter* splitter) {
        const QString raw = store_->getSetting(key, "");
        if (raw.isEmpty()) {
            return false;
        }
        const bool ok = splitter->restoreState(QByteArray::fromBase64(raw.toUtf8()));
        return ok && !isCollapsed(splitter);
    };

    const bool vOk = restore("split_main", verticalSplitter_);
    const bool tOk = restore("split_request", topSplitter_);
    const bool bOk = restore("split_log", bottomSplitter_);
    if (!(vOk && tOk && bOk)) {
        applyDefaultSizes();
    }
}

void MainWindow::persistSplitterState() {
    if (!store_) {
        return;
    }

    QString err;
    store_->setSetting("split_main", QString::fromUtf8(verticalSplitter_->saveState().toBase64()), &err);
    store_->setSetting("split_request", QString::fromUtf8(topSplitter_->saveState().toBase64()), &err);
    store_->setSetting("split_log", QString::fromUtf8(bottomSplitter_->saveState().toBase64()), &err);
}

int MainWindow::selectedCaseId() const {
    if (caseBox_->currentIndex() < 0) {
        return -1;
    }
    return caseBox_->currentData().toInt();
}

const CaseSession* MainWindow::findCaseSession(int caseId) const {
    for (const auto& c : caseSessions_) {
        if (c.id == caseId) {
            return &c;
        }
    }
    return nullptr;
}

CaseSession MainWindow::buildCaseSessionFromUi(int caseId) const {
    CaseSession session;
    if (const auto* existing = findCaseSession(caseId)) {
        session = *existing;
    }

    session.id = caseId;
    session.problemDescription = caseProblemEdit_->text().trimmed();
    session.targetEndpoint = caseTargetEdit_->text().trimmed();
    session.analysisMode = (modeBox_ && modeBox_->currentIndex() == 0) ? "replay" : "log";
    session.incidentTraceId = incidentTraceEdit_ ? incidentTraceEdit_->text().trimmed() : QString();
    session.useIncidentTime = useIncidentTimeCheck_ && useIncidentTimeCheck_->isChecked();
    session.incidentTime = session.useIncidentTime && incidentTimeEdit_ ? incidentTimeEdit_->dateTime() : QDateTime();
    session.requestDraft = collectRequestFromUi();
    session.requestDraft.caseId = caseId;
    session.logPath = currentLogPath_;
    return session;
}

void MainWindow::saveCaseState(int caseId) {
    if (suppressCaseSync_ || !store_ || caseId < 0) {
        return;
    }

    QString err;
    const CaseSession session = buildCaseSessionFromUi(caseId);
    if (!store_->saveCaseSession(session, &err) && !err.isEmpty()) {
        statusBar()->showMessage("案例保存失败: " + err, 4000);
        return;
    }

    for (auto& item : caseSessions_) {
        if (item.id == caseId) {
            item = session;
            break;
        }
    }

    refreshCaseSummary();
}

void MainWindow::resetWorkspaceState(bool clearCaseMeta) {
    suppressCaseSync_ = true;

    if (clearCaseMeta) {
        caseProblemEdit_->clear();
        caseTargetEdit_->clear();
        modeBox_->setCurrentIndex(1);
        incidentTraceEdit_->clear();
        useIncidentTimeCheck_->setChecked(false);
        incidentTimeEdit_->setDateTime(QDateTime::currentDateTime());
    }

    methodBox_->setCurrentText("GET");
    urlEdit_->clear();
    traceHintEdit_->clear();
    timeoutSpin_->setValue(5000);
    retrySpin_->setValue(1);
    headersEdit_->clear();
    bodyEdit_->clear();

    historyRequests_.clear();
    historyBox_->clear();
    currentLogPath_.clear();
    currentLogSummary_ = LogLoadSummary();
    pendingLogPath_.clear();
    pendingLogCaseId_ = -1;

    lastRequest_ = HttpRequestData();
    lastResponse_ = HttpResponseData();
    allLogs_.clear();
    filteredLogs_.clear();
    scopedLogs_.clear();
    lastDiagnosis_ = DiagnosisResult();
    lastAiAdvice_.clear();

    statusLabel_->setText("状态: 未执行");
    latencyLabel_->setText("耗时: -");
    errorLabel_->setText("错误: -");
    setEmptyStates();

    suppressCaseSync_ = false;
    refreshCaseSummary();
}

bool MainWindow::loadLogFileSync(const QString& filePath) {
    allLogs_.clear();
    filteredLogs_.clear();
    scopedLogs_.clear();
    currentLogPath_.clear();
    currentLogSummary_ = LogLoadSummary();
    refreshLogSummaryUi();

    if (filePath.trimmed().isEmpty()) {
        renderLogs({});
        return false;
    }

    QElapsedTimer timer;
    timer.start();
    QString error;
    const QVector<LogRecord> records = LogParser::parseFile(filePath, &error);
    if (!error.isEmpty()) {
        logView_->setPlainText("日志加载失败: " + error);
        statusBar()->showMessage("日志加载失败: " + error, 4000);
        return false;
    }

    allLogs_ = records;
    currentLogPath_ = filePath;
    currentLogSummary_ = LogParser::buildLoadSummary(filePath, records, timer.elapsed());
    refreshLogSummaryUi();
    applyLogFilters();
    return true;
}

void MainWindow::applyCaseSession(const CaseSession& session) {
    suppressCaseSync_ = true;

    caseProblemEdit_->setText(session.problemDescription);
    caseTargetEdit_->setText(session.targetEndpoint);
    modeBox_->setCurrentIndex(session.analysisMode == "replay" ? 0 : 1);
    incidentTraceEdit_->setText(session.incidentTraceId);
    useIncidentTimeCheck_->setChecked(session.useIncidentTime);
    incidentTimeEdit_->setDateTime(session.incidentTime.isValid() ? session.incidentTime : QDateTime::currentDateTime());

    methodBox_->setCurrentText(session.requestDraft.method.isEmpty() ? "GET" : session.requestDraft.method);
    urlEdit_->setText(session.requestDraft.url);
    headersEdit_->setPlainText(session.requestDraft.headersText);
    bodyEdit_->setPlainText(session.requestDraft.bodyText);
    traceHintEdit_->setText(session.requestDraft.traceIdHint);
    timeoutSpin_->setValue(session.requestDraft.timeoutMs > 0 ? session.requestDraft.timeoutMs : 5000);
    retrySpin_->setValue(session.requestDraft.retryCount > 0 ? session.requestDraft.retryCount : 1);

    suppressCaseSync_ = false;
    loadHistoryForCurrentCase();

    if (!session.logPath.trimmed().isEmpty() && QFileInfo::exists(session.logPath)) {
        loadLogFileSync(session.logPath);
    } else {
        currentLogPath_.clear();
        allLogs_.clear();
        filteredLogs_.clear();
        scopedLogs_.clear();
        renderLogs({});
    }

    lastRequest_ = session.requestDraft;
    lastResponse_ = HttpResponseData();
    statusLabel_->setText("状态: 未执行");
    latencyLabel_->setText("耗时: -");
    errorLabel_->setText("错误: -");
    diagnosisView_->setPlainText("请根据当前案例导入日志或补充复现请求后再生成诊断。");
    lastDiagnosis_ = DiagnosisResult();
    lastAiAdvice_.clear();
    refreshCaseSummary();
}

void MainWindow::refreshCaseSummary() {
    if (!currentCaseTitleLabel_ || !currentCaseMetaLabel_) {
        return;
    }

    const int caseId = selectedCaseId();
    const CaseSession* session = findCaseSession(caseId);
    currentCaseTitleLabel_->setText(session ? session->title : "未选择案例");

    QStringList meta;
    if (modeBox_) {
        meta << (modeBox_->currentIndex() == 0 ? "补发请求验证" : "按日志排查");
    }
    if (caseTargetEdit_ && !caseTargetEdit_->text().trimmed().isEmpty()) {
        meta << caseTargetEdit_->text().trimmed();
    }
    if (incidentTraceEdit_ && !incidentTraceEdit_->text().trimmed().isEmpty()) {
        meta << QString("ID %1").arg(incidentTraceEdit_->text().trimmed());
    }
    if (useIncidentTimeCheck_ && useIncidentTimeCheck_->isChecked() && incidentTimeEdit_ && incidentTimeEdit_->dateTime().isValid()) {
        meta << QString("故障时间 %1").arg(incidentTimeEdit_->dateTime().toString("MM-dd HH:mm:ss"));
    }
    if (!currentLogPath_.trimmed().isEmpty()) {
        meta << QString("日志 %1").arg(QFileInfo(currentLogPath_).fileName());
    }

    if (meta.isEmpty()) {
        currentCaseMetaLabel_->setText("请先导入日志，补充 reqId/traceId 或故障时间，再生成诊断。");
    } else {
        currentCaseMetaLabel_->setText(meta.join("  ·  "));
    }
}

void MainWindow::refreshLogSummaryUi() {
    if (!logFileSummaryLabel_ || !logStatsSummaryLabel_ || !logRenderHintLabel_) {
        return;
    }

    if (currentLogSummary_.filePath.trimmed().isEmpty()) {
        logFileSummaryLabel_->setText("文件：未导入");
        logStatsSummaryLabel_->setText("统计：-");
        logRenderHintLabel_->setText("显示：导入后会展示解析耗时、reqId 数量和渲染截断信息。");
        return;
    }

    const double sizeKb = static_cast<double>(currentLogSummary_.fileSizeBytes) / 1024.0;
    logFileSummaryLabel_->setText(
        QString("文件：%1（%2 KB）")
            .arg(currentLogSummary_.fileName)
            .arg(QString::number(sizeKb, 'f', 1)));
    logStatsSummaryLabel_->setText(
        QString("统计：%1 行 · %2 个 reqId · 解析耗时 %3 ms")
            .arg(currentLogSummary_.totalRecords)
            .arg(currentLogSummary_.distinctReqIds)
            .arg(currentLogSummary_.elapsedMs));
    if (currentLogSummary_.wasTruncated) {
        logRenderHintLabel_->setText(
            QString("显示：当前仅渲染前 %1 / %2 行，诊断仍基于全量日志。")
                .arg(currentLogSummary_.renderedRecords)
                .arg(currentLogSummary_.totalRecords));
    } else {
        logRenderHintLabel_->setText(
            QString("显示：当前渲染 %1 / %2 行；超过 %3 行时只渲染前 %3 行，诊断仍基于全量日志。")
                .arg(currentLogSummary_.renderedRecords)
                .arg(currentLogSummary_.totalRecords)
                .arg(LogParser::kRenderRecordLimit));
    }
}

bool MainWindow::createCaseSession() {
    if (!store_) {
        return false;
    }

    QDialog dlg(this);
    dlg.setWindowTitle("新建案例");
    dlg.setModal(true);
    dlg.resize(520, 280);

    auto* layout = new QVBoxLayout(&dlg);
    auto* form = new QFormLayout();
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(10);

    auto* titleEdit = new QLineEdit(&dlg);
    titleEdit->setPlaceholderText("例如：下单接口 500 复盘");
    titleEdit->setClearButtonEnabled(true);

    auto* problemEdit = new QPlainTextEdit(&dlg);
    problemEdit->setPlaceholderText("描述现象、影响范围、触发条件");
    problemEdit->setMinimumHeight(90);

    auto* endpointEdit = new QLineEdit(&dlg);
    endpointEdit->setPlaceholderText("可选，例如 /api/v1/orders");
    endpointEdit->setClearButtonEnabled(true);

    form->addRow("案例标题", titleEdit);
    form->addRow("问题描述", problemEdit);
    form->addRow("目标接口", endpointEdit);

    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    if (auto* okBtn = btnBox->button(QDialogButtonBox::Ok)) {
        okBtn->setText("创建");
    }
    if (auto* cancelBtn = btnBox->button(QDialogButtonBox::Cancel)) {
        cancelBtn->setText("取消");
    }
    connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    layout->addLayout(form);
    layout->addWidget(btnBox);

    if (dlg.exec() != QDialog::Accepted) {
        return false;
    }

    const QString title = titleEdit->text().trimmed();
    const QString problem = problemEdit->toPlainText().trimmed();
    const QString endpoint = endpointEdit->text().trimmed();

    if (title.isEmpty()) {
        QMessageBox::warning(this, "缺少信息", "案例标题不能为空。");
        return false;
    }
    if (problem.isEmpty()) {
        QMessageBox::warning(this, "缺少信息", "问题描述不能为空。");
        return false;
    }

    QString err;
    const int newId = store_->createCaseSession(title, problem, endpoint, &err);
    if (newId < 0) {
        QMessageBox::warning(this, "创建失败", err.isEmpty() ? "无法创建案例" : err);
        return false;
    }

    loadCaseSessions();
    for (int i = 0; i < caseBox_->count(); ++i) {
        if (caseBox_->itemData(i).toInt() == newId) {
            caseBox_->setCurrentIndex(i);
            break;
        }
    }

    statusBar()->showMessage("案例创建成功", 2000);
    return true;
}

bool MainWindow::deleteCurrentCase() {
    if (!store_) {
        return false;
    }
    const int caseId = selectedCaseId();
    if (caseId < 0) {
        QMessageBox::information(this, "提示", "请先选择要删除的案例。");
        return false;
    }
    QString caseTitle;
    for (const auto& c : caseSessions_) {
        if (c.id == caseId) {
            caseTitle = c.title;
            break;
        }
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle("删除案例");
    box.setText(QString("确定删除案例「%1」吗？").arg(caseTitle));
    box.setInformativeText("该案例下的请求历史会一并删除。此操作不可撤销。");
    auto* confirmBtn = box.addButton("删除", QMessageBox::AcceptRole);
    box.addButton("取消", QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() != confirmBtn) {
        return false;
    }

    QString err;
    if (!store_->deleteCaseSession(caseId, &err)) {
        QMessageBox::warning(this, "删除失败", err.isEmpty() ? "无法删除案例。" : err);
        return false;
    }

    activeCaseId_ = -1;
    loadCaseSessions();
    onCaseChanged();
    statusBar()->showMessage("案例已删除", 2000);
    return true;
}

bool MainWindow::ensureCaseReadyForRequest() {
    if (selectedCaseId() < 0) {
        if (!createCaseSession()) {
            return false;
        }
    }

    if (caseProblemEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "缺少信息", "请先填写当前 Case 的问题描述。");
        caseProblemEdit_->setFocus();
        return false;
    }

    if (store_) {
        saveCaseState(selectedCaseId());
    }
    return true;
}

void MainWindow::onCaseChanged() {
    const int id = selectedCaseId();
    if (activeCaseId_ >= 0 && activeCaseId_ != id) {
        saveCaseState(activeCaseId_);
    }

    if (id < 0) {
        activeCaseId_ = -1;
        if (store_) {
            QString err;
            store_->setSetting("current_case_id", "-1", &err);
        }
        resetWorkspaceState(true);
        return;
    }

    activeCaseId_ = id;

    for (int row = 0; row < caseList_->count(); ++row) {
        auto* item = caseList_->item(row);
        if (item && item->data(Qt::UserRole).toInt() == id) {
            const QSignalBlocker blocker(caseList_);
            caseList_->setCurrentRow(row);
            break;
        }
    }

    if (store_ && id >= 0) {
        QString err;
        store_->setSetting("current_case_id", QString::number(id), &err);
    }

    if (const auto* session = findCaseSession(id)) {
        applyCaseSession(*session);
    } else {
        resetWorkspaceState(true);
    }
}
void MainWindow::wireSignals() {
    auto* sendShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), this);
    connect(sendShortcut, &QShortcut::activated, sendBtn_, &QPushButton::click);
    auto* importShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_O), this);
    connect(importShortcut, &QShortcut::activated, importLogBtn_, &QPushButton::click);
    auto* exportShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_S), this);
    connect(exportShortcut, &QShortcut::activated, exportBtn_, &QPushButton::click);

    connect(newCaseBtn_, &QPushButton::clicked, this, &MainWindow::createCaseSession);
    connect(deleteCaseBtn_, &QPushButton::clicked, this, &MainWindow::deleteCurrentCase);
    connect(caseBox_, &QComboBox::currentIndexChanged, this, [this]() { onCaseChanged(); });
    connect(caseList_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0 || row >= caseBox_->count()) {
            return;
        }
        if (caseBox_->currentIndex() != row) {
            caseBox_->setCurrentIndex(row);
        }
    });
    connect(caseSearchEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
        const QString key = text.trimmed();
        for (int i = 0; i < caseList_->count(); ++i) {
            auto* item = caseList_->item(i);
            const bool visible = key.isEmpty() || item->text().contains(key, Qt::CaseInsensitive) || item->toolTip().contains(key, Qt::CaseInsensitive);
            item->setHidden(!visible);
        }
    });
    connect(useIncidentTimeCheck_, &QCheckBox::toggled, this, [this](bool on) {
        if (incidentTimeEdit_) {
            incidentTimeEdit_->setEnabled(on);
            if (on) {
                incidentTimeEdit_->setDateTime(QDateTime::currentDateTime());
            }
        }
        saveCaseState(selectedCaseId());
    });
    connect(incidentTimeEdit_, &QDateTimeEdit::dateTimeChanged, this, [this](const QDateTime&) { saveCaseState(selectedCaseId()); });
    connect(modeBox_, &QComboBox::currentIndexChanged, this, [this](int idx) {
        const bool logMode = (idx == 1);
        if (logMode) {
            statusBar()->showMessage("已切换日志模式：可不发请求，直接按日志诊断", 2600);
            if (workTabs_) {
                workTabs_->setCurrentIndex(0);
            }
        } else {
            statusBar()->showMessage("已切换复现模式：建议先发请求再诊断", 2200);
            if (workTabs_) {
                workTabs_->setCurrentIndex(1);
            }
        }
        saveCaseState(selectedCaseId());
    });

    connect(caseProblemEdit_, &QLineEdit::editingFinished, this, [this]() {
        saveCaseState(selectedCaseId());
    });

    connect(caseTargetEdit_, &QLineEdit::editingFinished, this, [this]() {
        saveCaseState(selectedCaseId());
    });
    connect(incidentTraceEdit_, &QLineEdit::editingFinished, this, [this]() { saveCaseState(selectedCaseId()); });
    connect(urlEdit_, &QLineEdit::editingFinished, this, [this]() { saveCaseState(selectedCaseId()); });
    connect(traceHintEdit_, &QLineEdit::editingFinished, this, [this]() { saveCaseState(selectedCaseId()); });
    connect(methodBox_, &QComboBox::currentTextChanged, this, [this](const QString&) { saveCaseState(selectedCaseId()); });
    connect(timeoutSpin_, &QSpinBox::valueChanged, this, [this](int) { saveCaseState(selectedCaseId()); });
    connect(retrySpin_, &QSpinBox::valueChanged, this, [this](int) { saveCaseState(selectedCaseId()); });
    connect(headersEdit_, &QPlainTextEdit::textChanged, this, [this]() { saveCaseState(selectedCaseId()); });
    connect(bodyEdit_, &QPlainTextEdit::textChanged, this, [this]() { saveCaseState(selectedCaseId()); });

    connect(verticalSplitter_, &QSplitter::splitterMoved, this, [this](int, int) { persistSplitterState(); });
    connect(topSplitter_, &QSplitter::splitterMoved, this, [this](int, int) { persistSplitterState(); });
    connect(bottomSplitter_, &QSplitter::splitterMoved, this, [this](int, int) { persistSplitterState(); });

    connect(aiConfigBtn_, &QPushButton::clicked, this, &MainWindow::openAiConfigDialog);
    connect(sampleRequestBtn_, &QPushButton::clicked, this, &MainWindow::fillSampleRequestAndLog);
    connect(copyResponseBtn_, &QPushButton::clicked, this, &MainWindow::copyResponseText);

    connect(sendBtn_, &QPushButton::clicked, this, [this]() {
        if (!ensureCaseReadyForRequest()) {
            return;
        }

        lastRequest_ = collectRequestFromUi();
        if (lastRequest_.url.trimmed().isEmpty()) {
            QMessageBox::warning(this, "参数错误", "URL 不能为空");
            return;
        }

        if (store_) {
            QString err;
            store_->saveRequest(lastRequest_, &err);
            store_->setSetting("last_url", lastRequest_.url, &err);
            if (!err.isEmpty()) {
                statusBar()->showMessage("保存历史失败: " + err, 4000);
            }
        }

        sendBtn_->setEnabled(false);
        responseEdit_->setPlainText("请求执行中，请稍候...");
        statusBar()->showMessage("请求中...");
        lastAiAdvice_.clear();

        httpClient_.sendRequest(lastRequest_);
    });

    connect(&httpClient_, &HttpClient::requestFinished, this, [this](const HttpResponseData& response) {
        lastResponse_ = response;
        updateResponseUi(response);
        sendBtn_->setEnabled(true);
        statusBar()->showMessage("请求完成", 1500);

        if (!allLogs_.isEmpty()) {
            runDiagnosis();
        } else {
            diagnosisView_->setPlainText("请导入故障时段日志后再生成诊断。\n\n当前已获得请求结果，可作为关联锚点。");
        }

        loadHistoryForCurrentCase();
    });

    connect(importLogBtn_, &QPushButton::clicked, this, [this]() {
        QString defaultDir = QDir::homePath();
        if (store_) {
            defaultDir = store_->getSetting("last_log_dir", defaultDir);
        }
        const QString filePath = QFileDialog::getOpenFileName(this, "选择故障时段日志", defaultDir, "Log Files (*.log *.txt);;All Files (*.*)");
        if (filePath.isEmpty()) {
            return;
        }

        if (store_) {
            QString err;
            store_->setSetting("last_log_dir", QFileInfo(filePath).absolutePath(), &err);
        }

        logView_->setPlainText("日志解析中...");
        statusBar()->showMessage("日志解析中...");
        pendingLogPath_ = filePath;
        pendingLogCaseId_ = selectedCaseId();
        pendingLogTimer_.start();
        currentLogSummary_ = LogLoadSummary();
        currentLogSummary_.filePath = filePath;
        currentLogSummary_.fileName = QFileInfo(filePath).fileName();
        currentLogSummary_.fileSizeBytes = QFileInfo(filePath).size();
        refreshLogSummaryUi();
        logParser_.parseFileAsync(filePath);
    });

    connect(&logParser_, &LogParser::parseCompleted, this, [this](const QVector<LogRecord>& records, const QString& error) {
        if (!error.isEmpty()) {
            QMessageBox::warning(this, "日志解析失败", error);
            return;
        }

        if (pendingLogCaseId_ >= 0 && selectedCaseId() != pendingLogCaseId_) {
            pendingLogPath_.clear();
            pendingLogCaseId_ = -1;
            return;
        }

        allLogs_ = records;
        currentLogPath_ = pendingLogPath_;
        currentLogSummary_ = LogParser::buildLoadSummary(
            currentLogPath_,
            records,
            pendingLogTimer_.isValid() ? pendingLogTimer_.elapsed() : 0);
        pendingLogPath_.clear();
        pendingLogCaseId_ = -1;
        refreshLogSummaryUi();
        applyLogFilters();
        saveCaseState(selectedCaseId());
        statusBar()->showMessage(QString("日志加载完成，共 %1 行").arg(allLogs_.size()), 2000);

        if ((modeBox_ && modeBox_->currentIndex() == 1) || lastResponse_.startedAt.isValid()) {
            runDiagnosis();
        }
    });

    connect(applyFilterBtn_, &QPushButton::clicked, this, [this]() { applyLogFilters(); });
    connect(diagnoseBtn_, &QPushButton::clicked, this, [this]() { runDiagnosis(); });

    connect(loadHistoryBtn_, &QPushButton::clicked, this, [this]() {
        const int idx = historyBox_->currentIndex();
        if (idx < 0 || idx >= historyRequests_.size()) {
            return;
        }
        const auto& req = historyRequests_.at(idx);
        methodBox_->setCurrentText(req.method);
        urlEdit_->setText(req.url);
        headersEdit_->setPlainText(req.headersText);
        bodyEdit_->setPlainText(req.bodyText);
        traceHintEdit_->setText(req.traceIdHint);
        timeoutSpin_->setValue(req.timeoutMs > 0 ? req.timeoutMs : 5000);
        retrySpin_->setValue(req.retryCount);
        saveCaseState(selectedCaseId());
    });

    connect(exportBtn_, &QPushButton::clicked, this, [this]() {
        if (allLogs_.isEmpty() && !lastResponse_.startedAt.isValid()) {
            QMessageBox::information(this, "提示", "请先导入日志或发送请求后再导出报告。");
            return;
        }

        if (lastDiagnosis_.findings.isEmpty()) {
            runDiagnosis();
        }

        CaseSession currentCase;
        const int id = selectedCaseId();
        for (const auto& c : caseSessions_) {
            if (c.id == id) {
                currentCase = c;
                break;
            }
        }

        const QString report = MarkdownExporter::buildReport(currentCase, lastRequest_, lastResponse_, scopedLogs_, lastDiagnosis_, lastAiAdvice_);
        const QString defaultPath = QDir::homePath() + "/case_report_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".md";
        const QString outPath = QFileDialog::getSaveFileName(this, "保存报告", defaultPath, "Markdown (*.md)");
        if (outPath.isEmpty()) {
            return;
        }

        QFile file(outPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "导出失败", "无法写入报告文件。");
            return;
        }
        QTextStream ts(&file);
        ts.setEncoding(QStringConverter::Utf8);
        ts << report;
        file.close();

        statusBar()->showMessage("报告导出成功: " + outPath, 3000);
    });

    connect(&aiAdvisor_, &AiAdvisor::adviceReady, this, [this](const QString& advice) {
        lastAiAdvice_ = advice;
        renderDiagnosis();
        statusBar()->showMessage("AI 建议已更新", 2000);
    });

    connect(&aiAdvisor_, &AiAdvisor::adviceError, this, [this](const QString& error) {
        lastAiAdvice_ = "[AI 建议不可用] " + error;
        renderDiagnosis();
        statusBar()->showMessage("AI 请求失败，已自动降级", 3000);
    });
}
HttpRequestData MainWindow::collectRequestFromUi() const {
    HttpRequestData req;
    req.caseId = selectedCaseId();
    req.method = methodBox_->currentText();
    req.url = urlEdit_->text().trimmed();
    req.headersText = headersEdit_->toPlainText();
    req.bodyText = bodyEdit_->toPlainText();
    req.traceIdHint = traceHintEdit_->text().trimmed();
    req.timeoutMs = timeoutSpin_->value();
    req.retryCount = retrySpin_->value();
    return req;
}

MainWindow::CorrelationOutcome MainWindow::correlateLogs(int windowSeconds) const {
    CorrelationOutcome out;
    out.mode = "none";
    out.evidence = "未执行关联";

    if (allLogs_.isEmpty()) {
        out.mode = "none";
        out.evidence = "未导入日志";
        return out;
    }

    QString traceHint = incidentTraceEdit_ ? incidentTraceEdit_->text().trimmed() : QString();
    if (traceHint.isEmpty()) {
        traceHint = lastRequest_.traceIdHint.trimmed();
    }
    if (!traceHint.isEmpty()) {
        for (const auto& rec : allLogs_) {
            const bool idMatched = rec.reqId.compare(traceHint, Qt::CaseInsensitive) == 0 ||
                                   rec.traceId.compare(traceHint, Qt::CaseInsensitive) == 0 ||
                                   rec.rawLine.contains(traceHint, Qt::CaseInsensitive);
            if (idMatched) {
                out.logs.push_back(rec);
            }
        }

        if (!out.logs.isEmpty()) {
            out.mode = "trace-id";
            out.evidence = QString("命中 reqId/traceId: %1").arg(traceHint);
            return out;
        }
    }

    if (useIncidentTimeCheck_ && useIncidentTimeCheck_->isChecked() && incidentTimeEdit_ && incidentTimeEdit_->dateTime().isValid()) {
        const QDateTime anchor = incidentTimeEdit_->dateTime();
        const QDateTime begin = anchor.addSecs(-windowSeconds);
        const QDateTime end = anchor.addSecs(windowSeconds);

        for (const auto& rec : allLogs_) {
            if (!rec.timestamp.isValid()) {
                continue;
            }
            if (rec.timestamp >= begin && rec.timestamp <= end) {
                out.logs.push_back(rec);
            }
        }
        out.mode = "incident-time";
        out.evidence = QString("按故障时间 %1 前后 %2 秒关联")
                           .arg(anchor.toString("yyyy-MM-dd HH:mm:ss"))
                           .arg(windowSeconds);
        return out;
    }

    if (!lastResponse_.startedAt.isValid()) {
        if (modeBox_ && modeBox_->currentIndex() == 1) {
            out.logs = allLogs_;
            out.mode = "full-log-scan";
            out.evidence = "未提供可用时间锚点，使用全量日志扫描（低置信度）";
        } else {
            out.mode = "none";
            out.evidence = "请求时间无效，无法按窗口关联";
        }
        return out;
    }

    const QDateTime begin = lastResponse_.startedAt.addSecs(-windowSeconds);
    const QDateTime end = lastResponse_.startedAt.addSecs(windowSeconds);

    for (const auto& rec : allLogs_) {
        if (!rec.timestamp.isValid()) {
            continue;
        }
        if (rec.timestamp >= begin && rec.timestamp <= end) {
            out.logs.push_back(rec);
        }
    }

    out.mode = "request-time";
    out.evidence = QString("按复现请求时间前后 %1 秒关联").arg(windowSeconds);
    return out;
}

void MainWindow::updateResponseUi(const HttpResponseData& response) {
    statusLabel_->setText(QString("状态: %1").arg(response.statusCode == 0 ? "无HTTP状态" : QString::number(response.statusCode)));
    latencyLabel_->setText(QString("耗时: %1 ms").arg(response.latencyMs));
    errorLabel_->setText(QString("错误: %1").arg(response.errorMessage.isEmpty() ? "-" : response.errorMessage));

    QString content = response.responseBody.trimmed();
    if (content.isEmpty()) {
        if (!response.errorMessage.isEmpty()) {
            content = QString("请求失败：%1\n\n说明：本次没有可展示的响应体。").arg(response.errorMessage);
        } else if (response.statusCode > 0) {
            content = "响应体为空。\n\n说明：该接口可能仅返回状态码，不返回正文。";
        } else {
            content = "请先发送请求以查看结果。";
        }
    }

    responseEdit_->setPlainText(content);
}

void MainWindow::renderLogs(const QVector<LogRecord>& logs) {
    if (logs.isEmpty()) {
        logView_->setPlainText("请导入故障时段日志后查看。\n\n建议：导入包含问题发生时间点前后 1-3 分钟的日志。");
        return;
    }

    QString text;
    text.reserve(logs.size() * 80);

    int count = 0;
    for (const auto& rec : logs) {
        text += rec.rawLine;
        text += '\n';
        ++count;
        if (count >= LogParser::kRenderRecordLimit) {
            text += "... (日志显示截断，避免界面卡顿)\n";
            break;
        }
    }

    logView_->setPlainText(text);
}

void MainWindow::applyLogFilters() {
    const QString level = levelFilterBox_->currentText();
    const QString kw = keywordEdit_->text().trimmed();

    filteredLogs_.clear();
    for (const auto& rec : allLogs_) {
        if (level != "ALL" && rec.level != level) {
            continue;
        }
        if (!kw.isEmpty() && !rec.rawLine.contains(kw, Qt::CaseInsensitive)) {
            continue;
        }
        filteredLogs_.push_back(rec);
    }

    renderLogs(filteredLogs_);
    statusBar()->showMessage(QString("日志显示：%1 / 总计 %2 行").arg(filteredLogs_.size()).arg(allLogs_.size()), 2500);
}

void MainWindow::runDiagnosis() {
    if (allLogs_.isEmpty()) {
        diagnosisView_->setPlainText("请先导入故障时段日志，再进行关联分析。");
        return;
    }

    const bool hasRequestTime = lastResponse_.startedAt.isValid();
    const bool hasIncidentTime = useIncidentTimeCheck_ && useIncidentTimeCheck_->isChecked() &&
                                 incidentTimeEdit_ && incidentTimeEdit_->dateTime().isValid();
    const bool hasTraceHint = (incidentTraceEdit_ && !incidentTraceEdit_->text().trimmed().isEmpty()) ||
                              !lastRequest_.traceIdHint.trimmed().isEmpty();
    const bool reproduceMode = !modeBox_ || modeBox_->currentIndex() == 0;

    if (reproduceMode && !hasRequestTime && !hasIncidentTime && !hasTraceHint) {
        QMessageBox::information(this, "提示", "复现模式下请先发送请求，或切换到日志模式后直接按日志诊断。");
        return;
    }

    const CorrelationOutcome corr = correlateLogs(windowSpin_->value());
    scopedLogs_ = corr.logs;

    HttpResponseData basis = lastResponse_;
    if (!hasRequestTime) {
        basis = HttpResponseData();
    }
    lastDiagnosis_ = ruleEngine_.diagnoseWithCorrelation(basis, scopedLogs_, corr.mode, corr.evidence);
    lastAiAdvice_.clear();
    renderDiagnosis();

    if (!hasRequestTime && corr.mode == "full-log-scan") {
        statusBar()->showMessage("未提供故障时间或关联ID，已按全量日志降级诊断", 3500);
    }

    if (aiCheck_->isChecked()) {
        if (!aiAdvisor_.isConfigured()) {
            QMessageBox box(this);
            box.setIcon(QMessageBox::Question);
            box.setWindowTitle("AI 未配置");
            box.setText("当前未配置可用 AI，是否现在配置？");
            auto* configBtn = box.addButton("立即配置", QMessageBox::AcceptRole);
            box.addButton("稍后再说", QMessageBox::RejectRole);
            box.exec();
            if (box.clickedButton() == configBtn) {
                openAiConfigDialog();
            }
            lastAiAdvice_ = "[AI 建议不可用] 当前未配置可用 API。";
            renderDiagnosis();
            return;
        }

        statusBar()->showMessage("AI 分析中...");
        aiAdvisor_.requestAdvice(buildAiPrompt());
    }
}

QString MainWindow::buildAiPrompt() const {
    QString prompt;
    QTextStream s(&prompt);
    s << "Case 信息:\n";
    s << "- 问题: " << caseProblemEdit_->text() << "\n";
    s << "- 接口: " << caseTargetEdit_->text() << "\n\n";

    s << "请求信息:\n";
    s << "- Method: " << lastRequest_.method << "\n";
    s << "- URL: " << lastRequest_.url << "\n";
    s << "- Status: " << lastResponse_.statusCode << "\n";
    s << "- Error: " << lastResponse_.errorMessage << "\n";
    s << "- Latency: " << lastResponse_.latencyMs << "ms\n\n";

    s << "关联信息:\n";
    s << "- 方式: " << lastDiagnosis_.correlationMode << "\n";
    s << "- 依据: " << lastDiagnosis_.correlationEvidence << "\n";
    s << "- 可信度: " << lastDiagnosis_.confidence << "\n\n";

    s << "规则诊断摘要:\n" << lastDiagnosis_.summary << "\n\n";

    s << "关键日志（最多20行）:\n";
    int n = 0;
    for (const auto& rec : scopedLogs_) {
        s << rec.rawLine << "\n";
        if (++n >= 20) {
            break;
        }
    }

    s << "\n请输出三段：根因猜测、验证步骤、修复建议。";
    return prompt;
}

void MainWindow::renderDiagnosis() {
    QString text;
    QTextStream s(&text);

    s << "诊断摘要: " << lastDiagnosis_.summary << "\n\n";
    s << "关联方式: " << lastDiagnosis_.correlationMode << "\n";
    s << "关联依据: " << lastDiagnosis_.correlationEvidence << "\n";
    s << "关联可信度: " << lastDiagnosis_.confidence << "\n";
    s << "关联日志条数: " << lastDiagnosis_.correlatedCount << "\n\n";

    s << "行动项:\n";
    int idx = 1;
    for (const auto& f : lastDiagnosis_.findings) {
        s << idx++ << ". [S" << f.severity << "] " << f.title << "\n";
        s << "   证据: " << f.evidence << "\n";
        s << "   建议: " << f.suggestion << "\n\n";
    }

    if (!lastAiAdvice_.trimmed().isEmpty()) {
        s << "AI 建议:\n" << lastAiAdvice_ << "\n";
    }

    diagnosisView_->setPlainText(text);
}
void MainWindow::openAiConfigDialog() {
    AiConfigDialog dlg(this);
    AiConfigInput cfg = aiConfig_;
    if (cfg.baseUrl.isEmpty()) {
        cfg.baseUrl = "https://api.openai.com/v1";
    }
    if (cfg.model.isEmpty()) {
        cfg.model = "gpt-4o-mini";
    }
    dlg.setConfig(cfg);

    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    saveAiConfig(dlg.config());
}

void MainWindow::fillSampleRequestAndLog() {
    struct SampleScenario {
        QString title;
        QString problem;
        QString target;
        QString mode;
        QString incidentTrace;
        bool useIncidentTime = false;
        QDateTime incidentTime;
        HttpRequestData request;
        QString logPath;
    };

    const auto buildScenarioEvidenceText = [](const SampleScenario& scenario) {
        const QString logName = QFileInfo(scenario.logPath).fileName();
        if (!scenario.incidentTrace.trimmed().isEmpty()) {
            return QString("对应日志：%1\n主要依据：reqId/traceId = %2").arg(logName, scenario.incidentTrace.trimmed());
        }
        if (scenario.useIncidentTime && scenario.incidentTime.isValid()) {
            return QString("对应日志：%1\n主要依据：故障时间 = %2").arg(
                logName,
                scenario.incidentTime.toString("yyyy-MM-dd HH:mm:ss"));
        }
        return QString("对应日志：%1\n主要依据：日志内容 + 当前时间窗").arg(logName);
    };

    const auto resolveSamplePath = [](const QString& name) {
        const QStringList candidates = {
            QDir::currentPath() + "/sample_data/" + name,
            QDir::currentPath() + "/../sample_data/" + name,
            QDir::currentPath() + "/tests/data/" + name,
            QDir::currentPath() + "/../tests/data/" + name
        };
        for (const auto& path : candidates) {
            if (QFileInfo::exists(path)) {
                return path;
            }
        }
        return QString();
    };

    QVector<SampleScenario> scenarios;
    {
        SampleScenario scenario;
        scenario.title = "下单接口 500：库存超时";
        scenario.problem = "用户反馈下单接口偶发 500，需要判断是网关问题还是库存下游超时。";
        scenario.target = "/api/v1/orders";
        scenario.mode = "replay";
        scenario.incidentTrace = "9f2a1d";
        scenario.request.method = "POST";
        scenario.request.url = "https://httpbin.org/anything/api/v1/orders";
        scenario.request.headersText = "Content-Type: application/json\nX-Trace-Id: 9f2a1d";
        scenario.request.bodyText = "{\n  \"sku\": \"SPX-13\",\n  \"qty\": 2\n}";
        scenario.request.traceIdHint = "9f2a1d";
        scenario.request.timeoutMs = 5000;
        scenario.request.retryCount = 1;
        scenario.logPath = resolveSamplePath("scenario_order_timeout.log");
        scenarios.push_back(scenario);
    }
    {
        SampleScenario scenario;
        scenario.title = "查询接口 500：数据库死锁";
        scenario.problem = "订单查询接口报 500，需要判断是慢查询还是数据库死锁导致。";
        scenario.target = "/api/v1/orders/98321";
        scenario.mode = "replay";
        scenario.incidentTrace = "7ab4d0";
        scenario.request.method = "GET";
        scenario.request.url = "https://httpbin.org/anything/api/v1/orders/98321";
        scenario.request.headersText = "Accept: application/json\nX-Trace-Id: 7ab4d0";
        scenario.request.bodyText.clear();
        scenario.request.traceIdHint = "7ab4d0";
        scenario.request.timeoutMs = 5000;
        scenario.request.retryCount = 0;
        scenario.logPath = resolveSamplePath("scenario_query_deadlock.log");
        scenarios.push_back(scenario);
    }
    {
        SampleScenario scenario;
        scenario.title = "支付回调失败：仅凭日志排查";
        scenario.problem = "支付回调偶发失败，现场无法复现，只知道大概故障时间。";
        scenario.target = "/api/v1/payment/callback";
        scenario.mode = "log";
        scenario.useIncidentTime = true;
        scenario.incidentTime = QDateTime::fromString("2026-04-18 09:15:12", "yyyy-MM-dd HH:mm:ss");
        scenario.request.method = "POST";
        scenario.request.url = "https://httpbin.org/anything/api/v1/payment/callback";
        scenario.request.headersText = "Content-Type: application/json";
        scenario.request.bodyText = "{\n  \"provider\": \"wxpay\",\n  \"event\": \"pay_success\"\n}";
        scenario.request.timeoutMs = 3000;
        scenario.request.retryCount = 0;
        scenario.logPath = resolveSamplePath("scenario_payment_callback.log");
        scenarios.push_back(scenario);
    }

    QDialog dlg(this);
    dlg.setWindowTitle("载入示例场景");
    dlg.resize(460, 180);
    auto* layout = new QVBoxLayout(&dlg);
    auto* combo = new QComboBox(&dlg);
    for (const auto& scenario : scenarios) {
        combo->addItem(scenario.title);
    }
    auto* desc = new QLabel(&dlg);
    desc->setWordWrap(true);
    const auto refreshDesc = [&]() {
        const int idx = combo->currentIndex();
        if (idx >= 0 && idx < scenarios.size()) {
            const auto& scenario = scenarios.at(idx);
            desc->setText(scenario.problem + "\n\n" + buildScenarioEvidenceText(scenario));
        }
    };
    refreshDesc();
    connect(combo, &QComboBox::currentIndexChanged, &dlg, [&]() { refreshDesc(); });
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    buttons->button(QDialogButtonBox::Ok)->setText("载入");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(combo);
    layout->addWidget(desc);
    layout->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    const SampleScenario scenario = scenarios.at(combo->currentIndex());

    if (selectedCaseId() < 0 && store_) {
        QString err;
        const int newId = store_->createCaseSession(scenario.title, scenario.problem, scenario.target, &err);
        if (newId < 0) {
            QMessageBox::warning(this, "载入失败", err.isEmpty() ? "无法创建示例案例。" : err);
            return;
        }
        loadCaseSessions();
        for (int i = 0; i < caseBox_->count(); ++i) {
            if (caseBox_->itemData(i).toInt() == newId) {
                caseBox_->setCurrentIndex(i);
                break;
            }
        }
    }

    suppressCaseSync_ = true;
    caseProblemEdit_->setText(scenario.problem);
    caseTargetEdit_->setText(scenario.target);
    modeBox_->setCurrentIndex(scenario.mode == "replay" ? 0 : 1);
    incidentTraceEdit_->setText(scenario.incidentTrace);
    useIncidentTimeCheck_->setChecked(scenario.useIncidentTime);
    incidentTimeEdit_->setDateTime(scenario.useIncidentTime ? scenario.incidentTime : QDateTime::currentDateTime());
    methodBox_->setCurrentText(scenario.request.method);
    urlEdit_->setText(scenario.request.url);
    headersEdit_->setPlainText(scenario.request.headersText);
    bodyEdit_->setPlainText(scenario.request.bodyText);
    traceHintEdit_->setText(scenario.request.traceIdHint);
    timeoutSpin_->setValue(scenario.request.timeoutMs);
    retrySpin_->setValue(scenario.request.retryCount);
    suppressCaseSync_ = false;

    loadLogFileSync(scenario.logPath);
    saveCaseState(selectedCaseId());
    diagnosisView_->setPlainText(
        "示例场景已载入。\n\n"
        + buildScenarioEvidenceText(scenario)
        + "\n\n下一步：先查看“日志排查”，再到“诊断报告”生成结论。");
    workTabs_->setCurrentIndex(scenario.mode == "replay" ? 1 : 0);
    refreshCaseSummary();
    statusBar()->showMessage("已载入示例场景：" + scenario.title + " · " + buildScenarioEvidenceText(scenario).replace('\n', ' '), 5000);
}

void MainWindow::copyResponseText() {
    const QString text = responseEdit_->toPlainText();
    if (text.isEmpty()) {
        statusBar()->showMessage("当前没有可复制的响应内容", 2000);
        return;
    }
    QGuiApplication::clipboard()->setText(text);
    statusBar()->showMessage("响应内容已复制", 1800);
}

void MainWindow::setEmptyStates() {
    responseEdit_->setPlainText("请求验证是补充步骤。\n需要验证修复前后差异时，再回到这里补发请求。");
    logView_->setPlainText("先导入故障时段日志。\n建议带上问题发生前后 1-3 分钟内容，或直接提供 reqId/traceId。");
    diagnosisView_->setPlainText("先在“日志排查”准备证据。\n再到这里生成诊断，查看根因候选、关键证据和下一步动作。");
    refreshLogSummaryUi();
}
