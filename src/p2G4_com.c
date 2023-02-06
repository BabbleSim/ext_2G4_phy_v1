/*
 * Copyright 2018 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "bs_pc_base.h"
#include "bs_pc_2G4_types.h"
#include "bs_pc_2G4.h"
#include "bs_tracing.h"
#include <unistd.h>

static pb_phy_state_t cb_med_state = {0};

#pragma GCC diagnostic ignored "-Wunused-result"

void p2G4_phy_initcom(const char* s, const char* p, uint n){
  if (pb_phy_initcom(&cb_med_state, s, p, n))
    bs_trace_error_line("Cannot establish communication with devices\n");
}

void p2G4_phy_disconnect_all_devices(){
  pb_phy_disconnect_devices(&cb_med_state);
}

void p2G4_phy_resp_wait(uint d) {
  pb_phy_resp_wait(&cb_med_state, d);
}

/**
 *  Respond to the device with P2G4_MSG_TX_END and the tx done structure
 */
void p2G4_phy_resp_tx(uint d, p2G4_tx_done_t *tx_done_s) {
  if (pb_phy_is_connected_to_device(&cb_med_state, d)) {
    pb_send_msg(cb_med_state.ff_ptd[d], P2G4_MSG_TX_END,
                (void *)tx_done_s, sizeof(p2G4_tx_done_t));
  }
}

/**
 * Respond to the device with P2G4_MSG_RX_ADDRESSFOUND a
 * p2G4_rx_done_t and a possible packet of p2G4_rx_done_t->packet_size bytes
 */
void p2G4_phy_resp_rx_addr_found(uint d, p2G4_rx_done_t* rx_done_s, uint8_t *packet) {
  if (pb_phy_is_connected_to_device(&cb_med_state, d)) {
    pb_send_msg(cb_med_state.ff_ptd[d], P2G4_MSG_RX_ADDRESSFOUND,
                (void *)rx_done_s, sizeof(p2G4_rx_done_t));
    pb_send_payload(cb_med_state.ff_ptd[d], packet, rx_done_s->packet_size);
  }
}

/**
 * Respond to the device with P2G4_MSG_RXV2_ADDRESSFOUND a
 * p2G4_rx_done_t and a possible packet of p2G4_rxv2_done_t->packet_size bytes
 */
void p2G4_phy_resp_rxv2_addr_found(uint d, p2G4_rxv2_done_t* rx_done_s, uint8_t *packet) {
  if (pb_phy_is_connected_to_device(&cb_med_state, d)) {
    pb_send_msg(cb_med_state.ff_ptd[d], P2G4_MSG_RXV2_ADDRESSFOUND,
                (void *)rx_done_s, sizeof(p2G4_rxv2_done_t));
    pb_send_payload(cb_med_state.ff_ptd[d], packet, rx_done_s->packet_size);
  }
}

/**
 * Respond to the device with P2G4_MSG_RX_END and a p2G4_rx_done_t
 * (note that the packet was already sent out in the address found)
 */
void p2G4_phy_resp_rx(uint d, p2G4_rx_done_t* rx_done_s) {
  if (pb_phy_is_connected_to_device(&cb_med_state, d)) {
    pb_send_msg(cb_med_state.ff_ptd[d], P2G4_MSG_RX_END,
                (void *)rx_done_s, sizeof(p2G4_rx_done_t));
  }
}

/**
 * Respond to the device with P2G4_MSG_RX_ENDV2 and a p2G4_rx_donev2_t
 * (note that the packet was already sent out in the address found)
 */
void p2G4_phy_resp_rxv2(uint d, p2G4_rxv2_done_t* rx_done_s) {
  if (pb_phy_is_connected_to_device(&cb_med_state, d)) {
    pb_send_msg(cb_med_state.ff_ptd[d], P2G4_MSG_RXV2_END,
                (void *)rx_done_s, sizeof(p2G4_rxv2_done_t));
  }
}

/**
 * Respond to the device with P2G4_MSG_CCA_END and a p2G4_cca_done_t
 */
void p2G4_phy_resp_cca(uint d, p2G4_cca_done_t *sc_done_s) {
  if (pb_phy_is_connected_to_device(&cb_med_state, d)) {
    pb_send_msg(cb_med_state.ff_ptd[d], P2G4_MSG_CCA_END,
                (void *)sc_done_s, sizeof(p2G4_cca_done_t));
  }
}

/**
 * Respond to the device with P2G4_MSG_RSSI_END and a p2G4_rssi_done_t
 */
void p2G4_phy_resp_RSSI(uint d, p2G4_rssi_done_t* RSSI_done_s) {
  if (pb_phy_is_connected_to_device(&cb_med_state, d)) {
    pb_send_msg(cb_med_state.ff_ptd[d], P2G4_MSG_RSSI_END,
                (void *)RSSI_done_s, sizeof(p2G4_rssi_done_t));
  }
}

/**
 * Respond to the device with P2G4_MSG_ABORTREVAL_RRSI and a p2G4_rssi_done_t
 * (during an abort reeval procedure, as a result of the device asking for
 * an immediate RSSI measurement)
 */
void p2G4_phy_resp_IMRSSI(uint d, p2G4_rssi_done_t* RSSI_done_s) {
  if (pb_phy_is_connected_to_device(&cb_med_state, d)) {
    pb_send_msg(cb_med_state.ff_ptd[d], P2G4_MSG_IMMRSSI_RRSI_DONE,
                (void *)RSSI_done_s, sizeof(p2G4_rssi_done_t));
  }
}

pc_header_t p2G4_get_next_request(uint d){
  return pb_phy_get_next_request(&cb_med_state, d);
}

void p2G4_phy_get(uint d, void* b, size_t size) {
  if (pb_phy_is_connected_to_device(&cb_med_state, d)) {
    read(cb_med_state.ff_dtp[d], b, size);
  }
}

/**
 * Ask the device for a new abort struct for the ongoing Tx or Rx
 */
int p2G4_phy_get_new_abort(uint d, p2G4_abort_t* abort_s) {
  if (pb_phy_is_connected_to_device(&cb_med_state, d)) {
    pc_header_t r_header = P2G4_MSG_ABORTREEVAL;
    write(cb_med_state.ff_ptd[d], &r_header, sizeof(r_header));

    ssize_t read_size = 0;
    pc_header_t header = PB_MSG_DISCONNECT;
    read(cb_med_state.ff_dtp[d], &header, sizeof(header));

    if (header == PB_MSG_TERMINATE) {
      return PB_MSG_TERMINATE;
    } else if (header == P2G4_MSG_RERESP_IMMRSSI) {
      return P2G4_MSG_RERESP_IMMRSSI;
    } else if (header != P2G4_MSG_RERESP_ABORTREEVAL) {
      //if the read failed or the device really wants to disconnect in the
      //middle of an abort (which is not really supported) we try to
      //handle it gracefully
      pb_phy_free_one_device(&cb_med_state ,d);
    } else {
      read_size = read(cb_med_state.ff_dtp[d], abort_s, sizeof(p2G4_abort_t));
    }

    if (read_size != sizeof(p2G4_abort_t)) {
      //There is some likelihood that a device will crash badly during abort
      //reevaluation, therefore we try to handle it
      bs_trace_warning_line(
          "Low level communication with device %i broken during Abort reevaluation (tried to get %i got %i bytes) (most likely the device was terminated)\n",
          d, sizeof(p2G4_abort_t), read_size);
      pb_phy_disconnect_devices(&cb_med_state);
      bs_trace_error_line("Exiting\n");
    }
  }

  return 0;
}
