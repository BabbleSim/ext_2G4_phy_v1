/*
 * Copyright 2018 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * Wrapper to the channel and modems
 *
 * It provides functions to initialize and delete the channel and modem
 * and to calculate bit errors, if a packet is sync'ed, and to do RSSI measurements:
 *
 * This module provides:
 *  channel_and_modem_init() : set everything up
 *  channel_and_modem_delete(): tear everything down
 *
 *  chm_is_packet_synched(): Is the modem able to synchronize a packet or not
 *  chm_bit_errors(): how many bit errors there is while receiving a given micros of a packet
 *  chm_RSSImeas(): Return a RSSI measurement for a given modem
 *
 * It interfaces with a channel (library) and a set of modems (libraries)
 * One channel will be loaded for all links (the channel shall keep the status of NxN links)
 * N modems will be loaded (one for each receiver). Each modem may be of the same type or different type
 *
 * For details on the modem and channel interfaces see modem_if.h and channel_if.h
 */

//#define DONTCLOSELIBRARIES

#include <dlfcn.h>
#include <math.h>
#include <limits.h>
#include "bs_types.h"
#include "bs_tracing.h"
#include "bs_oswrap.h"
#include "bs_pc_2G4.h"
#include "bs_pc_2G4_utils.h"
#include "bs_rand_main.h"
#include "bs_rand_inline.h"
#include "p2G4_channel_and_modem_priv.h"
#include "p2G4_dump.h"
#include "p2G4_pending_tx_rx_list.h"

static uint n_devs;

static void *channel_lib = NULL;
static void **modem_lib  = NULL;

typedef int  (*cha_init_f)(int argc, char *argv[], uint n_devs);
typedef int  (*cha_calc_f)(const uint *tx_used, tx_el_t *tx_list, uint txnbr, uint rxnbr, bs_time_t now, double *att, double *ISI_SNR);
typedef void (*cha_delete_f)();

static cha_init_f   channel_init;
static cha_calc_f   channel_calc;
static cha_delete_f channel_delete;

typedef void*  (*m_init_f)(int argc, char *argv[], uint dev_nbr, uint n_devs);
typedef void   (*m_delete_f)(void *m_obj);
typedef void   (*m_analog_rx_f)(void *m_obj, p2G4_radioparams_t *radio_params, double *OutputSNR,double *Output_RSSI_power_level, double *rx_pow, tx_l_c_t *txl_c, uint tx_nbr);
typedef uint32_t (*m_dig_perf_sync_f)(void *m_obj, p2G4_radioparams_t *radio_params, double SNR, p2G4_txv2_t* tx_s);
typedef uint32_t (*m_dig_perf_ber_f)(void *m_obj, p2G4_radioparams_t *radio_params, double SNR);
typedef uint32_t (*m_dig_RSSI_f)(void *m_obj, p2G4_radioparams_t *radio_params, double RSSI_power_level, p2G4_rssi_power_t* RSSI);

static m_init_f          *m_init      = NULL;
static m_delete_f        *m_delete    = NULL;
static m_analog_rx_f     *m_analog_rx = NULL;
static m_dig_perf_sync_f *m_dig_perf_sync = NULL;
static m_dig_perf_ber_f  *m_dig_perf_ber  = NULL;
static m_dig_RSSI_f      *m_dig_RSSI      = NULL;

static void** modem_o = NULL; //Array of modem objects: one for each device/receiver

// status of each receiver (its receiver chain and the channel fading towards it from all paths)
static rec_status_t *rec_status;

