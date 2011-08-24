#!/bin/bash

ID="WIXXXXX"
PASSWORD="PASSWORD"
SOFTWARE="wview2wlrapid"

TMPDIR=`mktemp -d --tmpdir "${SOFTWARE}.XXXXXXXXXXXXX"`
WGETOUT="$TMPDIR/wgetout"
FIFO="$TMPDIR/fifo"
mkfifo "$FIFO"

trap 'rm -r $TMPDIR' 0

log () {
   logger -t "$SOFTWARE/$$" "$@"
}

runner () {
   log "runner beginning"
   while read -t 40 LINE < $FIFO; do
     WGETURL="http://rtupdate.wunderground.com/weatherstation/updateweatherstation.php?ID=${ID}&PASSWORD=${PASSWORD}&softwaretype=${SOFTWARE}&realtime=1&rtfreq=15&${LINE}"
     log "Sending to ${WGETURL}"
     wget --tries=2 -q -O - "$WGETURL" > "$WGETOUT"
     if grep -qvi success "$WGETOUT"; then
        log "Error from wunderground"
     fi
   done
}

while true; do
   ./datafeedClient > "$FIFO" &
   sleep 2
   runner
   log "Got error from read; will retry in 15s"
   killall datafeedClient
   sleep 15
done

