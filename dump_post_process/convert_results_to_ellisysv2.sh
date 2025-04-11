#! /usr/bin/env bash
# Copyright 2018 Oticon A/S
# SPDX-License-Identifier: Apache-2.0

# Script to convert the BabbleSim 2G4 phy BLE traffic dumps into the format the ellisys can
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
  echo -e "\n" >> $TMP_FILE
done

echo -e "FileFormat:Bluetooth\n\
                version=1.0\n\
\n\
ItemFormat:LE version=1.1"

awk '
function rfchannel(freq) { return (freq-2)/2; }
function rtrim(s)        { sub(/[ \t\r\n]+$/, "", s); return s; }

BEGIN{
FS=",";
}
{
Packet=$10;
if ((NF != 10) || (length(Packet) == 0)) { next } # Corrupted line

Mod=$5;
Packet_size=$9;

if ((Mod == 80) && (Packet_size > 4)) { # BLE Coded Phy - FEC2 part
  if (CI=="00") {
     CR=8;
  } else {
     CR=2;
  }
  printf("%.0f aa=%s rssi=%i rfchannel=%i phy=Coded coding=Coded%i rawdata=\"%s\"\n", TI, AA, RSSI, RF_CHANNEL, CR, rtrim(Packet));
  next
}
TI = $1 * 1000;
RF_CHANNEL=rfchannel($3);
AA=$4;
RSSI=-40;
if (length(Packet) >= 2) {
  CI=substr(Packet, 1, 2);
}
if (Mod == 16){
printf("%.0f aa=%s rssi=%i rfchannel=%i phy=1Mbps rawdata=\"%s\"\n",TI,AA,RSSI, RF_CHANNEL, rtrim(Packet));
}
if (Mod == 32 || Mod == 33){
printf("%.0f aa=%s rssi=%i rfchannel=%i phy=2Mbps rawdata=\"%s\"\n",TI,AA,RSSI, RF_CHANNEL, rtrim(Packet));
}
}' $TMP_FILE | sort -g | awk '{print "Item time="$0}'



#Examples of the importable format:
#v1.0:
#Item time=522186250 aa=0x8E89BED6 rssi=-58 rfchannel=12 rawdata="40 1E 60 B6 6E 2D A3 4A 02 01 06 02 0A 00 11 06 92 15 37 84 E8 CC 2C 86 31 44 4A C7 BD F4 74 7D 9E CD 7E"
#v1.1:
#Item time=15102072375 aa=0x1848870A rssi=-23 rfchannel=35 phy=1Mbps rawdata="09 00 38 A4 5B"
#Item time=15351840875 aa=0x1848870A rssi=-37.5 rfchannel=3 phy=2Mbps rawdata="01 00 9E A9 5B"

rm $TMP_FILE
