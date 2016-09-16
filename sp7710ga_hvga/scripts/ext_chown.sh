#!/system/bin/sh

if [ "$1" = "-e" ]; then
	prop=`getprop sys.mac.wifi`
        setprop ro.mac.wifi $prop
	prop=`getprop sys.mac.bluetooth`
        setprop ro.mac.bluetooth $prop
	prop=`getprop sys.bt.bdaddr_path`
        setprop ro.bt.bdaddr_path $prop
fi
