/*
 * Copyright 2018 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bs_tracing.h"
#include "bs_oswrap.h"
#include "bs_utils.h"
#include "bs_pc_2G4.h"
#include "bs_pc_2G4_utils.h"
#include "bs_rand_main.h"
#include "p2G4_args.h"
#include "p2G4_channel_and_modem.h"
#include "p2G4_dump.h"
#include "p2G4_func_queue.h"
#include "p2G4_pending_tx_rx_list.h"
#include "p2G4_com.h"
#include "p2G4_v1_v2_remap.h"

static bs_time_t current_time = 0;
static int nbr_active_devs; //How many devices are still active (devices may disconnect during the simulation)
extern tx_l_c_t tx_l_c; //list of all transmissions
static p2G4_rssi_t *RSSI_a; //array of all RSSI measurements
static rx_status_t *rx_a; //array of all receptions
static cca_status_t *cca_a; //array of all "compatible" searches
static p2G4_args_t args;

static void p2G4_handle_next_request(uint d);

static void f_wait_done(uint d){
  bs_trace_raw_time(8,"Device %u - Wait done\n", d);
  p2G4_phy_resp_wait(d);
  p2G4_handle_next_request(d);
}

static void f_tx_end(uint d){

  tx_el_t* tx_el;
  p2G4_tx_done_t tx_done_s;

  tx_el = &tx_l_c.tx_list[d];
  if ( tx_el->tx_s.abort.abort_time < tx_el->tx_s.end_tx_time ) {
    bs_trace_raw_time(8,"Device %u - Tx done (Tx aborted)\n", d);
  } else {
    bs_trace_raw_time(8,"Device %u - Tx done (Tx ended)\n", d);
  }

  dump_tx(tx_el, d);

  txl_clear(d);

  tx_done_s.end_time = current_time;
  p2G4_phy_resp_tx(d, &tx_done_s);
  p2G4_handle_next_request(d);
}


static int pick_abort_tail(uint d, p2G4_abort_t *ab, const char* type) {
  if ( current_time > ab->abort_time ){
    bs_trace_error_time_line("Device %u requested %s abort in %"PRItime" which has passed\n", d, type, ab->abort_time);
  }
  if ( current_time > ab->recheck_time ){ //we allow to recheck just now
    bs_trace_error_time_line("Device %u requested %s abort recheck in %"PRItime" which has passed\n", d, type, ab->recheck_time);
  }
  if ( current_time == ab->recheck_time ){
    bs_trace_raw_time(4,"Device %u - Note: Abort reevaluation in same time (possible infinite loop in device?)\n", d);
    /* In this case it may be better to call here pick_and_validate_abort() recursively
     * As some states may not handle too gracefully this case and still advance 1 microsecond */
  }

  bs_trace_raw_time(8,"Device %u (%s) requested to reschedule abort,recheck at %"PRItime ",%"PRItime ")\n",
                    d, type, ab->abort_time, ab->recheck_time);

  return 0;
}
/*
 * Pick abort from device.
 * If something goes wrong it returns != 0, and the simulation shall be terminated
 */
static int pick_and_validate_abort(uint d, p2G4_abort_t *ab, const char* type) {
  int ret;

  bs_trace_raw_time(8,"Device %u - Reevaluating %s abort\n", d, type);

  do {
    ret = p2G4_phy_get_new_abort(d, ab);
    if ( ret == PB_MSG_TERMINATE) {
      bs_trace_raw_time(4,"Device %u terminated the simulation (there was %i left)\n", d, nbr_active_devs-1);
      nbr_active_devs = 0;
      return 1;
    } else if ( ret == P2G4_MSG_RERESP_IMMRSSI ) {
      p2G4_rssi_t rssi_req;
      p2G4_rssi_done_t rssi_resp;

      p2G4_phy_get(d, &rssi_req, sizeof(rssi_req));
      chm_RSSImeas(&tx_l_c, rssi_req.antenna_gain, &rssi_req.radio_params, &rssi_resp, d, current_time);
      p2G4_phy_resp_IMRSSI(d, &rssi_resp);
    }
  } while (ret != 0);

  return pick_abort_tail(d, ab, type);
}

/*
 * Pick abort from device after RXV2CONT (during header evaluation)
 * If something goes wrong it returns != 0, and the simulation shall be terminated
 */
static int pick_and_validate_abort_Rxcont(uint d, p2G4_abort_t *ab) {

  bs_trace_raw_time(8,"Device %u - Picking abort during header eval\n", d);

  p2G4_phy_get_abort_struct(d, ab);

  return pick_abort_tail(d, ab, "Rx header");
}

/**
 * Check if this Tx and Rx match
 * and return true if found, false otherwise
 */
static bool tx_and_rx_match(const p2G4_txv2_t *tx_s, rx_status_t *rx_st)
{
  if ((tx_s->radio_params.center_freq == rx_st->rx_s.radio_params.center_freq) &&
     (tx_s->radio_params.modulation & P2G4_MOD_SIMILAR_MASK) ==
         (rx_st->rx_s.radio_params.modulation & P2G4_MOD_SIMILAR_MASK) )
  {
    bs_time_t chopped_preamble = current_time - tx_s->start_packet_time; /*microseconds of preamble not transmitted */

    if (chopped_preamble > rx_st->rx_s.acceptable_pre_truncation) {
      return false; //we already lost too much preamble, we can't sync
    }

    /* Let's check if it is any of the addresses the Rx searches for */
    p2G4_address_t *rx_addr = rx_st->phy_address;
    p2G4_address_t tx_addr = tx_s->phy_address;

    for (int i = 0; i < rx_st->rx_s.n_addr; i++) {
      if ( tx_addr == rx_addr[i] ) {
        return true;
      }
    }
  }

  return false;
}