void channel_and_modem_init(uint ch_argc, char** ch_argv, const char* ch_name, uint *mo_argc, char*** mo_argv, char** mo_name, uint n_devs_i){

   char *error;
   uint d;

   n_devs = n_devs_i;

   rec_status = (rec_status_t*) bs_calloc(n_devs, sizeof(rec_status_t));
   for (d = 0; d < n_devs; d ++){
     rec_status[d].att = (double*) bs_calloc(n_devs, sizeof(double));
     rec_status[d].rx_pow = (double*) bs_calloc(n_devs, sizeof(double));
   }

   //CHANNEL:
   char ch_lib_name[1024];
   snprintf(ch_lib_name,1024,"../lib/lib_2G4Channel_%s.so",ch_name);

   channel_lib = dlopen (ch_lib_name, RTLD_NOW);
   if (!channel_lib) {
     bs_trace_error_line("%s\n",dlerror());
   }
   if ((error = dlerror()) != NULL) {
     bs_trace_error_line("%s\n",error);
   }

   /* Look into the man pages for an explanation on the *(void**) & syntax */
   *(void **) (&channel_init) = dlsym(channel_lib, "channel_init");
   if ((error = dlerror()) != NULL) {
     bs_trace_error_line("%s\n",error);
   }

   *(void **) (&channel_delete) = dlsym(channel_lib, "channel_delete");
   if ((error = dlerror()) != NULL) {
     bs_trace_error_line("%s\n",error);
   }

   *(void **) (&channel_calc) = dlsym(channel_lib, "channel_calc");
   if ((error = dlerror()) != NULL) {
     bs_trace_error_line("%s\n",error);
   }

   channel_init(ch_argc, ch_argv, n_devs);

   //MODEM:
   modem_lib = bs_calloc(n_devs, sizeof(void*));
   m_init = (m_init_f*) bs_calloc(n_devs, sizeof(m_init_f));
   m_delete = (m_delete_f*) bs_calloc(n_devs, sizeof(m_delete_f));
   m_analog_rx = (m_analog_rx_f*) bs_calloc(n_devs, sizeof(m_analog_rx_f));
   m_dig_perf_sync = (m_dig_perf_sync_f*) bs_calloc(n_devs, sizeof(m_dig_perf_sync_f));
   m_dig_perf_ber = (m_dig_perf_ber_f*) bs_calloc(n_devs, sizeof(m_dig_perf_ber_f));
   m_dig_RSSI = (m_dig_RSSI_f*) bs_calloc(n_devs, sizeof(m_dig_RSSI_f));
   modem_o = bs_calloc(n_devs, sizeof(void*));

   for (d = 0; d < n_devs; d++) {
     char mo_lib_name[1024];
     snprintf(mo_lib_name,1024,"../lib/lib_2G4Modem_%s.so",mo_name[d]);
     modem_lib[d] = dlopen (mo_lib_name, RTLD_NOW);
     if (!modem_lib) {
       bs_trace_error_line("%s\n",dlerror());
     }
     if ((error = dlerror()) != NULL) {
       bs_trace_error_line("%s\n",error);
     }
     //load functions
     *(void **) (&(m_init[d])) = dlsym(modem_lib[d], "modem_init");
     if ((error = dlerror()) != NULL) {
       bs_trace_error_line("%s\n",error);
     }

     *(void **) (&(m_delete[d])) = dlsym(modem_lib[d], "modem_delete");
     if ((error = dlerror()) != NULL) {
       bs_trace_error_line("%s\n",error);
     }

     *(void **) (&(m_analog_rx[d])) = dlsym(modem_lib[d], "modem_analog_rx");
     if ((error = dlerror()) != NULL) {
       bs_trace_error_line("%s\n",error);
     }

     *(void **) (&(m_dig_perf_sync[d])) = dlsym(modem_lib[d], "modem_digital_perf_sync");
     if ((error = dlerror()) != NULL) {
       bs_trace_error_line("%s\n",error);
     }

     *(void **) (&(m_dig_perf_ber[d])) = dlsym(modem_lib[d], "modem_digital_perf_ber");
     if ((error = dlerror()) != NULL) {
       bs_trace_error_line("%s\n",error);
     }
     *(void **) (&(m_dig_RSSI[d])) = dlsym(modem_lib[d], "modem_digital_RSSI");
     if ((error = dlerror()) != NULL) {
       bs_trace_error_line("%s\n",error);
     }

     //initialize modem for this device
     modem_o[d] = m_init[d](mo_argc[d], mo_argv[d], d, n_devs);
   }
}

