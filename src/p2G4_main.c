/*
 * Copyright 2018 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bs_tracing.h"
#include "bs_oswrap.h"
#include "bs_utils.h"
#include "bs_pc_2G4.h"
#include "bs_rand_main.h"
#include "p2G4_args.h"
#include "p2G4_channel_and_modem.h"
#include "p2G4_dump.h"
#include "p2G4_func_queue.h"
#include "p2G4_pending_tx_rx_list.h"
#include "p2G4_com.h"

static bs_time_t current_time = 0;
static int nbr_active_devs; //How many devices are still active (devices may disconnect during the simulation)
extern tx_l_c_t tx_l_c; //list of all transmissions
static p2G4_rssi_t *RSSI_a; //array of all RSSI measurements
static rx_status_t *rx_a; //array of all receptions
static p2G4_args_t args;

static void p2G4_handle_next_request(uint d);

static void f_wait_done(uint d){
  bs_trace_raw_time(8,"Wait done for device %u\n", d);
  p2G4_phy_resp_wait(d);
  p2G4_handle_next_request(d);
}

static void f_tx_end(uint d){

  tx_el_t* tx_el;
  p2G4_tx_done_t tx_done_s;

  tx_el = &tx_l_c.tx_list[d];
  if ( tx_el->tx_s.abort.abort_time < tx_el->tx_s.end_time ) {
    bs_trace_raw_time(8,"Tx done (Tx aborted) for device %u\n", d);
  } else {
    bs_trace_raw_time(8,"Tx done (Tx ended) for device %u\n", d);
  }

  dump_tx(tx_el, d);

  txl_clear(d);

  tx_done_s.end_time = current_time;
  p2G4_phy_resp_tx(d, &tx_done_s);
  p2G4_handle_next_request(d);
}

/*
 * Pick abort from device.
 * If something goes wrong it returns != 0, and the simulation shall be terminated
 */
static int pick_and_validate_abort(uint d, p2G4_abort_t *ab, const char* type) {

  bs_trace_raw_time(8,"Reevaluating %s abort for device %u\n", type, d);

  if ( p2G4_phy_get_new_abort(d, ab) != 0) {
    bs_trace_raw_time(4,"Device %u terminated the simulation (there was %i left)\n", d, nbr_active_devs-1);
    nbr_active_devs = 0;
    return 1;
  }

  if ( current_time > ab->abort_time ){
    bs_trace_error_time_line("Device %u requested %s abort in %"PRItime" which has passed\n", d, type, ab->abort_time);
  }
  if ( current_time > ab->recheck_time ){ //we allow to recheck just now
    bs_trace_error_time_line("Device %u requested %s abort recheck in %"PRItime" which has passed\n", d, type, ab->recheck_time);
  }
  if ( current_time == ab->recheck_time ){
    bs_trace_raw_time(4,"(device %u) Note: Abort reevaluation in same time (possible infinite loop in device?)\n", d);
  }

  bs_trace_raw_time(8,"device %u (%s) requested to reschedule abort,recheck at %"PRItime ",%"PRItime ")\n",
                    d, type, ab->abort_time, ab->recheck_time);
  return 0;
}

/**
 * All devices which may be able to receive this transmission are moved
 * to the Rx_Found state, where they may start synchronizing to it
 */
static void find_and_activate_rx(const p2G4_tx_t *tx_s, uint tx_d) {
  for (int rx_d = 0 ; rx_d < args.n_devs; rx_d++) {
    rx_status_t *rx_s = &rx_a[rx_d];
    if (rx_s->state == Rx_State_Searching &&
       (tx_s->phy_address == rx_s->rx_s.phy_address) &&
       (tx_s->radio_params.center_freq == rx_s->rx_s.radio_params.center_freq) &&
       (tx_s->radio_params.modulation & P2G4_MOD_SIMILAR_MASK) ==
           (rx_s->rx_s.radio_params.modulation & P2G4_MOD_SIMILAR_MASK))
    {
      rx_s->tx_nbr = tx_d;
      rx_s->state = Rx_State_NotSearching;
      fq_add(current_time, Rx_Found, rx_d);
    }
  }
}


