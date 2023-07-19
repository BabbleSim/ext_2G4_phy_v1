These are postprocessing utility scripts which convert
the phy dumps in csv format to pcap (or pcapng) files.
This way the BLE & 15.4 traffic can be analyzed by Wireshark and other tools.

Currently the following scripts exist:
* csv2pcap         : Encapsulate bsim BLE traffic into pcap format
* csv2pcap_15.4.py : Encapsulate bsim 15.4 traffic into pcap format
* csv2pcapng       : Encapsulate bsim traffic (all types) into pcapng format

To merge the TX activity from all devices into a single pcap trace do:

$ dump_post_process/csv2pcapng -o mytrace.pcap results/<sim_id>/d_2G4*.Tx.csv

Replace paths as necessary.

Alternatively one can merge RX and TX activity from a single device
to get a trace as seen by that device:

$ dump_post_process/csv2pcapng -o mytrace.pcap results/<sim_id>/d_2G4_00.{Rx,Tx}.csv

However be careful not to merge Rx and Tx files from different devices
or you'll get duplicated records.
