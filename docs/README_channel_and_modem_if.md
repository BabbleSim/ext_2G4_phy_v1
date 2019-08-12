## Channels and modems

To be able to emulate the system performance in different environments, with
different modems and analog components, and with different tradeoffs of
simulation speed and accuracy, this Phy allows to use exchangable channel
and modem models.
How a user selects then is described in
[this page](https://babblesim.github.io/2G4_select_ch_mo.html)

All a channel needs to do for this Phy to be able to load and use it is to
implement the interface described in [channel_if.h](../src/channel_if.h)

Similarly modems models would need to implement the interface described in
[modem_if.h](../src/modem_if.h)
