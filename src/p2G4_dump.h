/*
 * Copyright 2018 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef P2G4_DUMP_H
#define P2G4_DUMP_H

#include "bs_types.h"
#include "bs_pc_2G4_types.h"
#include "p2G4_channel_and_modem_priv.h"
#include "p2G4_pending_tx_rx_list.h"

#ifdef __cplusplus
extern "C"{
#endif

void open_dump_files(uint8_t comp_i, uint8_t stop, const char* s,
                     const char* p, const uint n_dev_i);
int close_dump_files();
void dump_tx(tx_el_t *tx, uint d);
void dump_rx(rx_status_t *rx_st, uint8_t* packet, uint d);
void dump_RSSImeas(p2G4_rssi_t *RSSI_req, p2G4_rssi_done_t* RSSI_res, uint d);
void dump_ModemRx(bs_time_t now, uint tx_nbr, uint d, uint n_dev, uint CalNotRecal, p2G4_radioparams_t *radio_params, rec_status_t *rec_s, tx_l_c_t *txl_c );

#ifdef __cplusplus
}
#endif

#endif
