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
| OpenCV 4.x (`core`, `imgproc`) | 图像缩放、letterbox 和像素转换 | CMake 配置与构建日志 |
| ONNX Runtime C++ CPU | ONNX 模型推理 | CMake 配置与构建日志 |
| GoogleTest | 测试 | CMake 配置 |
| MediaMTX | 本地 RTSP 模拟 | `scripts/README.md` |

## 3. 构建要求

目标是提供以下可重复入口：

```text
cmake --preset <platform-debug>
cmake --build --preset <platform-debug>
ctest --preset <platform-debug>
```

依赖路径只通过 CMake cache 变量或环境变量传入，不写入业务配置或提交开发机绝对路径。

当前 CMake 支持通过 `FFMPEG_ROOT` 查找 FFmpeg；如果未指定，也会尝试从 PATH 中的 `ffmpeg.exe` 自动推导根目录。该目录至少需要包含：

```text
<FFMPEG_ROOT>/include/libavformat/avformat.h
<FFMPEG_ROOT>/include/libavcodec/avcodec.h
<FFMPEG_ROOT>/include/libavutil/avutil.h
<FFMPEG_ROOT>/include/libswscale/swscale.h
<FFMPEG_ROOT>/lib/avformat、avcodec、avutil、swscale 对应的链接库
```

Windows 当前使用 MSVC，应使用与 MSVC ABI 匹配的 FFmpeg 开发文件：头文件位于 `include/`，链接库必须包含 `lib/avcodec.lib`、`lib/avformat.lib`、`lib/avutil.lib` 和 `lib/swscale.lib`。`bin/ffmpeg.exe` 只代表运行时程序，不能替代这些开发库。运行程序时还要确保 FFmpeg DLL 所在目录在 `PATH` 中。

检测器依赖通过以下变量定位：

- `OpenCV_DIR` 指向包含 `OpenCVConfig.cmake` 的目录，且安装中包含 `core` 和 `imgproc`。
- `ONNXRUNTIME_ROOT` 指向解压后的 ONNX Runtime C++ CPU 包根目录；Windows 需要 `include/onnxruntime_cxx_api.h`、`lib/onnxruntime.lib` 和 `lib/onnxruntime.dll`。
- Windows 上应使用与 MSVC/目标架构匹配的 OpenCV 和 ONNX Runtime 包。当前验证基线为 OpenCV 4.12.0 与 ONNX Runtime 1.30.0 CPU 包。

当前 Windows 工作区的 CMake 会在未设置 ONNXRUNTIME_ROOT 时自动尝试 D:/Onnx/onnxruntime-win-x64-1.30.0；其他机器仍应通过 ONNXRUNTIME_ROOT 或 -DONNXRUNTIME_ROOT=... 指定实际路径。

MSVC x64 配置示例：

```powershell
cmake --preset msvc-debug `
  -DOpenCV_DIR="<opencv-prefix>/build/x64/vc16/lib" `
  -DONNXRUNTIME_ROOT="<onnxruntime-package-root>"
cmake --build --preset msvc-debug --config Debug
ctest --preset msvc-debug
```

Windows 检测测试目标会把 ONNX Runtime 和 OpenCV 的运行时 DLL 复制到测试输出目录。将 `vision_onnx_detector` 链接到其他可执行文件时，也必须部署匹配的 DLL。`onnx_detector_jsonl_test` 仅在本地存在 `models/detector.onnx` 时注册；ONNX 文件因体积/许可证原因由 `.gitignore` 排除。

当前可执行文件提供最小解码验证入口：

```text
multi_rtsp_video_analysis.exe --decode-mp4 <input.mp4> [max-frames]
```

## 4. 模型与媒体资产

- 模型必须配套 manifest，记录输入尺寸、类别顺序、预处理、后处理和许可证。
- 测试视频只使用有明确来源和使用许可的文件。
- 不把真实摄像头账号、密码、令牌或私人地址提交到仓库。
