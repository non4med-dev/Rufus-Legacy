# Rufus-Legacy
### The Reliable USB Formatting Utility for Windows 2000 - Windows 7

![Rufus-Legacy-Banner](https://raw.githubusercontent.com/non4med-dev/Rufus-Legacy/master/res/icons/Banner.png)

The original project can be found [under this link.](https://github.com/pbatard/rufus)

Important Notes
---------------

DBX updates are currently broken. Follow pull [#468](https://github.com/microsoft/secureboot_objects/pull/468) and thread [#3039](https://github.com/pbatard/rufus/issues/3039) for more information.<br>
Rufus already contains embedded copies of these files, so no functionality should be affected.<br>
The most recent dbx files of this format can still be found inside winxp-win2k.zip<br>

Features
--------

- Basic USB, flash card and virtual drives formatting to FAT/FAT32/NTFS (Windows 2000+), UDF/exFAT/ext2/ext3 (Windows Vista+), and ReFS (Windows 8.1+)<br>
- Creating DOS bootable USB drives using [FreeDOS](https://www.freedos.org) or MS-DOS (Windows 2000+)<br>
- Create BIOS or UEFI bootable drives, including [UEFI bootable NTFS](https://github.com/pbatard/uefi-ntfs) (Windows 2000+)<br>
- Create bootable drives from bootable ISOs (Windows, Linux, etc.) (Windows 2000+)<br>
- Create bootable drives from bootable disk images, including compressed ones (Windows 2000+)<br>
- Create [Windows To Go](https://en.wikipedia.org/wiki/Windows_To_Go) drives (Windows XP+)
- Create VHD/DD (Windows 2000+), VHDX (Windows 8+) and FFU (Windows 10 1709+) images of an existing drive<br>
- Create persistent Linux partitions (Windows 2000+)<br>
- Compute MD5, SHA-1, SHA-256 and SHA-512 checksums of the selected image (Windows 2000+)<br>
- Perform runtime validation of UEFI bootable media (Windows 2000+)<br>
- Perform bad blocks checks, including detection of "fake" flash drives (Windows 2000+)<br>
- Download official Microsoft Windows 8, Windows 10 or Windows 11 retail ISOs (Windows 7+)<br>
&nbsp;&nbsp;&nbsp;&nbsp;-> (WARNING: On Windows 7 [Powershell 7.2](https://github.com/PowerShell/PowerShell/releases/tag/v7.2.24) must be installed for ISO downloading to work)<br>

Changes
--------

- Native and proper Windows 7, Vista, XP and 2000 compatibility with proper API implementations<br>
- Common Controls v6 backported to Windows 2000 (and XP) to fix theme-related issues<br>
- GUID Partitioning Table (GPT) formatting support for Windows 2000 and Windows XP, to create EFI-bootable media (!!!)<br>
- Increased file-copying performance (1MiB buffer size) -> Flashing ISO files is ~15% faster on average<br>
- All unsupported features (VDS, VHDX, Windows To Go) are only visible and enabled on supported operating systems
- VHD saving has been restored from v3.22 for Windows 7 and all the way down to Windows 2000
- Rufus updates have been permanently disabled, without affecting remaining network functionality
- Connecting to the internet with a VPN on Windows 7 and older has been fixed
- Application closing through the Close button has been optimized
- autorun.inf / .ico files have been permanently disabled
- Maximum URL download length has been increased from 128 to 1024 to prevent issues in some regions
- Before Fido (ISO downloading) runs, it checks for the installed .NET, WMF and Powershell versions
- Original code paths have been retained for natively supported Windows versions (so nothing breaks!)<br>
Note: Rufus-Legacy mostly creates OS-specific fallbacks rather than replacing original code

More about the project
----------------------
Rufus-Legacy is based on Rufus 4.7.2231 (released April 9, 2025). It's the last version of Rufus to compile with Visual Studio 2022 17.6 and use a non-WIMlib base, which older operating systems don't benefit from.

Newer releases of Rufus-Legacy will slowly and selectively implement features found in newer versions of Rufus, keeping 4.7 as a base.

Over time I'll attempt to make this project more independent from the upstream main branch so it can focus entirely on features older OSes can actually benefit from.

AI Disclaimer
-------------

Some parts of the code; i.e. the API implementations winxp.c win2k.c as well as win2k_imports.asm have been developed with the use of AI and underwent numerous revisions.<br>
Certain parts of the code that have been written with the help of AI are marked with comments ending with (port-AI)
I have used AI to look over parts of my code, improve, safecheck and optimize them, to ensure the stability of the custom-written code.

I am an amateur, and a beginner.
My main and only objective is delivering actually working code.

Compilation
-----------

Use Visual Studio 2022 and then invoke the `.sln` 

MinGW compiling isn't supported for this fork.

#### Visual Studio

Rufus is an OSI compliant Open Source project. You are entitled to
download and use the *freely available* [Visual Studio Community Edition](https://www.visualstudio.com/vs/community/)
to build, run or develop for Rufus. As per the Visual Studio Community Edition license,
this applies regardless of whether you are an individual or a corporate user.

Rufus is 100% [Free Software](https://www.gnu.org/philosophy/free-sw) ([GPL v3](https://www.gnu.org/licenses/gpl-3.0))
All credits for the original project go to [Pete Batard](https://github.com/pbatard) and all other contributors.
Rufus-Legacy is an unofficial fork and is not affiliated with, endorsed by, or otherwise associated with the upstream Rufus project.
