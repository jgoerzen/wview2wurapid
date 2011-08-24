#!/bin/bash

#  Set your ID and PASSWORD here.
#  If they contain a space, "&", or "?", use HTTP %xx encoding
# for those characters.

ID="WIXXXXX"
PASSWORD="PASSWORD"
DATAFEED="./wudatafeedClient"

SOFTWARE="wview2wurapid"

TMPDIR=`mktemp -d --tmpdir "${SOFTWARE}.XXXXXXXXXXXXX"`
WGETOUT="$TMPDIR/wgetout"
ERRORFIFO="$TMPDIR/errorfifo"
mkfifo "$ERRORFIFO"

trap 'rm -r $TMPDIR' 0

log () {
   logger -t "$SOFTWARE/$$" "$@"
}

runner () {
   log "runner beginning"
   UPDATEDUE=20   # Update every 5 mins (20 packets)
   while read LINE; do
     WGETURL="http://rtupdate.wunderground.com/weatherstation/updateweatherstation.php?ID=${ID}&PASSWORD=${PASSWORD}&softwaretype=${SOFTWARE}&realtime=1&rtfreq=15&${LINE}"
     let UPDATEDUE-=1
     if [ "$UPDATEDUE" = "0" ]; then
       log "20 packets sent; now sending to ${WGETURL}"
       UPDATEDUE=20
     fi
     wget --tries=2 -q -O - "$WGETURL" > "$WGETOUT"
     if grep -qvi success "$WGETOUT"; then
        log "Error from wunderground"
     fi
   done
}

while true; do
   logger -t "$SOFTWARE/$$/$DATAFEED" < "$ERRORFIFO" &
   LOGPID="$!"
   ./wudatafeedClient 2> "$ERRORFIFO" | runner
   sleep 2
   log "Got error from read; will retry in 15s"
   kill "$LOGPID" &> /dev/null || true
   kill -9 "$LOGPID" &> /dev/null || true
   sleep 15
done


