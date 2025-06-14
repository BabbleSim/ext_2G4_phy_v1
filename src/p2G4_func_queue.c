/*
 * Copyright 2018 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "p2G4_func_queue.h"
#include "bs_oswrap.h"
#include "bs_tracing.h"

/*
 * Array with one element per device interface
 * Each interface can have 1 function pending
 */
static bs_time_t *f_queue_time = NULL;
static f_index_t *f_queue_f_index = NULL;

static uint32_t next_d = 0;
static uint32_t n_devs = 0;

static queable_f fptrs[N_funcs];

void fq_init(uint32_t n_dev){
  f_queue_time = bs_calloc(n_dev, sizeof(bs_time_t));
  f_queue_f_index = bs_calloc(n_dev, sizeof(f_index_t));
  n_devs = n_dev;

  for (int i = 0 ; i < n_devs; i ++) {
    f_queue_time[i] = TIME_NEVER;
    f_queue_f_index[i] = State_None;
  }
}

void fq_register_func(f_index_t type, queable_f fptr) {
  fptrs[type] = fptr;
}

/**
 * Find the next function which should be executed,
 * Based on the following order, from left to right:
 *  time (lower first), function index (higher first), device number (lower first)
 * TOOPT: The whole function queue is implemented in a simple/naive way,
 *        which is perfectly fine for simulations with a few devices.
 *        But, if there is many devices, this would be quite slow.
 */
void fq_find_next(){
  bs_time_t chosen_f_time;
  next_d = 0;
  chosen_f_time = f_queue_time[0];

  for (int i = 1; i < n_devs; i ++) {
    if (f_queue_time[i] > chosen_f_time) {
      continue;
    } else if (f_queue_time[i] < chosen_f_time) {
      next_d = i;
      chosen_f_time = f_queue_time[i];
      continue;
    } else if (f_queue_time[i] == chosen_f_time) {
      if (f_queue_f_index[i] > f_queue_f_index[next_d]) {
        next_d = i;
        continue;
      }
    }
  }
}

/**
 * Add a function for dev_nbr to the queue
 */
void fq_add(bs_time_t time, f_index_t index, uint32_t dev_nbr) {
  f_queue_time[dev_nbr] = time;
  f_queue_f_index[dev_nbr] = index;
}

/**
 * Remove an element from the queue and reorder it
 */
void fq_remove(uint32_t d){
  f_queue_f_index[d] = State_None;
  f_queue_time[d] = TIME_NEVER;
}

/**
 * Call the next function in the queue
 * Note: The function itself is left in the queue.
 */
void fq_call_next(){
  fptrs[f_queue_f_index[next_d]](next_d);
}

/**
 * Get the time of the next element of the queue
 */
bs_time_t fq_get_next_time(){
  return f_queue_time[next_d];
}

void fq_free(){
  if (f_queue_time != NULL) {
    free(f_queue_time);
    f_queue_time = NULL;
  }
  if (f_queue_f_index != NULL) {
    free(f_queue_f_index);
    f_queue_f_index = NULL;
  }
}
