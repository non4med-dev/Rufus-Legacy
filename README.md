# Rufus-Legacy: The Reliable USB Formatting Utility

![Rufus-Legacy-Banner](https://raw.githubusercontent.com/non4med-dev/Rufus-Legacy/master/res/icons/Banner.png)

Rufus is a utility that helps format and create bootable USB flash drives.

Introduction
-------------
This fork of Rufus v4.7 is designed to function on the following operating systems:

**Windows 2000**<br>
**Windows XP**<br>
**Windows Vista**<br>
**Windows 7**<br>
both 32 and 64 bit

What does it have to offer?
---------------------------

It restores full functionality back to Windows 7, and all the way down to Windows 2000.<br>
What sets it apart is attention to detail, proper networking on Windows Vista and 7, <br>
and many small optimizations along the way, which improved UI and ISO flashing performance:<br>

3,6GB - Windows 10 ISO - USB3 - Windows 7<br>
Rufus v3.22&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;took **3m02s**<br>
Rufus-Legacy &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;took **2m32s**<br>
Around **17% faster** on average

700MB - Windows XP ISO - USB2 - Windows XP<br>
Rufus v2.18&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;took **3m38s**<br>
Rufus-Legacy&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;took **3m12s**<br>
Around **13% faster** on average

This project additionally backports:<br>
Fido ISO downloading to Windows 7<br>
DBX UEFI Signature Validation (Uses local files on XP and 2000, downloadable since Vista)<br>
Creating EFI-bootable GUID Partition Table USB-Drives on Windows XP and 2000, to be used on modern machines<br>

Features / OS Requirements
---------------------------

* Basic USB, flash card and virtual drives formatting to FAT/FAT32/NTFS (Windows 2000+), UDF/exFAT/ext2/ext3 (Windows Vista+), ReFS (Windows 8.1+)
* Creating DOS bootable USB drives using [FreeDOS](https://www.freedos.org) or MS-DOS (Windows 2000+)
* Create BIOS or UEFI bootable drives, including [UEFI bootable NTFS](https://github.com/pbatard/uefi-ntfs) (Windows 2000+)
* Create bootable drives from bootable ISOs (Windows, Linux, etc.) (Windows 2000+)
* Create bootable drives from bootable disk images, including compressed ones (Windows 2000+)
* Create VHD/DD (Windows 2000+), VHDX (Windows 8+) and FFU (Windows 10 1709+) images of an existing drive
* Create persistent Linux partitions (Windows 2000+)
* Compute MD5, SHA-1, SHA-256 and SHA-512 checksums of the selected image (Windows 2000+)
* Perform runtime validation of UEFI bootable media (Windows 2000+) (Local DBX files required!!)
* Perform bad blocks checks, including detection of "fake" flash drives (Windows 2000+)
* Download official Microsoft Windows 8, Windows 10 or Windows 11 retail ISOs (Windows 7+)
* Download [UEFI Shell](https://github.com/pbatard/UEFI-Shell) ISOs (Windows 7+)
* Create [Windows To Go](https://en.wikipedia.org/wiki/Windows_To_Go) drives (Windows 8+)

What makes it different
-----------------------

* Native and proper Windows 7, Vista, XP and 2000 compatibility with proper API implementations
* Common Controls v6 backported to Windows 2000 and XP to fix classic theme compatibility issues
* GUID Partitioning Table (GPT) formatting support for Windows 2000 and Windows XP, to create EFI-bootable media (!!!)
* 1MiB buffer size as it was found to be the fastest on average on USB2 and USB3 drives
* Created with performance in mind; flashing ISO files is reportedly ~15% faster compared to officially supported versions
* ISO Downloading through Fido on Windows 7
  ([Powershell 7](https://github.com/PowerShell/PowerShell/releases/tag/v7.2.24) is required)
* Modern, unbackportable features like VDS are disabled, and related options hidden
* VHD saving has been restored for Windows 2000 up to Windows 7
* Rufus updates have been permanently disabled, without affecting remaining network functionality
* Rufus tamper protection ("Unofficial version" warning) has been disabled
* Connecting to the internet with a VPN has been fixed
* Application closing through the Close button has been optimized
* Autorun.ini / .ico files have been disabled (Out of personal preference)
* Maximum downloading URL length has been increased from 128 to 1024 to prevent issues in some regions
* Before Fido runs, it checks for the installed .NET, WMF and Powershell versions
* Original code paths have been retained for natively supported Windows versions

AI Disclaimer
-------------

The winxp.c and win2k.c aswell as the win2k_imports.asm were developed with the use of AI, and underwent numerous revisions.
Certain parts of the code I couldn't implement myself that are 100% AI generated are marked with comments ending with (port-AI)
I have used AI to improve and optimize CERTAIN functions I couldn't have written better myself.
Without it, this project wouldn't have gotten further than Vista.

I am an amateur, and a complete beginner.
I only care about my code working. That is my main and only objective.
If you wish to re-write the AI parts yourself, submit a Pull request or be free to fork this directory. 

Compilation
-----------

Use Visual Studio 2022 and then invoke the `.sln` 

MinGW is not supported for this fork.

#### Visual Studio

Rufus is an OSI compliant Open Source project. You are entitled to
download and use the *freely available* [Visual Studio Community Edition](https://www.visualstudio.com/vs/community/)
to build, run or develop for Rufus. As per the Visual Studio Community Edition license,
this applies regardless of whether you are an individual or a corporate user.

Rufus is 100% [Free Software](https://www.gnu.org/philosophy/free-sw) ([GPL v3](https://www.gnu.org/licenses/gpl-3.0))
All credits for the original project go to [Pete Batard](https://github.com/pbatard) and all other contributors.
Rufus-Legacy is an unofficial fork and is not affiliated with, endorsed by, or otherwise associated with the upstream Rufus project.
