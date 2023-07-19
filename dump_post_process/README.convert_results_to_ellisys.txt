This is a postprocessing utility which converts bsim Phy dumps of the Tx
activity of 1 or several devices into the import format for the Ellisys
Bluetooth Analyzer SW (.bttrp).
This way the BLE standard traffic (and some industry propietary extensions)
can be analyzed by this Ellisys tool.

You can call it for example as:
  components/ext_2G4_phy_v1/dump_post_process/csv2bttrp results/<sim_id>/d_2G4*.Tx.csv > ~/Trace.bttrp

This trace can then be *imported* into the Ellisys SW:
  File > Import ; Select Bluetooth packets ; Click Next ; Click Browse ; and select/open the file.

Note that all packets which are not BLE modulated won't be coverted (will be ignored).

Note that the Ellisys analyzer SW is _very_ picky, and ignores many packets which are
in some way malformed.

Note: convert_results_to_ellisys.sh is deprecated, please use csv2bttrp (or convert_results_to_ellisysv2.sh) instead.
