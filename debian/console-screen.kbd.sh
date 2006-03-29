#!/bin/sh
### BEGIN INIT INFO
# Provides:          console-screen
# Required-Start:    $local_fs $remote_fs
# Required-Stop:     $local_fs $remote_fs
# Default-Start:     S 1 2 3 4 5
# Default-Stop:      0 6
### END INIT INFO

# This is the boot script for the `kbd' package.
# It loads parameters from /etc/kbd/config, maybe loads
# default font and map, and maybe start "vcstime"
# (c) 1997 Yann Dirson

PKG=kbd
if [ -r /etc/$PKG/config ]; then
    . /etc/$PKG/config
fi

if [ -d /etc/$PKG/config.d ]; then
    for i in `run-parts --list /etc/$PKG/config.d `; do
       . $i
    done
fi

PATH=/sbin:/bin:/usr/sbin:/usr/bin
SETFONT="/usr/bin/setfont"
SETFONT_OPT="-v"
VCSTIME="/usr/sbin/vcstime"

# Different device name for 2.6 kernels and devfs
if [ `uname -r | cut -f 2 -d .` = 6 ] && [ -e /dev/.devfsd ]; then
    VCSTIME_OPT="-2 /dev/vcsa0"
else
    VCSTIME_OPT=""
fi



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
    # be sure the main program is installed
    [ -x "${SETFONT}" ] || return

    VT="no"
    # If we can't access the console, quit
    CONSOLE_TYPE=`fgconsole 2>/dev/null` || return

    if [ ! $CONSOLE_TYPE = "serial" ]; then
        readlink /proc/self/fd/0 | grep -q -e /dev/vc -e '/dev/tty[^p]' -e /dev/console
        if [ $? -eq 0 ]; then
            VT="yes"
            reset_vga_palette
        fi
    fi

    [ $VT = "no" ] && return

    # start vcstime
    if [ "${DO_VCSTIME}" = "yes" ] && [ -x ${VCSTIME} ]; then
        echo -n Starting clock on text console: `basename ${VCSTIME}`
        ${VCSTIME} ${VCSTIME_OPT} &
        echo .
    fi

    # Global default font+map
    if [ "${CONSOLE_FONT}" ]; then
        echo -n "Setting up general console font... "
        [ "${CONSOLE_MAP}" ] && SETFONT_OPT="$SETFONT_OPT -m ${CONSOLE_MAP}"

        # Set for the first 6 VCs (as they are allocated in /etc/inittab)
        NUM_CONSOLES=`fgconsole --next-available`
        NUM_CONSOLES=`expr ${NUM_CONSOLES} - 1`
        [ ${NUM_CONSOLES} -eq 1 ] && NUM_CONSOLES=6
        for vc in `seq 0 ${NUM_CONSOLES}` 
        do
            ${SETFONT} -C ${DEVICE_PREFIX}$vc ${SETFONT_OPT} ${CONSOLE_FONT} || { echo " failed."; break; }
        done
        echo " done."
    fi


    # Per-VC font+sfm
    PERVC_FONTS="`set | grep "^CONSOLE_FONT_vc[0-9]*="  | tr -d \' `"
    if [ "${PERVC_FONTS}"  ]; then
        echo -n "Setting up per-VC fonts: "
        for font in ${PERVC_FONTS}
        do
            # extract VC and FONTNAME info from variable setting
            vc=`echo $font | cut -b15- | cut -d= -f1`
            eval font=\$CONSOLE_FONT_vc$vc
            [ X"$QUIET_PERVC" = X1 ] || echo -n "${DEVICE_PREFIX}${vc}, "
            # eventually find an associated SFM
            eval sfm=\${CONSOLE_MAP_vc${vc}}
            [ "$sfm" ] && sfm="-u $sfm"
            ${SETFONT} -C ${DEVICE_PREFIX}$vc ${SETFONT_OPT} $sfm $font
        done
        echo "done."
    fi


    # Global ACM
    #[ "${APP_CHARSET_MAP}" ] && ${CHARSET} G0 ${APP_CHARSET_MAP}


    # Per-VC ACMs
    PERVC_ACMS="`set | grep "^APP_CHARSET_MAP_vc[0-9]*="  | tr -d \' `"
    if [ "${PERVC_ACMS}" ]; then
        echo -n "Setting up per-VC ACM's: "
        for acm in ${PERVC_ACMS}
        do
            # extract VC and FONTNAME info from variable setting
            vc=`echo $acm | cut -b19- | cut -d= -f1`
            eval acm=\$APP_CHARSET_MAP_vc$vc
            [ X"$QUIET_PERVC" = X1 ] || echo -n "${DEVICE_PREFIX}${vc} ($acm), "
            #eval "${CHARSET} --tty='${DEVICE_PREFIX}$vc' G0 '$acm'"
        done
        echo "done."
    fi


    # Go to UTF-8 mode as necessary
    # 
    if [ -f /etc/environment ]
    then
        for var in LANG LC_CTYPE LC_ALL
        do
            value=$(egrep "^[^#]*${var}=" /etc/environment | tail -n1 | cut -d= -f2)
            eval $var=$value
        done
    fi
    CHARMAP=`LANG=$LANG LC_ALL=$LC_ALL LC_CTYPE=$LC_CTYPE locale charmap 2>/dev/null`
    if [ "$CHARMAP" = "UTF-8" ]; then
        /usr/bin/unicode_start 2> /dev/null || true
    else
        /usr/bin/unicode_stop 2> /dev/null|| true
    fi

    # screensaver stuff
    setterm_args=""
    if [ "$BLANK_TIME" ]; then
        setterm_args="$setterm_args -blank $BLANK_TIME"
    fi
    if [ "$BLANK_DPMS" ]; then
        setterm_args="$setterm_args -powersave $BLANK_DPMS"
    fi
    if [ "$POWERDOWN_TIME" ]; then
        setterm_args="$setterm_args -powerdown $POWERDOWN_TIME"
    fi
    if [ "$setterm_args" ]; then
        setterm $setterm_args 
    fi

    # Keyboard rate and delay
    KBDRATE_ARGS=""
    if [ -n "$KEYBOARD_RATE" ]; then
        KBDRATE_ARGS="-r $KEYBOARD_RATE"
    fi
    if [ -n "$KEYBOARD_DELAY" ]; then
        KBDRATE_ARGS="$KBDRATE_ARGS -d $KEYBOARD_DELAY"
    fi
    if [ -n "$KBDRATE_ARGS" ]; then
        echo -n "Setting keyboard rate and delay: "
        kbdrate -s $KBDRATE_ARGS
        echo "done."
    fi

    # Inform gpm if present, of potential changes.
    if [ -f /var/run/gpm.pid ]; then
        kill -WINCH `cat /var/run/gpm.pid` 2> /dev/null
    fi

    # Allow user to remap keys on the console
    if [ -r /etc/$PKG/remap ]; then
	dumpkeys < ${DEVICE_PREFIX}1 | sed -f /etc/$PKG/remap | loadkeys --quiet
    fi
    # Set LEDS here
    if [ -n "$LEDS" ]; then
        for i in `seq 1 12`
        do
            setleds -D $LEDS < $DEVICE_PREFIX$i
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

:

