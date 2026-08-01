# VDI-Tools

VDI-Tools is a Windows utility for cloning, converting, inspecting, enlarging, and optimizing virtual disk images. It supports VDI input as well as selected VHD, VMDK, Parallels HDD, and RAW images; output is a dynamically expanding VDI.

## License

VDI-Tools is distributed under the GNU General Public License, version 3 only (GPL-3.0). The complete license text is in [LICENSE](LICENSE).

## Upstream code and notices

VDI-Tools includes modified code derived from CloneVDI, authored by Don Milne, and incorporates work from the SlimVDI fork by hakito. The project has been renamed to **VDI-Tools** and is not affiliated with, endorsed by, or supported by the original authors.

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
