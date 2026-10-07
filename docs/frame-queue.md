# FrameQueue 实现说明

## 1. 目标

`FrameQueue` 位于解码线程和处理线程之间，负责把帧安全地从一个线程交给另一个线程，同时限制内存和延迟。

当前实现使用 C++17 的 `std::deque`、`std::mutex` 和 `std::condition_variable`。第一版不使用无锁队列，因为每路视频只有一个生产者和一个消费者，简单实现更容易验证停止流程和资源生命周期。

## 2. 帧所有权

`media::FramePacket` 定义在 `include/media/frame.hpp`。其中 `metadata` 描述帧身份和时间，`image` 是独立的 BGR24 字节缓冲区，入队时通过移动转移所有权：

```text
FFmpeg 解码器拥有当前帧
    ↓ std::move
FrameQueue 拥有排队中的帧
    ↓ std::move
处理线程拥有当前处理帧
```

队列不保存 `AVFrame*`、`AVPacket*` 或指向临时缓冲区的裸指针。

## 3. 满载策略

- `DropOldest`：删除最旧帧，再放入新帧，适合实时 RTSP。
- `DropNewest`：丢弃新到帧，保留已经排队的帧。
- `Block`：生产者等待消费者取走帧，适合离线 MP4。

`push()` 返回实际结果，调用方可以据此更新日志或指标。

## 4. 生命周期

`close()` 表示生产者不会再放入新帧，但消费者可以继续取出队列中已有帧。

`abort()` 表示立即停止，清空队列并唤醒等待中的生产者和消费者。

推荐停止顺序：

```text
停止解码线程接收新帧
    ↓
close() 或 abort()
    ↓
等待处理线程退出
    ↓
释放解码器和其他媒体资源
```

## 5. 队列年龄

`max_age_ms` 使用 `FrameMetadata::received_at_steady_ms` 判断帧是否过期，不使用 PTS。PTS 是视频源时间轴，不能直接表示当前帧在程序中的等待时长。

当 `max_age_ms <= 0` 时，不启用年龄清理。配置层允许 `max_age_ms: 0`；尤其是 MP4 的 `block` 模式必须使用 `0`，避免离线处理因帧龄限制丢帧。

## 6. 统计信息

队列记录：

- 当前深度和峰值深度；
- 入队数量和出队数量；
- 因容量丢弃的旧帧和新帧；
- 因超过最大年龄清理的帧。

这些指标后续在 `StreamWorker` 中按 `stream_id` 和 `source_epoch` 汇总。

## 7. 测试

`tests/frame_queue_test.cpp` 不依赖 FFmpeg，覆盖 FIFO、丢旧帧、丢新帧、阻塞唤醒、关闭排空、立即中止、超时和最大年龄清理。