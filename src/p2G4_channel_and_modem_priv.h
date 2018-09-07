/*
 * Copyright 2018 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef P2G4_CHANNEL_AND_MODEM_PRIV_H
#define P2G4_CHANNEL_AND_MODEM_PRIV_H

#include "bs_types.h"
#include "bs_pc_2G4.h"
#include "p2G4_pending_tx_rx_list.h"

#ifdef __cplusplus
extern "C"{
#endif

typedef struct {
  uint64_t last_tx_ctr; //If the Tx doesn't change we don't need to recalculate the channel
  uint64_t last_rx_ctr; //During the same Rx, if the Tx doesn't change we assume the channel doesnt change

  uint64_t rx_ctr; //This counter changes every time we are told to check if a packet is synchronized in this receiver

  double *att; //Array of attenuations from each possible transmitter [0..nbr_devices-1]
  double SNR_ISI;

  double *rx_pow;     //Array of power received from each possible transmitter [0..nbr_devices-1]

  double RSSI_meas_power; //equivalent CW power in dBm in the analog input which we measure by the analog
  double SNR_analog_o;

  double SNR_total;

  uint32_t BER;
  uint32_t sync_prob;
} rec_status_t;

#ifdef __cplusplus
}
#endif

#endif
