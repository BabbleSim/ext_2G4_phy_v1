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

The interface between the simulated devices and this Phy is provided by
[ext_2G4_libPhyComv1](https://github.com/BabbleSim/ext_2G4_libPhyComv1).
Please refer to
[it's documentation](https://github.com/BabbleSim/ext_2G4_libPhyComv1/blob/master/docs/README.md)
for more information about the exact protocol and the messages which each device
exchanges with this Phy.