/**
 * All devices which may be able to receive this transmission are moved
 * to the Rx_Found state, where they may start synchronizing to it
 */
static void find_and_activate_rx(const p2G4_txv2_t *tx_s, uint tx_d) {
  for (int rx_d = 0 ; rx_d < args.n_devs; rx_d++) {
    rx_status_t *rx_s = &rx_a[rx_d];
    if ( rx_s->state == Rx_State_Searching &&
        tx_and_rx_match(tx_s, rx_s) ) {
      rx_s->tx_nbr = tx_d;
      rx_s->state = Rx_State_NotSearching;
      fq_add(current_time, Rx_Found, rx_d);
    }
  }
}

static void tx_start_packet_common(p2G4_txv2_t* tx_s, uint d){
  txl_start_packet(d);
  bs_trace_raw_time(8,"Device %u - Tx packet start\n", d);
  find_and_activate_rx(tx_s, d);
}

static void tx_schedule_next_event(p2G4_txv2_t* tx_s, uint d){
  bs_time_t TxEndt, NextTime;
  bs_time_t Packet_start_time;
  bs_time_t Packet_end_time = TIME_NEVER; /*Must be later than Packet_start_time*/

  if ( (tx_l_c.used[d] & TXS_PACKET_ONGOING) == 0){
    Packet_start_time = tx_s->start_packet_time;
  } else {
    /* Packet already started => no need to schedule another start */
    Packet_start_time = TIME_NEVER;

    if (tx_l_c.used[d] & TXS_PACKET_ENDED){
      /* Packet already ended */
      Packet_end_time = TIME_NEVER;
    } else {
      Packet_end_time = tx_s->end_packet_time;
    }
  }

  TxEndt = BS_MIN(tx_s->end_tx_time, tx_s->abort.abort_time);
  NextTime = BS_MIN(BS_MIN(Packet_start_time, TxEndt),Packet_end_time);

  if ( NextTime >= tx_s->abort.recheck_time ){
    fq_add(tx_s->abort.recheck_time, Tx_Abort_Reeval, d);
  } else if ( NextTime == Packet_start_time ) {
    fq_add(TxEndt, Tx_Packet_Start, d);
  } else if ( NextTime == TxEndt ) {
    // If end_packet_time and end_tx_time are the same we just end
    fq_add(TxEndt, Tx_End, d);
  } else {
    fq_add(TxEndt, Tx_Packet_End, d);
  }
}

static void f_tx_start(uint d) {
  p2G4_txv2_t* tx_s;
  tx_s = &tx_l_c.tx_list[d].tx_s;

  txl_start_tx(d);
  bs_trace_raw_time(8,"Device %u - Tx start\n", d);

  if ( current_time >= tx_s->start_packet_time) {
    tx_start_packet_common(tx_s, d);
    /* Technically we could let another event be scheduled,
     * but this saves a bit of time for the most common scenario */
  }

  tx_schedule_next_event(tx_s, d);
}

static void f_tx_abort_reeval(uint d){
  p2G4_txv2_t* tx_s;

  tx_s = &tx_l_c.tx_list[d].tx_s;

  if (pick_and_validate_abort(d, &(tx_s->abort), "Tx"))
    return;

  tx_schedule_next_event(tx_s, d);
}

static void f_tx_packet_start(uint d){
  p2G4_txv2_t* tx_s;
  tx_s = &tx_l_c.tx_list[d].tx_s;

  tx_start_packet_common(tx_s, d);
  tx_schedule_next_event(tx_s, d);
}

static void f_tx_packet_end(uint d){
  p2G4_txv2_t* tx_s;
  tx_s = &tx_l_c.tx_list[d].tx_s;

  bs_trace_raw_time(8,"Device %u - Tx packet end\n", d);
  txl_end_packet(d);
  tx_schedule_next_event(tx_s, d);
}


static void f_RSSI_meas(uint d) {
  p2G4_rssi_done_t RSSI_meas;

  chm_RSSImeas(&tx_l_c, RSSI_a[d].antenna_gain, &RSSI_a[d].radio_params, &RSSI_meas, d, current_time);

  dump_RSSImeas(&RSSI_a[d], &RSSI_meas, d);

  bs_trace_raw_time(8,"RSSIDone for device %u\n", d);

  p2G4_phy_resp_RSSI(d, &RSSI_meas);
  p2G4_handle_next_request(d);
}

static void rx_do_RSSI(uint d) {
  chm_RSSImeas(&tx_l_c, rx_a[d].rx_s.antenna_gain, &rx_a[d].rx_s.radio_params, &rx_a[d].rx_done_s.rssi, d, current_time);
}

/**
 * Schedule an Rx_Search_reeval event when the search scan ends or
 * the next check of the abort reeval
 */
static inline void rx_enqueue_search_reeval(uint d){
  rx_status_t *rx_status = &rx_a[d];

  rx_status->state = Rx_State_Searching;
  bs_time_t end_time = BS_MIN((rx_status->scan_end + 1), rx_status->rx_s.abort.recheck_time);
  fq_add(end_time, Rx_Search_reeval, d);
  return;
}

/**
 * Find if there is a fitting tx for this rx attempt
 * if there is not, return -1
 * if there is, return the device number
 */
