# Task Plan: 多路 RTSP 视频分析项目梳理

## Goal
从新手视角说明项目的技术框架、运行链路和分阶段实施路线，并逐步完成可运行的项目骨架。

## Phases
- [x] Phase 1: 读取规范与建立分析计划
- [x] Phase 2: 梳理目录、入口和核心模块
- [x] Phase 3: 追踪数据处理与运行流程
- [x] Phase 4: 整理新手分阶段实施路线
- [x] Phase 5: 复核结论并交付文档
- [x] Phase 6: 创建 CMake 空工程并验证构建
- [x] Phase 7: 实现 YAML 配置读取与启动校验

## Key Questions
1. 项目的入口在哪里，启动时依次加载什么？
2. RTSP 输入、解码、推理、结果输出分别由哪些文件负责？
3. 新手应该按什么顺序学习和实现，怎样验证每一步？
4. 最小 CMake 工程怎样在当前 Windows/MinGW 环境构建？
5. 配置错误能否在创建线程前被发现？

## Decisions Made
- 先基于源码和配置还原真实架构，再给出学习与实现建议。
- 将当前设计和未来需要编写的实现明确区分。
- 推荐以单路 MP4 作为第一个可运行里程碑，再逐步增加模型、RTSP、并发和故障恢复。
- 当前阶段使用无第三方依赖的 CTest smoke test，后续依赖统一后再切换到 GoogleTest。
- 当前机器使用 MinGW Makefiles，因为可用 g++ 7.3.0 和 mingw32-make，但没有 cl 或 ninja。
- YAML 使用固定版本 yaml-cpp-0.9.0，由 CMake FetchContent 管理。
- 相对路径按程序当前工作目录解析；运行程序时从项目根目录启动。
- 配置阶段校验模型、manifest、MP4 路径、阈值、队列策略、流类型、URL 格式、重复 stream ID 和未知字段。

## Errors Encountered
- apply_patch 返回 Access is denied；改用当前工作区允许的 PowerShell 文件写入方式。
- WinGet 在 Codex 执行环境中无法启动；未影响当前 CMake 构建。
- GCC 7.3.0 缺少完整 filesystem 和部分现代 API；加入 experimental filesystem 兼容层并链接 stdc++fs。
- Windows YAML 路径反斜杠需要转成 /，否则双引号字符串会触发 YAML 转义解析错误。
- 旧版 MinGW 临时目录清理存在权限问题，配置测试改为忽略清理失败。

## Deliverables
- project_analysis.md：项目框架分析与新手实施指南。
- notes.md：源码和文档梳理笔记。
- CMakeLists.txt：构建 yaml-cpp、配置库、主程序和测试。
- CMakePresets.json：当前 MinGW Debug 配置、构建和测试预设。
- include/config/config.hpp：配置数据结构和接口。
- src/config/config.cpp：YAML 解析、字段校验和路径校验。
- src/main.cpp：读取配置并在启动阶段校验。
- tests/config_test.cpp：有效配置和错误配置测试。

## Validation
- cmake --preset mingw-debug：通过。
- cmake --build --preset mingw-debug --parallel 4：通过。
- ctest --preset mingw-debug --output-on-failure：2/2 通过。
- 缺少 data/demo.mp4 时，程序返回退出码 2 并提前报告配置错误。

## Status
**Completed** - 已完成项目分析、CMake 骨架和 YAML 配置阶段。
