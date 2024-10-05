#!/bin/sh

# Startup script for aesdsocket.
# Author: Trapti Damodar Balgi
# References: ECEN 5713 slides - Week_4_-_Time_and_Sockets

case "$1" in
    start)
        echo "Starting aesdsocket"
        start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket
        ;;
    stop)
        echo "Stopping aesdsocket"
        start-stop-daemon -K -n aesdsocket
        ;;
    *)
        echo "Usage: $0 {start|stop}"
        exit 1
esac

exit 0