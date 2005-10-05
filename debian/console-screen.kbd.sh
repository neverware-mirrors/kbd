#!/bin/sh

# This is the boot script for the `kbd' package.
# It loads parameters from /etc/kbd/config, maybe loads
# default font and map, and maybe start "vcstime"
# (c) 1997 Yann Dirson

if [ -r /etc/kbd/config ]; then
    . /etc/kbd/config
fi

PATH=/sbin:/bin:/usr/sbin:/usr/bin
SETFONT="/usr/bin/setfont"
SETFONT_OPT="-v"
VCSTIME="/usr/sbin/vcstime"

# be sure the main program is installed
[ -x "${SETFONT}" ] || exit 0


# set DEVICE_PREFIX depending on devfs/udev
if [ -d /dev/vc ]; then
    DEVICE_PREFIX="/dev/vc/"
else
    DEVICE_PREFIX="/dev/tty"
fi

reset_vga_palette ()
{
	if [ -f /proc/fb ]; then
           # They have a framebuffer device.
           # That means we have work to do...
	    echo -n "]R"
	fi
}

setup ()
{
    VT="no"
    # If we can't access the console, quit
    CONSOLE_TYPE=`fgconsole 2>/dev/null` || exit 0
    if [ ! $CONSOLE_TYPE = "serial" ]; then
		readlink /proc/self/fd/0 | grep -q -e /dev/vc -e '/dev/tty[^p]' -e /dev/console
		if [ $? -eq 0 ]; then
	    	VT="yes"
	    	reset_vga_palette
		fi
    fi

    [ $VT = "no" ] && exit 0

    # start vcstime
    if [ "${DO_VCSTIME}" = "yes" ] && [ -x ${VCSTIME} ] ; then
		echo -n Starting clock on text console: `basename ${VCSTIME}`
		${VCSTIME} ${VCSTIME_OPT} &
		echo .
    fi

    # Global default font+map
    if [ "${CONSOLE_FONT}" ]
	then
		echo -n "Setting up general console font... "
		[ "${CONSOLE_MAP}" ] && CONSOLE_MAP="-m ${CONSOLE_MAP}"

		# Set for the first 6 VCs (as they are allocated in /etc/inittab)
		NUM_CONSOLES=`fgconsole --next-available`
		NUM_CONSOLES=`expr ${NUM_CONSOLES} - 1`
		for vc in `seq 0 ${NUM_CONSOLES}` 
	    do
	    	${SETFONT} -C ${DEVICE_PREFIX}$vc ${SETFONT_OPT} ${CONSOLE_FONT} || { echo " failed."; break; }
	    	if [ "$vc" -eq 6 ]; then echo " done."; fi 
		done
    fi
}

case "$1" in
    start|reload|restart|force-reload)
	setup
	;;
    stop)
	;;
    *)
	setup
	;;
esac

exit 0
