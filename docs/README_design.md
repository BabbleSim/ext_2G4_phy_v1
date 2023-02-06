## Overall internal design

In short this Phy is composed of the following main components:

* A list of current (and future) transmissions in the air
* For each device interface: A state machine where transitions/operations
  can either be done immediately or be scheduled for a future time
* A list of (timed) events (function queue)

### The pending transmissions list

The pending list of transmissions (Tx list,
[pending_tx_list.c](../src/p2G4_pending_tx_list.c)/
[h](../src/p2G4_pending_tx_rx_list.h))
is an array with one entry per device (`tx_l_c_t`).
Where each element keeps a current or future transmission parameters,
as well as an indication of the transmission being currently active.<br>
An interface is provided to modify (register) and entry, set it to active,
and clear it.<br>

### Functions queue

The list of events/function queue,
([func_queue.h](../src/p2G4_func_queue.h),
[funcqueue.c](../src/p2G4_func_queue.c))
is used by each of the device interfaces to schedule future operations.
This queue has one entry per interface, which consists just of a function to be
called and the simulated time when the call should be done.<br>

This queue provides functions to modify (add) an entry, to remove it,
as well as to get the time of the next event.<br>
Each time a new event is queued the list finds which is the next event
which needs to be executed. The queue is ordered first by time, and then by
event type (`f_index_t`).<br>
You can see more details in [p2G4_func_queue.h](../src/p2G4_func_queue.h).

### Device interface state machine

Each device interface implements the same state machine, which in short works
as follows:

1. The next operation is requested from the device.
   Depending on what the operation is, either one of the substatemachines events
   is started (Rx, Tx, Wait, RSSI, CCA), or the device interface is disabled
   (DISCONNECT), or the whole simulation is ended (TERMINATE)
2. The substate machine transitions over time (using the function queue)
   until it eventually comes back to 1.

You can find more details about each substate machine in
[README_device_interface.md](README_device_interface.md)

## Overall workings

The Phy starts by parsing the command line parameters, intializing all its
components, and loading and initializing the channel and modem libraries.

Then it will block until receiving the next request from each device.

Note that this is the basic way of working of the Phy:
It cannot let time pass until it knows what is it that all devices want to do.
It cannot know if a reception will be succesfull until it knows what all others
devices will be doing during that reception time.

Once it knows what it is that all devices want to do, it can let time pass
until it handles the first (earliest) device needs, and then it must wait until
that device requests what it wants next.

It will then repeat this process again and again, until the simulation end time
has been reached, or all devices have disconnected.