static int find_fitting_tx(rx_status_t *rx_s){
  register uint *used = tx_l_c.used;
  int max_tx = txl_get_max_tx_nbr();
  for (int i = 0 ; i <= max_tx; i++) {
    if ((used[i] & TXS_PACKET_ONGOING) &&
        tx_and_rx_match(&tx_l_c.tx_list[i].tx_s, rx_s) )
    {
      return i;
    }
  }
  return -1;
}

static void f_rx_found(uint d);

static void rx_possible_abort_recheck(uint d, rx_status_t *rx_st, bool scanning){
  if ( current_time >= rx_st->rx_s.abort.recheck_time ) {
    if ( pick_and_validate_abort(d, &(rx_a[d].rx_s.abort), "Rx") != 0 ){
      return;
    }
    if (scanning && (rx_st->rx_s.abort.abort_time < rx_st->scan_end) ) {
        rx_st->scan_end = rx_st->rx_s.abort.abort_time - 1;
    }
  }
}

static void f_rx_search_start(uint d) {
  int tx_d;
  rx_status_t *rx_status;
  rx_status = &rx_a[d];

  rx_possible_abort_recheck(d, rx_status, true);

  bs_trace_raw_time(8,"Device %u - Starting Rx\n", d);

  /* Let's check for possible ongoing transmissions we may still catch */
  tx_d = find_fitting_tx(rx_status);

  if (tx_d >= 0) {
    rx_status->tx_nbr = tx_d;
    rx_status->state = Rx_State_NotSearching;
    /* Let's call it directly and save an event */
    f_rx_found(d);
    return;
  }

  /* If we haven't found anything, we just schedule a new event to wait until the end */
  rx_enqueue_search_reeval(d);
  return;
}

static void rx_respond_done(uint d, rx_status_t *rx_status) {
  if ( rx_status->v1_request ) {
    p2G4_rx_done_t rx_done_v1;
    map_rxv2_resp_to_rxv1(&rx_done_v1, &rx_status->rx_done_s);
    p2G4_phy_resp_rx(d, &rx_done_v1);
  } else {
    p2G4_phy_resp_rxv2(d, &rx_status->rx_done_s);
  }
}

static void rx_resp_addr_found(uint d,  rx_status_t *rx_status, uint8_t *packet) {
  if ( rx_status->v1_request ) {
    p2G4_rx_done_t rx_done_v1;
    map_rxv2_resp_to_rxv1(&rx_done_v1, &rx_status->rx_done_s);
    p2G4_phy_resp_rx_addr_found(d, &rx_done_v1, packet);
  } else {
    p2G4_phy_resp_rxv2_addr_found(d, &rx_status->rx_done_s, packet);
  }
}

static void rx_scan_ended(uint d, rx_status_t *rx_status, const char * const state){
  rx_status->rx_done_s.packet_size = 0;
  rx_status->rx_done_s.rx_time_stamp = current_time;
  rx_status->rx_done_s.status = P2G4_RXSTATUS_NOSYNC;
  if ( rx_status->rx_s.abort.abort_time != current_time ) { //if we are not aborting
    rx_do_RSSI(d);
  }

  bs_trace_raw_time(8,"Device %u - RxDone (NoSync during %s)\n", d, state);

  rx_status->state = Rx_State_NotSearching;
  rx_status->rx_done_s.end_time = current_time;

  rx_respond_done(d, rx_status);
  dump_rx(&rx_a[d],NULL,d);
  p2G4_handle_next_request(d);
}

static void f_rx_search_reeval(uint d) {
  rx_status_t *rx_status;
  rx_status = &rx_a[d];

  rx_possible_abort_recheck(d, rx_status, true);

  if ( current_time > rx_status->scan_end ) {
    rx_scan_ended(d, rx_status, "Rx Search");
    return;
  }

  /* If we havent found anything yet, we just continue */
  rx_enqueue_search_reeval(d);
  return;
}

static void f_rx_found(uint d){
  rx_status_t *rx_status = &rx_a[d];
  uint tx_d = rx_status->tx_nbr;

  /*
   * Improvement
   *   There is a minor issue in this design, as rx_search_start will lock to the first matching Tx
   *   (even if it has way too low power, and lose the option to find other already ongoing Tx).
   *   And similarly several simultaneously starting Tx's in the same us will always lose to the
   *   one with the bigger device number
   *   Having rx_found actually go thru all possible Tx's until it syncs to one instead of
   *   having its choice preselected would be better.
   *   (This flaw existed also in the v1 API FSM version)
   */
  if ( chm_is_packet_synched( &tx_l_c, tx_d, d,  &rx_status->rx_s, current_time ) )
  {
    p2G4_txv2_t *tx_s = &tx_l_c.tx_list[tx_d].tx_s;
    rx_status->sync_end    = tx_s->start_packet_time + BS_MAX((int)rx_status->rx_s.pream_and_addr_duration - 1,0);
    rx_status->header_end  = rx_status->sync_end + rx_status->rx_s.header_duration;
    rx_status->payload_end = tx_s->end_packet_time;
    rx_status->biterrors = 0;
    rx_status->rx_done_s.phy_address = tx_s->phy_address;
    rx_status->sync_start = tx_s->start_packet_time + rx_status->rx_s.acceptable_pre_truncation;
    bs_trace_raw_time(8,"Device %u - Matched Tx %u\n", d, tx_d);

    bs_time_t next_time = BS_MIN(rx_status->sync_start, rx_status->rx_s.abort.recheck_time);
    next_time = BS_MIN(next_time, rx_status->scan_end + 1);
    fq_add(next_time, Rx_Sync, d);
    return;
  }

  rx_enqueue_search_reeval(d);
  return;
}


