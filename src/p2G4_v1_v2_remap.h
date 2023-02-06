/*
 * Copyright 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef P2G4_V1_V2_REMAP_H
#define P2G4_V1_V2_REMAP_H

#include "bs_types.h"
#include "bs_cmd_line_typical.h"
#include "bs_pc_2G4_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void map_rxv2_resp_to_rxv1(p2G4_rx_done_t *rx_done_v1, p2G4_rxv2_done_t *rx_done_v2);
void map_rxv1_to_rxv2(p2G4_rxv2_t *rx_v2_s, p2G4_rx_t *rx_v1_s);
void map_txv1_to_txv2(p2G4_txv2_t *tx_v2_s, p2G4_tx_t *tx_v1_s);

#ifdef __cplusplus
}
#endif

#endif
