# src

实现目录建议按职责拆分，而不是把所有逻辑集中在 `main.cpp`：

- `main.cpp`：进程入口、配置加载和生命周期编排。
- `config/`：配置解析、默认值和校验。
- `media/`：单路 MP4 的 FFmpeg 解码、帧转换和稳定的 BGR24 帧契约；后续再拆分 `VideoSource` 与通用 `Decoder`。
- `pipeline/`：`FrameQueue`、`StreamWorker` 和停止流程。
- `inference/`：ONNX Runtime 会话、预处理和后处理。
- `tracking/`：简化 Tracker 和轨迹状态。
- `events/`：`RuleEngine`、ROI、越线和去重。
- `output/`：标注视频、JSONL、截图和指标输出。
- `observability/`：结构化日志和运行指标。