static int rx_bit_error_calc(uint d, uint tx_nbr, rx_status_t *rx_st) {
  int biterrors = 0;
  rx_error_calc_state_t *st = &rx_st->err_calc_state;
  if (st->us_to_next_calc-- <= 0) {
    if ( ( tx_l_c.used[tx_nbr] & TXS_PACKET_ONGOING ) == 0 ) {
      biterrors = st->errorspercalc;
    } else {
      biterrors = chm_bit_errors(&tx_l_c, tx_nbr, d, &rx_st->rx_s, current_time, st->errorspercalc);
    }
    st->us_to_next_calc  = st->rate_uspercalc - 1;
  }
  return biterrors;
}

static void f_rx_sync(uint d){

  rx_possible_abort_recheck(d, &rx_a[d], true);

  if ( current_time > rx_a[d].scan_end ) {
    rx_scan_ended(d, &rx_a[d], "Rx sync");
    return;
  }

  if ( current_time < rx_a[d].sync_start ) {
    //we are not yet meant to try to sync, but we got here due to an abort reeval (or abort) being earlier than the sync_start
    //so we wait until we should
    bs_time_t next_time = BS_MIN(rx_a[d].sync_start,rx_a[d].rx_s.abort.recheck_time);
    next_time = BS_MIN(next_time, rx_a[d].scan_end + 1);
    fq_add(next_time, Rx_Sync, d);
    return;
  }

  if ( ( tx_l_c.used[rx_a[d].tx_nbr] & TXS_PACKET_ONGOING ) == 0 ) { //if the Tx aborted
    //An abort of the Tx during a sync results in the sync being lost always (even if we are just at the end of the syncword)
    bs_trace_raw_time(8,"Device %u - Sync lost (Tx disappeared)\n", d);
    fq_add(current_time + 1, Rx_Search_start, d); //Note that we go to start, to search again in between the ongoing transmissions
    return;
  } else {
    rx_a[d].biterrors += rx_bit_error_calc(d, rx_a[d].tx_nbr, &rx_a[d]);
  }

  if ( rx_a[d].biterrors > rx_a[d].rx_s.sync_threshold ) {
    bs_trace_raw_time(8,"Device %u - Sync lost (errors)\n", d);
    fq_add(current_time + 1, Rx_Search_start, d); //Note that we go to start, to search again in between the ongoing transmissions
    return;
  }

  if ( current_time >= rx_a[d].sync_end ) {
    //we have correctly synch'ed:
    rx_do_RSSI(d);
    rx_a[d].biterrors = 0;
    int device_accepted = 0;

    //Ask from the device if it wants us to continue..
    rx_a[d].rx_done_s.rx_time_stamp = rx_a[d].sync_end;
    rx_a[d].rx_done_s.end_time = current_time;
    rx_a[d].rx_done_s.packet_size = tx_l_c.tx_list[rx_a[d].tx_nbr].tx_s.packet_size;
    rx_a[d].rx_done_s.status = P2G4_RXSTATUS_INPROGRESS;

    bs_trace_raw_time(8,"Device %u - Sync done\n", d);
    rx_resp_addr_found(d, &rx_a[d], tx_l_c.tx_list[rx_a[d].tx_nbr].packet);

    pc_header_t header;
    header = p2G4_get_next_request(d);
    switch (header) {
    case PB_MSG_DISCONNECT:
      nbr_active_devs -= 1;
      bs_trace_raw_time(5,"Device %u disconnected during Rx (minor protocol violation) (%i left)\n", d, nbr_active_devs);
      device_accepted = 0;
      fq_remove(d);
      return;
      break;
    case PB_MSG_TERMINATE:
      bs_trace_raw_time(4,"Device %u terminated the simulation (there was %i left)\n", d, nbr_active_devs-1);
      nbr_active_devs = 0;
      return;
      break;
    case P2G4_MSG_RXCONT:
      device_accepted = 1;
      break;
    case P2G4_MSG_RXV2CONT:
      device_accepted = 1;
      if (pick_and_validate_abort_Rxcont(d, &(rx_a[d].rx_s.abort)) != 0 ){
        return;
      }
      break;
    case P2G4_MSG_RXSTOP:
      device_accepted = 0;
      break;
    default:
      bs_trace_error_line("Device %u has violated the protocol during an Rx (%u) => Terminate\n",d, header);
      break;
    }

    if (device_accepted) {
      uint delta;
      if ( rx_a[d].rx_s.pream_and_addr_duration == 0 ) {
        delta = 0; //We pretend we have not spent time here
      } else {
        delta = 1;
      }
      if ( rx_a[d].rx_s.header_duration == 0 ) {
        fq_add(current_time + delta, Rx_Payload, d);
      } else {
        fq_add(current_time + delta, Rx_Header, d);
      }
    } else {
      dump_rx(&rx_a[d], tx_l_c.tx_list[rx_a[d].tx_nbr].packet, d);
      p2G4_handle_next_request(d);
    }
    return;
  } else {
    fq_add(current_time + 1, Rx_Sync, d);
    return;
  }
}

static void f_rx_header(uint d){

  rx_possible_abort_recheck(d, &rx_a[d], false);

  rx_a[d].biterrors += rx_bit_error_calc(d, rx_a[d].tx_nbr, &rx_a[d]);


  if ( ( ( current_time >= rx_a[d].header_end )
        && ( rx_a[d].biterrors > rx_a[d].rx_s.header_threshold ) )
      ||
       ( current_time >= rx_a[d].rx_s.abort.abort_time ) ) {
    rx_a[d].rx_done_s.packet_size = 0;
    rx_a[d].rx_done_s.status = P2G4_RXSTATUS_HEADER_ERROR;
    bs_trace_raw_time(8,"Device %u - RxDone (Header error)\n", d);
    rx_a[d].rx_done_s.end_time = current_time;

    rx_respond_done(d, &rx_a[d]);
    dump_rx(&rx_a[d],NULL,d);
    p2G4_handle_next_request(d);
    return;
  } else if ( current_time >= rx_a[d].header_end ) {
    bs_trace_raw_time(8,"Device %u - Header done\n", d);
    fq_add(current_time + 1, Rx_Payload, d);
    return;
  } else {
    fq_add(current_time + 1, Rx_Header, d);
    return;
  }
}

