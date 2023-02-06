/*
 * Copyright 2018 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef P2G4_MODEM_IF
#define P2G4_MODEM_IF

#include "bs_types.h"
#include "p2G4_pending_tx_rx_list.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the modem internal status
 *
 * The input parameters are:
 *   dev_nbr      : the device number this modem library corresponds to
 *                  (note that this library may be reused in between several devices)
 *   nbr_devices  : the total number of devices in the simulation (gives the length of vectors later)
 *   char *argv[] : a set of arguments passed to the modem library (same convention as POSIX main() command line arguments)
 *   int argc     : the number of arguments passed to the modem library
 *
 * It is up to the modem to define which arguments it can or shall receive to configure it
 * The only argument all modems shall understand is :
 *    [-h] [--h] [--help] [-?]
 * The modem shall print a list of arguments it supports with a descriptive message for each
 *
 * This function shall return a pointer to this modems status structure
 * This same pointer will be passed back to consecutive function calls of the library (void *this)
 *
 * If the library does not require to keep any status it can just return NULL
 */
void* modem_init(int argc, char *argv[], uint dev_nbr, uint nbr_devices);

/**
 * Calculate the SNR of the desired input signal at the digital modem input/analog output
 * including all modem analog impairments
 *
 * inputs:
 *  this               : Pointer to this modem object
 *  rx_radioparams     : Radio parameters/configuration of this receiver for this Rx/RSSI measurement
 *  rx_powers          : For each possible transmitter ([0..n_devices-1]) what is their power level at this device antenna connector in dBm
 *  txl_c              : For each possible transmitter what are their Tx parameters
 *  desired_tx_nbr     : which of the transmitters is the one we are trying to receive
 *
 * outputs:
 *  OutputSNR               : SNR in the analog output / digital input of this modem (dB)
 *  Output_RSSI_power_level : RSSI level (analog, dBm) sent to the modem digital
 */
void modem_analog_rx(void *this, p2G4_radioparams_t *rx_radioparams, double *OutputSNR,double *Output_RSSI_power_level,
                     double *rx_powers, tx_l_c_t *txl_c, uint desired_tx_nbr);

/**
 * Return the bit error probability ([0.. RAND_PROB_1]) for a given SNR
 *
 * inputs:
 *  this           : Pointer to this modem object
 *  rx_radioparams : Radio parameters/configuration of this receiver for this Rx/RSSI measurement
 *  SNR            : SNR level at the analog output as calculated by modem_analog_rx()
 */
uint32_t modem_digital_perf_ber(void *this, p2G4_radioparams_t *rx_radioparams, double SNR);

/**
 * Return the probability of the packet sync'ing ([0.. RAND_PROB_1]) for a given SNR and
 * transmission parameters
 *
 * (note that this should ONLY include excess packet error probability,
 * as over the sync word and address normal bit errors will also be calculated)
 *
 * inputs:
 *  this           : Pointer to this modem object
 *  rx_radioparams : Radio parameters/configuration of this receiver for this Rx/RSSI measurement
 *  SNR            : SNR level at the analog output as calculated by modem_analog_rx()
 *  tx_s           : Parameters of the transmission we are receiving (in case the sync. probability depends on any of them)
 */
uint32_t modem_digital_perf_sync(void *this, p2G4_radioparams_t *rx_radioparams, double SNR, p2G4_txv2_t* tx_s);

/**
 * Return the digital RSSI value the modem would have produced for this given
 * RSSI_power_level in the digital input
 *
 * inputs:
 *  this             : Pointer to this modem object
 *  rx_radioparams   : Radio parameters/configuration of this receiver for this Rx/RSSI measurement
 *  RSSI_power_level : Analog power level as measured by modem_analog_rx()
 *
 * outputs:
 *  RSSI  : RSSI "digital" value returned by this modem, following the p2G4_rssi_power_t format (16.16 signed value) *
 */
void modem_digital_RSSI(void *this, p2G4_radioparams_t *rx_radioparams, double RSSI_power_level, p2G4_rssi_power_t *RSSI);

/**
 * Clean up: Free the memory the modem may have allocated
 * close any file descriptors etc.
 * (the simulation has ended)
 */
void modem_delete(void *this);

#ifdef __cplusplus
}
#endif

#endif
