# 配置说明

配置采用 YAML。配置校验应在创建线程和加载模型前完成；未知字段默认视为错误，避免拼写错误被静默忽略。

## 1. 配置层级

- `app`：进程级行为和停止策略。
- `model`：模型路径、输入尺寸和阈值。
- `queue`：队列容量和丢帧策略。
- `output`：输出目录、视频和事件开关。
- `streams`：每路输入、重连和事件规则。

环境变量使用 `${NAME}` 形式替换，密码和令牌只允许来自环境变量，不写入样例文件。

## 2. 关键字段

| 字段 | 类型 | 说明 |
|---|---|---|
| `app.shutdown_timeout_ms` | integer | 停止阶段最大等待时间 |
| `model.path` | string | ONNX 模型路径 |
| `model.manifest` | string | 模型 manifest 路径 |
| `model.confidence_threshold` | number | 置信度阈值 |
| `model.nms_threshold` | number | NMS 阈值 |
| `queue.max_frames` | integer | 每路最大帧数 |
| `queue.max_age_ms` | integer | 队列允许的最大帧龄 |
| `queue.drop_policy` | enum | `drop_oldest`、`drop_newest` 或 `block` |
| `streams[].id` | string | 稳定的流标识 |
| `streams[].type` | enum | `mp4` 或 `rtsp` |
| `streams[].url/path` | string | 输入地址或文件路径 |
| `streams[].reconnect` | object | 超时、退避和最大重试配置 |
| `streams[].rules` | object | ROI、越线和冷却配置 |

## 3. 默认策略

- 实时 RTSP 默认 `drop_oldest`。
- MP4 离线模式默认使用 `block`，保证不主动丢帧。
- 单路队列必须同时受 `max_frames` 和 `max_age_ms` 限制。
- 模型和配置校验失败属于启动失败。
- RTSP 连接失败属于单路重连，不影响其他流。

