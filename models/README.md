# 模型文件

模型目录存放 ONNX 模型说明、manifest 和下载记录。模型文件本身可以因为体积或许可证原因不提交，但必须留下可复现的来源信息。

每个模型至少记录：

- 名称、版本、来源和许可证。
- 文件校验值。
- 输入尺寸和动态维度。
- 类别顺序。
- resize/letterbox、归一化和通道顺序。
- 输出张量格式、置信度计算和 NMS 方式。

参考模板：`model-manifest.example.yaml`。

当前本地检测器为 `detector.onnx`，其契约记录在 `detector-manifest.yaml`。该 ONNX 文件由 `.gitignore` 排除，需由使用者自行提供；manifest 记录了当前文件的 SHA-256。模型内嵌元数据声明许可证为 AGPL-3.0，原始下载地址尚未记录，分发前应确认模型来源及对应许可证义务。
