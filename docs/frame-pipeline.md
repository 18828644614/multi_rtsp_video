# 单路生产者—消费者流水线

## 数据流

```text
FrameSource（当前为 FFmpeg MP4）
    → 有界 FrameQueue
    → Detector / 后续 Tracker / RuleEngine
    → DetectionSink / EventSink / Metrics
```

`FramePacket` 是输入线程与处理线程之间的唯一帧契约，包含 `FrameMetadata`、stride、像素格式和独立拥有的图像缓冲。当前 `FramePipeline` 不绑定检测算法，消费者回调可以接入 Detector。

## 输入状态

当前 MP4 解码器实现 `FrameSource`，读取结果为 `FrameReadResult`：

- `Frame`：成功取得一帧；
- `EndOfStream`：文件正常结束；
- `RetryableError`：预留给 RTSP 可重试断流；
- `FatalError`：预留给不可恢复错误；
- `Interrupted`：预留给停止流程。

MP4 EOF 不应被当作 RTSP 断流处理。

## 线程与停止

- `run()` 为生产者和消费者各启动一个线程。
- 生产者得到 `EndOfStream` 后调用 `close()`，消费者排空队列后退出。
- 生产者或消费者抛出异常时调用 `abort()`，清空待处理帧并唤醒另一线程；异常在线程汇合后重新抛给调用方。
- MP4 使用 `Block` 策略，队列 `max_age_ms` 应为 `0`，避免离线文件因推理较慢而丢帧。
- RTSP 停止时还需要增加可取消的读取和 FFmpeg interrupt callback；这属于后续 StreamWorker 阶段。

## 测试

`frame_pipeline_test` 不需要 FFmpeg 或测试视频，通过合成帧检查 FIFO 顺序、队列容量、两个线程分离，以及生产者/消费者异常能否传回调用线程。