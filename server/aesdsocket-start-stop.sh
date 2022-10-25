#! /bin/sh

case "$1" in
    start)
        echo "Starting simpelserver"
        /etc/init.d/init.sh start
        /etc/init.d/misc-modules-start-stop.sh start
        /usr/bin/aesdchar_load
        start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket
        ;;
    stop)
        echo "Stopping simpleserver"
        start-stop-daemon -K -n aesdsocket
        ;;
    *)
        echo "Usage: $0 {start|stop}"
    exit 1
esac
exit 0