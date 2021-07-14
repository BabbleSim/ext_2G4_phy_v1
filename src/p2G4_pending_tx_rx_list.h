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

/**
 * For each device, current/next transmission parameters and packet
 */
typedef struct {
  p2G4_tx_t tx_s;
  uint8_t *packet;
} tx_el_t;

/**
 * Transmission list container
 */
typedef struct{
  uint64_t ctr; //Counter: every time the tx list changes this counter is updated
  tx_el_t *tx_list; //Array of transmission parameters with one element per device
  uint *used; //Array with one element per device: Is it currently transmitting or not
} tx_l_c_t;

/**
 * Allocate whatever the Txlist requires
 *
 * @param n_devs Number of device interfaces
 */
void txl_create(uint n_devs);

/**
 * Free any resources used by the Tx List
 * (To be called before exiting)
 */
void txl_free(void);

/**
 * Register a new transmission for a given device/interface
 *
 * Note that registering a transmission does not mean the transmission is yet
 * active (registering can be done arbitrarily earlier than the actual start)
 * For the transmission to be "active" txl_activate() must be called
 *
 * @param dev_nbr Device which will transmit
 * @param tx_s Transmission parameters
 * @param packet Pointer to the transmitted packet
 */
void txl_register(uint dev_nbr, p2G4_tx_t *tx_s, uint8_t* packet);

/**
 * Remove a transmission from the list
 *
 * @param dev_nbr Device whos transmission is to be removed
 */
void txl_clear(uint dev_nbr);

/**
 * Set a transmission as active (currently transmitting)
 *
 * Note that txl_register() must be called first to record the actual
 * transmission parameters and packet
 *
 * @param dev_nbr Device which has just started transmitting
 */
void txl_activate(uint dev_nbr);

/**
 * Reception state
 */
typedef enum {
    Rx_State_NotSearching = 0, //Not actively searching for any tx
    Rx_State_Searching,//Searching for a fitting tx
} rx_state_t;

/**
 * Reception status (per device interface)
 */
typedef struct {
  uint8_t bpus; //bits per us
  bs_time_t scan_end; //Last us (included) in which we will scan
  bs_time_t sync_end; //Last us (included) in which the preamble + address ends
  bs_time_t header_end; //Last us (included) in which the header ends
  bs_time_t payload_end; //Last us (included) in which the payload ends
  p2G4_rx_t rx_s; //Reception request parameters
  p2G4_rx_done_t rx_done_s; //Response message (being prepared)
  int tx_nbr; //If we found a fitting Tx, which is its device number
  uint biterrors;
  rx_state_t state;
} rx_status_t;

#ifdef __cplusplus
}
#endif

#endif
