# 依赖与构建环境

## 1. 依赖原则

- 所有依赖必须有明确版本记录。
- 构建脚本不依赖开发机上的隐式路径。
- Windows 和 Linux 的差异通过 CMake Preset 或 toolchain 文件隔离。
- 模型、样例视频和第三方组件必须记录来源、许可证和校验信息。

## 2. 依赖清单

| 组件 | 用途 | 版本记录位置 |
|---|---|---|
| C++17 编译器 | 编译 | `CMakePresets.json` 或构建日志 |
| CMake | 构建 | `CMakePresets.json` |
| FFmpeg | 解封装、解码、时间戳 | 构建日志 |
| OpenCV | 图像转换和绘制 | 构建日志 |
| ONNX Runtime | 模型推理 | 构建日志 |
| GoogleTest | 测试 | CMake 配置 |
| MediaMTX | 本地 RTSP 模拟 | `scripts/README.md` |

## 3. 构建要求

目标是提供以下可重复入口：

```text
cmake --preset <platform-debug>
cmake --build --preset <platform-debug>
ctest --preset <platform-debug>
```

在实现 CMake 工程之前，先确定 FFmpeg、OpenCV 和 ONNX Runtime 的发现方式，避免把本机绝对路径写入业务配置。

当前 CMake 支持通过 `FFMPEG_ROOT` 查找 FFmpeg；如果未指定，也会尝试从 PATH 中的 `ffmpeg.exe` 自动推导根目录。该目录至少需要包含：

```text
<FFMPEG_ROOT>/include/libavformat/avformat.h
<FFMPEG_ROOT>/include/libavcodec/avcodec.h
<FFMPEG_ROOT>/include/libavutil/avutil.h
<FFMPEG_ROOT>/include/libswscale/swscale.h
<FFMPEG_ROOT>/lib/avformat、avcodec、avutil、swscale 对应的链接库
```

Windows 当前使用 MSVC，应使用与 MSVC ABI 匹配的 FFmpeg 开发文件：头文件位于 `include/`，链接库必须包含 `lib/avcodec.lib`、`lib/avformat.lib`、`lib/avutil.lib` 和 `lib/swscale.lib`。`bin/ffmpeg.exe` 只代表运行时程序，不能替代这些开发库。运行程序时还要确保 FFmpeg DLL 所在目录在 `PATH` 中。

当前可执行文件提供最小解码验证入口：

```text
multi_rtsp_video_analysis.exe --decode-mp4 <input.mp4> [max-frames]
```

## 4. 模型与媒体资产

- 模型必须配套 manifest，记录输入尺寸、类别顺序、预处理、后处理和许可证。
- 测试视频只使用有明确来源和使用许可的文件。
- 不把真实摄像头账号、密码、令牌或私人地址提交到仓库。