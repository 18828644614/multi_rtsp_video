# 数据模型与接口契约

## 1. `FrameMetadata`

`FrameMetadata` 是所有跨模块结果共享的帧元数据，定义在 `include/media/frame.hpp`。

| 字段 | 类型 | 说明 |
|---|---|---|
| `stream_id` | string | 稳定的流标识 |
| `sequence` | uint64 | 输入后递增的帧序号；实时丢帧后允许出现间隔 |
| `source_epoch` | uint64 | 输入源重新打开后的代数；当前 MP4 每次成功打开时递增 |
| `pts` | optional int64 | FFmpeg 原始 PTS；无效时为空 |
| `time_base` | rational | PTS 对应时基 |
| `received_at_unix_ms` | int64 | 程序收到/解码出当前帧的墙上时钟 |
| `received_at_steady_ms` | optional int64 | 程序收到帧时的单调时钟，仅用于延迟和队列年龄计算 |
| `width/height` | int | 解码后图像尺寸 |

PTS 是媒体时间轴，不是现实世界的拍摄时间。`received_at_unix_ms` 可以用于日志和外部系统关联，但 MP4 输入下它表示程序接收时间。`received_at_steady_ms` 不能当作日期时间输出。

## 2. `FramePacket`

`FramePacket` 是解码线程与处理线程之间的帧契约：

```text
FrameMetadata metadata
int stride
PixelFormat pixel_format
std::vector<uint8_t> image
```

`image` 是独立拥有的 BGR24 缓冲区。队列只移动 `FramePacket` 的所有权，不保存 `AVFrame*`、`AVPacket*` 或临时裸指针。

## 3. 输入源读取状态

未来 MP4 和 RTSP 输入统一通过 `FrameSource` 表达读取结果：

```text
Frame
EndOfStream
RetryableError
FatalError
Interrupted
```

当前 `FfmpegMp4Decoder` 已实现 `FrameSource`。MP4 正常结束返回 `EndOfStream`；RTSP 后续使用 `RetryableError` 表示可重连断流，使用 `Interrupted` 表示收到停止请求。

## 4. `Detection`

`BoundingBox` 使用原始图像的像素坐标：`x/y` 是左上角，`width/height` 是框尺寸。检测框必须在原始帧边界内，不能使用模型输入尺寸或归一化坐标。

`Detection` 保存 `class_id`、类别名称、`[0, 1]` 范围内的置信度和边界框。

## 5. `DetectionResult`

`DetectionResult` 复用完整的 `FrameMetadata`：

```text
FrameMetadata metadata
std::vector<Detection> detections
```

Detector 必须保留输入帧的流 ID、序号、代数、PTS、时基、接收时间和尺寸。图像缓冲不进入检测结果，避免不必要的复制。

## 6. Tracker 和 Event 预留契约

```text
Tracker:
  输入 DetectionResult
  输出带 track_id 的结果
  状态按 stream_id 和 source_epoch 隔离

RuleEngine:
  输入带 track_id 的结果
  输出当前帧统计和零个或多个 Event
```

`track_id` 至少在单路流生命周期内稳定。断流重连后是否清空轨迹由后续 StreamWorker 配置决定。

## 7. 时间与所有权规则

- 延迟、队列等待时间和帧龄使用 `received_at_steady_ms`。
- 事件和输出同时保留媒体时间（PTS/time_base）与接收墙钟时间。
- 无效 PTS 使用空值，不使用伪造的 `0`。
- 所有跨线程对象优先使用值语义、引用计数或明确的对象池。