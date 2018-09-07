/*
 * Copyright 2018 Oticon A/S
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

/* Ordered by priority: last to first */
typedef enum {
  None = 0,
  Wait_Done,
  Tx_End,
  RSSI_Meas,
  Rx_Search,
  Rx_Sync,
  Rx_Header,
  Rx_Payload,
  Tx_Abort_Reeval,
  Tx_Start,
  N_funcs
} f_index_t;

//Note: We need to use these indexes, instead of just keeping the function pointers
//to be able to set the order between the functions

typedef struct {
  bs_time_t time;
  f_index_t f_index;
} fq_element_t;

void fq_init(uint32_t n_devs);
void fq_register_func(f_index_t index, queable_f fptr);
void fq_add(bs_time_t time, f_index_t index, uint32_t dev_nbr);
bs_time_t fq_get_next_time();
void fq_call_next();
void fq_remove(uint32_t dev_nbr);
void fq_free();

#ifdef __cplusplus
}
#endif

#endif
