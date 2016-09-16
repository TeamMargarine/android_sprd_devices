#!/system/bin/sh

if [ "$1" = "-c" ]; then
	chmod 660 /proc/nk/guest-02/dsp_bank
	chmod 660 /proc/nk/guest-02/guestOS_2_bank
	chown system:root /proc/nk/guest-02/dsp_bank
	chown system:root /proc/nk/guest-02/guestOS_2_bank
elif [ "$1" = "-e" ]; then
	prop=`getprop sys.mac.wifi`
        setprop ro.mac.wifi $prop
	prop=`getprop sys.mac.bluetooth`
        setprop ro.mac.bluetooth $prop
	prop=`getprop sys.bt.bdaddr_path`
        setprop ro.bt.bdaddr_path $prop
fi
