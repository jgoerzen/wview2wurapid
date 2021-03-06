#!/bin/bash

WVIEW2WURAPID=./wview2wurapid.sh

if pgrep -f wview2wurapid.sh > /dev/null &&
   pgrep -f wudatafeedClient > /dev/null; then
  exit 0
else
  sleep 10
  killall wview2wurapid.sh &> /dev/null
  killall wudatafeedClient &> /dev/null
  "$WVIEW2WURAPID" &> /dev/null &
fi
