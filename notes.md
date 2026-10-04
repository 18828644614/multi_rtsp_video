# Notes: 多路 RTSP 视频分析项目

## Repository Snapshot
- 项目当前只有设计文档、配置模板和目录说明，没有实际 C++ 源文件、头文件、CMake 工程或测试代码。
- `README.md` 将目标定义为 C++17/CMake 项目：输入 MP4/RTSP，FFmpeg 解码，OpenCV 图像处理，ONNX Runtime 推理，输出检测、事件和指标。
- 第一版明确不做模型训练、复杂 GUI、云服务、鉴权、集群调度和高可用。
- 当前 Git 只有一个“系统架构”提交，说明项目仍处于规划/架构阶段。

## Architecture Findings
- 主链路：输入源 → StreamManager → 每路 StreamWorker → FFmpeg 解码 → FrameConverter → 每路有界 FrameQueue → ProcessingWorker → Detector → Tracker → RuleEngine → ResultSink/EventSink。
- 第一版线程模型：每路一个输入/解码线程 + 每路一个处理线程；共享推理线程池属于后续优化。
- `StreamManager` 管全局配置、启动/停止和进程级错误；`StreamWorker` 管单路生命周期、状态、重连和资源释放。
- `VideoSource` 负责输入生命周期，`Decoder` 负责 FFmpeg 解封装/解码，两者需要解耦。
- `FramePacket` 是解码线程与处理线程之间的稳定数据契约，必须携带 `stream_id`、序号、PTS、time_base、墙钟时间、单调时钟时间、尺寸和图像所有权。
- `Detector` 负责模型签名校验、预处理、ONNX 推理、后处理、类别过滤和 NMS，并向上层暴露统一 `DetectionResult`。
- `Tracker` 提供稳定 `track_id`；`RuleEngine` 只做 ROI、越线、进入/离开、冷却和去重，不负责输出文件或网络发送。
- `ResultSink` 输出标注视频/检测结果；`EventSink` 输出 JSONL、截图和事件；`Metrics` 记录 FPS、延迟、队列、丢帧、重连等指标。

## Execution Flow
1. 启动时读取 YAML，校验未知字段、路径、阈值、队列策略和每路流配置。
2. 加载并校验 ONNX 模型 manifest，确认尺寸、布局、颜色、归一化、类别顺序和输出格式。
3. 为每路输入创建 `StreamWorker`、输入线程、队列、处理线程和输出对象。
4. MP4/RTSP 输入由 FFmpeg 解码；MP4 EOF 与 RTSP Disconnect 使用不同处理策略。
5. 解码帧转换为模型需要的 RGB/BGR，保存时间戳并放入有界队列。
6. 处理线程取帧，执行检测、跟踪和事件规则，再写入检测 JSONL、事件 JSONL、截图和标注视频。
7. 队列积压时实时流默认 `drop_oldest`，通过限制帧数和最大帧龄保持延迟有界。
8. RTSP 连接失败/超时进入重连状态，采用退避；单路失败不能结束其他流。
9. 停止时先停止接收、关闭队列，再按输入、处理、输出顺序释放资源。

## Beginner Roadmap Findings
- 阶段 0：准备编译器/CMake/FFmpeg/OpenCV/ONNX Runtime/GoogleTest、固定视频和授权模型。
- 阶段 1：先建立能编译的 CMake 空工程、配置解析、日志、版本、停止令牌和错误类型。
- 阶段 2：只做单路 MP4 解码，验证序号、PTS、time_base、`AVFrame` → `cv::Mat`、资源释放。
- 阶段 3：加入 ONNX Detector，先完成预处理/推理/后处理/NMS，再输出标注视频和 Detection JSONL。
- 阶段 4：加入简化 Tracker、ROI、计数、越线、事件冷却和截图。
- 阶段 5：用 MediaMTX 将 MP4 发布为 RTSP，实现单路连接、超时、断流识别和指数退避重连。
- 阶段 6：复制为多路 `StreamWorker`，增加每路有界队列、丢帧、独立指标和故障隔离。
- 阶段 7：做故障注入、Sanitizer、长时间运行、固定硬件/模型/输入的性能基线。
- 阶段 8：补文档、截图、原始指标和面试说明，只写实际完成和实际测得的内容。

## Risks and Beginner Traps
- 不要一开始就做四路 RTSP；否则很难判断问题来自 FFmpeg、线程、模型还是网络。
- 不要把 FFmpeg `AVFrame` 私有缓冲区直接跨线程传递；必须明确复制、引用计数或对象池所有权。
- 不要用墙上时间计算延迟；系统时间可能跳变，延迟应使用单调时钟。
- 不要把模型特有输出格式泄漏到 Tracker/RuleEngine；先定义统一检测数据结构。
- 不要把队列做成无限长度；推理变慢时会导致延迟和内存一起增长。
- 不要把 MP4 EOF 当成 RTSP 断流；前者通常是正常结束，后者需要重连。
- 不要先优化共享推理线程池；第一版每路独立处理更容易调试和证明正确。

## Configuration Implementation Findings
- 接入 yaml-cpp-0.9.0，由 CMake FetchContent 固定版本下载并构建。
- 配置加载器将 YAML 转换为 AppConfig，并在启动阶段校验未知字段、必填字段、阈值、枚举、路径、URL 和重复流 ID。
- 当前相对路径按程序当前工作目录解析，因此应从项目根目录运行程序。
- configs/example.yaml 当前会因为缺少 data/demo.mp4 被拒绝，这是预期的启动前路径校验行为。
- 当前阶段只统计 rois/lines 数量，尚未解析 ROI 和越线几何数据；模型 manifest 也只检查文件存在性，详细字段校验属于后续 Detector 阶段。
