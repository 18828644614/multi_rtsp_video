# 单路生产者—消费者流水线

## 数据流

```text
FFmpeg MP4 解码器（生产者线程）
    → 有界 FrameQueue
    → 帧处理回调（消费者线程）
```

`FramePipeline` 不绑定检测算法。生产者回调负责取得下一帧并写入 `FramePacket`；消费者回调负责处理一帧。现在的 MP4 命令只检查图像数据并统计帧数，后续可在消费者回调中接入 Detector。

## 线程与停止

- `run()` 为生产者和消费者各启动一个线程。
- 生产者读到 MP4 EOF 后调用 `close()`，消费者排空队列后退出。
- 生产者或消费者抛出异常时调用 `abort()`，清空待处理帧并唤醒另一线程；异常在线程汇合后重新抛给调用方。
- MP4 使用 `Block` 策略，队列满时生产者等待，避免离线文件因推理较慢而丢帧。

## 运行

```powershell
multi_rtsp_video_analysis.exe --process-mp4 <input.mp4> [max-frames] [queue-capacity]
```

`max-frames` 默认为 `0`，表示处理到文件结束；`queue-capacity` 默认为 `4`。消费者当前不执行模型推理。

## 测试

`frame_pipeline_test` 不需要 FFmpeg 或测试视频，通过合成帧检查 FIFO 顺序、队列容量、两个线程分离，以及生产者/消费者异常能否传回调用线程。