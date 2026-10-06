# 数据模型与接口契约

## 1. `FramePacket`

`FramePacket` 是解码线程与处理线程之间的唯一帧契约，代码定义在 `include/media/frame.hpp`。

| 字段 | 类型 | 说明 |
|---|---|---|
| `stream_id` | string | 流的稳定标识 |
| `sequence` | uint64 | 输入后递增的帧序号，丢帧后允许出现间隔 |
| `pts` | int64 | FFmpeg 原始 PTS |
| `time_base` | rational | PTS 对应时基 |
| `capture_time_ms` | int64 | 输入接收时的墙上时钟时间 |
| `monotonic_time_ms` | int64 | 输入接收时的单调时钟时间 |
| `width/height` | int | 图像尺寸 |
| `pixel_format` | enum | 当前输出为 `Bgr24` |
| `image` | `std::vector<uint8_t>` | 独立拥有的 BGR24 字节缓冲区 |

## 2. `BoundingBox`、`Detection` 与 `DetectionResult`

`BoundingBox` 使用原始图像的像素坐标：`x/y` 是左上角，`width/height` 是框的尺寸。坐标不能使用模型输入尺寸或 0 到 1 的归一化坐标。

`BoundingBox::isWithinFrame(frame_width, frame_height)` 只做校验，不会自动裁剪。它要求图像尺寸为正、坐标和尺寸都是有限值、左上角不为负、框宽高为正，且 `x + width`、`y + height` 不超过图像边界。检测器若得到越界框，应先按策略裁剪，再将结果交给下游；否则校验会返回 `false`。

`Detection` 保存 `class_id`、可读类别 `label`、`[0, 1]` 范围内的 `confidence` 和 `bbox`。

`DetectionResult` 保存一次帧检测结果：流 ID、帧序号、图像尺寸、检测列表，以及完整的帧时间信息。`pts + time_base` 表示源媒体时间；`capture_time_ms` 是接收帧时的墙钟时间；`monotonic_time_ms` 用于延迟计算。不要把这些时间字段混为一个时间戳。

## 3. `Track`

```text
track_id          当前 stream_id 内唯一的目标编号
detection         当前帧检测结果
center            当前中心点
previous_center   上一帧中心点
age               轨迹存活帧数
lost_frames       连续未匹配帧数
state             Tentative/Confirmed/Lost/Removed
```

`track_id` 至少在单路流生命周期内稳定；断流重连后是否重置由配置决定。

## 4. `Event`

事件必须包含 `event_id`、`stream_id`、`event_type`、`timestamp_ms`、`frame_sequence` 和相关 `track_ids`。事件去重由 `event_type + stream_id + track_id + rule_id` 与冷却窗口共同决定。

## 5. 时间与所有权规则

- 延迟使用单调时钟计算。
- 事件和输出使用源 PTS 转换后的时间，实时输入同时保存接收时间。
- 队列中的帧必须拥有独立、可追踪的生命周期。
- 输出线程不能持有已被复用的解码缓冲区。
- 所有跨线程对象优先使用值语义、引用计数或明确的对象池。