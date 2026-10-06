# 输出格式

## 1. Detection JSONL

每行代表一帧检测结果：

```json
{
  "schema_version": 1,
  "type": "detection",
  "stream_id": "cam-01",
  "sequence": 1024,
  "timestamp_ms": 1730000000123,
  "width": 1280,
  "height": 720,
  "objects": [
    {
      "class_id": 0,
      "label": "person",
      "confidence": 0.94,
      "bbox": {"x": 100, "y": 80, "width": 120, "height": 260},
      "track_id": 7
    }
  ]
}
```

当前 `DetectionJsonlSink` 将 `DetectionResult.capture_time_ms` 映射为 `timestamp_ms`。`pts`、`time_base` 和 `monotonic_time_ms` 暂不写入该 schema；`track_id` 也会等 Tracker 接入后再输出。

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

## 3. Metrics JSON

指标输出必须区分单路和进程汇总，并包含统计窗口起止时间、单位和采样方式。延迟使用毫秒，FPS 使用帧/秒，内存使用 MiB。

推荐字段：`stream_id`、`window_start_ms`、`window_end_ms`、`input_fps`、`decode_fps`、`inference_fps`、`output_fps`、`latency_p50_ms`、`latency_p95_ms`、`latency_p99_ms`、`queue_depth`、`queue_peak`、`received_frames`、`processed_frames`、`dropped_frames`、`reconnect_count` 和 `last_error`。
