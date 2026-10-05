# 单路 MP4 解码

## 1. 目标

`media::FfmpegMp4Decoder` 负责单个本地 MP4 的解封装、视频流选择、FFmpeg 解码和 `AVFrame` 到 BGR24 的转换。解码器不把 `AVFrame` 或 `AVPacket` 暴露给上层，输出帧拥有自己的 `std::vector<uint8_t>` 内存，可以安全交给后续队列或处理线程。

## 2. 实现步骤

1. `avformat_open_input` 打开 MP4 文件。
2. `avformat_find_stream_info` 读取媒体流信息，并用 `av_find_best_stream` 选择视频流。
3. 创建 `AVCodecContext`，复制 `codecpar`，调用 `avcodec_open2` 打开解码器。
4. 循环调用 `av_read_frame`，只把目标视频流的 `AVPacket` 发送给 `avcodec_send_packet`。
5. 循环调用 `avcodec_receive_frame`，处理 FFmpeg 的 `EAGAIN`，文件读完后发送空包刷新解码器。
6. 通过 `sws_getContext`/`sws_scale` 将源像素格式转换为 BGR24，并复制到独立 `vector`。
7. 从 `best_effort_timestamp` 获取帧 PTS，同时保留视频流 `time_base`，并记录墙钟时间和单调时钟时间。
8. 在 `close`、析构和异常路径释放 `SwsContext`、`AVFrame`、`AVPacket`、`AVCodecContext` 和 `AVFormatContext`。

## 3. 帧数据契约

`FramePacket` 当前包含；代码中保留 `DecodedFrame` 作为兼容别名：

- `stream_id`：单路解码默认值为 `mp4`。
- `sequence`：从 0 开始递增的解码帧序号。
- `pts` 与 `time_base`：源视频时间戳，不用墙钟时间替代。
- `width`、`height`、`stride`：BGR24 输出布局。
- `pixel_format`：当前为 `Bgr24`。
- `capture_time_ms`、`monotonic_time_ms`：帧完成解码时的接收时间。
- `image`：连续的 BGR24 字节，大小为 `stride * height`。

## 4. 命令行验证

先完成 FFmpeg 开发包配置。建议从 Visual Studio 2022 的 Developer PowerShell 或 Developer Command Prompt（x64）中运行以下命令。

```powershell
cmake --preset msvc-debug -DFFMPEG_ROOT=C:/path/to/ffmpeg
cmake --build --preset msvc-debug --config Debug
```

完整解码：

```powershell
build/msvc-debug/Debug/multi_rtsp_video_analysis.exe --decode-mp4 data/demo.mp4
```

快速验证前 10 帧：

```powershell
build/msvc-debug/Debug/multi_rtsp_video_analysis.exe --decode-mp4 data/demo.mp4 10
```

模块测试会打开仓库内的 `data/demo.mp4`，检查元数据、BGR24 缓冲区尺寸、连续序号、PTS 单调性和 EOF 收尾：

```powershell
ctest --preset msvc-debug -C Debug -R mp4_decoder_test --output-on-failure
```

## 5. 当前范围

当前实现只负责单路离线 MP4 解码，不包含模型推理、OpenCV `cv::Mat` 封装、队列、实时节奏控制、RTSP 超时或重连。后续 `StreamWorker` 可以直接消费 `DecodedFrame`，再将其放入有界队列。