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
## [ERR-20261004-005] config_loader_old_toolchain

**Logged**: 2026-10-04T13:40:00+08:00
**Priority**: medium
**Status**: pending
**Area**: backend

### 摘要（Summary）
配置加载器首次编译时使用了当前 GCC 7.3.0/实验 filesystem 不支持的 API，导致构建失败。

### 原始错误（Error）
```
error: 'const KeySet ...' has no member named 'contains'
error: 'path' has no member named 'lexically_normal'
error: 'const class YAML::Node' has no member named 'empty'
```

### 上下文（Context）
- 当前 MinGW 为 GCC 7.3.0，虽然以 C++17 模式编译，但标准库实现较旧。
- `std::set::contains` 属于 C++20，不能用于 C++17。
- GCC 7.3.0 的 `std::experimental::filesystem` 不提供 `lexically_normal`。
- 当前 yaml-cpp 头文件接口不提供 `YAML::Node::empty()`。

### 建议修复（Suggested Fix）
改用 `set.find(...) != set.end()`、直接使用 `absolute(...)`、以及 `size() == 0`，保持 C++17 和旧工具链兼容。

### 元数据（Metadata）
- Reproducible: yes
- Related Files: src/config/config.cpp
- See Also: none

---
## [ERR-20261004-006] config_test_old_fstream

**Logged**: 2026-10-04T13:45:00+08:00
**Priority**: low
**Status**: pending
**Area**: tests

### 摘要（Summary）
配置测试在 GCC 7.3.0 下无法直接使用 `std::experimental::filesystem::path` 构造 `std::ofstream`。

### 原始错误（Error）
```
error: no matching function for call to 'std::basic_ofstream<char>::basic_ofstream(const std::experimental::filesystem::v1::__cxx11::path&, const openmode&)'
```

### 上下文（Context）
- 新版 C++ 标准库支持路径直接传给文件流，但当前 GCC 7.3.0 的实现只接受字符串路径。
- 主程序和配置库本身已经成功编译。

### 建议修复（Suggested Fix）
在测试辅助函数中使用 `path.string()`，保持对旧版 MinGW 的兼容。

### 元数据（Metadata）
- Reproducible: yes
- Related Files: tests/config_test.cpp
- See Also: ERR-20261004-005

---
## [ERR-20261004-008] powershell_cmake_replace

**Logged**: 2026-10-04T13:55:00+08:00
**Priority**: low
**Status**: pending
**Area**: infra

### 摘要（Summary）
尝试用 PowerShell 字符串替换追加 CMake 链接配置时，嵌套引号导致命令解析失败；源码文件没有被写入。

### 原始错误（Error）
```
ParserError: Missing ')' in method call.
```

### 上下文（Context）
- 失败发生在 PowerShell 命令解析阶段。
- 当前 `CMakeLists.txt` 未被该命令修改。

### 建议修复（Suggested Fix）
对多行 CMake 内容使用 here-string 或直接重写完整文件，避免在 `.Replace()` 参数中嵌套复杂引号。

### 元数据（Metadata）
- Reproducible: yes
- Related Files: CMakeLists.txt
- See Also: none

---
## [ERR-20261004-009] yaml_windows_path_escape

**Logged**: 2026-10-04T14:05:00+08:00
**Priority**: medium
**Status**: pending
**Area**: tests

### 摘要（Summary）
配置测试把 Windows 反斜杠路径直接写入 YAML 双引号字符串，导致 yaml-cpp 将 `\U` 等内容当作转义序列并解析失败。

### 原始错误（Error）
```
yaml-cpp: error at line 6, column 22: bad character found while scanning hex number
```

### 上下文（Context）
- 测试生成的路径类似 `C:\Users\...`。
- YAML 双引号字符串中的反斜杠有特殊含义。

