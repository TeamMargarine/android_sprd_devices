#!/bin/bash
VERSION=2.1.4
# prepare adb connection
echo "version:$VERSION"
java -version
#adb kill-server
# this will cause adb reconnect
adb devices
sleep 2
adb wait-for-device
echo "- device connected!"
echo "- record log version"
type=`adb shell getprop ro.build.type`
echo $type
type=${type%%\?*}
ttype=`echo $type | tr -d "[\r]"`
if [ "$ttype" == "user" ];then
echo "- user version"
else
adb root
adb wait-for-device

mkdir logs
mkdir tmp

fi
#| grep -r "\[ro.build.type\]" > `pwd`/tmp/ro.build.type.txt
board=`adb shell getprop ro.product.board`
#| grep -r "\[ro.product.board\]" > `pwd`/tmp/ro.product.board.txt
echo $board
board=${board%%\?*}
tboard=`echo $board | tr -d "[\r]"`
androidVer=`adb shell getprop ro.build.version.release`
#| grep -r "\[ro.build.version.release\]" > `pwd`/tmp/ro.build.version.release.txt
echo $androidVer
androidVer=${androidVer%%\?*}

tandroidVer=`echo $androidVer | tr -d "[\r]"`

#date -d tomorrow +%Y%m%d
LOGDIR="slog_`date +%Y%m%d%H%M%S`_$tboard"_"$ttype"
echo "save location "$LOGDIR

echo "create dirs..."
target_dir=$LOGDIR
top_dir=`pwd`/logs/$target_dir
mkdir -p $top_dir
echo  "- get ro.build.fingerprint"
#echo `adb shell getprop ro.build.fingerprint` > "$top_dir/ro.build.fingerprint.txt"
echo $VERSION > "$top_dir/toolsversion.txt"

mkdir $top_dir/internal_storage
mkdir $top_dir/external_storage
mkdir -p $top_dir/dropbox
mkdir -p $top_dir/hprofs
mkdir -p $top_dir/mms
mkdir -p $top_dir/corefile
mkdir -p $top_dir/audio
mkdir -p $top_dir/sysdump
mkdir -p $top_dir/bt_reg
mkdir -p $top_dir/wifi_reg

# if external_storage and external_storage is full then log store in data/slog/
internal_log_dir=`adb shell slogctl query | grep "^internal" | cut -d',' -f2`
external_log_dir=`adb shell slogctl query | grep "^external" | cut -d',' -f2`

echo "get external path"
external_sdcard=`echo $external_log_dir | sed 's/\/slog//g'`

echo "capture logs..."
adb shell slogctl screen
adb shell slogctl snap
adb shell slogctl snap bugreport

echo " - get dropbox hprofs corefile"
adb pull /data/system/dropbox $top_dir/dropbox
adb pull /data/misc/hprofs $top_dir/hprofs
adb pull  /data/corefile $top_dir/corefile
adb pull /data/data/com.android.providers.telephony/ $top_dir/mms

echo  "- get modem_memory log..."
adb pull /sdcard/modem_memory.log $top_dir/internal_storage/
adb pull /sdcard/external/modem_memory.log $top_dir/external_storage/

echo "dump logs..."
cd $top_dir/internal_storage
adb pull $internal_log_dir
cd $top_dir/external_storage
adb pull $external_log_dir

echo "- get audio log files"
for i in {1..10};
do
adb shell cat /proc/asound/sprdphone/vbc > $top_dir/audio/vbc-$i
done
adb pull /proc/asound/sprdphone/sprd-codec $top_dir/audio
adb pull /proc/asound/sprdphone/pcm0p/sub0/status $top_dir/audio
adb shell tinymix > $top_dir/audio/tinymix.log

echo "- clear log files"
adb shell slogctl clear

echo "- get kernel panic core file"
adb shell rm -r $external_sdcard/sysdump/*
adb shell mkdir $external_sdcard/sysdump/
adb shell mv $external_sdcard/sysdump.core*  $external_sdcard/sysdump/
adb pull $external_sdcard/sysdump $top_dir/sysdump

echo "- get kernel dump elf file"
adb shell rm -r $external_sdcard/kdump/*
adb shell mkdir $external_sdcard/kdump/
adb shell mv $external_sdcard/cdump_*  $external_sdcard/kdump/
adb pull $external_sdcard/kdump $top_dir/kdump

echo "- get Trout BT log..."
adb shell echo 1 > /sys/kernel/trout_debug/trout_btdebug_cmd
adb pull /data/bt_regsdata.txt $top_dir/bt_reg/

echo "- get Trout WiFi log..."
adb shell iwconfig wlan0 trout_dbg 2
adb shell iwconfig wlan0 trout_dbg 8
adb pull /data/allregs.txt $top_dir/wifi_reg/

echo "- clear core file"
adb shell rm -r $external_sdcard/sysdump/*

echo "- clear elf file"
adb shell rm -r $external_sdcard/kdump/*
#for support 8825 4.1 4 cores
adb shell rm -r /data/corefile/*
