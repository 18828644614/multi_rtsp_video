# tests

测试按依赖层级组织：

- `unit/`：配置、时间戳、NMS、ROI、Tracker、事件去重和有界队列。
- `module/`：FFmpeg 解码、像素转换、模型签名和 JSONL 输出；当前 `mp4_decoder_test` 覆盖单路 MP4 的元数据、BGR24 帧所有权、PTS 和 EOF。
- `integration/`：单路 MP4、单路 RTSP、多路 RTSP 和断流重连。
- `fault_injection/`：连接失败、读取超时、推理变慢、输出失败和磁盘异常。

优先使用 FakeVideoSource 和 FakeDetector 验证状态机、队列和事件逻辑，再补充真实媒体链路测试。测试数据、模型和性能基线应固定并记录在测试配置中。
