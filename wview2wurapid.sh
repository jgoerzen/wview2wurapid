#!/bin/bash

#  Set your ID and PASSWORD here.
#  If they contain a space, "&", or "?", use HTTP %xx encoding
# for those characters.

ID="WIXXXXX"
PASSWORD="PASSWORD"
DATAFEED="./wudatafeedClient"

SOFTWARE="wview2wurapid"

set -e
set pipefail

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
   while read LINE; do
     WGETURL="http://rtupdate.wunderground.com/weatherstation/updateweatherstation.php?ID=${ID}&PASSWORD=${PASSWORD}&softwaretype=${SOFTWARE}&realtime=1&rtfreq=15&${LINE}"
     log "Sending to ${WGETURL}"
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
   runner
   log "Got error from read; will retry in 15s"
   kill "$LOGPID"
   kill -9 "$LOGPID"
   sleep 15
done


