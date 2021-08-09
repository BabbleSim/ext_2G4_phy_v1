/*
 * Copyright 2018 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef P2G4_ARGS_H
#define P2G4_ARGS_H

#include "bs_types.h"
#include "bs_cmd_line_typical.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct{
  unsigned int n_devs;
  ARG_S_ID
  ARG_P_ID
  bs_time_t sim_length;
  bool dont_dump;
  bool dump_imm;
  bool compare;
  bool stop_on_diff;
  ARG_VERB
  ARG_SEED

  char **channel_argv;
  char **defmodem_argv;
  uint channel_argc;
  uint defmodem_argc;
  char *channel_name;
  char *defmodem_name;

  char **modem_name;
  char ***modem_argv;
  uint *modem_argc;
} p2G4_args_t;

void p2G4_argsparse(int argc, char *argv[], p2G4_args_t *args);

void p2G4_clear_args_struct(p2G4_args_t *args);
#ifdef __cplusplus
}
#endif

#endif