static void f_rx_payload(uint d){

  rx_possible_abort_recheck(d, &rx_a[d], false);

  rx_a[d].biterrors += rx_bit_error_calc(d, rx_a[d].tx_nbr, &rx_a[d]);

  if (((current_time >= rx_a[d].payload_end) && (rx_a[d].biterrors > 0))
      || (current_time >= rx_a[d].rx_s.abort.abort_time)) {
    if (!args.crcerr_data) {
        rx_a[d].rx_done_s.packet_size = 0;
    }
    rx_a[d].rx_done_s.status = P2G4_RXSTATUS_PACKET_CONTENT_ERROR;
    bs_trace_raw_time(8,"Device %u - RxDone (CRC error)\n", d);
    rx_a[d].rx_done_s.end_time = current_time;
    rx_respond_done(d, &rx_a[d]);
    dump_rx(&rx_a[d],NULL,d);
    p2G4_handle_next_request(d);
    return;
  } else if ( current_time >= rx_a[d].payload_end ) {
    rx_a[d].rx_done_s.status = P2G4_RXSTATUS_OK;
    bs_trace_raw_time(8,"Device %u - RxDone (CRC ok)\n", d);
    rx_a[d].rx_done_s.end_time = current_time;
    rx_respond_done(d, &rx_a[d]);
    dump_rx(&rx_a[d],tx_l_c.tx_list[rx_a[d].tx_nbr].packet,d);
    p2G4_handle_next_request(d);
    return;
  } else {
    fq_add(current_time + 1, Rx_Payload, d);
    return;
  }
}


/**
 * Find if there is a compatible modulation for this CCA search
 * if there is not, return -1
 * if there is, return the device number
 *
 * Note that this search is not the same as for Rx
 * a) there does not need to be an actual packet in the air
 * b) we do not check the address
 * c) we do not check where in the transmittion we may be
 */
static int find_fitting_tx_cca(p2G4_cca_t *req){
  register uint *used = tx_l_c.used;
  int max_tx = txl_get_max_tx_nbr();
  for (int i = 0 ; i <= max_tx; i++) {
    if (used[i] != TXS_OFF)
    {
      p2G4_txv2_t* tx_s = &tx_l_c.tx_list[i].tx_s;
      if ((tx_s->radio_params.center_freq == req->radio_params.center_freq) &&
         (tx_s->radio_params.modulation & P2G4_MOD_SIMILAR_MASK) ==
             (req->radio_params.modulation & P2G4_MOD_SIMILAR_MASK) )
      {
        return i;
      }
    }
  }
  return -1;
}


static void f_cca_meas(uint d) {
  cca_status_t *cca_s = &cca_a[d];
  p2G4_cca_t *req = &cca_a[d].req;
  p2G4_cca_done_t *resp = &cca_a[d].resp;
  p2G4_rssi_done_t RSSI_meas;

  if ( current_time >= req->abort.recheck_time ) {
    if ( pick_and_validate_abort(d, &(req->abort), "CCA") != 0 ) {
      return;
    }
    if ((req->abort.abort_time < cca_s->scan_end) ) {
      cca_s->scan_end = req->abort.abort_time;
    }
  }

  if ( current_time >= cca_s->next_meas ) {
    { //Check RSSI level
      chm_RSSImeas(&tx_l_c, req->antenna_gain, &req->radio_params, &RSSI_meas, d, current_time);

      double power = p2G4_RSSI_value_to_dBm(RSSI_meas.RSSI);
      power = pow(10, power/10);
      cca_s->RSSI_acc += power;

      resp->RSSI_max = BS_MAX(resp->RSSI_max, RSSI_meas.RSSI);

      if (RSSI_meas.RSSI >= req->rssi_threshold) {
        resp->rssi_overthreshold = true;
        bs_trace_raw_time(8,"Device %u - RSSI over threshold \n", d);
        if (req->stop_when_found & 2) {
          cca_s->scan_end = current_time;
        }
      }
    }

    { //check for compatible modulation
      int tx_match;
      tx_match = find_fitting_tx_cca(req);
      if (tx_match >= 0){
        p2G4_power_t power = p2G4_RSSI_value_to_dBm(RSSI_meas.RSSI);
        resp->mod_rx_power = BS_MAX(resp->mod_rx_power, power);
        if (power >= req->mod_threshold) {
          resp->mod_found = true;
          bs_trace_raw_time(8,"Device %u - Modulated signal over threshold \n", d);
          if (req->stop_when_found & 1) {
            cca_s->scan_end = current_time;
          }
        }
      }
    }

    cca_s->next_meas += req->scan_period;
    cca_s->n_meas++;
  }

  if ( current_time >= cca_s->scan_end ) {
    bs_trace_raw_time(8,"Device %u - CCA completed\n", d);

    double power_dBm = 10*log10(cca_s->RSSI_acc/cca_s->n_meas);
    resp->RSSI_ave = p2G4_RSSI_value_from_dBm(power_dBm); //average the result
    resp->end_time = current_time;
    p2G4_phy_resp_cca(d, resp);
    dump_cca(cca_s, d);
    p2G4_handle_next_request(d);
    return;
  }

  /* Otherwise, we just continue */
  bs_time_t next_time = BS_MIN(cca_s->scan_end, req->abort.recheck_time);
  next_time = BS_MIN(cca_s->next_meas, next_time);
  fq_add(next_time, Rx_CCA_meas, d);

  return;
}

