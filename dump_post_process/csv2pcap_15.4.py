#!/usr/bin/env python3
# Copyright 2019 Oticon A/S
# Copyright 2023 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

import time
import argparse
import struct
from csv_common import *

# default max packet length (phdr + access address + pdu + crc)
SNAPLEN = 512

def write(outfile, *inputs, snaplen = SNAPLEN, basetime=None):
	#For information on the pcap format see https://wiki.wireshark.org/Development/LibpcapFileFormat

	buf = bytearray(30)

	if basetime == None:
		basetime = int(time.time() * 1000000)

	# write pcap header
	struct.pack_into('<IHHiIII', buf, 0,
			0xa1b2c3d4, # magic_number
			2,          # version_major
			4,          # version_minor
			0,          # thiszone
			0,          # sigfigs
			snaplen,    # snaplen
			215)        # network, 215 = DLT_IEEE802_15_4_NONASK_PHY ; https://www.tcpdump.org/linktypes.html
	outfile.write(buf[:24])

	inputs = [ CSVFile(f) for f in inputs ]
	rows = [ next(cf, None) for cf in inputs ]

	while True:
		min_ts = None
		min_idx = None
		for idx, row in enumerate(rows):
			if not row:
				continue
			ts = row['start_time']
			if min_ts == None or ts < min_ts:
				min_ts = ts
				min_idx = idx

		if min_idx == None:
			break

		row = rows[min_idx]
		rows[min_idx] = next(inputs[min_idx], None)

		orig_len = int(row['packet_size'], 10)
		if orig_len == 0:
			continue
		
		modulation = int(row['modulation'], 10)
		if modulation != 256:
			continue

		freq = float(row['center_freq'])
		if freq >= 1.0 and freq < 81.0:
			rf_channel = int((freq - 1.0) / 2)
		elif freq >= 2401.0 and freq < 2481.0:
			rf_channel = int((freq - 2401.0) / 2)
		else:
			raise ValueError

		pdu_crc = bytes.fromhex(row['packet'])
		if len(pdu_crc) != orig_len:
			raise ValueError
		orig_len += 5; # 4 bytes preamble + 1 bytes address/SFD
		incl_len = min(orig_len, snaplen)

		access_address = int(row['phy_address'], 16)

		ts = basetime + min_ts
		ts_sec = ts // 1000000
		ts_usec = ts % 1000000

		struct.pack_into('<IIII', buf, 0,
				# pcap record header, 16 bytes
				ts_sec,
				ts_usec,
				incl_len,
				orig_len)
		outfile.write(buf[:16])

		struct.pack_into('<IB', buf, 0,
				#Header
				0, # preamble (0)
				access_address) #SFD
		outfile.write(buf[:5])

		#The packet starting from the lenght byte
		outfile.write(pdu_crc[:(incl_len - 5)])

def parse_args():
	parser = argparse.ArgumentParser(description='Convert BabbleSim Phy csv files to pcap')
	parser.add_argument(
			'-er', '--epoch_real',
			action='store_true',
			dest='epoch_real',
			required=False,
			help='If set, the pcap timestamps will be offset based on the host time when the simulation was run. Otherwise they are directly the simulation timestamps',
			)
	parser.add_argument(
			'-es', '--epoch_simu',
			action='store_false',
			dest='epoch_real',
			required=False,
			help='If set, pcap timestamp are directly the simulation timestamps (default)',
			)
	parser.add_argument('-o', '--output',
			dest='output',
			metavar='OUTFILE',
			help='Write to this pcap file (required)',
			required=True,
			type=argparse.FileType(mode='wb'))
	parser.add_argument(
			dest='inputs',
			metavar='INFILE',
			help='Input csv file(s) (at least one is required)',
			nargs='+',
			type=open_input)
	return parser.parse_args()

args = parse_args()

if (args.epoch_real == True):
	basetime = int(min([ p[0] for p in args.inputs ]) * 1000000)
else:
	basetime = 0

write(args.output, *[ p[1] for p in args.inputs ], basetime=basetime)

# vim: set ts=4 sw=4 noet:
