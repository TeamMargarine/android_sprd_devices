#!/system/bin/sh

term="/dev/pts/* "

if [ "$1" = "-u" ]; then
	link=`getprop sys.symlink.umts_router`
	rm ${link##${term}}
	ln -s $link;
fi
