#!/bin/sh
# /etc/fnkey/default.sh
#
# This shell script is called by fnkey automatically when any key is catched
# from /proc/sci, you can use it for debugging or do according actions for
# different keys' events.

case $1 in
	screen)
	case $2 in
		on)
		echo "backlight on"
		;;
		off)
		echo "backlight off"
		;;
	esac
	;;
	switchvideo)
	case $2 in
		crt)
		echo "display on"
		xrandr --output default --mode 1024x768
		echo "DISPLAY : CRT" > /proc/sci
		;;
		lcd)
		echo "display off"
		xrandr --output default --mode 1024x600 --rate 60
		echo "DISPLAY : LCD" > /proc/sci
		;;
		all)
		echo "DISPLAY : ALL" > /proc/sci
		;;
	esac
	;;
	ac)
	case $2 in
		on)
		echo "ac on"
		;;
		off)
		echo "ac off"
		;;
	esac
	;;
	crt)
	case $2 in
		on)
		echo "DISPLAY : ALL" > /proc/sci
		;;
		off)
		xrandr --output default --mode 1024x600 --rate 60
		echo "DISPLAY : LCD" > /proc/sci
		;;
	esac
	;;
	lid)
	case $2 in
		on)     # open the LID
		echo "lid on"
		;;
		off)	# close the LID
		echo "lid off"
		;;
	esac
	;;
	camera)
	case $2 in
		on)
		echo "CAMERA : ON" > /proc/sci
		modprobe uvcvideo
		;;
		off)

		if [ -e /dev/video0 ]; then
			 pid="$(lsof /dev/video0 | awk 'NR==2 {print $2}')"
		fi
		while [ "$pid" != "" ]; do
			kill -9 $pid
			pid="$(lsof /dev/video0 | awk 'NR==2 {print $2}')"
		done
		rmmod uvcvideo
		echo "CAMERA : OFF" > /proc/sci
		;;
	esac
	;;
	mute)
	case $2 in
		on)
		echo "mute on"
		amixer sset Master mute > /dev/null
		;;
		off)
		echo "mute off"
		amixer sset Master unmute > /dev/null
		;;
	esac
	;;
	wifi)
	case $2 in
		on)
		echo "wireless on"
		;;
		off)
		echo "wireless off"
		;;
	esac
	;;
	volume)
	amixer -c 0 sset Master,0 $2%,$2% > /dev/null
	if [ $2 == 0 ] ; then
		amixer sset Master mute > /dev/null
	else
		amixer sset Master unmute > /dev/null
	fi
	;;
	brightness)
	echo "brightness $2"
	;;
esac
