## BLE Phy for BabbleSim

The overall
[BabbleSim documentation](https://babblesim.github.io/)
provides information about
[BabbleSim's architecture](https://babblesim.github.io/architecture.html),
as well as
[general information on this physical layer](https://babblesim.github.io/2G4.html).

There you can also find some
[simple usage examples](https://babblesim.github.io/example_2g4.html).

This Phy accepts many command line parameters, for more information about these,
you can run it with the `--help` option.

For more details about the design of this Phy please check
[README_design.md](./README_design.md).

For information about how to select a different channel or modem model
[check this page](https://babblesim.github.io/2G4_select_ch_mo.html).
For information about how to implement a new channel or modem model,
check [README_channel_and_modem_if.md](README_channel_and_modem_if.md)

The interface between the simulated devices and this Phy is provided by
[ext_2G4_libPhyComv1](https://github.com/BabbleSim/ext_2G4_libPhyComv1).
Please refer to
[its documentation](https://github.com/BabbleSim/ext_2G4_libPhyComv1/blob/master/docs/README.md)
for more information about the exact protocol and the messages which each device
exchanges with this Phy.

### A few notes about the device-Phy interface
A device is free to advance its simulated time even before it gets
a response to a request and to pipe several consecutive requests.
This can be done easily and safely for Tx and Wait requests.
It would be more difficult though for a device to be able to
pipe any request after a Rx, RSSI, or CCA attempt.<br>
But this Phy follows the principle of not responding to a request until the
time in which that request would have been completed.

Note that piping several requests ahead of time will greatly increase the
speed of the simulation if there is free CPU cores.
Even if there is no free CPUs, it will overall increase performance by
decreasing the number of required context switches.
