## BLE Phy activity dumps

The Physical layer (Phy) can dump all radio activity to files.
These files will contain all transmissions, receptions and RSSI measurements
from each device.

The files are stored in `results/<sim_id>/d_2G4_<dev_number>.{Rx|Tx|RSSI}.csv`

These files are formated as comma separated files (csv) with one line heading,
and, after, one line for each transaction.

This dumps can be converted to other formats other tools can process.
Check the `dump_post_process/` folder for more info.

The Phy can also be run in "check" mode. In this mode it will compare the
content of already existing files (for ex. from a previous run) with what it
would have generated otherwise, and warn when differences are found.
Run the Phy with `--help` to get more info about this option.

### Tx format

For each device transmission, these are the columns:

* start_time : microsecond when the first bit of the packet in air starts (1st bit of the preamble)
* end_time: microsecond when the last bit of the packet ends (last bit of the CRC). Note that a packet may have been aborted before reaching this point.
* center_freq: As an offset from 2400MHz, in MHz, carrier frequency. Note that frequencies below 0 and above 80 are valid for interferers.
* phy_address : Physical address / sync word of the packet
* modulation: Modulation type. One of P2G4_MOD_* : 0x10 = BLE 1 Mbps, 0x20 = BLE 2Mbps
* power_level: In dBm, transmitted power level (accounting for the device antenna gain)
* abort_time: When was the packet transmission aborted, or TIME_NEVER (2^64-1) if not aborted.
* recheck_time: (internal info) When did the device request to reevaluate the possibility to abort the packet last time
* packet_size: Size of the packet in bytes (not counting preamble or syncword, but counting header, payload, MIC and CRC)
* packet: Actual packet data (packet_size bytes)

### Rx

For each device reception attempt, these are the columns:

* start_time: When (in air time) was the receiver opened
* scan_duration: For how long was the receiver opened attempting to have a physical address match
* phy_address: Physical address / sync word which was searched for
* modulation: Type of modulation the receiver is configured to receive. One of P2G4_MOD_*
* center_freq: As an offset from 2400MHz, in MHz, carrier frequency.
* antenna_gain: Receiver antenna gain in dB
* sync_threshold: Reception tolerance: How many errors do we accept before considering the preamble + address sync lost
* header_threshold: How many errors do we accept in the header before giving a header error (automatically in the phy).
* pream_and_addr_duration: Duration of the preamble and syncword (for 1Mbps BLE, 40 micros)
* header_duration:  Duration of the packet header (for 1Mbps BLE, 16 micros)
* bps : Phy bitrate in bits per second (for 1Mbps 1e6)
* abort_time: When was the packet reception aborted, or TIME_NEVER (2^64-1) if not aborted.
* recheck_time: (internal info) When did the device request to reevaluate the possibility to abort the reception last time
* tx_nbr: Which simulated device number was synchronized to (-1 if none)
* biterrors: How many bit errors there was during the packet reception
* sync_end: Last micros (included) in which the address ended (only if there was a sync)
* header_end: Last micros (included) in which the packet header ended (only if there was a sync)
* payload_end: Last micros (included) in which the packet (CRC) ended (only if there was a sync)
* rx_time_stamp: Reception time stamp (last microsecond of the sync word/address)
* status: Reception status. One of P2G4_RXSTATUS*: 1:Ok, 2: CRC error, 3: Header Error, 4: No sync
* RSSI: Measured RSSI (Received signal strenght indication) value in dBm
* packet_size: Size of the packet in bytes (not counting preamble or syncword, but counting header, payload, MIC and CRC)
* packet: Actual packet data (packet_size bytes), note that this is the actual transmitted packet, without possible bit errors

### RSSI:

For each RSSI measurement (excluding the ones performed automatically in Rx measurements)

* meas_time: microsecond in which the mesaurement was performed
* modulation: Type of modulation the receiver is configured for during the measurement. One of P2G4_MOD_*
* center_freq: As an offset from 2400MHz, in MHz.
* antenna_gain: Receiver antenna gain in dB
* RSSI: Measured RSSI (Received signal strenght indication) value in dBm
