# include

存放公共头文件，建议与实现目录保持对应：

- `frame_packet.hpp`
- `video_source.hpp`
- `decoder.hpp`
- `frame_converter.hpp`
- `frame_queue.hpp`
- `detector.hpp`
- `tracker.hpp`
- `rule_engine.hpp`
- `stream_worker.hpp`
- `metrics.hpp`
- `event_sink.hpp`
- `result_sink.hpp`

跨线程接口优先使用 `FramePacket`、`DetectionResult` 和 `Event` 等稳定数据契约，避免上层依赖 FFmpeg 私有对象的生命周期。
