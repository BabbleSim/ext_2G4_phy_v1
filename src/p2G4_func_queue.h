/*
 * Copyright 2019 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef P2G4_FUNC_QUEUE_H
#define P2G4_FUNC_QUEUE_H

#include "bs_types.h"

#ifdef __cplusplus
extern "C"{
#endif

typedef void (*queable_f)(uint dev_nbr);

/**
 * Functions (types of events) which can be queued
 * Ordered by priority: last to first */
typedef enum {
  State_None = 0,
  Wait_Done,
  Tx_End,
  Tx_Packet_End,
  RSSI_Meas,
  Rx_Found,
  Rx_Sync,
  Rx_Header,
  Rx_Payload,
  Tx_Abort_Reeval,
  Tx_Start,
  Tx_Packet_Start,
  Rx_Search_reeval,
  Rx_Search_start,
  Rx_CCA_meas,
  N_funcs //Minor issue: one too many
} f_index_t;
//Note: We need to use these indexes, instead of just keeping the function pointers
//to be able to set the order between the functions

/* Queue element time */
typedef struct {
  bs_time_t time; /* Simualted time when the call should be done */
  f_index_t f_index; /* Function type to be called */
} fq_element_t;

/**
 * @brief Initialize the function queue
 *
 * @param n_devs Number of devices we are connected to
 */
void fq_init(uint32_t n_devs);

/**
 * Register which function will be called for a type of event
 *
 * @param index Type of event/function for which to register
 * @param fptr Function pointer to call when that event is due
 */
void fq_register_func(f_index_t index, queable_f fptr);

/**
 * Add (modify) an entry in the queue for a given device
 *
 * @param time When is the function meant to be run
 * @parm index Which function to run
 * @param dev_nbr For which device interface
 */
void fq_add(bs_time_t time, f_index_t index, uint32_t dev_nbr);

/**
 * Get the simulated time, in microseconds, of the next scheduled function
 */
bs_time_t fq_get_next_time();

/**
 * Call the next function in the queue
 * Note: The function itself is left in the queue.
 */
void fq_call_next();

/**
 * Find and update the next function which should be executed
 */
void fq_find_next();

/**
 * Remove whichever entry may be queued for this interface
 * (and find the next one)
 *
 * It is safe to call it on an interface which does not have anything queued
 *
 * Note that it is the responsibility of the user to either update an entry
 * after it has triggered, or to remove it. Otherwise the same entry will
 * stay on the top, being the next one all the time.
 *
 * @param dev_nbr Which device interface
 */
void fq_remove(uint32_t dev_nbr);

/**
 * Free resources allocated by the function queue
 *
 * This must be called before exiting to clean up any memory allocated by the
 * queue
 */
void fq_free();

#ifdef __cplusplus
}
#endif

#endif
