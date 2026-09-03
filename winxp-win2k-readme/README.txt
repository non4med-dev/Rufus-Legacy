Windows 2000 / Windows XP local Rufus cache
=======================================

Networking is deliberately disabled on systems older than Windows Vista.

Copy the three files from the "Rufus" directory beside this README into:

    %USERPROFILE%\Local Settings\Application Data\Rufus

If Rufus is running in portable mode (rufus.ini exists beside the executable),
copy them into the "Rufus" subdirectory beside the executable instead.

Rufus will use dbx_ia32.bin and dbx_x64.bin when they are present and will
otherwise fall back to its embedded DBX data. It will use diskcopy.dll for
MS-DOS creation when that file is present and will not try to download it.