void channel_and_modem_delete(){
  uint d;

  if ( rec_status != NULL ) {
    for (d = 0; d < n_devs; d ++){
      free(rec_status[d].att);
      free(rec_status[d].rx_pow);
    }
    free(rec_status);
  }

  if (modem_lib != NULL) {
    for (d = 0 ; d < n_devs; d ++){
      if ( modem_lib[d] != NULL ) {
        if ( m_delete[d] )  {
          m_delete[d](modem_o[d]);
        }
#ifndef DONTCLOSELIBRARIES
        dlclose(modem_lib[d]);
#endif
      }
    }
    free(modem_lib);

    if ( m_init != NULL )
      free(m_init);
    if ( m_delete != NULL )
      free(m_delete);
    if ( m_dig_perf_sync != NULL )
      free(m_dig_perf_sync);
    if ( m_dig_perf_ber != NULL )
      free(m_dig_perf_ber);
    if ( m_dig_RSSI != NULL )
      free(m_dig_RSSI);
    if (m_analog_rx != NULL )
      free(m_analog_rx);

    if ( modem_o != NULL)
      free(modem_o);
  }

  if (channel_lib != NULL) {
    channel_delete();
#ifndef DONTCLOSELIBRARIES
    dlclose(channel_lib);
#endif
  }
}

/**
 * Calculate the received power ("at the antenna connector") for a given <rx_nbr> from each transmitter (it will be stored in rec_s->rx_pow[*])
 * Calculate also the ISI for the desired <tx_nbr> (if tx_nbr == UINT_MAX it wont be calculated) (it will be stored in rec_s->SNR_ISI)
 */
static inline void CalculateRxPowerAndISI(tx_l_c_t *tx_l, rec_status_t *rec_s, p2G4_power_t ant_gain, uint tx_nbr, uint rx_nbr, bs_time_t current_time){
  uint i;

  channel_calc(tx_l->used, tx_l->tx_list, tx_nbr, rx_nbr, current_time, rec_s->att, &rec_s->SNR_ISI);

  for ( i = 0; i < n_devs ; i++ ){
    if ( tx_l->used[i] ){
      rec_s->rx_pow[i] = p2G4_power_to_d(tx_l->tx_list[i].tx_s.power_level) - rec_s->att[i] + p2G4_power_to_d(ant_gain);
    }
  }
}

static inline void combine_SNR(rec_status_t *rx_status ) {
  //eventually we may want to add a Tx SNR
  double N_ana = pow(10,-rx_status->SNR_analog_o/10);
  double N_ISI = pow(10,-rx_status->SNR_ISI/10);
  double N_total = N_ana + N_ISI; //we assume the noise sources are uncorrelated gausian noise
  rx_status->SNR_total = -10*log10(N_total);
}

/**
 * Return the number of biterrors while receiving this microsecond of the packet sent by device <tx_nbr>
 * and received by device <rx_nbr>.
 * Where <n_calcs> error samples are taken, each with the same parameters
 */
