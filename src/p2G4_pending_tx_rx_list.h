/*
 * Copyright 2018 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef P2G4_PENDING_TX_LIST_H
#define P2G4_PENDING_TX_LIST_H

#include "bs_types.h"
#include "bs_pc_2G4_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { //For each device, current/next transmission
  p2G4_tx_t tx_s;
  uint8_t *packet;
} tx_el_t;

typedef struct{
  uint64_t ctr; //every time the tx list changes this counter is updated
  tx_el_t *tx_list;
  uint *used;
} tx_l_c_t;

typedef struct { //For each device, current/next reception
  uint8_t bpus; //bits per us
  bs_time_t scan_end; //Last us (included) in which we will scan
  bs_time_t sync_end; //Last us (included) in which the preamble + address ends
  bs_time_t header_end; //Last us (included) in which the header ends
  bs_time_t payload_end; //Last us (included) in which the payload ends
  p2G4_rx_t rx_s;
  p2G4_rx_done_t rx_done_s;
  int tx_nbr; //If we found a fitting Tx, which is its device number
  uint biterrors;
} rx_status_t;

void txl_create(uint n_devs);
void txl_free(void);
void txl_register(uint dev_nbr, p2G4_tx_t *tx_s, uint8_t* packet);
void txl_clear(uint dev_nbr);
void txl_activate(uint dev_nbr);
int txl_find_fitting_tx(p2G4_rx_t *rx_s, bs_time_t current_time);

#ifdef __cplusplus
}
#endif

#endif
