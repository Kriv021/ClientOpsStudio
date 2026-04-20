# ClientOps Studio

`ClientOps Studio` 是一个基于 **C++20 + Qt6** 实现的桌面故障排查工具。  
它把接口联调、故障日志分析、规则诊断、AI 建议和报告导出放到一个本地客户端里，目标是帮助开发和测试更高效地完成接口故障定位与问题复盘。

> 这个项目更关注“排障工作流”而不是“底层抓包细节”。  
> 它不是 Wireshark / Fiddler 的替代品，而是一个面向 **接口排障闭环** 的桌面工具。

---

## 功能概览

### 1. Case 驱动的排障流程

每个故障都以一个 `Case` 组织，支持保存：

1. 问题描述
2. 目标接口
3. 排障模式
4. 故障时间
5. `reqId / traceId`
6. 请求草稿
7. 日志路径
8. 诊断结果

这样可以把一次排障过程完整地沉淀下来，而不是散落在日志、请求工具和聊天记录里。

### 2. 双入口排障

项目支持两种入口：

1. **按日志排查**
   - 导入故障时段日志
   - 根据 `reqId / traceId` 或故障时间做关联
   - 直接生成诊断

2. **请求验证**
   - 在已有日志证据基础上补发请求
   - 对比响应状态、错误信息和返回体

日志应该是第一入口，请求验证只是补充。

### 3. 日志关联与规则诊断

当前支持：

1. 解析时间戳、级别、`reqId`、`traceId`
2. 根据 `reqId / traceId` 做精确关联
3. 根据故障时间 + 时间窗做范围关联
4. 关键字和级别过滤
5. 基于规则输出可解释的诊断结果

目前内置的典型场景包括：

1. 下单接口 500：库存服务超时
2. 查询接口 500：数据库死锁
3. 支付回调失败：仅凭日志排查

### 4. AI 建议作为增强层

支持配置兼容 OpenAI 风格的 AI API，对规则诊断结果补充自然语言建议。  
AI 不是主流程依赖，配置缺失或调用失败时，系统会自动回退到规则诊断。

### 5. 面向客户端的性能取舍

1. 日志解析采用异步流程，避免阻塞主线程
2. 导入后展示摘要信息：
   - 文件名
   - 文件大小
   - 总行数
   - 不同 `reqId` 数量
   - 解析耗时
3. 日志视图最多渲染前 `5000` 行，避免文本控件卡顿
4. 诊断仍基于全量日志，不受显示截断影响

---

## 为什么做这个项目

在真实开发和测试工作中，排查一个接口故障通常需要来回切换多个工具：

1. Postman / curl 补发请求
2. 文本编辑器或平台查看日志
3. 手工比对 `reqId / traceId`
4. 整理排查结论并输出报告

`ClientOps Studio` 的设计目标，就是把这些步骤组织成一个更自然的桌面工作流：

```text
Case 管理 -> 日志导入 -> 证据关联 -> 请求验证 -> 诊断输出 -> 报告导出
```

这个项目的核心价值不在于“功能堆得多”，而在于：

1. 它有明确的使用场景
2. 它体现了桌面客户端的状态管理与交互组织
3. 它对真实排障流程做了抽象，而不是只做一个请求编辑器

---

## 技术栈

1. `C++20`
2. `Qt6 Widgets / Network / Sql / Concurrent / Svg`
3. `SQLite`
4. `CMake`

---

## 项目结构

```text
ClientOpsStudio_upload/
├─ src/
│  ├─ app/                    # UI 与交互逻辑
│  ├─ domain/                 # 数据模型与规则引擎
│  └─ infra/                  # HTTP、日志解析、存储、AI、导出
├─ resources/
│  ├─ icons/                  # 内置 SVG 图标
│  └─ resources.qrc
├─ tests/
│  ├─ data/                   # 测试样本
│  ├─ unit_tests.cpp          # 单元测试
│  ├─ log_parser_bench.cpp    # 日志解析基准
│  └─ generate_bench_log.ps1  # 生成性能样本
├─ build.ps1                  # PowerShell 构建脚本
├─ run.ps1                    # PowerShell 运行脚本
├─ CMakeLists.txt
└─ README.md
```

---

## 快速开始

### 环境要求

建议环境：

1. Windows 10 / 11
2. Qt 6
3. CMake 3.21+
4. MinGW 或其他可用 C++ 编译器

### 方式一：使用 PowerShell 脚本

#### 构建

```powershell
.\build.ps1 -QtPrefixPath "D:\Qt\6.6.3\mingw_64"
```

如果你已经设置了环境变量 `QT_ROOT`，也可以直接：

```powershell
.\build.ps1
```

#### 运行

如果你的 Qt 运行时目录已经在 `PATH` 中：

```powershell
.\run.ps1
```

如果没有，可以先设置：

```powershell
$env:QT_RUNTIME_DIR="D:\Qt\6.6.3\mingw_64\bin"
.\run.ps1
```

### 方式二：手动使用 CMake

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="D:/Qt/6.6.3/mingw_64"
cmake --build build -j 8
```

运行：

```powershell
.\build\ClientOpsStudio.exe
```

---

## 测试

### 单元测试

```powershell
ctest --test-dir .\build --output-on-failure
```

当前覆盖的重点包括：

1. 日志解析最小样本
2. 三个故障场景的规则诊断区分
3. 场景日志是否包含噪声和多请求
4. Case 状态持久化
5. 日志导入摘要统计

### 生成性能样本

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\generate_bench_log.ps1
```

默认会生成：

```text
tests/data/bench_mixed_5mb.log
```

### 跑日志解析基准

```powershell
.\build\LogParserBench.exe .\tests\data\bench_mixed_5mb.log
```

---

## 内置示例场景

项目内置 3 个轻度真实化的故障样本，用于测试和演示：

1. **下单接口 500：库存超时**
2. **查询接口 500：数据库死锁**
3. **支付回调失败：仅凭日志排查**

这些样本特意加入了：

1. 主故障链路
2. 正常请求噪声
3. 干扰性 `WARN`

这样既便于测试，也更接近真实排障场景。

