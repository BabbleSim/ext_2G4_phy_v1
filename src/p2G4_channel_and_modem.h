/*
 * Copyright 2018 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef P2G4_CHANNEL_AND_MODEM_H
#define P2G4_CHANNEL_AND_MODEM_H

#include "bs_types.h"
#include "bs_pc_2G4.h"
#include "p2G4_pending_tx_rx_list.h"

#ifdef __cplusplus
extern "C"{
#endif

void channel_and_modem_init(uint cha_argc, char** cha_argv, const char* cha_name, uint *mo_argc, char*** mo_argv, char** mo_name, uint n_devs);
void channel_and_modem_delete();
uint chm_bit_errors(tx_l_c_t *tx_l, uint tx_nbr, uint rx_nbr, p2G4_rx_t *rx_s, bs_time_t current_time);
uint chm_is_packet_synched(tx_l_c_t *tx_l, uint tx_nbr, uint rx_nbr, p2G4_rx_t *rx_s, bs_time_t current_time);
void chm_RSSImeas(tx_l_c_t *tx_l, p2G4_rssi_t *RSSI_s, p2G4_rssi_done_t* RSSI_meas, uint rx_nbr, bs_time_t current_time);

#ifdef __cplusplus
}
#endif

#endif