#define PAST_CHECK(start, d, type) \
  if (current_time >= start) { \
    bs_trace_error_time_line("Device %u wants to start a %s in "\
                             "%"PRItime" which has already passed\n",\
                             d, type, start); \
  }

static void prepare_wait(uint d){
  pb_wait_t wait;

  p2G4_phy_get(d, &wait, sizeof(wait));

  bs_trace_raw_time(8,"Device %u wants to wait until %"PRItime"\n",
                    d, wait.end);

  fq_add(wait.end, Wait_Done, d);

  if (current_time > wait.end) {
    bs_trace_error_time_line("Device %u wants to start a wait in "
                             "%"PRItime" which has already passed\n",
                             d, wait.end);
  }
}

static void check_valid_abort(p2G4_abort_t *abort, bs_time_t start_time, const char* type, uint d){
  if (current_time >= abort->abort_time){
    bs_trace_error_time_line("Device %u wants to abort a %s in the past (in %"PRItime")\n",
                             d, type, abort->abort_time);
  }
  if (current_time >= abort->recheck_time){
    bs_trace_error_time_line("Device %u wants to recheck a %s abort in the past (in %"PRItime")\n",
                             d, type, abort->recheck_time);
  }

  if (start_time > abort->abort_time) {
    bs_trace_error_time_line("Device %u wants a %s abort before the %s start (%"PRItime" > %"PRItime")\n",
                             d, type, type, start_time, abort->abort_time);
  }
  if (start_time > abort->recheck_time) {
    bs_trace_error_time_line("Device %u wants a %s abort recheck before %s start (%"PRItime" > %"PRItime")\n",
                             d, type, type, start_time, abort->recheck_time);
  }
}

static void prepare_tx_common(uint d, p2G4_txv2_t *tx_s){
  uint8_t *data = NULL;

  if ( tx_s->packet_size > 0 ){
    data = bs_malloc(tx_s->packet_size);
    p2G4_phy_get(d, data, tx_s->packet_size);
  }

  PAST_CHECK(tx_s->start_tx_time, d, "Tx");

  if ( tx_s->start_tx_time >= tx_s->end_tx_time ) {
    bs_trace_error_time_line("Device %u wants a tx end time <= tx start time (%"PRItime" <= %"PRItime")\n",d, tx_s->end_tx_time, tx_s->start_tx_time);
  }
  if ( tx_s->start_packet_time >= tx_s->end_tx_time ) {
    bs_trace_error_time_line("Device %u wants a tx end time <= tx packet start time (%"PRItime" <= %"PRItime")\n",d, tx_s->end_tx_time, tx_s->start_packet_time);
  }
  if ( tx_s->start_packet_time >= tx_s->end_packet_time ) {
    bs_trace_error_time_line("Device %u wants a packet end time <= tx packet start time (%"PRItime" <= %"PRItime")\n",d, tx_s->end_packet_time, tx_s->start_packet_time);
  }

  check_valid_abort(&tx_s->abort, tx_s->start_tx_time , "Tx", d);

  bs_trace_raw_time(8,"Device %u wants to Tx in: %"PRItime "-%"PRItime ""
                    " (packet: %"PRItime "-%"PRItime ")"
                    " (abort,recheck at %"PRItime ",%"PRItime ")"
                    "\n",
                    d, tx_s->start_tx_time, tx_s->end_tx_time,
                    tx_s->start_packet_time, tx_s->end_packet_time,
                    tx_s->abort.abort_time, tx_s->abort.recheck_time);

  txl_register(d, tx_s, data);

  fq_add(tx_s->start_tx_time, Tx_Start, d);
  /* Note: It is irrelevant if an ideal packet would have started before for the Tx side,
   * until the Tx starts nothing happens */
}

static void prepare_txv2(uint d){
  p2G4_txv2_t tx_s;

  p2G4_phy_get(d, &tx_s, sizeof(tx_s));

  prepare_tx_common(d, &tx_s);
}

static void prepare_txv1(uint d){
  p2G4_tx_t tx_s;
  p2G4_txv2_t tx_v2_s;

  p2G4_phy_get(d, &tx_s, sizeof(tx_s));
  map_txv1_to_txv2(&tx_v2_s, &tx_s);

  prepare_tx_common(d, &tx_v2_s);
}

static void prepare_RSSI(uint d){
  p2G4_rssi_t RSSI_s;

  p2G4_phy_get(d, &RSSI_s, sizeof(RSSI_s));

  PAST_CHECK(RSSI_s.meas_time, d, "RSSI measurement");

  bs_trace_raw_time(8,"Device %u want to do a RSSI measurement in %"PRItime "\n", \
                    d,  RSSI_s.meas_time); \

  memcpy(&RSSI_a[d],&RSSI_s, sizeof(p2G4_rssi_t));

  fq_add(RSSI_s.meas_time, RSSI_Meas, d);
}