static void f_tx_start(uint d) {
  p2G4_tx_t* tx_s;

  tx_s = &tx_l_c.tx_list[d].tx_s;

  txl_activate(d);
  bs_trace_raw_time(8,"Starting Tx for device %u\n", d);

  find_and_activate_rx(tx_s, d);

  bs_time_t TxEndt;
  TxEndt = BS_MIN(tx_s->end_time, tx_s->abort.abort_time);
  if ( TxEndt < tx_s->abort.recheck_time ) {
    fq_add(TxEndt, Tx_End, d);
  } else {
    fq_add(tx_s->abort.recheck_time, Tx_Abort_Reeval, d);
  }
}

static void f_tx_abort_reeval(uint d){
  p2G4_tx_t* tx_s;

  tx_s = &tx_l_c.tx_list[d].tx_s;

  if (pick_and_validate_abort(d, &(tx_s->abort), "Tx"))
    return;

  bs_time_t tx_end_t;
  tx_end_t = BS_MIN(tx_s->end_time, tx_s->abort.abort_time);
  if ( tx_end_t < tx_s->abort.recheck_time ) {
    fq_add(tx_end_t, Tx_End, d);
  } else {
    fq_add(tx_s->abort.recheck_time, Tx_Abort_Reeval, d);
  }
}

static void f_RSSI_meas(uint d) {
  p2G4_rssi_done_t RSSI_meas;

  chm_RSSImeas(&tx_l_c, &RSSI_a[d], &RSSI_meas, d, current_time);

  dump_RSSImeas(&RSSI_a[d], &RSSI_meas, d);

  bs_trace_raw_time(8,"RSSIDone for device %u\n", d);

  p2G4_phy_resp_RSSI(d, &RSSI_meas);
  p2G4_handle_next_request(d);
}

static void rx_do_RSSI(uint d) {
  p2G4_rssi_t RSSI_s;
  RSSI_s.radio_params.center_freq = rx_a[d].rx_s.radio_params.center_freq;
  RSSI_s.radio_params.modulation = rx_a[d].rx_s.radio_params.modulation;
  RSSI_s.meas_time = current_time;
  chm_RSSImeas(&tx_l_c, &RSSI_s, &rx_a[d].rx_done_s.rssi, d, current_time);
}

static inline void rx_enqueue_search_end(uint d){
  rx_status_t *rx_status = &rx_a[d];

  rx_status->state = Rx_State_Searching;
  bs_time_t end_time = BS_MIN((rx_status->scan_end + 1), rx_status->rx_s.abort.recheck_time);
  fq_add(end_time, Rx_Search, d);
  return;
}

static void f_rx_search(uint d) {
  rx_status_t *rx_status;
  rx_status = &rx_a[d];

  if ( current_time >= rx_status->rx_s.abort.recheck_time ) {
    if ( pick_and_validate_abort(d, &(rx_a[d].rx_s.abort), "Rx") != 0 ){
      return;
    }
    if ( rx_status->rx_s.abort.abort_time < rx_status->scan_end ) {
        rx_status->scan_end = rx_status->rx_s.abort.abort_time - 1;
    }
  }

  if ( current_time > rx_status->scan_end ) {
    rx_status->rx_done_s.packet_size = 0;
    rx_status->rx_done_s.rx_time_stamp = current_time;
    rx_status->rx_done_s.status = P2G4_RXSTATUS_NOSYNC;
    if ( rx_status->rx_s.abort.abort_time != current_time ) { //if we are not aborting
      rx_do_RSSI(d);
    }

    bs_trace_raw_time(8,"RxDone (NoSync during Rx_search) for device %u\n", d);

    rx_status->state = Rx_State_NotSearching;
    rx_status->rx_done_s.end_time = current_time;

    p2G4_phy_resp_rx(d, &rx_status->rx_done_s);
    dump_rx(&rx_a[d],NULL,d);
    p2G4_handle_next_request(d);
    return;
  }

  rx_enqueue_search_end(d);
  return;
}