### 建议修复（Suggested Fix）
写入 YAML 前将 Windows 路径中的 `\` 转换为 `/`，或使用 YAML 单引号/正确转义。

### 元数据（Metadata）
- Reproducible: yes
- Related Files: tests/config_test.cpp
- See Also: none

---
## [ERR-20261004-010] config_test_temp_cleanup

**Logged**: 2026-10-04T14:15:00+08:00
**Priority**: low
**Status**: pending
**Area**: tests

### 摘要（Summary）
配置测试在旧版 MinGW 的 experimental filesystem 下清理 Windows 临时目录时遇到权限错误，导致测试在业务断言通过后仍失败。

### 原始错误（Error）
```
filesystem error: cannot remove all: Permission denied [C:\Users\Lenovo\AppData\Local\Temp\multi_rtsp_config_test]
```

### 上下文（Context）
- 配置解析和校验逻辑已经执行到测试收尾阶段。
- 失败发生在测试目录清理，不是测试数据或业务校验失败。

### 建议修复（Suggested Fix）
使用带 `std::error_code` 的尽力清理，忽略清理失败，避免环境权限问题影响测试结果。

### 元数据（Metadata）
- Reproducible: yes
- Related Files: tests/config_test.cpp
- See Also: ERR-20261004-009

---
## [ERR-20261004-011] powershell_notes_sync

**Logged**: 2026-10-04T14:25:00+08:00
**Priority**: low
**Status**: pending
**Area**: infra

### 摘要（Summary）
同步任务计划和笔记时，PowerShell 多行字符串包含无效转义处理，命令未完成写入。

### 原始错误（Error）
```
Command exited with code 1 and produced no output.
```

### 上下文（Context）
- 代码、构建和测试均已完成。
- 失败只发生在分析记录文件同步阶段。

### 建议修复（Suggested Fix）
使用不包含伪转义字符的纯文本 here-string 分别写入任务计划和笔记。

### 元数据（Metadata）
- Reproducible: yes
- Related Files: task_plan.md, notes.md
- See Also: none

---
## [ERR-20261004-012] ffmpeg_development_libraries_missing

**Logged**: 2026-10-04T21:30:00+08:00
**Priority**: high
**Status**: pending
**Area**: infra

### 摘要（Summary）
当前 FFmpeg 安装可被 CMake 找到头文件，但没有可供 MinGW 链接的开发库，因此项目无法完成 FFmpeg 解码模块的本机构建。

### 原始错误（Error）
```
-- Could NOT find FFMPEG (missing: FFMPEG_AVCODEC_LIBRARY FFMPEG_AVFORMAT_LIBRARY FFMPEG_AVUTIL_LIBRARY FFMPEG_SWSCALE_LIBRARY)
CMake Error at CMakeLists.txt:114 (message):
  FFmpeg development files were not found.  Set FFMPEG_ROOT to an FFmpeg
  prefix containing include/ and lib/.
```

### 上下文（Context）
- 使用 `C:\Users\Lenovo\AppData\Local\Microsoft\WinGet\Packages\Gyan.FFmpeg.Shared_Microsoft.Winget.Source_8wekyb3d8bbwe\ffmpeg-9.0.2-full_build-shared` 作为 `FFMPEG_ROOT`。
- CMake 配置阶段没有找到 `avcodec`、`avformat`、`avutil` 和 `swscale` 的链接库。
- 当前编译器是 MinGW GCC 7.3.0；Windows 上的 FFmpeg 运行时 DLL 不等于可链接的 MinGW 开发包。

### 建议修复（Suggested Fix）
安装与 MinGW ABI 匹配的 FFmpeg development package，确保 `FFMPEG_ROOT/include` 下有头文件、`FFMPEG_ROOT/lib` 下有 `.dll.a` 或兼容的导入库，然后重新配置 CMake。

### 元数据（Metadata）
- Reproducible: yes
- Related Files: CMakeLists.txt
- See Also: none

---

## [ERR-20261004-013] mp4_decoder_test_filesystem_link

**Logged**: 2026-10-04T21:36:00+08:00
**Priority**: medium
**Status**: resolved
**Area**: tests

### 摘要（Summary）
新增 `mp4_decoder_test` 在 GCC 7.3.0 下链接失败，因为测试目标没有复用项目对 `std::experimental::filesystem` 所需的 `stdc++fs` 链接配置。

### 原始错误（Error）
```
undefined reference to `std::experimental::filesystem::v1::__cxx11::path::_M_split_cmpts()'
undefined reference to `std::experimental::filesystem::v1::__cxx11::filesystem_error::~filesystem_error()'
```

### 上下文（Context）
- `ffmpeg_decoder` 和主程序已经完成编译。
- `config_test` 已有 GCC 9 以下版本的 `stdc++fs` 链接分支。
- 新增测试目标漏掉了相同分支。

### 建议修复（Suggested Fix）
为所有直接或间接使用 `app_fs::path` 的 GCC 9 以下目标链接 `stdc++fs`。

### 元数据（Metadata）
- Reproducible: yes
- Related Files: CMakeLists.txt, tests/mp4_decoder_test.cpp
- See Also: ERR-20261004-005

---
