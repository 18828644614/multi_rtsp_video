# 多路 RTSP 实时视频分析系统

## 项目定位

这是一个以 C++ 为核心、面向音视频与计算机视觉工程实践的多路视频分析项目。系统从本地 MP4 或 RTSP 输入读取视频，使用 FFmpeg 完成媒体读取与解码，使用 OpenCV 完成图像处理，使用 ONNX Runtime 运行目标检测，并输出检测结果、事件和运行指标。

当前状态：基础工程与单路 MP4 解码已实现。配置解析、FFmpeg 解封装/解码、BGR24 帧转换、时间戳保留和模块测试已具备；检测、跟踪、事件与 RTSP 仍按路线逐步实现。

## 第一版目标

- 支持本地 MP4 和 RTSP 两类输入。
- 支持至少四路本地模拟 RTSP 流并行处理。
- 检测人员、汽车、卡车和公交车等目标。
- 输出检测框、目标数量、区域事件、越线事件和 JSON Lines。
- 具备断流重连、有界队列、丢帧控制、结构化日志和运行指标。
- 所有性能数字来自固定硬件、固定模型和可重复测试。

第一版不训练模型、不实现复杂 GUI、不接入云服务，也不把生产级鉴权、集群调度和高可用作为目标。

## 技术栈

- C++17 或更高版本
- CMake
- FFmpeg
- OpenCV
- ONNX Runtime
- MediaMTX：将本地 MP4 模拟为 RTSP 流
- GoogleTest：单元测试和模块测试
- Git、Linux/Windows、GDB 或 Visual Studio 调试工具

## 目录结构

```text
multi_rtsp_video_analysis/
├─ README.md
├─ docs/
│  ├─ requirements.md
│  ├─ architecture.md
│  ├─ data-models.md
│  ├─ configuration.md
│  ├─ output-schemas.md
│  ├─ dependency.md
│  ├─ development-roadmap.md
│  ├─ testing.md
│  └─ decisions/
├─ src/
├─ include/
├─ tests/
├─ models/
├─ configs/
└─ scripts/
```

## 推荐实现顺序

```text
项目骨架与配置校验
  → 单路 MP4 解码与时间戳
  → 单路检测与标注输出
  → Tracker 与事件规则
  → 单路 RTSP 与重连
  → 多路并发与有界队列
  → 故障注入、压力测试与性能基线
```

## 文档入口

- 需求和可量化验收：`docs/requirements.md`
- 系统架构、线程模型和状态机：`docs/architecture.md`
- 模块间数据契约：`docs/data-models.md`
- 配置字段和默认值：`docs/configuration.md`
- 输出 JSON/JSONL 结构：`docs/output-schemas.md`
- 依赖和构建环境：`docs/dependency.md`
- 开发路线：`docs/development-roadmap.md`
- 测试与性能验证：`docs/testing.md`
- 配置样例：`configs/example.yaml`

## 快速开始目标

实现后应支持以下最小验证流程：

```text
准备模型和本地 MP4
  → 使用 configs/example.yaml 启动单路 MP4
  → 输出标注视频和 JSONL 事件
  → 使用 MediaMTX 发布多路 RTSP
  → 运行四路并发和断流重连测试
```

## 单路 MP4 解码验证

FFmpeg 开发包需要同时提供 `include/` 和 `lib/`。如果 `ffmpeg.exe` 已经在 `PATH` 中，CMake 会自动尝试从它的 `bin/` 父目录推导 `FFMPEG_ROOT`。建议从 Visual Studio 2022 的 Developer PowerShell 或 Developer Command Prompt（x64）中运行以下命令：

```powershell
cmake --fresh --preset msvc-debug
cmake --build --preset msvc-debug --config Debug
```

直接解码并输出帧数、分辨率、编码器、time base 和最后一帧信息：

```powershell
build/msvc-debug/Debug/multi_rtsp_video_analysis.exe --decode-mp4 data/demo.mp4
```

只解码前 10 帧可用于快速检查：

```powershell
build/msvc-debug/Debug/multi_rtsp_video_analysis.exe --decode-mp4 data/demo.mp4 10
```

实现细节、资源生命周期和排错方式见 `docs/mp4-decoder.md`。