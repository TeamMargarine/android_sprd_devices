::2.1.4
@echo off
echo - set env
set VERSION=2.1.1
set ADB_CMD="%cd%\tools\adb.exe"
set FIND_CMD="%cd%\tools\find.exe"

echo - version:%VERSION%
echo - check adb connection
rem %ADB_CMD% kill-server
rem %ADB_CMD% root
%ADB_CMD% devices
%ADB_CMD% wait-for-device
echo  - device connected.
md logs
md tmp
echo  - get ro.build.type
%ADB_CMD% shell getprop | %FIND_CMD% "[ro.build.type]" > "%cd%\tmp\ro.build.type.txt"
for /f "tokens=1,* usebackq delims= " %%i in ("%cd%\tmp\ro.build.type.txt") do set btype=%%j
set btype=%btype:~1,-2%

if {%btype%}=={user} (
echo  - user version
) else (
%ADB_CMD% root
%ADB_CMD% wait-for-device
)

rem win7 date behaves differently..
rem echo get pc system version
ver | %FIND_CMD% "5.1" > NUL && set XT=windowsXP
ver | %FIND_CMD% "6.1" > NUL && set XT=windows7

%ADB_CMD% shell getprop | %FIND_CMD% "[ro.product.board]" > "%cd%\tmp\ro.product.board.txt"
%ADB_CMD% shell getprop | %FIND_CMD% "[ro.build.version.release]" > "%cd%\tmp\ro.build.version.release.txt"

echo  - get ro.product.board
for /f "tokens=1,* usebackq delims= " %%i in ("%cd%\tmp\ro.product.board.txt") do set board=%%j
set board=%board:~1,-2%
rem echo %board%

echo  - get ro.build.version.release
for /f "tokens=1,* usebackq delims= " %%i in ("%cd%\tmp\ro.build.version.release.txt") do set androidVer=%%j
set androidVer=%androidVer:~1,-2%
rem echo %androidVer%

echo  - set log dir name
set LOGDIR=slog_%DATE:~0,10%%TIME:~0,8%
if {%XT%}=={windows7} (set LOGDIR=%LOGDIR:/=%) else (set LOGDIR=%LOGDIR:-=%)
set LOGDIR=%LOGDIR::=%
set LOGDIR=%LOGDIR: =%
set LOGDIR1=logs\%LOGDIR%_%board%_%btype%
set LOGDIR=%CD%\logs\%LOGDIR%
set LOGDIR=%LOGDIR%_%board%_%btype%
set LOGDIR=%LOGDIR%
md "%LOGDIR%"
echo  - get ro.build.fingerprint
%ADB_CMD% shell getprop | %FIND_CMD% "[ro.build.fingerprint]" > "%LOGDIR%\ro.build.fingerprint.txt"
echo %VERSION% > "%LOGDIR%\toolsversion.txt"

echo  - create dirs
md "%LOGDIR%/internal_storage"
md "%LOGDIR%/external_storage"
md "%LOGDIR%/dropbox"
md "%LOGDIR%/hprofs"
md "%LOGDIR%/mms"
md "%LOGDIR%/corefile"
md "%LOGDIR%/audio"
md "%LOGDIR%/sysdump"
md "%LOGDIR%/bt_reg"
md "%LOGDIR%/wifi_reg"
md "%LOGDIR%/kdump"

echo  - query internal_storage and external_storage
%ADB_CMD% shell slogctl query | %FIND_CMD% "internal" > "%cd%\tmp\internal_storage.txt"
%ADB_CMD% shell slogctl query | %FIND_CMD% "external" > "%cd%\tmp\external_storage.txt"

for /f "tokens=1,2 usebackq delims=," %%i in ("%cd%\tmp\internal_storage.txt") do set internal_storage=%%j
rem echo %internal_storage%

for /f "tokens=1,2 usebackq delims=," %%i in ("%cd%\tmp\external_storage.txt") do set external_storage=%%j
rem echo %external_storage%

echo  - get external path
set external_path=%external_storage:slog=%
echo  - external path is %external_path%

echo  - get current screen snap bugreport
%ADB_CMD% shell slogctl screen
%ADB_CMD% shell slogctl snap
%ADB_CMD% shell slogctl snap bugreport

echo  - get dropbox hprofs corefile
%ADB_CMD% pull /data/system/dropbox %LOGDIR%\dropbox
%ADB_CMD% pull /data/misc/hprofs %LOGDIR%\hprofs
%ADB_CMD% pull /data/corefile %LOGDIR%\corefile
%ADB_CMD% pull /data/data/com.android.providers.telephony/ %LOGDIR%/mms


echo  - get modem_memory log...
%ADB_CMD% pull /sdcard/modem_memory.log %LOGDIR%/internal_storage/ 1>NUL 2>NUL
%ADB_CMD% pull /sdcard/external/modem_memory.log %LOGDIR%/external_storage/ 1>NUL 2>NUL

echo  - get log files
%ADB_CMD% pull %internal_storage% %LOGDIR1%\internal_storage
%ADB_CMD% pull %external_storage% %LOGDIR1%\external_storage

echo  - get audio log files
for /L %%i in (1,1,10) do %ADB_CMD% cat /proc/asound/sprdphone/vbc %LOGDIR%/audio/vbc-%%i
%ADB_CMD% pull /proc/asound/sprdphone/sprd-codec %LOGDIR%/audio
%ADB_CMD% pull /proc/asound/sprdphone/pcm0p/sub0/status %LOGDIR%/audio
%ADB_CMD% shell tinymix > %LOGDIR%/audio/tinymix.log

echo - clear log files
%ADB_CMD% shell slogctl clear

echo  - get kernel panic core file
%ADB_CMD% shell rm -r %external_path%sysdump/*
%ADB_CMD% shell mkdir %external_path%sysdump/
%ADB_CMD% shell mv %external_path%sysdump.core*  %external_path%sysdump/
%ADB_CMD% pull %external_path%sysdump %LOGDIR%\sysdump

echo  - get kernel dump elf file
%ADB_CMD% shell rm -r %external_path%kdump/*
%ADB_CMD% shell mkdir %external_path%kdump/
%ADB_CMD% shell mv %external_path%cdump_* %external_path%kdump/
%ADB_CMD% pull %external_path%kdump %LOGDIR1%\kdump

echo  - get Trout BT log...
%ADB_CMD% shell "echo 1 > /sys/kernel/trout_debug/trout_btdebug_cmd"
%ADB_CMD% pull /data/bt_regsdata.txt %LOGDIR%\bt_reg

echo  - get Trout WiFi log...
%ADB_CMD% shell "iwconfig wlan0 trout_dbg 2"
%ADB_CMD% shell "iwconfig wlan0 trout_dbg 8"
%ADB_CMD% pull /data/allregs.txt %LOGDIR%/wifi_reg/

echo  - clear core file
%ADB_CMD% shell rm -r %external_path%sysdump/*

echo  - clear elf file
%ADB_CMD% shell rm -r %external_path%kdump/*

rem for support 8825 4.1 4 cores
%ADB_CMD% shell rm -r /data/corefile/*

