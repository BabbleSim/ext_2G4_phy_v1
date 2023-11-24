# Copyright 2019 Oticon A/S
# Copyright 2023 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

import os
import csv

KEY_ALTERNATIVES = [
	('start_time',  'Tx_Start_Time'),
	('center_freq', 'CenterFreq'),
	('phy_address', 'PhyAddress'),
	('packet_size', 'PacketSize'),
	('packet',      'Packet'),
]

class CSVFile:
	def __init__(self, f):
		if not hasattr(f, 'read'):
			f = open(f, newline='')
		self.file = f
		self.reader = csv.reader(self.file, delimiter=',', lineterminator='\n')
		try:
			headers = self.reader.__next__()
		except StopIteration:
			print("File %s is fully empty"%self.file.name)
			headers = [];
		for (key, alt) in KEY_ALTERNATIVES:
			if not key in headers and alt in headers:
				headers[headers.index(alt)] = key
		self.headers = headers

	def __del__(self):
		self.file.close()

	def __enter__(self):
		return self

	def __exit__(self, exc_type, exc_val, exc_tb):
		self.file.close()
		return False

	def __iter__(self):
		return self

	def __next__(self):
		row = {}
		lst = self.reader.__next__()
		for idx, header in enumerate(self.headers):
			try:
				row[header] = lst[idx]
			except IndexError: # The last line may be corrupted, so let's end if we find a corrupted one
				print("Input file %s truncated mid line, ignoring line"%self.file.name) 
				return None
		row['start_time'] = int(row['start_time'], 10)
		return row

def open_input(filename):
	try:
		mtime = os.path.getmtime(filename)
		return (mtime, open(filename, mode='r', newline=''))
	except OSError as e:
		raise argparse.ArgumentTypeError(
				"can't open '{}': {}".format(filename, e))
