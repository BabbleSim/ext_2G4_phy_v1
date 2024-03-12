/*
 * Copyright 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef P2G4_MODEM_IF_TYPES_H
#define P2G4_MODEM_IF_TYPES_H

#include "bs_types.h"
#include "bs_pc_2G4_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Modem configuration parameters as passed to the modem models
 */
typedef struct __attribute__ ((packed)) {
  /* One of P2G4_MOD_* */
  p2G4_modulation_t modulation;
  /* Carrier frequency */
  p2G4_freq_t  center_freq;
  /* Coding rate */
  uint16_t coding_rate;
} p2G4_modemdigparams_t;

#ifdef __cplusplus
}
#endif

#endif /* P2G4_MODEM_IF_TYPES_H */
