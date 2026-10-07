# 配置说明

配置采用 YAML。配置校验应在创建线程和加载模型前完成；未知字段默认视为错误，避免拼写错误被静默忽略。

## 1. 配置层级

- `app`：进程级行为和停止策略。
- `model`：模型文件路径、manifest 路径、运行时置信度阈值和类别过滤。
- `queue`：所有流的默认队列容量、帧龄和丢帧策略。
- `output`：输出目录、视频/事件/截图开关和指标窗口。
- `streams`：每路输入、队列覆盖、重连和事件规则。

模型 manifest 是模型固有契约，负责描述输入尺寸、布局、颜色、归一化、输出格式、类别顺序和 NMS 参数。运行 YAML 不重复声明 manifest 中的输入尺寸和 NMS 阈值。

当前相对路径继续按**程序当前工作目录**解析。建议从项目根目录启动程序；`AppConfig::base_directory` 保留该语义。

## 2. 关键字段

| 字段 | 类型 | 说明 |
|---|---|---|
| `app.shutdown_timeout_ms` | integer | 停止阶段最大等待时间 |
| `model.path` | string | ONNX 模型路径 |
| `model.manifest` | string | 模型 manifest 路径 |
| `model.confidence_threshold` | number | 运行时置信度阈值，范围 `[0, 1]` |
| `model.class_filter` | sequence | 类别名称列表；空列表表示不过滤 |
| `queue.max_frames` | integer | 默认每路最大帧数，必须大于 0 |
| `queue.max_age_ms` | integer | 默认最大帧龄；`0` 表示不按帧龄清理 |
| `queue.drop_policy` | enum | `drop_oldest`、`drop_newest` 或 `block` |
| `streams[].id` | string | 稳定的流标识 |
| `streams[].type` | enum | `mp4` 或 `rtsp` |
| `streams[].url/path` | string | 输入地址或文件路径 |
| `streams[].queue` | object | 可覆盖全局队列设置 |
| `streams[].reconnect` | object | 超时、退避和最大重试配置 |
| `streams[].rules` | object | Tracker 和事件规则；当前 ROI/越线几何尚未实现 |

## 3. 队列策略

- MP4 离线流建议使用 `block`，并将 `max_age_ms` 设置为 `0`，保证不因处理速度较慢而主动丢帧。
- RTSP 实时流建议使用 `drop_oldest`，并设置正的 `max_age_ms`，优先保证低延迟。
- `drop_policy: block` 与正的 `max_age_ms` 不能同时使用，配置加载时会失败。
- `streams[].queue` 继承全局 `queue`，只覆盖显式填写的字段。

## 4. 类别过滤

`model.class_filter` 使用 manifest 中的类别名称。检测器启动时会检查每个名称是否存在；未知类别属于启动错误，而不是运行时静默忽略。

过滤发生在 YOLO 后处理阶段，因此 Tracker、RuleEngine 和 JSONL 都只会看到过滤后的目标。空列表表示保留模型输出的所有类别。

## 5. 当前未实现字段

`rules.rois` 和 `rules.lines` 当前只允许空列表。填写非空几何规则会在启动配置阶段明确失败，避免规则配置被静默丢弃。等 ROI/越线模块完成后，再扩展这些字段的数据结构和校验。

## 6. 配置驱动的单路 MP4

当前单路离线检测入口使用同一份 YAML 配置，不再接受模型、manifest、队列容量或输出文件作为第二套命令行参数：

```powershell
multi_rtsp_video_analysis.exe --process-mp4 configs/example.yaml demo-mp4 100
```

命令格式为 `--process-mp4 <config.yaml> [stream-id] [max-frames]`：

- 未填写 `stream-id` 时，选择配置中第一个 `type: mp4` 的流。
- `max-frames` 只用于本次运行的快速限制；省略或填写 `0` 表示处理到文件结束。
- 输出目录由 `output.directory` 决定，结果文件名为 `<stream-id>.detections.jsonl`。
- 当前 MP4 入口不支持 `loop: true`；配置加载可以保留该字段供后续输入源实现，但运行时会明确报错。
- `output.save_annotated_video: true` 时，会额外生成 `<stream-id>.annotated.mp4`；当前使用输入视频的帧率和 `mp4v` 编码，并在每帧上绘制类别、置信度和绿色检测框。
- `output.save_annotated_video: false` 时，不生成标注视频；JSONL 检测结果仍然照常输出。
- 事件和截图开关已经进入配置契约，但事件输出和告警截图仍属于后续阶段。

环境变量 `${NAME}` 替换尚未实现；配置文档暂不把它作为可用能力。
