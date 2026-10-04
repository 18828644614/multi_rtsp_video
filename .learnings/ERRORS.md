# Errors

## [ERR-20261004-001] apply_patch_shell_invocation

**Logged**: 2026-10-04T00:00:00+08:00
**Priority**: medium
**Status**: pending
**Area**: docs

### 摘要（Summary）
PowerShell 直接调用 `apply_patch.bat` 返回 `Access is denied`，导致首轮文档补丁未写入。

### 原始错误（Error）
```text
Access is denied.
```

### 上下文（Context）
- 在 `D:\LearningProjects\multi_rtsp_video_analysis` 中通过 PowerShell 管道调用 `apply_patch`。
- `apply_patch` 实际是位于 `C:\Users\Lenovo\.codex\tmp\arg0\codex-arg0NWmo5N\apply_patch.bat` 的批处理包装器。

### 建议修复（Suggested Fix）
通过 `cmd.exe /d /c` 调用批处理包装器仍然返回相同错误，因此本次使用受控的 PowerShell 文件写入完成文档修改。

### 元数据（Metadata）
- Reproducible: yes
- Related Files: C:\Users\Lenovo\.codex\tmp\arg0\codex-arg0NWmo5N\apply_patch.bat
- Tags: apply_patch, powershell, windows

---
## [ERR-20261004-001] create-standalone-obsidian-vault

**Logged**: 2026-10-04T00:00:00+08:00
**Priority**: medium
**Status**: pending
**Area**: docs

### Summary
Sandbox denied creation of the requested standalone vault directly under `D:\LearningProjects`.

### Error
```text
Access to the path 'D:\LearningProjects\multi_rtsp_video_analysis_knowledge' is denied.
```

### Context
- Attempted to create a new Obsidian vault outside the current writable project root.
- The requested target is `D:\LearningProjects\multi_rtsp_video_analysis_knowledge`.

### Suggested Fix
Request a narrowly scoped elevated filesystem operation for the new vault directory only.

### Metadata
- Reproducible: yes
- Related Files: D:\LearningProjects\multi_rtsp_video_analysis
- Tags: sandbox, filesystem, obsidian

---