static void prepare_rx_common(uint d, p2G4_rxv2_t *rxv2_s){
  PAST_CHECK(rxv2_s->start_time, d, "Rx");

  check_valid_abort(&rxv2_s->abort, rxv2_s->start_time , "Rx", d);

  bs_trace_raw_time(8,"Device %u wants to Rx in %"PRItime " (abort,recheck at %"PRItime ",%"PRItime ")\n",
                    d, rxv2_s->start_time, rxv2_s->abort.abort_time, rxv2_s->abort.recheck_time);

  if (rxv2_s->prelocked_tx != 0) {
    bs_trace_error_time_line("Device %u - Prelocked Tx not yet supported, must be set to 0 (was %u)\n",
                             d, rxv2_s->prelocked_tx);
  }

  //Initialize the reception status
  memcpy(&rx_a[d].rx_s, rxv2_s, sizeof(p2G4_rxv2_t));
  rx_a[d].scan_end = rx_a[d].rx_s.start_time + rx_a[d].rx_s.scan_duration - 1;
  rx_a[d].sync_end = 0;
  rx_a[d].header_end = 0;
  rx_a[d].payload_end = 0;
  rx_a[d].tx_nbr = -1;
  rx_a[d].biterrors = 0;
  memset(&rx_a[d].rx_done_s, 0, sizeof(p2G4_rxv2_done_t));
  if ( rxv2_s->abort.abort_time < rx_a[d].scan_end ) {
    rx_a[d].scan_end = rxv2_s->abort.abort_time - 1;
  }

  if (rxv2_s->error_calc_rate % 1000000 == 0) {
    rx_a[d].err_calc_state.errorspercalc = rxv2_s->error_calc_rate / 1000000;
    rx_a[d].err_calc_state.rate_uspercalc = 1;
    rx_a[d].err_calc_state.us_to_next_calc = 0;
  } else if (1000000 % rxv2_s->error_calc_rate == 0) {
    rx_a[d].err_calc_state.errorspercalc = 1;
    rx_a[d].err_calc_state.rate_uspercalc = 1000000 / rxv2_s->error_calc_rate;
    rx_a[d].err_calc_state.us_to_next_calc = 0;
  } else {
    bs_trace_error_time_line("The device %u requested a reception with an error calc rate "
                             "of %u times per s, but only integer multiples or integer dividers of 1MHz "
                             "are supported so far (for ex. 3MHz, 1MHz, 250KHz, 62.5KHz)\n",
                             d, rxv2_s->error_calc_rate);
  }

  fq_add(rxv2_s->start_time, Rx_Search_start, d);
}

static void prepare_rxv1(uint d){
  p2G4_rx_t rxv1_s;
  p2G4_rxv2_t rxv2_s;

  p2G4_phy_get(d, &rxv1_s, sizeof(p2G4_rx_t));
  map_rxv1_to_rxv2(&rxv2_s, &rxv1_s);
  rx_a[d].phy_address[0] = rxv1_s.phy_address;
  rx_a[d].v1_request = true;

  prepare_rx_common(d, &rxv2_s);
}

static void prepare_rxv2(uint d){
  p2G4_rxv2_t rxv2_s;

  p2G4_phy_get(d, &rxv2_s, sizeof(p2G4_rxv2_t));

  if ( rxv2_s.n_addr > P2G4_RXV2_MAX_ADDRESSES ) {
    bs_trace_error_time_line("Device %u: Attempting to Rx w more than the allowed number of Rx addresses,"
        "%i > %i\n",d, rxv2_s.n_addr, P2G4_RXV2_MAX_ADDRESSES);
  }

  if ( rxv2_s.resp_type != 0 ){
    bs_trace_error_time_line("Device %u: Attempting to Rx w resp_type = %i (not yet supported)\n",
        d, rxv2_s.resp_type);
  }

  if ( ( rxv2_s.pream_and_addr_duration > 0 ) &&
      ( rxv2_s.acceptable_pre_truncation >= rxv2_s.pream_and_addr_duration ) ) {
    bs_trace_error_time_line("Device %u: Attempting to Rx w acceptable_pre_truncation >= pream_and_addr_duration (%u >= %u)\n",
        d, rxv2_s.acceptable_pre_truncation, rxv2_s.pream_and_addr_duration);
  }
  if ( rxv2_s.acceptable_pre_truncation > rxv2_s.pream_and_addr_duration ) {
    bs_trace_error_time_line("Device %u: Attempting to Rx w acceptable_pre_truncation > pream_and_addr_duration (%u >= %u)\n",
        d, rxv2_s.acceptable_pre_truncation, rxv2_s.pream_and_addr_duration);
  }

  p2G4_phy_get(d, rx_a[d].phy_address, rxv2_s.n_addr*sizeof(p2G4_address_t));
  rx_a[d].v1_request = false;

  prepare_rx_common(d, &rxv2_s);
}

static void prepare_CCA(uint d){
  p2G4_cca_t *cca_req = &cca_a[d].req;

  //Clear status (and response)
  memset(&cca_a[d], 0, sizeof(cca_status_t));

  p2G4_phy_get(d, cca_req, sizeof(p2G4_cca_t));

  PAST_CHECK(cca_req->start_time, d, "CCA");

  check_valid_abort(&cca_req->abort, cca_req->start_time , "CCA", d);

  if ( cca_req->scan_period == 0 ){
    bs_trace_error_time_line("Device %u: scan period must be bigger than 0\n", d);
  }

  if ( cca_req->stop_when_found > 3 ){
    bs_trace_error_time_line("Device %u: Attempting to CCA w stop_when_found %u > 3 (not supported)\n",
           d, cca_req->stop_when_found);
  }

  cca_a[d].scan_end = cca_req->start_time + cca_req->scan_duration -1;
  cca_a[d].next_meas = cca_req->start_time;
  cca_a[d].RSSI_acc = 0;
  cca_a[d].resp.RSSI_max = P2G4_RSSI_POWER_MIN;
  cca_a[d].resp.mod_rx_power = P2G4_RSSI_POWER_MIN;

  bs_trace_raw_time(8,"Device %u wants to CCA starting in %"PRItime " every %u us; for %u us (abort,recheck at %"PRItime ",%"PRItime ")\n",
                    d, cca_req->start_time, cca_req->scan_period, cca_req->scan_duration, cca_req->abort.abort_time, cca_req->abort.recheck_time);

  fq_add(cca_req->start_time, Rx_CCA_meas, d);
}

