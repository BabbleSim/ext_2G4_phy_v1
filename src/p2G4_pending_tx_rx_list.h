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
 * Find if there is a fitting Tx for a given Rx attempt
 * if there is not, return -1
 * if there is, return the device number
 *
 * @param rx_s Reception parameters
 * @param current_time Current simulation time (a transmission must be starting
 *                     in this exact microsecond to match
 */
int txl_find_fitting_tx(p2G4_rx_t *rx_s, bs_time_t current_time);


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
} rx_status_t;

#ifdef __cplusplus
}
#endif

#endif
