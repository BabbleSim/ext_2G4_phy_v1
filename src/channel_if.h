/*
 * Copyright 2018 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef P2G4_CHANNEL_IF
#define P2G4_CHANNEL_IF

#include "bs_types.h"
#include "p2G4_pending_tx_rx_list.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the channel internal status
 *
 * The input parameters are:
 *   n_devices    : the number of devices (transmitter/receivers)
 *   char *argv[] : a set of arguments passed to the channel library (same convention as POSIX main() command line arguments)
 *   int argc     : the number of arguments passed to the channel library
 *
 * It is up to the channel to define which arguments it can or shall receive to configure it
 * The only argument all channels shall understand is :
 *    [-h] [--h] [--help] [-?]
 * for which the channel shall print a list of arguments it supports with a descriptive message for each
 */
int channel_init(int argc, char *argv[], uint n_devs);

/**
 * Recalculate the fading and path loss of the channel in this current moment (<Now>)
 * in between the N used paths and the receive path (<rxnbr>)
 *
 * inputs:
 *  tx_used    : array with n_devs elements, 0: that tx is not transmitting,
 *                                           !=0 that tx is transmitting,
 *               e.g. {0,1,1,0}: devices 1 and 2 are transmitting, device 0 and 3 are not.
 *  tx_list    : array with all transmissions status (the channel can check here the modulation type of the transmitter if necessary)
 *  txnbr      : desired transmitter number (the channel will calculate the ISI only for the desired transmitter)
 *  rxnbr      : device number which is receiving
 *  now        : current time
 *  att        : array with n_devs elements. The channel will overwrite the element i
 *               with the average attenuation from path i to rxnbr (in dBs)
 *               The caller allocates this array
 *  ISI_SNR    : The channel will return here an estimate of the SNR limit due to multipath
 *               caused ISI for the desired transmitter (in dBs)
 *               If the channel does not estimate this parameter it shall set it to 100.0
 *
 * This function shall return < 0 on error ; 0 otherwise
 *
 *
 * Note: It is ensured that for each call time (now) will be >= than in the previous calls
 */
int channel_calc(const uint *tx_used, tx_el_t *tx_list, uint txnbr, uint rxnbr, bs_time_t now, double *att, double *ISI_SNR);

/**
 * Clean up: Free the memory the channel may have allocate
 * close any file descriptors etc.
 * (the simulation has ended)
 */
void channel_delete();

#ifdef __cplusplus
}
#endif

#endif
