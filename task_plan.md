# Task Plan: 多路 RTSP 视频分析项目梳理

## Goal
从新手视角说明项目的技术框架、运行链路和分阶段实现路线，并完成第一阶段的最小 CMake 骨架。

## Phases
- [x] Phase 1: 读取规范与建立分析计划
- [x] Phase 2: 梳理目录、入口和核心模块
- [x] Phase 3: 追踪数据处理与运行流程
- [x] Phase 4: 整理新手分阶段实施路线
- [x] Phase 5: 复核结论并交付文档
- [x] Phase 6: 创建 CMake 空工程并验证构建

## Key Questions
1. 项目的入口在哪里，启动时依次加载什么？
2. RTSP 输入、解码、推理、结果输出分别由哪些文件负责？
3. 新手应该按什么顺序学习和实现，怎样验证每一步？
4. 最小 CMake 工程怎样在当前 Windows/MinGW 环境构建？

## Decisions Made
- 先基于源码和配置还原真实架构，再给出学习与实现建议。
- 将“当前设计”和“未来需要编写的实现”明确区分。
- 推荐以单路 MP4 作为第一个可运行里程碑，再逐步增加模型、RTSP、并发和故障恢复。
- 当前阶段使用无第三方依赖的 CTest smoke test，避免在 CMake 骨架阶段被 GoogleTest 安装阻塞；后续依赖统一后再切换到 GoogleTest。
- 当前机器使用 `MinGW Makefiles`，因为可用 `g++ 7.3.0` 和 `mingw32-make`，但没有 `cl` 或 `ninja`。

## Errors Encountered
- `apply_patch` 返回 `Access is denied.`；改用当前工作区允许的 PowerShell 文件写入方式建立分析记录。
- WinGet 在 Codex 执行环境中无法启动；未影响当前 CMake 骨架构建。

## Deliverables
- `project_analysis.md`：完整的框架分析与新手实施指南。
- `notes.md`：源码/文档梳理笔记。
- `CMakeLists.txt`：最小构建目标和 CTest 测试注册。
- `CMakePresets.json`：当前 MinGW Debug 配置、构建和测试预设。
- `src/main.cpp`：版本信息入口。
- `tests/smoke_test.cpp`：无第三方依赖的最小测试。

## Validation
- `cmake --preset mingw-debug`：通过。
- `cmake --build --preset mingw-debug --parallel 4`：通过。
- `ctest --preset mingw-debug --output-on-failure`：1/1 通过。
- 程序输出 `multi-rtsp-video-analysis` 和 `version: 0.1.0`。

## Status
**Completed** - 已完成项目分析和第一阶段 CMake 空工程验证。
