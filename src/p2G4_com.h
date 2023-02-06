/*
 * Copyright 2018 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef P2G4_COM_H
#define P2G4_COM_H

#include "bs_pc_2G4_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C"{
#endif

void p2G4_phy_initcom(const char* s, const char* p, uint n);
void p2G4_phy_disconnect_all_devices();
void p2G4_phy_resp_rx(uint d, p2G4_rx_done_t* rx_d);
void p2G4_phy_resp_rxv2(uint d, p2G4_rxv2_done_t* rx_done_s);
void p2G4_phy_resp_RSSI(uint d, p2G4_rssi_done_t* RSSI_d);
void p2G4_phy_resp_IMRSSI(uint d, p2G4_rssi_done_t* RSSI_done_s);
void p2G4_phy_resp_tx(uint d, p2G4_tx_done_t * tx_d);
void p2G4_phy_resp_rx_addr_found(uint d, p2G4_rx_done_t* rx_d, uint8_t *p);
void p2G4_phy_resp_rxv2_addr_found(uint d, p2G4_rxv2_done_t* rx_done_s, uint8_t *packet);
void p2G4_phy_resp_cca(uint d, p2G4_cca_done_t *sc_done_s);
void p2G4_phy_resp_wait(uint d);
void p2G4_phy_get(uint d, void* b, size_t size);
int p2G4_phy_get_new_abort(uint d, p2G4_abort_t* abort);
pc_header_t p2G4_get_next_request(uint d);

#ifdef __cplusplus
}
#endif

#endif