static void f_rx_found(uint d){
  rx_status_t *rx_status = &rx_a[d];

  uint tx_d = rx_status->tx_nbr;
  if ( chm_is_packet_synched( &tx_l_c, tx_d, d,  &rx_status->rx_s, current_time ) ) {
    rx_status->sync_end    = current_time + rx_status->rx_s.pream_and_addr_duration - 1;
    rx_status->header_end  = rx_status->sync_end + rx_status->rx_s.header_duration;
    rx_status->payload_end = tx_l_c.tx_list[tx_d].tx_s.end_time;
    rx_status->biterrors = 0;
    fq_add(current_time, Rx_Sync, d);
    return;
  }

  rx_enqueue_search_end(d);
  return;
}

static void f_rx_sync(uint d){
  if ( current_time >= rx_a[d].rx_s.abort.recheck_time ) {
    if ( pick_and_validate_abort(d, &(rx_a[d].rx_s.abort), "Rx") != 0 ) {
      return;
    }
    if ( rx_a[d].rx_s.abort.abort_time < rx_a[d].scan_end ) {
        rx_a[d].scan_end = rx_a[d].rx_s.abort.abort_time - 1;
    }
  }

  if ( current_time > rx_a[d].scan_end ) {
    rx_a[d].rx_done_s.packet_size = 0;
    rx_a[d].rx_done_s.rx_time_stamp = current_time;
    rx_a[d].rx_done_s.status = P2G4_RXSTATUS_NOSYNC;
    if ( rx_a[d].rx_s.abort.abort_time != current_time ) {
      rx_do_RSSI(d);
    }

    bs_trace_raw_time(8,"RxDone (no sync during Rx sync) for device %u\n", d);

    rx_a[d].rx_done_s.end_time = current_time;

    p2G4_phy_resp_rx(d, &rx_a[d].rx_done_s);
    dump_rx(&rx_a[d],NULL,d);
    p2G4_handle_next_request(d);
    return;
  }

  if ( tx_l_c.used[rx_a[d].tx_nbr] == 0 ) { //if the Tx aborted
    //An abort of the Tx during a sync results in the sync being lost always (even if we are just at the end of the syncword)
    fq_add(current_time + 1, Rx_Search, d);
    return;
  } else {
    rx_a[d].biterrors += chm_bit_errors(&tx_l_c, rx_a[d].tx_nbr, d, &rx_a[d].rx_s, current_time);
  }

  if ( rx_a[d].biterrors >= rx_a[d].rx_s.sync_threshold ) {
    fq_add(current_time + 1, Rx_Search, d);
    return;
  }

  if ( current_time >= rx_a[d].sync_end ) {
    //we have correctly synched:
    rx_do_RSSI(d);
    rx_a[d].biterrors = 0;
    int device_accepted = 0;

    //Ask from the device if it wants us to continue..
    rx_a[d].rx_done_s.rx_time_stamp = rx_a[d].sync_end;
    rx_a[d].rx_done_s.end_time = current_time;
    rx_a[d].rx_done_s.packet_size = tx_l_c.tx_list[rx_a[d].tx_nbr].tx_s.packet_size;
    rx_a[d].rx_done_s.status = P2G4_RXSTATUS_INPROGRESS;

    bs_trace_raw_time(8,"Rx Address found for device %u\n", d);
    p2G4_phy_resp_rx_addr_found(d, &rx_a[d].rx_done_s, tx_l_c.tx_list[rx_a[d].tx_nbr].packet);

    pc_header_t header;
    header = p2G4_get_next_request(d);
    switch (header) {
    case PB_MSG_DISCONNECT:
      nbr_active_devs -= 1;
      bs_trace_raw_time(5,"device %i disconnected during Rx (minor protocol violation) (%i left)\n", d, nbr_active_devs);
      device_accepted = 0;
      fq_remove(d);
      return;
      break;
    case PB_MSG_TERMINATE:
      bs_trace_raw_time(4,"device %i terminated the simulation (there was %i left)\n", d, nbr_active_devs-1);
      nbr_active_devs = 0;
      return;
      break;
    case P2G4_MSG_RXCONT:
      device_accepted = 1;
      break;
    case P2G4_MSG_RXSTOP:
      device_accepted = 0;
      break;
    default:
      bs_trace_error_line("The device %u has violated the protocol during an Rx (%u) => Terminate\n",d, header);
      break;
    }

    if (device_accepted) {
      fq_add(current_time + 1, Rx_Header, d);
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
  if ( current_time >= rx_a[d].rx_s.abort.recheck_time ) {
    if (pick_and_validate_abort(d, &(rx_a[d].rx_s.abort), "Rx") != 0) {
      return;
    }
  }

  if ( tx_l_c.used[rx_a[d].tx_nbr] == 0 ) { //if the Tx aborted
    rx_a[d].biterrors += rx_a[d].bpus;
  } else {
    rx_a[d].biterrors += chm_bit_errors(&tx_l_c, rx_a[d].tx_nbr, d, &rx_a[d].rx_s, current_time);
  }

  if ( ( ( current_time >= rx_a[d].header_end )
        && ( rx_a[d].biterrors > rx_a[d].rx_s.header_threshold ) )
      ||
       ( current_time >= rx_a[d].rx_s.abort.abort_time ) ) {
    rx_a[d].rx_done_s.packet_size = 0;
    rx_a[d].rx_done_s.status = P2G4_RXSTATUS_HEADER_ERROR;
    bs_trace_raw_time(8,"RxDone (Header error) for device %u\n", d);
    rx_a[d].rx_done_s.end_time = current_time;
    p2G4_phy_resp_rx(d, &rx_a[d].rx_done_s);
    dump_rx(&rx_a[d],NULL,d);
    p2G4_handle_next_request(d);
    return;
  } else if ( current_time >= rx_a[d].header_end ) {
    fq_add(current_time + 1, Rx_Payload, d);
    return;
  } else {
    fq_add(current_time + 1, Rx_Header, d);
    return;
  }
}

static void f_rx_payload(uint d){
  if ( current_time >= rx_a[d].rx_s.abort.recheck_time ) {
    if (pick_and_validate_abort(d, &(rx_a[d].rx_s.abort), "Rx") != 0) {
      return;
    }
  }

  if ( tx_l_c.used[rx_a[d].tx_nbr] == 0 ) { //if the Tx aborted
    rx_a[d].biterrors += rx_a[d].bpus;
  } else {
    rx_a[d].biterrors += chm_bit_errors(&tx_l_c, rx_a[d].tx_nbr, d, &rx_a[d].rx_s, current_time);
  }

  if (((current_time >= rx_a[d].payload_end) && (rx_a[d].biterrors > 0))
      || (current_time >= rx_a[d].rx_s.abort.abort_time)) {
    if (!args.crcerr_data) {
        rx_a[d].rx_done_s.packet_size = 0;
    }
    rx_a[d].rx_done_s.status = P2G4_RXSTATUS_CRC_ERROR;
    bs_trace_raw_time(8,"RxDone (CRC error) for device %u\n", d);
    rx_a[d].rx_done_s.end_time = current_time;
    p2G4_phy_resp_rx(d, &rx_a[d].rx_done_s);
    dump_rx(&rx_a[d],NULL,d);
    p2G4_handle_next_request(d);
    return;
  } else if ( current_time >= rx_a[d].payload_end ) {
    rx_a[d].rx_done_s.status = P2G4_RXSTATUS_OK;
    bs_trace_raw_time(8,"RxDone (CRC ok) for device %u\n", d);
    rx_a[d].rx_done_s.end_time = current_time;
    p2G4_phy_resp_rx(d, &rx_a[d].rx_done_s);
    dump_rx(&rx_a[d],tx_l_c.tx_list[rx_a[d].tx_nbr].packet,d);
    p2G4_handle_next_request(d);
    return;
  } else {
    fq_add(current_time + 1, Rx_Payload, d);
    return;
  }
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
    bs_trace_error_time_line("Device %u wants a %s abort before the %s start (in %"PRItime")\n",
                             d, type, type, abort->abort_time);
  }
  if (start_time > abort->recheck_time) {
    bs_trace_error_time_line("Device %u wants a %s abort recheck before %s start (in %"PRItime")\n",
                             d, type, type, abort->recheck_time);
  }
}

static void prepare_tx(uint d){
  p2G4_tx_t tx_s;
  uint8_t *data = NULL;

  p2G4_phy_get(d, &tx_s, sizeof(tx_s));

  if ( tx_s.packet_size > 0 ){
    data = bs_malloc(tx_s.packet_size);
    p2G4_phy_get(d, data, tx_s.packet_size);
  }

  PAST_CHECK(tx_s.start_time, d, "Tx");

  if ( tx_s.start_time >= tx_s.end_time ) {
    bs_trace_error_time_line("Device %u wants a tx end time <= tx start time (%"PRItime" <= %"PRItime")\n",d, tx_s.end_time, tx_s.start_time);
  }

  check_valid_abort(&tx_s.abort, tx_s.start_time , "Tx", d);

  bs_trace_raw_time(8,"Device %u wants to Tx in %"PRItime " (abort,recheck at %"PRItime ",%"PRItime ")\n",
                    d, tx_s.start_time, tx_s.abort.abort_time, tx_s.abort.recheck_time);

  txl_register(d, &tx_s, data);

  fq_add(tx_s.start_time, Tx_Start, d);
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

static void prepare_rx(uint d){
  p2G4_rx_t rx_s;

  p2G4_phy_get(d, &rx_s, sizeof(p2G4_rx_t));

  PAST_CHECK(rx_s.start_time, d, "Rx");

  check_valid_abort(&rx_s.abort, rx_s.start_time , "Rx", d);

  bs_trace_raw_time(8,"Device %u wants to Rx in %"PRItime " (abort,recheck at %"PRItime ",%"PRItime ")\n",
                    d, rx_s.start_time, rx_s.abort.abort_time, rx_s.abort.recheck_time);

  //Initialize the reception status
  memcpy(&rx_a[d].rx_s, &rx_s, sizeof(p2G4_rx_t));
  rx_a[d].scan_end = rx_a[d].rx_s.start_time + rx_a[d].rx_s.scan_duration - 1;
  rx_a[d].sync_end = 0;
  rx_a[d].header_end = 0;
  rx_a[d].payload_end = 0;
  rx_a[d].tx_nbr = -1;
  rx_a[d].biterrors = 0;
  memset(&rx_a[d].rx_done_s, 0, sizeof(p2G4_rx_done_t));
  if ( rx_s.abort.abort_time < rx_a[d].scan_end ) {
    rx_a[d].scan_end = rx_s.abort.abort_time - 1;
  }
  if (rx_s.bps % 1000000 != 0) {
    bs_trace_error_time_line("The device %u requested a reception with a rate "
                             "of %u bps, but only multiples of 1Mbps are "
                             "supported so far\n",
                             d, rx_s.bps);
  }
  rx_a[d].bpus = rx_s.bps/1000000;

  fq_add(rx_s.start_time, Rx_Search, d);
}

static void p2G4_handle_next_request(uint d) {
  pc_header_t header;
  header = p2G4_get_next_request(d);

  switch (header) {
    case PB_MSG_DISCONNECT:
      nbr_active_devs -= 1;
      bs_trace_raw_time(3,"Device %i disconnected (%i left)\n", d, nbr_active_devs);
      fq_remove(d);
      break;
    case PB_MSG_TERMINATE:
      bs_trace_raw_time(4,"Device %i terminated the simulation (there was %i left)\n", d, nbr_active_devs-1);
      nbr_active_devs = 0;
      break;
    case PB_MSG_WAIT:
      prepare_wait(d);
      break;
    case P2G4_MSG_TX:
      prepare_tx(d);
      break;
    case P2G4_MSG_RSSIMEAS:
      prepare_RSSI(d);
      break;
    case P2G4_MSG_RX:
      prepare_rx(d);
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
  fq_register_func(Tx_End,         f_tx_end         );
  fq_register_func(RSSI_Meas,      f_RSSI_meas      );
  fq_register_func(Rx_Search,      f_rx_search      );
  fq_register_func(Rx_Found,       f_rx_found       );
  fq_register_func(Rx_Sync,        f_rx_sync        );
  fq_register_func(Rx_Header,      f_rx_header      );
  fq_register_func(Rx_Payload,     f_rx_payload     );
  fq_register_func(Tx_Abort_Reeval,f_tx_abort_reeval);
  fq_register_func(Tx_Start,       f_tx_start       );

  bs_random_init(args.rseed);
  txl_create(args.n_devs);
  RSSI_a = bs_calloc(args.n_devs, sizeof(p2G4_rssi_t));
  rx_a = bs_calloc(args.n_devs, sizeof(rx_status_t));

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
