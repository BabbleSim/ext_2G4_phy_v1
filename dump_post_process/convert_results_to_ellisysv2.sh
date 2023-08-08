#! /usr/bin/env bash
# Copyright 2018 Oticon A/S
# SPDX-License-Identifier: Apache-2.0

# Script to convert the BabbleSim 2G4 phy dumps into the format the ellisys can
# import (v1.1), supporting 2Mbps BLE
#
# Run for example as:
#./convert_results_to_ellisysv2.sh ../../results/Hola/d_*.Tx.csv > ~/c_drive/Users/alpi/LocalDocs/trial.bttrp

Input_files="$@"
TMP_FILE=$(mktemp)

echo -n "" > $TMP_FILE
for file in $Input_files
do
  tail -n+2 $file >> $TMP_FILE
done

sort -g $TMP_FILE | awk '
function rfchannel(freq) { return (freq-2)/2; }
function rtrim(s)        { sub(/[ \t\r\n]+$/, "", s); return s; }

BEGIN{
FS=",";
print("FileFormat:Bluetooth\n\
                version=1.0\n\
\n\
ItemFormat:LE version=1.1")

}
{
TI = $1 * 1000;
RF_CHANNEL=rfchannel($3);
AA=$4;
Mod=$5;
Packet=$10;
RSSI=-40;
if (Mod == 16){
printf("Item time=%.0f aa=%s rssi=%i rfchannel=%i phy=1Mbps rawdata=\"%s\"\n",TI,AA,RSSI, RF_CHANNEL, rtrim(Packet));
}
if (Mod == 32 || Mod == 33){
printf("Item time=%.0f aa=%s rssi=%i rfchannel=%i phy=2Mbps rawdata=\"%s\"\n",TI,AA,RSSI, RF_CHANNEL, rtrim(Packet));
}
}'

#Examples of the importable format:
#v1.0:
#Item time=522186250 aa=0x8E89BED6 rssi=-58 rfchannel=12 rawdata="40 1E 60 B6 6E 2D A3 4A 02 01 06 02 0A 00 11 06 92 15 37 84 E8 CC 2C 86 31 44 4A C7 BD F4 74 7D 9E CD 7E"
#v1.1:
#Item time=15102072375 aa=0x1848870A rssi=-23 rfchannel=35 phy=1Mbps rawdata="09 00 38 A4 5B"
#Item time=15351840875 aa=0x1848870A rssi=-37.5 rfchannel=3 phy=2Mbps rawdata="01 00 9E A9 5B"

rm $TMP_FILE
