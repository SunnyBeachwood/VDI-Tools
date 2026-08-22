# VDI-Tools

[English README](README.md)

VDI-Tools 是一款 Windows 虚拟磁盘工具，可用于克隆、转换、检查、扩容和优化虚拟磁盘镜像。它支持读取 VDI，以及部分 VHD、VMDK、Parallels HDD 和 RAW 镜像；输出格式为动态扩展的 VDI。

VDI-Tools 建立在 CloneVDI 和 SlimVDI 的基础之上，在保留原有核心能力的同时，增加了多语言主界面、更加安全的原地修改 VDI 功能，以及磁盘加密检测功能，帮助用户在执行磁盘操作前作出更稳妥的判断。

## 主要增强

### 多语言界面

主窗口内置语言选择器。你可以选择“默认（跟随系统语言）”，也可以手动选择语言。当前支持以下语言：

- English（\`en\`）
- 简体中文（\`zh-CN\`）
- Deutsch（\`de\`）
- 日本語（\`ja\`）
- Français（\`fr\`）
- Español（\`es\`）
- 繁体中文（\`zh-TW\`）
- 한국어（\`ko\`）
- Русский（\`ru\`）
- Português do Brasil（\`pt-BR\`）

语言设置会保存在 \`VDI-Tools.ini\` 中，下次启动时自动恢复。

### 直接原地修改 VDI

除了创建新的 VDI 之外，VDI-Tools 还可以直接修改受支持的源 VDI。该模式支持压缩未使用的块和扩展虚拟磁盘容量，并通过恢复日志辅助处理未完成的操作。执行期间恢复日志保持打开，并在恢复检查点刷盘；源 VDI 所在目录须预留日志空间。

原地修改会直接改变源文件。执行前请关闭所有相关虚拟机，并确认已经创建可用的备份。

### 批量处理与并行任务

主界面的“批处理...”可一次加入多个源镜像，并将当前选项应用到整批任务。线程上限可设为自动或 1-32，界面选择会保存到 `VDI-Tools.ini`；复制结果默认生成在各源文件旁，并采用缓冲写入、完成前刷入数据与元数据。已有目标文件会先写入临时 VDI，只有任务完成后才替换旧文件。原地修改在取消时会停在恢复检查点并保留日志。

命令行支持多个源路径；使用 `--threads auto|N` 设置并发上限，使用 `--output-dir <目录>` 统一指定复制输出目录。`--output <文件>` 仍仅适用于单个源文件。

### 磁盘加密检测

在克隆或修改虚拟磁盘之前，VDI-Tools 会扫描磁盘中的加密迹象。检测到或怀疑存在加密时，软件会显示警告，以便你在写入前停止操作并核查风险。

## 许可证

VDI-Tools 使用 GNU General Public License v3.0（GPL-3.0）发布。完整许可证文本见 [LICENSE](LICENSE)。

## 上游项目、版权与致谢

VDI-Tools 包含基于 **Don Milne** 创作的 CloneVDI 修改而来的代码，也吸收了 **hakito** 的 SlimVDI fork 中的工作。本项目已更名为 **VDI-Tools**；它不是原作者的官方项目，也不代表获得原作者背书或支持。

衷心感谢 Don Milne 创建了稳定可靠的 CloneVDI，并以宽松许可证提供源码；也感谢 hakito 对 SlimVDI 的维护与改进。没有他们的工作，VDI-Tools 无法成为现实。

CloneVDI 派生部分的版权为：

\`\`\`text
Copyright (C) 2010, Don Milne. All Rights Reserved.
\`\`\`

这些部分原先采用 BSD 2-Clause（FreeBSD 风格）许可证发布。为满足其再分发条件，原许可证和免责声明完整保留在 [LICENSE.TXT](LICENSE.TXT) 中。原作者希望 fork 使用不同于 \`CloneVDI\` 的名称，本项目以 \`VDI-Tools\` 的名称遵循该请求。

\`Random.c\` 包含基于 MT19937 的代码，其版权为：

\`\`\`text
Copyright (C) 1997-2002, Makoto Matsumoto and Takuji Nishimura.
\`\`\`

其原始 BSD 风格版权声明和许可证保留在 [Random.c](Random.c) 文件顶部。

除非文件另有说明，VDI-Tools 的新增和修改部分版权为：

\`\`\`text
Copyright (C) 2026, VDI-Tools contributors.
\`\`\`

## 编译

使用 Visual Studio 2022 打开 \`CloneVDI-2022.sln\`，并编译 \`Release|Win32\` 配置。生成的可执行文件名为 \`VDI-Tools.exe\`。

也可以使用命令行：

\`\`\`powershell
msbuild CloneVDI-2022.sln /p:Configuration=Release /p:Platform=Win32
\`\`\`

## 免责声明

虚拟磁盘操作存在数据丢失风险。请在操作前备份源镜像，并自行承担使用本软件的风险。\`LICENSE.TXT\` 中的上游 BSD 免责声明继续适用于相应的上游代码部分。
