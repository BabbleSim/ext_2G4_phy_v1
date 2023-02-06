/*
 * Copyright 2018 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Phy activity dumping/tracing
 *
 * Unless told otherwise thru command line parameters the Phy will dump
 * in results/<sim_id>/ all activity from all devices as well as information
 * about the results it received from the modem models
 *
 * The dumping can be run in 2 modes: It can either dump, or compare what it
 * would have dumped with the content of the files present in the disk
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

/**
 * Open all dump files (as configured from command line)
 */
void open_dump_files(uint8_t comp_i, uint8_t stop, uint8_t dump_imm, const char* s,
                     const char* p, const uint n_dev_i);

/**
 * Close all dump files (the simulation has ended)
 */
int close_dump_files();

/**
 * Write to file information about a transmission
 * (v2 API)
 */
void dump_tx(tx_el_t *tx, uint dev_nbr);

/**
 * Write to file information about a completed reception
 */
void dump_rx(rx_status_t *rx_st, uint8_t* packet, uint d);

/**
 * Write to file information about a completed RSSI measurement
 */
void dump_RSSImeas(p2G4_rssi_t *RSSI_req, p2G4_rssi_done_t* RSSI_res, uint d);

/**
 * Write to file information about a completed CCA check
 */
void dump_cca(cca_status_t *cca, uint dev_nbr);

/**
 * Write to file information about a modem model invocation
 */
void dump_ModemRx(bs_time_t now, uint tx_nbr, uint d, uint n_dev, uint CalNotRecal, p2G4_radioparams_t *radio_params, rec_status_t *rec_s, tx_l_c_t *txl_c );

#ifdef __cplusplus
}
#endif

#endif