static void p2G4_handle_next_request(uint d) {
  pc_header_t header;
  header = p2G4_get_next_request(d);

  switch (header) {
    case PB_MSG_DISCONNECT:
      nbr_active_devs -= 1;
      bs_trace_raw_time(3,"Device %u disconnected (%i left)\n", d, nbr_active_devs);
      fq_remove(d);
      break;
    case PB_MSG_TERMINATE:
      bs_trace_raw_time(4,"Device %u terminated the simulation (there was %i left)\n", d, nbr_active_devs-1);
      nbr_active_devs = 0;
      break;
    case PB_MSG_WAIT:
      prepare_wait(d);
      break;
    case P2G4_MSG_TX:
      prepare_txv1(d);
      break;
    case P2G4_MSG_TXV2:
      prepare_txv2(d);
      break;
    case P2G4_MSG_RSSIMEAS:
      prepare_RSSI(d);
      break;
    case P2G4_MSG_RX:
      prepare_rxv1(d);
      break;
    case P2G4_MSG_RXV2:
      prepare_rxv2(d);
      break;
    case P2G4_MSG_CCA_MEAS:
      prepare_CCA(d);
      break;
    default:
      bs_trace_error_time_line("The device %u has violated the protocol (%u)\n",d, header);
      break;
  }
}

uint8_t p2G4_main_clean_up(){
  int return_error;
  bs_trace_raw(9, "main: Cleaning up...\n");
  return_error = close_dump_files();
  if (RSSI_a != NULL)
    free(RSSI_a);
  if (rx_a != NULL)
    free(rx_a);
  if (cca_a != NULL)
    free(cca_a);
  txl_free();
  channel_and_modem_delete();
  fq_free();
  bs_random_free();
  p2G4_phy_disconnect_all_devices();
  p2G4_clear_args_struct(&args);
  return return_error;
}

static bs_time_t p2G4_get_time(){
  return current_time;
}

int main(int argc, char *argv[]) {

  bs_trace_register_cleanup_function(p2G4_main_clean_up);
  bs_trace_set_prefix_phy("???");
  bs_trace_register_time_function(p2G4_get_time);

  bs_trace_raw(9,"main: Parsing command line...\n");
  p2G4_argsparse(argc, argv, &args);

  channel_and_modem_init(args.channel_argc, args.channel_argv, args.channel_name,
                         args.modem_argc, args.modem_argv, args.modem_name, args.n_devs);

  bs_trace_raw(7,"main: Connecting...\n");
  p2G4_phy_initcom(args.s_id, args.p_id, args.n_devs);

  fq_init(args.n_devs);
  fq_register_func(Wait_Done,      f_wait_done      );
  fq_register_func(RSSI_Meas,      f_RSSI_meas      );
  fq_register_func(Rx_Search_start,f_rx_search_start);
  fq_register_func(Rx_Search_reeval,f_rx_search_reeval);
  fq_register_func(Rx_Found,       f_rx_found       );
  fq_register_func(Rx_Sync,        f_rx_sync        );
  fq_register_func(Rx_Header,      f_rx_header      );
  fq_register_func(Rx_Payload,     f_rx_payload     );
  fq_register_func(Tx_Abort_Reeval,f_tx_abort_reeval);
  fq_register_func(Tx_Start,       f_tx_start       );
  fq_register_func(Tx_Packet_Start,f_tx_packet_start);
  fq_register_func(Tx_Packet_End,  f_tx_packet_end  );
  fq_register_func(Tx_End,         f_tx_end         );
  fq_register_func(Rx_CCA_meas,    f_cca_meas       );

  bs_random_init(args.rseed);
  txl_create(args.n_devs);
  RSSI_a = bs_calloc(args.n_devs, sizeof(p2G4_rssi_t));
  rx_a = bs_calloc(args.n_devs, sizeof(rx_status_t));
  cca_a = bs_calloc(args.n_devs, sizeof(cca_status_t));

  if (args.dont_dump == 0) open_dump_files(args.compare, args.stop_on_diff, args.dump_imm, args.s_id, args.p_id, args.n_devs);

  nbr_active_devs = args.n_devs;

  for (uint d = 0; d < args.n_devs && nbr_active_devs > 0; d ++) {
    p2G4_handle_next_request(d);
  }

  fq_find_next();
  current_time = fq_get_next_time();
  while ((nbr_active_devs > 0) && (current_time < args.sim_length)) {
    fq_call_next();
    fq_find_next();
    current_time = fq_get_next_time();
  }

  if (current_time >= args.sim_length) {
    bs_trace_raw(9, "main: @%"PRItime" simulation ended\n", args.sim_length);
  }

  return p2G4_main_clean_up();
}

/*
 * TODO:
 *
 * Enable Txv2, Rxv2 & CCA dumps
 *
 * Future features (see doc/Current_API_shortcommings.txt):
 * implement prelocked_tx
 * implement forced_packet_duration
 * implement support for != coding_rate in Tx and Rx
 * implement support for immediate bit error masks
 */
