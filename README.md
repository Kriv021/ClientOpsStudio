﻿﻿# ClientOps Studio

基于 **C++20 + Qt6** 的桌面故障排查工具，面向接口联调、线上问题复盘和日志驱动排障场景。

排障闭环包含以下步骤：

**Case 管理 -> 日志关联 -> 请求验证 -> 候选根因排序 -> AI 复核 -> 报告导出**

---

## 主界面总览

> ![mainWindow](ClientOpsStudio\assets\screenshots\mainWindow.png)

---

## 项目定位

排障往往需要以下证据：

1. 用户反馈的问题描述
2. 故障发生时间
3. `reqId / traceId`
4. 故障时段日志
5. 可选的补发请求结果

`ClientOps Studio` 做的事情，就是把这些证据组织成一个可持续使用的本地客户端工作台。

---

## 核心功能

### 1. Case 驱动排障

每个故障都以一个 `Case` 保存，独立记录：

1. 问题描述
2. 目标接口
3. 排查模式
4. 故障时间
5. `reqId / traceId`
6. 请求草稿
7. 日志路径
8. 诊断结果

### 2. 日志优先，请求验证可选

支持两种入口：

1. **按日志排查**
   - 导入故障时段日志
   - 基于 `reqId / traceId` 或故障时间做关联
   - 直接生成诊断结果
2. **请求验证**
   - 在已有日志证据基础上补发请求
   - 对比响应状态、延迟和错误信息

### 3. 诊断内核分层

```text
原始日志
  -> LogParser
  -> SignalExtractor
  -> DiagnosisEngine
  -> AI Review
  -> Diagnosis Report
```

其中：

1. `LogParser`：解析时间、级别、`reqId / traceId`
2. `SignalExtractor`：把原始日志抽成稳定语义信号
3. `DiagnosisEngine`：对候选根因做评分排序，默认保留 Top3
4. `AI Review`：基于结构化证据做受约束复核，不直接吃全量原始日志

### 4. 大日志处理优化

面向桌面客户端的可用性做了明确取舍：

1. 异步解析日志，避免阻塞 Qt 主线程
2. 导入后显示摘要信息：文件大小、总行数、`reqId` 数量、解析耗时
3. 日志视图只渲染前 `5000` 行，避免文本控件卡顿
4. 诊断仍然基于全量日志，不因显示截断而丢失分析能力

### 5. 报告导出

支持导出 Markdown 报告，方便保存排查结论和复盘过程。

---

## 架构简图

```text
┌───────────────────────────────┐
│         ClientOps Studio      │
└───────────────┬───────────────┘
                │
        ┌───────▼────────┐
        │  Case / 锚点输入 │
        │ reqId / 时间 / 接口 │
        └───────┬────────┘
                │
        ┌───────▼────────┐
        │   LogParser     │
        │ 文本日志 -> 记录  │
        └───────┬────────┘
                │
        ┌───────▼────────┐
        │ SignalExtractor │
        │ 记录 -> 诊断信号 │
        └───────┬────────┘
                │
        ┌───────▼────────┐
        │ DiagnosisEngine │
        │ 候选根因评分排序 │
        └───────┬────────┘
                │
        ┌───────▼────────┐
        │    AI Review     │
        │ 受约束复核与建议 │
        └───────┬────────┘
                │
        ┌───────▼────────┐
        │ Diagnosis Report│
        │ 文本报告 / 导出  │
        └────────────────┘
```

---

## 技术栈

### 客户端

1. `C++20`
2. `Qt6`

### 本地能力

1. `SQLite`
2. 本地日志解析
3. 本地 Markdown 报告导出

### 构建

1. `CMake`
2. `MinGW`

---

## 项目亮点

1. **更贴近真实排障流程**  
   支持日志优先、请求验证为辅的工作方式。

2. **诊断内核有明确分层**  
   “信号抽取 + 候选根因排序 + AI 复核”的结构。

3. **强调可解释性**  
   AI 不直接读取全量原始日志，只对规则已收敛出的候选和证据做复核。

4. **客户端工程工作流**  
   包含本地状态管理、异步解析、渲染截断、摘要统计和桌面工具工作流设计。

---

## 目录结构

```text
ClientOpsStudio/
├─ src/
│  ├─ app/                   # 主窗口、AI 配置弹窗、交互逻辑
│  ├─ domain/                # 诊断模型、信号抽取、候选根因排序
│  └─ infra/                 # HTTP、日志解析、SQLite、AI、导出
├─ resources/
│  ├─ icons/                 # 内置 SVG 图标
│  └─ resources.qrc
├─ sample_data/              # GUI 演示用日志样本
├─ tests/
│  ├─ data/                  # 单测与基准样本
│  ├─ unit_tests.cpp         # 自动化测试
│  ├─ log_parser_bench.cpp   # 日志解析基准
│  └─ generate_bench_log.ps1 # 生成大日志样本
├─ assets/
│  └─ screenshots/           # README 截图占位目录
├─ build.ps1
├─ run.ps1
├─ CMakeLists.txt
└─ README.md
```

---

## 快速开始

### 环境建议

1. Windows 10 / 11
2. Qt 6
3. CMake 3.21+
4. MinGW 或其他可用的 C++ 编译器

### 使用脚本构建

```powershell
.\build.ps1 -QtPrefixPath "D:\Qt\6.6.3\mingw_64"
```

如果已设置环境变量 `QT_ROOT`，可以直接：

```powershell
.\build.ps1
```

### 运行

如果 Qt 运行时已在 `PATH` 中：

```powershell
.\run.ps1
```

如果没有，可先设置：

```powershell
$env:QT_RUNTIME_DIR="D:\Qt\6.6.3\mingw_64\bin"
.\run.ps1
```

### 手动构建

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="D:/Qt/6.6.3/mingw_64"
cmake --build build -j 8
```

---

## 测试

### 单元测试

```powershell
ctest --test-dir .\build --output-on-failure
```

当前覆盖重点：

1. 日志解析最小样本
2. 信号抽取
3. 候选根因排序
4. 多请求噪声场景
5. Case 状态持久化
6. 日志摘要统计

### 生成大日志样本

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\generate_bench_log.ps1
```

默认生成：

```text
tests/data/bench_mixed_5mb.log
```

---

## 示例场景

项目内置 3 个轻度真实化的演示场景：

1. **下单接口 500：库存超时**
2. **查询接口 500：数据库死锁**
3. **支付回调失败：仅凭日志排查**

这些样本刻意包含：

1. 主故障链路
2. 正常请求噪声
3. 干扰性 `WARN`

用于测试和演示“规则诊断 + AI 复核”链路是否能有效排障。

---

## 已知边界

1. 当前优先支持 **单行文本日志**，尚未扩展到 JSON 日志
2. 当前主要验证 **Windows** 环境，未完整验证 macOS / Linux
3. 诊断目标是高频故障大类定位，不是覆盖所有生产环境异常
4. AI 只做受约束复核，不直接替代规则诊断
