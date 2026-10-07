# 输出格式

## 1. Detection JSONL

每行代表一帧检测结果，当前 schema 版本为 `2`：

```json
{
  "schema_version": 2,
  "type": "detection",
  "stream_id": "cam-01",
  "source_epoch": 1,
  "sequence": 1024,
  "timestamp_ms": 1730000000123,
  "received_at_unix_ms": 1730000000123,
  "pts": 9000,
  "time_base": {"numerator": 1, "denominator": 90000},
  "width": 1280,
  "height": 720,
  "objects": [
    {
      "class_id": 0,
      "label": "person",
      "confidence": 0.94,
      "bbox": {"x": 100, "y": 80, "width": 120, "height": 260}
    }
  ]
}
```

`timestamp_ms` 保留为接收墙钟时间的兼容字段；新代码应优先读取 `received_at_unix_ms`。`pts` 无效时输出 `null`。单调时间不写入 JSONL，只用于进程内延迟和队列年龄计算。

当前 `DetectionJsonlSink` 不输出 `received_at_steady_ms`，避免把进程启动相关的单调时钟暴露给外部系统。

## 2. Event JSONL

```json
{
  "schema_version": 1,
  "type": "event",
  "event_id": "cam-01-line-01-00007-1730000000123",
  "event_type": "line_crossing",
  "rule_id": "line-01",
  "stream_id": "cam-01",
  "track_ids": [7],
  "direction": "in",
  "sequence": 1024,
  "timestamp_ms": 1730000000123,
  "snapshot_path": "events/cam-01/1730000000123.jpg"
}
```

事件 schema 会在 Event 模型和规则引擎实现时固定。事件应同时关联 `stream_id`、`source_epoch`、帧序号、track ID 和规则 ID。

## 3. Metrics JSON

指标输出必须区分单路和进程汇总，并包含统计窗口起止时间、单位和采样方式。延迟使用毫秒，FPS 使用帧/秒，内存使用 MiB。

推荐字段：`stream_id`、`source_epoch`、`window_start_ms`、`window_end_ms`、`input_fps`、`decode_fps`、`inference_fps`、`output_fps`、`latency_p50_ms`、`latency_p95_ms`、`latency_p99_ms`、`queue_depth`、`queue_peak`、`received_frames`、`processed_frames`、`dropped_frames`、`reconnect_count` 和 `last_error`。