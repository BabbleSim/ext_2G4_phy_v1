/*
 * Copyright 2018 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include <string.h>
#include "bs_pc_2G4_types.h"
#include "bs_oswrap.h"
#include "bs_utils.h"
#include "p2G4_pending_tx_rx_list.h"

tx_l_c_t tx_l_c;
static tx_el_t *tx_list = NULL;

static uint nbr_devs;

void txl_create(uint n_devs){
  tx_l_c.tx_list = bs_calloc(n_devs, sizeof(tx_el_t));
  tx_l_c.used = bs_calloc(n_devs, sizeof(uint));
  tx_l_c.ctr = 0;
  tx_list = tx_l_c.tx_list;
  nbr_devs = n_devs;
}

void txl_free(void){
  if ( tx_l_c.tx_list != NULL ) {
    for (int d = 0 ; d < nbr_devs; d++){
      if ( tx_list[d].packet != NULL ) {
        free(tx_list[d].packet);
      }
    }
    free(tx_l_c.tx_list);
    free(tx_l_c.used);
  }
}

/**
 * Register a tx which has just been initiated by a device
 * Note that the tx itself does not start yet (when that happens txl_activate() should be called)
 */
void txl_register(uint d, p2G4_tx_t *tx_s, uint8_t* packet){
  tx_l_c.used[d] = 0;
  memcpy(&(tx_list[d].tx_s), tx_s, sizeof(p2G4_tx_t) );
  tx_list[d].packet = packet;
}

/**
 * Activate a given tx in the tx_list_c (we have reached the begining of the Tx)
 */
void txl_activate(uint d){
  tx_l_c.used[d] = 1;
  tx_l_c.ctr++;
}

/**
 * A given tx has just ended
 */
void txl_clear(uint d){
  tx_l_c.used[d] = 0;
  if (tx_list[d].packet != NULL) {
    free(tx_list[d].packet);
    tx_list[d].packet = NULL;
  }
  tx_l_c.ctr++;
}
