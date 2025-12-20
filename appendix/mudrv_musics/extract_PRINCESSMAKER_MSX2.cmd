@echo off
python extract_fe00.py "disks/PM_No1.dsk" MUDRV/PM D1
python extract_fe00.py "disks/PM_No2.dsk" MUDRV/PM D2
python extract_fe00.py "disks/PM_No3.dsk" MUDRV/PM D3
python extract_fe00.py "disks/PM_No4.dsk" MUDRV/PM D4
python extract_fe00.py "disks/PM_No5.dsk" MUDRV/PM D5
python extract_fe00.py "disks/PM_No6.dsk" MUDRV/PM D6
python extract_fe00.py "disks/PM_No7.dsk" MUDRV/PM D7
python extract_binary.py "disks/PM_No1.dsk" 0xB1200 0x200 MUDRV/PM/PM.KIK
pause

