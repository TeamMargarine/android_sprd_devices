#!/system/bin/sh
echo 533000 >  /sys/devices/platform/scxx30-dmcfreq.0/devfreq/scxx30-dmcfreq.0/ondemand/set_freq
echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
echo 0 >  /sys/devices/system/cpu/cpufreq/dynamic_hotplug/enabled
