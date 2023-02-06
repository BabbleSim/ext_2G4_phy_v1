/*
 * Copyright 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "bs_pc_2G4_types.h"

void map_rxv2_resp_to_rxv1(p2G4_rx_done_t *rx_done_v1, p2G4_rxv2_done_t *rx_done_v2){
  rx_done_v1->end_time = rx_done_v2->end_time;
  rx_done_v1->packet_size = rx_done_v2->packet_size;
  memcpy(&rx_done_v1->rssi, &rx_done_v2->rssi, sizeof(p2G4_rssi_done_t));
  rx_done_v1->rx_time_stamp = rx_done_v2->rx_time_stamp;
  rx_done_v1->status = rx_done_v2->status;
}

void map_rxv1_to_rxv2(p2G4_rxv2_t *rx_v2_s, p2G4_rx_t *rx_v1_s){
  rx_v2_s->start_time    = rx_v1_s->start_time;
  rx_v2_s->scan_duration = rx_v1_s->scan_duration;
  rx_v2_s->error_calc_rate = rx_v1_s->bps;
  rx_v2_s->antenna_gain  = rx_v1_s->antenna_gain;
  rx_v2_s->pream_and_addr_duration  = rx_v1_s->pream_and_addr_duration;
  rx_v2_s->header_duration  = rx_v1_s->header_duration;
  rx_v2_s->acceptable_pre_truncation = 0;
  rx_v2_s->sync_threshold = rx_v1_s->sync_threshold;
  rx_v2_s->header_threshold = rx_v1_s->header_threshold;
  rx_v2_s->resp_type = 0;
  rx_v2_s->n_addr = 1;
  rx_v2_s->prelocked_tx = 0;
  rx_v2_s->coding_rate = 0;
  rx_v2_s->forced_packet_duration = UINT32_MAX;
  memcpy(&rx_v2_s->radio_params, &rx_v1_s->radio_params, sizeof(p2G4_radioparams_t));
  memcpy(&rx_v2_s->abort, &rx_v1_s->abort, sizeof(p2G4_abort_t));
}

void map_txv1_to_txv2(p2G4_txv2_t *tx_v2_s, p2G4_tx_t *tx_v1_s){
  tx_v2_s->start_tx_time = tx_v1_s->start_time;
  tx_v2_s->start_packet_time = tx_v1_s->start_time;
  tx_v2_s->end_tx_time = tx_v1_s->end_time;
  tx_v2_s->end_packet_time = tx_v1_s->end_time;
  tx_v2_s->phy_address = tx_v1_s->phy_address;
  tx_v2_s->packet_size = tx_v1_s->packet_size;
  tx_v2_s->power_level = tx_v1_s->power_level;
  tx_v2_s->coding_rate = 0;
  memcpy(&tx_v2_s->radio_params, &tx_v1_s->radio_params, sizeof(p2G4_radioparams_t));
  memcpy(&tx_v2_s->abort, &tx_v1_s->abort, sizeof(p2G4_abort_t));
}
