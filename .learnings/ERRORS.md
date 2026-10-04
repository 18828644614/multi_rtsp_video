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
## [ERR-20261004-001] apply_patch_command

**Logged**: 2026-10-04T11:30:00+08:00
**Priority**: medium
**Status**: pending
**Area**: infra

### 摘要（Summary）
在受限 Windows 工作区执行 `apply_patch` 时返回 `Access is denied.`，因此无法用补丁命令创建分析记录文件。

### 原始错误（Error）
```
Access is denied.
```

### 上下文（Context）
- 尝试在项目根目录通过 PowerShell 管道调用 `apply_patch` 创建 `task_plan.md` 和 `notes.md`。
- `Get-Command apply_patch` 可定位到 `C:\Users\Lenovo\.codex\tmp\arg0\codex-arg0lWCMc7\apply_patch.bat`，但执行被当前权限环境拒绝。

### 建议修复（Suggested Fix）
当前任务使用 PowerShell 的受限文件写入方式继续；后续如需补丁编辑，优先检查 Codex 补丁运行器权限或使用受支持的编辑接口。

### 元数据（Metadata）
- Reproducible: yes
- Related Files: task_plan.md, notes.md
- See Also: none

---
## [ERR-20261004-002] pip_ultralytics_index

**Logged**: 2026-10-04T11:45:00+08:00
**Priority**: medium
**Status**: pending
**Area**: infra

### 摘要（Summary）
使用当前 pip 镜像安装 `ultralytics` 时返回无可用版本；切换官方 PyPI 后可以查询到包版本。

### 原始错误（Error）
```
ERROR: Could not find a version that satisfies the requirement ultralytics (from versions: none)
ERROR: No matching distribution found for ultralytics
```

### 上下文（Context）
- Python: `3.12.5`，64-bit。
- pip: `24.3.1`。
- pip 配置文件：`C:\Users\Lenovo\AppData\Roaming\pip\pip.ini`。
- 当前全局源：`https://mirrors.tuna.tsinghua.edu.cn/pypi/web/simple`。
- 使用 `https://pypi.org/simple` 查询时可获得 `ultralytics` 版本列表，说明 Python 版本不是主要原因。

### 建议修复（Suggested Fix）
优先用 `python -m pip install -i https://pypi.org/simple ultralytics` 临时绕过镜像；若希望继续使用清华源，可尝试官方帮助中给出的 `https://pypi.tuna.tsinghua.edu.cn/pypi/web/simple` 域名。

### 元数据（Metadata）
- Reproducible: yes
- Related Files: C:\Users\Lenovo\AppData\Roaming\pip\pip.ini
- See Also: none

---
## [ERR-20261004-003] winget_detection

**Logged**: 2026-10-04T13:10:00+08:00
**Priority**: low
**Status**: pending
**Area**: infra

### 摘要（Summary）
当前 Codex PowerShell 环境无法启动 `winget.exe`，无法在沙箱中验证本机 WinGet 源查询。

### 原始错误（Error）
```
Program 'winget.exe' failed to run: StandardErrorEncoding is only supported when standard error is redirected.
An error occurred trying to start process 'C:\Users\Lenovo\AppData\Local\Microsoft\WindowsApps\winget.exe' with working directory 'D:\LearningProjects\multi_rtsp_video_analysis'. 系统无法访问此文件。
```

### 上下文（Context）
- 尝试执行 `winget --version` 和 `winget search --name ffmpeg --source winget`。
- 当前桌面用户终端可能仍可直接运行 WinGet；本次失败发生在 Codex 执行环境。

### 建议修复（Suggested Fix）
向用户提供 WinGet 安装方式，同时保留 Gyan 官方 Windows 构建的手动下载方案；如果用户使用 Visual Studio/CMake，优先考虑 vcpkg 管理 FFmpeg 开发依赖。

### 元数据（Metadata）
- Reproducible: yes
- Related Files: none
- See Also: none

---
## [ERR-20261004-004] powershell_select_string_regex

**Logged**: 2026-10-04T13:20:00+08:00
**Priority**: low
**Status**: pending
**Area**: infra

### 摘要（Summary）
校对文件行号时，PowerShell `Select-String` 将包含括号的搜索词当作正则表达式，导致一次检查命令报错；文件写入本身已完成。

### 原始错误（Error）
```
The string add_executable(multi is not a valid regular expression: Invalid pattern 'add_executable(multi' at offset 20. Not enough )'s.
```

### 上下文（Context）
- 使用 `Select-String -Pattern 'add_executable(multi'` 搜索 `CMakeLists.txt`。
- 该命令只影响行号检查，没有影响 CMake 工程或构建结果。

### 建议修复（Suggested Fix）
搜索包含括号的固定字符串时使用 `-SimpleMatch`，或转义正则特殊字符。

### 元数据（Metadata）
- Reproducible: yes
- Related Files: CMakeLists.txt
- See Also: none

---
