# Rufus-Legacy: The Reliable USB Formatting Utility

![Rufus logo](https://raw.githubusercontent.com/non4med-dev/Rufus-Legacy/master/res/icons/Page.png)

[Rufus](https://github.com/pbatard/rufus) is a utility that helps format and create bootable USB flash drives.

Introduction
-------------
The main goal of this port of Rufus v4.7.2231 is running on the following operating systems:

* Windows 2000
* Windows XP
* Windows XP x64 Edition
* Windows Vista 32-bit and 64-bit
* Windows 7 32-bit and 64-bit

It brings full functionality back to Windows 7, and all the way down to Windows 2000.
What sets it apart is obsessive attention to detail, and little optimizations along the way that added app to performance later.

Flashing a 3,6GB Windows 10 ISO took Rufus v3.22 3m2s, whilst Rufus-Legacy (with a Buffer size of 1MiB) took 2m31s.

The main features include:
* Fido ISO downloading on Windows 7
* DBX UEFI Signature Validations on all versions (local files), downloadable since Vista
* Creating EFI-bootable GUID Partition Tabled USB-Drives on Windows XP and 2000, to be used on modern machines

Rufus Features / Windows Versions That Support Them
--------------------------------------------------------------------

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

* Modern and familiar UI, with [38 languages natively supported](https://github.com/pbatard/rufus/wiki/FAQ#What_languages_are_natively_supported_by_Rufus)
* Small footprint. No installation required.
* Portable. Secure Boot compatible.
* 100% [Free Software](https://www.gnu.org/philosophy/free-sw) ([GPL v3](https://www.gnu.org/licenses/gpl-3.0))

What Sets This Port Apart
---------------------------

* Native and proper Windows 7, Vista, XP and 2000 compatibility with proper API implementations
* Common Controls v6 backported to Windows 2000 and XP to fix classic theme compatibility issues
* GUID Partitioning Table (GPT) formatting support for Windows 2000 and Windows XP, to create EFI-bootable media
* 1MiB buffer size as it was found to be the fastest on average on USB2 and USB3 drives
* Created with Windows 7 in mind; flashing ISO files is reportedly ~15% faster compared to v3.22
* ISO Downloading through Fido on Windows 7
  ([Powershell 7](https://github.com/PowerShell/PowerShell/releases/tag/v7.2.24) is required)
* Modern, unbackportable features like VDS are disabled, and related options hidden
* VHD saving has been restored for Windows 2000 up to Windows 7
* Rufus updates have been permanently disabled, without affecting remaining network functionality
* Rufus tamper protection ("Unofficial version" warning) have been disabled
* Connecting to the internet with a VPN has been fixed
* Application closing through the Close button has been optimized
* Autorun.ini / .ico files have been disabled (Out of personal preference)
* Maximum downloading URL length has been increased from 128 to 1024 to prevent issues in some regions
* Before Fido runs, it checks for the installed .NET, WMF and Powershell versions
* Original code paths have been retained for natively supported Windows versions


Future Goals
-------------
Work on adding Windows NT4.0 support (shouldn't be too hard)
Possibly bring Windows To Go support to Windows 7, we'll see from there
Find a way around Vista's SSL problem


AI Disclaimer
-------------

The winxp.c and win2k.c aswell as the win2k_imports.asm have been written by AI, and underwent numerous revisions.
Certain parts of the code I couldn't implement myself, and are 100% AI generated are marked with comments ending with (port-AI)
I have used AI to improve and optimize CERTAIN functions I couldn't have written better myself.
Without it, this project wouldn't have gotten further than Vista.

I am an amateur, and a complete beginner.
I care more about my code working, rather than crying myself to sleep about the fact 10% of it was AI generated.
If you wish to write those parts yourself, submit Pull requests or fork this directory. 

Compilation
-----------

Use Visual Studio 2022 and then invoke the `.sln` 

MinGW is not supported for this fork.

#### Visual Studio

Rufus is an OSI compliant Open Source project. You are entitled to
download and use the *freely available* [Visual Studio Community Edition](https://www.visualstudio.com/vs/community/)
to build, run or develop for Rufus. As per the Visual Studio Community Edition license,
this applies regardless of whether you are an individual or a corporate user.
