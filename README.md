# Rufus-Legacy
### The Reliable USB Formatting Utility for Windows 2000 - Windows 7

![Rufus-Legacy-Banner](https://raw.githubusercontent.com/non4med-dev/Rufus-Legacy/master/res/icons/Banner.png)

The original project can be found [under this link.](https://github.com/pbatard/rufus)

Important Notes
---------------

RC2 has been temporarily pulled down to fix a bunch of Windows To Go related bugs on NT5.

Features
--------

- Format USB, flash card and virtual drives to FAT/FAT32/NTFS/UDF/exFAT/ReFS/ext2/ext3
- Create DOS bootable USB drives using [FreeDOS](https://www.freedos.org) or MS-DOS<br>
- Create BIOS or UEFI bootable drives, including [UEFI bootable NTFS](https://github.com/pbatard/uefi-ntfs)<br>
- Create bootable drives from bootable ISOs (Windows, Linux, etc.)<br>
- Create bootable drives from bootable disk images, including compressed ones<br>
- Create [Windows To Go](https://en.wikipedia.org/wiki/Windows_To_Go) drives<br>
- Create VHD/DD (W2000+), VHDX (W8+) and FFU (W10 1709+) images of an existing drive<br>
- Create persistent Linux partitions<br>
- Compute MD5, SHA-1, SHA-256 and SHA-512 checksums of the selected image<br>
- Perform runtime validation of UEFI bootable media<br>
- Perform bad blocks checks, including detection of "fake" flash drivesbr>
- Download official Microsoft Windows 8, Windows 10 or Windows 11 retail ISOs<br>
- Download UEFI Shell ISOs<br>
&nbsp;&nbsp;&nbsp;-> Windows 7 requires [Powershell 7.2](https://github.com/PowerShell/PowerShell/releases/tag/v7.2.24) to be installed for downloading to work<br>
- Modern and familiar UI, with 38 languages natively supported
- Small footprint. No installation required.
- Portable. Secure Boot compatible.
- 100% Free Software (GPL v3)

Changes
--------

- GPT partitioning support for Windows 2000 and Windows XP<br>
- Support for creating Windows To Go drives on Windows 2000+
- VHD saving support for Windows 2000+<br>
- Native compatibility, fixes, API implementations<br>
- Common Controls v6 backported to Windows 2000<br>
- Optimized buffer size -> ~15% faster flashing on average<br>
- Disabled updates, without affecting DBX updating and ISO downloading<br>
- All unsupported features hidden and only visible on supported systems<br>
- Connecting to the internet with a VPN has been fixed for Windows 7<br>
- Application closing has been optimized
- autorun.inf / .ico files disabled
- Original code paths kept for natively supported systems<br>
&nbsp;&nbsp;&nbsp;-> (OS-specific fallbacks were prefered over replacing original code)

Future Plans
------------
- Merge the new DBX paths
- Port "Windows User Experience" options
- Screw libwim, use 7's wimgapi and implement esd and swm support
- Update GRUB, UEFI:NTFS, DD-Only, DBX
- Security / Safe dll loading / Fido issues

More about the project
----------------------
Rufus-Legacy is based on Rufus 4.7.2231 (released April 9, 2025).<br>
Newer releases switched to using wimlib, which I found to be pretty damn troublesome.<br>
The general idea was never to 'just get it running', but rather to actually adapt the existing features of Rufus to work natively on older systems.<br>
Future releases of Rufus-Legacy will mostly focus on selectively implementing upstream security and feature commits, as well as fixing bugs and issues.<br>

Compilation
-----------

Use Visual Studio 2022 and then invoke the `.sln` <br>
Starting with Release 1.0, x86 builds will be prioritized;<br>
x64 compilations may fail, and/or have issues.

MinGW compiling isn't supported for this fork.

#### Visual Studio

Rufus is an OSI compliant Open Source project. You are entitled to
download and use the *freely available* [Visual Studio Community Edition](https://www.visualstudio.com/vs/community/)
to build, run or develop for Rufus. As per the Visual Studio Community Edition license,
this applies regardless of whether you are an individual or a corporate user.

Rufus is 100% [Free Software](https://www.gnu.org/philosophy/free-sw) ([GPL v3](https://www.gnu.org/licenses/gpl-3.0))
All credits for the original project go to [Pete Batard](https://github.com/pbatard) and all other contributors.
Rufus-Legacy is an unofficial fork and is not affiliated with, endorsed by, or otherwise associated with the upstream Rufus project.

#### For Anyone Reading The Code
**MOST** changes marked with comments ending with "(port)",<br>
or contain full feature names like "Windows To Go".<br>

#### AI Use

Some parts of the code; i.e. the API implementations winxp.c win2k.c as well as win2k_imports.asm have been developed with the use of AI, and underwent numerous revisions.<br>
Certain parts of the code that have been written entirely by AI are marked with comments ending with (port-AI).<br>
I have used AI to look over my code, improve, safecheck and optimize parts of it, to ensure that what is released actually works.<br>

My main and only objective is delivering actually working code.
