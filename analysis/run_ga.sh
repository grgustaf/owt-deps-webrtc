#!/bin/bash

if [ -z "$3" ]; then
    echo usage: run_ga.sh NAME LATENCY LOSSRATE
    exit 1
fi

LOGFILE=$1-$2-$3.log
echo logfile: $LOGFILE

./gawebrtcclient.exe --peer_server_url https://172.16.1.2:8096 --sessionid ga > $LOGFILE

./analyze.sh $LOGFILE
