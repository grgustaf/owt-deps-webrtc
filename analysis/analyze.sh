#!/bin/bash

LOGFILE=$1

if [ -z "$LOGFILE" ]; then
    echo usage: ./analyze.sh LOGFILE
    exit 1
fi

percentize() {
    num=$1
    denom=$2
    whole=$(($num * 100 / $denom))
    fract=$((($num * 10000 / $denom) % 100))
    echo "$whole.$fract%"
}

last_video=$(grep "Video " $LOGFILE | grep -v MISSING | tail -1)
total_video=$(echo $last_video | cut -d# -f2 | cut -d\) -f 1)

echo "Total video packets: $total_video"

last_rtx=$(grep "RTX 9" $LOGFILE | tail -1)
total_rtx=$(echo $last_rtx | cut -d# -f2 | cut -d\) -f 1)

echo "Total RTX packets: $total_rtx ($(percentize $total_rtx $total_video))"

first_fec=$(grep "Real FEC" $LOGFILE | head -1)
last_fec=$(grep "Real FEC" $LOGFILE | tail -1)

if grep "Real FEC" $LOGFILE > /dev/null; then
    total_fec=$(echo $last_fec | cut -d# -f2 | cut -d\) -f 1)
    echo "Total FEC packets: $total_fec ($(percentize $total_fec $total_video))"

    echo
    echo -n "First FEC packet: "
    echo $first_fec
    echo -n " Last FEC packet: "
    echo $last_fec

    recovered=$(grep "recovered by FEC" $LOGFILE | wc -l)
    echo "Packets recovered by FEC: $recovered"
else
    echo
    echo "FlexFEC never started!"
fi

missing=$(grep "Missing packet" $LOGFILE | wc -l)
echo
echo "Missing packets: $missing"

freeze_count=$(grep freeze_count $LOGFILE | tail -1 | cut -d: -f2 | cut -d' ' -f2)
freeze_duration=$(grep total_freezes_duration $LOGFILE | tail -1 | cut -d: -f2 | cut -d' ' -f2)
frames_duration=$(grep total_frames_duration $LOGFILE | tail -1 | cut -d: -f2 | cut -d' ' -f2)

int_freeze=$(echo $freeze_duration | cut -d. -f1)
int_frames=$(echo $frames_duration | cut -d. -f1)

echo
echo "Total frames duration: ${frames_duration}s"
echo "Total freeze duration: ${freeze_duration}s ($(percentize $int_freeze $int_frames))"
echo "Freeze count: $freeze_count"

delay_sum=0
max_delay=0
count=0
# checking for bracket makes this more resilient against mixed output streams
for s in $(grep arrived $LOGFILE | cut -d\] -f2 | cut -d' ' -f7); do
    delay_sum=$(($delay_sum + $s))
    if [ $s -gt $max_delay ]; then
        max_delay=$s
    fi
    count=$(($count + 1))
done

avg_delay=$(($delay_sum / $count))

double_delay=$(($avg_delay * 2))

count=0
for s in $(grep arrived $LOGFILE | cut -d\] -f2 |  cut -d' ' -f7); do
    if [ $s -gt $avg_delay ]; then
        count=$(($count + 1))
    fi
done

echo
echo Average delay to receive RTX packet: $(($delay_sum / $total_rtx))ms \(max ${max_delay}ms\)
echo $count RTX packets were delayed more than double that \(${double_delay}ms\)

#echo
#grep FPS $1 | cut -d' ' -f2-
