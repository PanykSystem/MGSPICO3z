@echo off

:DISK234
set DESTDIR=MUDRV/GAZZEL_2
rem python extract_0c00.py "disks/TheTowerOfGazzel_disk1.dsk" %DESTDIR% No1
python extract_0c00.py "disks/TheTowerOfGazzel_disk2.dsk" %DESTDIR% No2
python extract_0c00.py "disks/TheTowerOfGazzel_disk3.dsk" %DESTDIR% No3
python extract_0c00.py "disks/TheTowerOfGazzel_disk4.dsk" %DESTDIR% No4
python extract_binary.py "disks/TheTowerOfGazzel_disk2.dsk" 0x13200 0x200 %DESTDIR%/Gazzel_2.KIK

:DISK1
set DISKNAME=disks/TheTowerOfGazzel_disk1.dsk
set DESTDIR=MUDRV\GAZZEL_1
mkdir %DESTDIR%
python extract_binary.py %DISKNAME% 0x14200 0x0800 %DESTDIR%/No1_001.mud
python extract_binary.py %DISKNAME% 0x14A00 0x0400 %DESTDIR%/No1_002.mud
python extract_binary.py %DISKNAME% 0x14E00 0x0600 %DESTDIR%/No1_003.mud
python extract_binary.py %DISKNAME% 0x15400 0x0200 %DESTDIR%/No1_004.mud
python extract_binary.py %DISKNAME% 0x15600 0x0E00 %DESTDIR%/No1_005.mud
python extract_binary.py %DISKNAME% 0x16400 0x0600 %DESTDIR%/No1_006.mud
python extract_binary.py %DISKNAME% 0x16A00 0x0000 %DESTDIR%/No1_007.mud
python extract_binary.py %DISKNAME% 0x16A00 0x0200 %DESTDIR%/No1_008.mud
python extract_binary.py %DISKNAME% 0x16C00 0x0000 %DESTDIR%/No1_009.mud
python extract_binary.py %DISKNAME% 0x16C00 0x0200 %DESTDIR%/No1_010.mud
python extract_binary.py %DISKNAME% 0x16E00 0x0400 %DESTDIR%/No1_011.mud
python extract_binary.py %DISKNAME% 0x17200 0x0200 %DESTDIR%/No1_012.mud
python extract_binary.py %DISKNAME% 0x17400 0x2000 %DESTDIR%/No1_013.mud
python extract_binary.py %DISKNAME% 0x19400 0x0E00 %DESTDIR%/No1_014.mud
python extract_binary.py %DISKNAME% 0x1A200 0x2200 %DESTDIR%/No1_015.mud
python extract_binary.py %DISKNAME% 0x1C400 0x3E00 %DESTDIR%/No1_016.mud
python extract_binary.py %DISKNAME% 0x20200 0x3E00 %DESTDIR%/No1_017.mud
python extract_binary.py %DISKNAME% 0x24000 0x4800 %DESTDIR%/No1_018.mud
python extract_binary.py %DISKNAME% 0x28800 0x3C00 %DESTDIR%/No1_019.mud
python extract_binary.py %DISKNAME% 0x2C400 0x3000 %DESTDIR%/No1_020.mud
python extract_binary.py %DISKNAME% 0x2F400 0x3200 %DESTDIR%/No1_021.mud
python extract_binary.py %DISKNAME% 0x32600 0x0E00 %DESTDIR%/No1_022.mud
python extract_binary.py %DISKNAME% 0x33400 0x3200 %DESTDIR%/No1_023.mud
python extract_binary.py %DISKNAME% 0x36600 0x3A00 %DESTDIR%/No1_024.mud
python extract_binary.py %DISKNAME% 0x3A000 0x4C60 %DESTDIR%/No1_025.mud
python extract_binary.py %DISKNAME% 0x3EE00 0x4C60 %DESTDIR%/No1_026.mud
python extract_binary.py %DISKNAME% 0x44200 0x4200 %DESTDIR%/No1_027.mud
python extract_binary.py %DISKNAME% 0x48400 0x4800 %DESTDIR%/No1_028.mud

python extract_binary.py %DISKNAME% 0x14000 0x200 %DESTDIR%/Gazzel_1.KIK

pause

