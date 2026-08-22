# VDI-Tools

[简体中文版](README.zh-CN.md)

VDI-Tools is a Windows utility for cloning, converting, inspecting, enlarging, and optimizing virtual disk images. It supports VDI input as well as selected VHD, VMDK, Parallels HDD, and RAW images; output is a dynamically expanding VDI.

Built on the foundations of CloneVDI and SlimVDI, VDI-Tools adds a modern multilingual main interface, safer in-place VDI operations, and encryption detection to help users make better-informed decisions before modifying a virtual disk.

## Key enhancements

### Multilingual interface

The main window has a language selector. It can follow the Windows UI language automatically or use a manually selected language. VDI-Tools currently provides English, Simplified Chinese, German, Japanese, French, Spanish, Traditional Chinese, Korean, Russian, and Brazilian Portuguese.

### Direct in-place VDI modification

In addition to creating a new VDI, VDI-Tools can directly modify a supported source VDI in place. This mode supports compacting unused blocks and enlarging the virtual drive, with a recovery journal for unfinished operations. The journal is kept open while work is running and flushed at recovery checkpoints; keep enough free space beside the source for the journal. Because an in-place operation changes the source file itself, close all virtual machines and make a verified backup before proceeding.

### Batch processing

Use **Batch...** in the main window to select several source images, apply the current options to every item, and choose a bounded worker-thread limit. The selected thread limit is remembered in `VDI-Tools.ini`. Clone output is buffered for throughput, then its data and metadata are flushed before completion; existing destinations are written through temporary files and replaced only after completion. In-place tasks retain their recovery journals when cancelled at a checkpoint.

The command line accepts more than one source path. Use `--threads auto|N` (1-32) to control the worker limit and `--output-dir <dir>` to place all cloned outputs in one directory. `--output <file>` remains a single-source option.

### Disk encryption detection

Before cloning or modifying a virtual disk, VDI-Tools scans for encryption indicators. When encryption is detected or suspected, the application shows a warning so that the user can stop and review the operation before any change is made.

## License

VDI-Tools is distributed under the GNU General Public License, version 3 only (GPL-3.0). The complete license text is in [LICENSE](LICENSE).

## Upstream code and notices

VDI-Tools includes modified code derived from CloneVDI, authored by Don Milne, and incorporates work from the SlimVDI fork by hakito. The project has been renamed to **VDI-Tools** and is not affiliated with, endorsed by, or supported by the original authors.

## Acknowledgements

Sincere thanks to **Don Milne**, the author of CloneVDI, for creating the robust foundation on which this project is built and for making the source available under a permissive license. We also thank **hakito** for the SlimVDI fork and its continued improvements. VDI-Tools would not exist without their work.

The CloneVDI-derived portions are copyright:

```text
Copyright (C) 2010, Don Milne. All Rights Reserved.
```

Those portions were originally supplied under a BSD 2-Clause (FreeBSD-style) license. That license and its disclaimer remain in [LICENSE.TXT](LICENSE.TXT), as required by its redistribution terms. The original author requested that forks use a name other than `CloneVDI`; this project complies by using the name `VDI-Tools`.

`Random.c` contains code based on MT19937 and is separately copyright:

```text
Copyright (C) 1997-2002, Makoto Matsumoto and Takuji Nishimura.
```

Its original BSD-style notice and license are preserved at the top of [Random.c](Random.c).

## Copyright for VDI-Tools changes

Unless a file says otherwise, modifications made for VDI-Tools are:

```text
Copyright (C) 2026, VDI-Tools contributors.
```

## Building

Open `CloneVDI-2022.sln` in Visual Studio 2022 and build the `Release|Win32` configuration. The resulting executable is named `VDI-Tools.exe`.

## Languages

VDI-Tools provides a language selector in its main window. Choose **Default (system language)** to follow the Windows UI language, or select a language manually. The choice is saved in `VDI-Tools.ini`.

The main interface is available in the following locales:

- English (`en`)
- Simplified Chinese (`zh-CN`)
- German (`de`)
- Japanese (`ja`)
- French (`fr`)
- Spanish (`es`)
- Traditional Chinese (`zh-TW`)
- Korean (`ko`)
- Russian (`ru`)
- Portuguese, Brazil (`pt-BR`)

## Disclaimer

Virtual disk operations can cause data loss if used incorrectly. Back up source images and use this software at your own risk. The upstream BSD disclaimer in `LICENSE.TXT` continues to apply to the corresponding upstream portions.