uint chm_bit_errors(tx_l_c_t *tx_l, uint tx_nbr, uint rx_nbr, p2G4_rxv2_t *rx_s , bs_time_t current_time, uint n_calcs){

  rec_status_t *status = &rec_status[rx_nbr];

  if ( ( status->rx_ctr != status->last_rx_ctr ) ||
      ( status->last_tx_ctr != tx_l->ctr ) ) { //If the activity in the channel hasn't changed (same Tx and Rx as last time we recalculated), we dont need to recalculate the channel conditions (most of the time we are going to reevaluate just 1us later == nothing changed
    status->last_tx_ctr = tx_l->ctr;
    status->last_rx_ctr = status->rx_ctr;

    //we need to recalculate things
    CalculateRxPowerAndISI(tx_l, status, rx_s->antenna_gain, tx_nbr, rx_nbr, current_time);

    m_analog_rx[rx_nbr](modem_o[rx_nbr], &rx_s->radio_params, &status->SNR_analog_o, &status->RSSI_meas_power, status->rx_pow, tx_l, tx_nbr);

    combine_SNR(status);

    status->BER = m_dig_perf_ber[rx_nbr](modem_o[rx_nbr], &rx_s->radio_params, status->SNR_total);

    dump_ModemRx(current_time, tx_nbr, rx_nbr, n_devs, 1, &rx_s->radio_params, status, tx_l );
  } //otherwise all we had calculated before still applies


  uint bit_errors = 0;
  for (uint ctr = 0; ctr < n_calcs ; ctr++){
    bit_errors += bs_random_Bern(status->BER);
  }
  return bit_errors;
}

/**
 * Is the packet sent by device <tx_nbr> correctly synchronized (not accounting for bit errors) by the receiver <rx_nbr>
 */
uint chm_is_packet_synched(tx_l_c_t *tx_l, uint tx_nbr, uint rx_nbr, p2G4_rxv2_t *rx, bs_time_t current_time){

  rec_status_t *rec_s = &rec_status[rx_nbr];

  { //Every time we are asked to check if a packet was sync'ed
    //we recall the channel fader and recalculate the analog
    //(we will be called only once per packet to check if it was sync'ed =>
    //for sure we will have a different Tx list of tx_nbr, also the rx moment could be quite far apart
    //after that, while the Rx is ongoing we assume we are close in time enough to not need to recalculate the fading
    //and therefore we will only recalculate things if we get changes in which devices which are transmitting
    rec_s->last_tx_ctr = tx_l->ctr;
    rec_s->rx_ctr = rec_s->rx_ctr + 1;
    rec_s->last_rx_ctr = rec_s->rx_ctr;

    CalculateRxPowerAndISI(tx_l, rec_s, rx->antenna_gain, tx_nbr, rx_nbr, current_time);

    m_analog_rx[rx_nbr](modem_o[rx_nbr], &rx->radio_params, &rec_s->SNR_analog_o,
                        &rec_s->RSSI_meas_power, rec_s->rx_pow, tx_l, tx_nbr);

    combine_SNR(rec_s);

    rec_s->BER = m_dig_perf_ber[rx_nbr](modem_o[rx_nbr], &rx->radio_params, rec_s->SNR_total);
    rec_s->sync_prob = m_dig_perf_sync[rx_nbr](modem_o[rx_nbr], &rx->radio_params, rec_s->SNR_total, &tx_l->tx_list[tx_nbr].tx_s);

    dump_ModemRx(current_time, tx_nbr, rx_nbr, n_devs, 0, &rx->radio_params, rec_s, tx_l );
  }

  return bs_random_Bern(rec_s->sync_prob);
}

/**
 * What RSSI power will the device <rx_nbr> measure in this instant
 */
void chm_RSSImeas(tx_l_c_t *tx_l, p2G4_power_t rx_antenna_gain, p2G4_radioparams_t *rx_radio_params , p2G4_rssi_done_t* RSSI_meas, uint rx_nbr, bs_time_t current_time){
  rec_status_t *rec_s = &rec_status[rx_nbr];
  p2G4_rssi_power_t RSSI;

  CalculateRxPowerAndISI(tx_l, rec_s, rx_antenna_gain, UINT_MAX, rx_nbr, current_time);

  m_analog_rx[rx_nbr](modem_o[rx_nbr], rx_radio_params,
                      &rec_s->SNR_analog_o, &rec_s->RSSI_meas_power,
                      rec_s->rx_pow, tx_l, UINT_MAX);

  m_dig_RSSI[rx_nbr](modem_o[rx_nbr], rx_radio_params,
                     rec_s->RSSI_meas_power, &RSSI);
  RSSI_meas->RSSI = RSSI;
}

