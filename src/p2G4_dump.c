/*
 * Copyright 2018 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdio.h>
#include "bs_tracing.h"
#include "bs_oswrap.h"
#include "bs_results.h"
#include "bs_pc_2G4_types.h"
#include "bs_pc_2G4_utils.h"
#include "p2G4_channel_and_modem_priv.h"
#include "bs_rand_main.h"
#include "p2G4_pending_tx_rx_list.h"

/*Max number of errors before ignoring a given file*/
#define MAX_ERRORS 15

static bool comp, stop_on_diff;
static uint n_dev = 0;

/* statistics per device and file */
typedef struct {
  //Number of lines with errors:
  uint32_t nbr_rxv1_er;
  uint32_t nbr_txv1_er;
  uint32_t nbr_rxv2_er;
  uint32_t nbr_txv2_er;
  uint32_t nbr_RSSI_er;
  uint32_t nbr_CCA_er;
  uint32_t nbr_modemrx_er;
  //Number lines dumped:
  uint32_t nbr_rxv1;
  uint32_t nbr_txv1;
  uint32_t nbr_rxv2;
  uint32_t nbr_txv2;
  uint32_t nbr_RSSI;
  uint32_t nbr_CCA;
  uint32_t nbr_modemrx;
} t_dump_check_stats;

static t_dump_check_stats *stats = NULL;

static FILE **txv1_f = NULL;
static FILE **rxv1_f = NULL;
static FILE **txv2_f = NULL;
static FILE **rxv2_f = NULL;
static FILE **RSSI_f = NULL;
static FILE **CCA_f = NULL;
static FILE **modemrx_f = NULL;

static void dump_txv1_heading(FILE *file){
  if (file == NULL) {
    return;
  }
  fprintf(file,
          "start_time,end_time,center_freq,"
          "phy_address,modulation,power_level,abort_time,"
          "recheck_time,packet_size,packet\n");
}

static void dump_txv2_heading(FILE *file){
  if (file == NULL) {
    return;
  }
  fprintf(file,
            "start_tx_time,end_tx_time,"
            "start_packet_time, end_packet_time, center_freq,"
            "phy_address,modulation,power_level,abort_time,"
            "recheck_time,packet_size,packet\n");
}

static void dump_rxv1_heading(FILE *file){
  if (file == NULL) {
    return;
  }
  fprintf(file,"start_time,scan_duration,phy_address,modulation,"
               "center_freq,antenna_gain,sync_threshold,header_threshold,"
               "pream_and_addr_duration,"
               "header_duration,bps,abort_time,recheck_time,"
               "tx_nbr,biterrors,sync_end,header_end,"
               "payload_end,rx_time_stamp,status,RSSI,"
               "packet_size,packet\n");
}

static void dump_rxv2_heading(FILE *file){
  if (file == NULL) {
    return;
  }
  fprintf(file,"start_time,scan_duration,n_addr,phy_address[],modulation,"
               "center_freq,antenna_gain,"
               "acceptable_pre_truncation,sync_threshold,header_threshold,"
               "pream_and_addr_duration,"
               "header_duration,error_calc_rate,resp_type,"
               "abort_time,recheck_time,"
               "tx_nbr,matched_addr,biterrors,sync_end,header_end,"
               "payload_end,rx_time_stamp,status,RSSI,"
               "packet_size,packet\n");
}

static void dump_CCA_heading(FILE *file){
  if (file == NULL) {
    return;
  }
  fprintf(file,"start_time,scan_duration,scan_period,"
      "modulation,center_freq,antenna_gain,"
      "threshold_mod,threshold_rssi,stop_when_found,"
      "abort_time,recheck_time,"
      "end_time, RSSI_ave, RSSI_max, mod_rx_power,"
      "mod_found, rssi_overthreshold\n");
}


static void dump_RSSI_heading(FILE *file){
  if (file == NULL) {
    return;
  }
  fprintf(file,"meas_time,modulation,center_freq,antenna_gain,RSSI\n");
}

static void dump_modemrx_heading(FILE *file){
  if (file == NULL) {
    return;
  }
  fprintf(file,"time,tx_nbr,CalNotRecal,center_freq,modulation,"
               "BER,syncprob,SNR,anaSNR,ISISNR,att[i],rxpow[i]\n");
}

/**
 * Compare 2 strings, return 0 if they match,
 * or -(the position of the 1st difference if they don't)
 */
static int compare(const char* a,const char* b, size_t size){
  int i = 0;
  while ( i < size && ( ( a[i] != 0 ) || ( b[i]!= 0 ) ) ){
    if (a[i] != b[i]){
      return -(i+1);
    }
    i++;
  }
  return 0;
}

static void generate_cursor(char *o, const uint n){
  int i = 0;
  while (i < n-1){
    o[i++] =' ';
  }
  o[i++]='^';
  o[i]=0;
}

static FILE* open_file(size_t fname_len, const char* results_path, const char* type,
                       const char* p_id, int dev_nbr) {
  FILE *file;
  char filename[fname_len];
  sprintf(filename,"%s/d_%s_%02i.%s.csv",results_path, p_id, dev_nbr, type);

  if (comp == 1) {
    file = bs_fopen(filename, "r");
    bs_skipline(file); //Skip heading
    if ( feof(file) ){
      bs_trace_warning_line("%s file for device %i empty => won't be checked\n",type, dev_nbr);
      fclose(file);
      file = NULL;
    }
  } else {
    file = bs_fopen(filename, "w");
  }
  return file;
}

/**
 * Prepare dumping
 */
void open_dump_files(uint8_t comp_i, uint8_t stop, uint8_t dump_imm, const char* s,
                     const char* p, const uint n_dev_i){
  char* path;

  comp = comp_i;
  stop_on_diff = stop;
  n_dev = n_dev_i;

  path = bs_create_result_folder(s);

  stats = bs_calloc(n_dev, sizeof(t_dump_check_stats));
  txv1_f = bs_calloc(n_dev, sizeof(FILE *));
  rxv1_f = bs_calloc(n_dev, sizeof(FILE *));
  txv2_f = bs_calloc(n_dev, sizeof(FILE *));
  rxv2_f = bs_calloc(n_dev, sizeof(FILE *));
  RSSI_f = bs_calloc(n_dev, sizeof(FILE *));
  CCA_f  = bs_calloc(n_dev, sizeof(FILE *));
  modemrx_f = bs_calloc(n_dev, sizeof(FILE *));

  int fname_len = 26 + strlen(path) + strlen(p);
  for (int i = 0; i < n_dev; i++) {
    txv1_f[i] = open_file(fname_len, path, "Tx", p, i);
    rxv1_f[i] = open_file(fname_len, path, "Rx", p, i);
    //txv2_f[i] = open_file(fname_len, path, "Txv2", p, i);
    //rxv2_f[i] = open_file(fname_len, path, "Rxv2", p, i);
    //CCA_f[i]  = open_file(fname_len, path, "CCA", p, i);
    RSSI_f[i] = open_file(fname_len, path, "RSSI", p, i);
    modemrx_f[i] = open_file(fname_len, path, "ModemRx", p, i);

    if (comp == 0) {
      if (dump_imm) {
        if (txv1_f[i]) setvbuf(txv1_f[i], NULL, _IOLBF, 0);
        if (rxv1_f[i]) setvbuf(rxv1_f[i], NULL, _IOLBF, 0);
        if (txv2_f[i]) setvbuf(txv2_f[i], NULL, _IOLBF, 0);
        if (rxv2_f[i]) setvbuf(rxv2_f[i], NULL, _IOLBF, 0);
        if (RSSI_f[i]) setvbuf(RSSI_f[i], NULL, _IOLBF, 0);
        if (CCA_f[i]) setvbuf(CCA_f[i] , NULL, _IOLBF, 0);
        if (modemrx_f[i]) setvbuf(modemrx_f[i], NULL, _IOLBF, 0);
      }
      dump_txv1_heading(txv1_f[i]);
      dump_rxv1_heading(rxv1_f[i]);
      dump_txv2_heading(txv2_f[i]);
      dump_rxv2_heading(rxv2_f[i]);
      dump_RSSI_heading(RSSI_f[i]);
      dump_CCA_heading(RSSI_f[i]);
      dump_modemrx_heading(modemrx_f[i]);
    }
  }

  free(path);
}

static int print_stats(const char* type, int i, uint32_t n_op, uint32_t n_err, FILE *f) {
  int tracel;

  if (n_op > 0) {
    if (n_err == 0) {
      tracel = 4;
    } else {
      tracel = 1;
    }
    bs_trace_raw(tracel, "Check: Device %2i, %s: found %i/%i differences (%.1f%%)\n",
                 i, type, n_err, n_op, n_err*100.0/(float)n_op);
  }
  if (f == NULL) {
    bs_trace_raw(1, "Check: Device %2i, %s: comparison stopped before end of simulation\n",
                 i, type);
  }
  return (n_err!=0);
}

int close_dump_files() {
  int i;
  int ret_error = 0;

  if (stats != NULL) {
    if (comp) {
      for ( i = 0 ; i < n_dev; i ++) {
        ret_error |= print_stats("Tx", i, stats[i].nbr_txv1, stats[i].nbr_txv1_er, txv1_f[i]);
        ret_error |= print_stats("Rx", i, stats[i].nbr_rxv1, stats[i].nbr_rxv1_er, rxv1_f[i]);
        ret_error |= print_stats("Txv2", i, stats[i].nbr_txv2, stats[i].nbr_txv2_er, txv2_f[i]);
        ret_error |= print_stats("Rxv2", i, stats[i].nbr_rxv2, stats[i].nbr_rxv2_er, rxv2_f[i]);
        ret_error |= print_stats("RSSI", i, stats[i].nbr_RSSI, stats[i].nbr_RSSI_er, RSSI_f[i]);
        ret_error |= print_stats("CCA" , i, stats[i].nbr_CCA,  stats[i].nbr_CCA_er, CCA_f[i]);
        ret_error |= print_stats("ModemRx", i, stats[i].nbr_modemrx, stats[i].nbr_modemrx_er, modemrx_f[i]);
      }
    }
    free(stats);
    stats = NULL;
  }

  if ((txv1_f != NULL) && (rxv1_f != NULL) &&
      (txv2_f != NULL) && (rxv2_f != NULL) &&
      (RSSI_f != NULL) && (modemrx_f != NULL) &&
      (CCA_f != NULL) ) {
    for (i = 0; i < n_dev; i ++) {
      if (txv1_f[i] != NULL)
        fclose(txv1_f[i]);
      if (rxv1_f[i] != NULL)
        fclose(rxv1_f[i]);
      if (txv2_f[i] != NULL)
        fclose(txv2_f[i]);
      if (rxv2_f[i] != NULL)
        fclose(rxv2_f[i]);
      if (RSSI_f[i] != NULL)
        fclose(RSSI_f[i]);
      if (CCA_f[i] != NULL)
        fclose(CCA_f[i]);
      if (modemrx_f[i] != NULL)
        fclose(modemrx_f[i]);
    }
    free(txv1_f);
    free(rxv1_f);
    free(txv2_f);
    free(rxv2_f);
    free(RSSI_f);
    free(CCA_f);
    free(modemrx_f);
    txv1_f = NULL;
    rxv1_f = NULL;
    txv2_f = NULL;
    rxv2_f = NULL;
    RSSI_f = NULL;
    CCA_f = NULL;
    modemrx_f = NULL;
  }

  return ret_error;
}

void print_or_compare(FILE** file, char *produced, size_t str_size, uint dev_nbr,
                      const char* type, uint32_t *err_count, uint32_t op_nbr) {

  if ( comp == 0 ) {
    fprintf(*file,"%s\n",produced);
    return;
  }

  int result ;
  char file_content[str_size];

  bs_readline(file_content, str_size, *file);
  result = compare(produced, file_content,str_size);
  if (result == 0)
    return;

  char cursor[-result+1];
  if (file_content[0] == 0) {
    strcpy(file_content,"<eof>[End of file reached]");
    fclose(*file);
    *file = NULL;
  }
  *err_count +=1;
  bs_trace_raw(1,"Comp: Device %i, %s %i differs\n", dev_nbr, type, op_nbr);
  bs_trace_raw(2,"Comp: Read:\"%s\"\n", file_content);
  bs_trace_raw(2,"Comp:      \"%s\"\n", produced);
  generate_cursor(cursor, -result);
  bs_trace_raw(2,"Comp:       %s\n", cursor);

  if (stop_on_diff) {
    bs_trace_error("Simulation terminated due to differences in kill mode\n");
  }

  if (*err_count > MAX_ERRORS) {
    fclose(*file);
    *file = NULL;
    bs_trace_warning_line("Too many errors in %s file for device %i. It won't be checked anymore\n", type, dev_nbr);
  }
}

void dump_txv1(tx_el_t *tx, uint dev_nbr) {
  if ( ( txv1_f == NULL ) || ( txv1_f[dev_nbr] == NULL ) ){
    return;
  }

  stats[dev_nbr].nbr_txv1++;

  size_t size = 1024 + tx->tx_s.packet_size*3+1;
  p2G4_txv2_t *txs = &tx->tx_s;
  char to_print[size];
  int printed;
  printed = snprintf(to_print, 1024,
                    "%"PRItime",%"PRItime","
                    "%.6f,"
                    "0x%08X,%u,"
                    "%.6f,"
                    "%"PRItime",%"PRItime","
                    "%u,",
                    txs->start_tx_time, txs->end_tx_time,
                    p2G4_freq_to_d(txs->radio_params.center_freq),
                    (uint32_t)txs->phy_address, txs->radio_params.modulation,
                    p2G4_power_to_d(txs->power_level),
                    txs->abort.abort_time, txs->abort.recheck_time,
                    txs->packet_size);

  if ( tx->tx_s.packet_size > 0 ) {
    char packetstr[tx->tx_s.packet_size*3+1];
    bs_hex_dump(packetstr, tx->packet, tx->tx_s.packet_size);
    sprintf(&to_print[printed],"%s",packetstr);
  }

  print_or_compare(&txv1_f[dev_nbr],
                   to_print, size, dev_nbr, "Tx",
                   &stats[dev_nbr].nbr_txv1_er,
                   stats[dev_nbr].nbr_txv1);
}

void dump_txv2(tx_el_t *tx, uint dev_nbr) {
  if ( ( txv2_f == NULL ) || ( txv2_f[dev_nbr] == NULL ) ){
    return;
  }

  stats[dev_nbr].nbr_txv2++;

  size_t size = 1024 + tx->tx_s.packet_size*3+1;
  p2G4_txv2_t *txs = &tx->tx_s;
  char to_print[size];
  int printed;
  printed = snprintf(to_print, 1024,
                    "%"PRItime",%"PRItime","
                    "%"PRItime",%"PRItime","
                    "%.6f,"
                    "0x%08X,%u,"
                    "%.6f,"
                    "%"PRItime",%"PRItime","
                    "%u,",
                    txs->start_tx_time, txs->end_tx_time,
                    txs->start_packet_time, txs->end_packet_time,
                    p2G4_freq_to_d(txs->radio_params.center_freq),
                    (uint32_t)txs->phy_address, txs->radio_params.modulation,
                    p2G4_power_to_d(txs->power_level),
                    txs->abort.abort_time, txs->abort.recheck_time,
                    txs->packet_size);

  if ( tx->tx_s.packet_size > 0 ) {
    char packetstr[tx->tx_s.packet_size*3+1];
    bs_hex_dump(packetstr, tx->packet, tx->tx_s.packet_size);
    sprintf(&to_print[printed],"%s",packetstr);
  }

  print_or_compare(&txv2_f[dev_nbr],
                   to_print, size, dev_nbr, "Tx",
                   &stats[dev_nbr].nbr_txv2_er,
                   stats[dev_nbr].nbr_txv2);
}


void dump_tx(tx_el_t *tx, uint dev_nbr){
  dump_txv1(tx, dev_nbr);
  dump_txv2(tx, dev_nbr);
}

void dump_rxv1(rx_status_t *rx_st, uint8_t* packet, uint dev_nbr){
  if ( ( rxv1_f == NULL ) || ( rxv1_f[dev_nbr] == NULL ) ) {
    return;
  }

  stats[dev_nbr].nbr_rxv1++;

  const uint dbufsi = 2048;
  size_t size = dbufsi + rx_st->rx_done_s.packet_size*3;

  char to_print[ size + 1];
  p2G4_rxv2_t *req = &rx_st->rx_s;
  p2G4_rxv2_done_t *resp = &rx_st->rx_done_s;
  int printed;

  printed = snprintf(to_print, dbufsi,
                    "%"PRItime",%u,"
                    "0x%08X,%u,"
                    "%.6f,"
                    "%.6f,"
                    "%u,%u,"
                    "%u,"
                    "%u,%u,%"PRItime",%"PRItime","
                    "%i,%u,%"PRItime",%"PRItime","
                    "%"PRItime",%"PRItime",%u,"
                    "%.6f,"
                    "%u,",
                    req->start_time, req->scan_duration,
                    (uint32_t)rx_st->phy_address[0], req->radio_params.modulation,
                    p2G4_freq_to_d(req->radio_params.center_freq),
                    p2G4_power_to_d(req->antenna_gain),
                    req->sync_threshold, req->header_threshold,
                    req->pream_and_addr_duration,
                    req->header_duration, req->error_calc_rate, req->abort.abort_time, req->abort.recheck_time,

                    rx_st->tx_nbr,
                    rx_st->biterrors,
                    rx_st->sync_end,
                    rx_st->header_end,

                    rx_st->payload_end,
                    resp->rx_time_stamp,
                    resp->status,
                    p2G4_RSSI_value_to_dBm(resp->rssi.RSSI),

                    resp->packet_size);

  if ( ( resp->packet_size > 0 ) && ( packet != NULL ) ) {
    char packetstr[resp->packet_size*3+1];
    bs_hex_dump(packetstr, packet, resp->packet_size);
    sprintf(&to_print[printed],"%s",packetstr);
  }

  print_or_compare(&rxv1_f[dev_nbr],
                   to_print, size, dev_nbr, "Rx",
                   &stats[dev_nbr].nbr_rxv1_er,
                   stats[dev_nbr].nbr_rxv1);
}

void dump_rxv2(rx_status_t *rx_st, uint8_t* packet, uint dev_nbr){
  if ( ( rxv2_f == NULL ) || ( rxv2_f[dev_nbr] == NULL ) ) {
    return;
  }

  stats[dev_nbr].nbr_rxv2++;

  const uint dbufsi = 2048; //The print below adds to 591 + commas, + a lot of margin for careless expansion
  size_t size = dbufsi + rx_st->rx_done_s.packet_size*3;

  char to_print[ size + 1];
  p2G4_rxv2_t *req = &rx_st->rx_s;
  p2G4_rxv2_done_t *resp = &rx_st->rx_done_s;
  int printed;

  printed = snprintf(to_print, dbufsi,
                    "%"PRItime",%u," //20chars + 10chars
                    "%u,\"[",        //3 + (4+16*18)
                    req->start_time, req->scan_duration,
                    req->n_addr);

  for (int i = 0 ; i < req->n_addr ; i++) {
    printed += snprintf(&to_print[printed], dbufsi - printed,
        "0x%08"PRIx64, (uint64_t)rx_st->phy_address[i]);
    if (i < (int)req->n_addr - 1) {
      printed += snprintf(&to_print[printed], dbufsi - printed,
              ",");
    }
  }

  printed += snprintf(&to_print[printed], dbufsi - printed,
                    "]\", %u," //+4
                    "%.6f,"    //10
                    "%.6f,"    //10

                    "%u,"      //10
                    "%u,%u,"   //10+10
                    "%u,"      //10
                    "%u,%u,"   //10+10
                    "%u,"      //3
                    "%"PRItime",%"PRItime"," //20+20

                    "%i,"      //10
                    "0x%08"PRIx64"," //16
                    "%u,"      //10
                    "%"PRItime"," //20
                    "%"PRItime"," //20

                    "%"PRItime"," //20
                    "%"PRItime"," //20
                    "%u,"         //3
                    "%.6f,"       //10

                    "%u,",        //10
                    req->radio_params.modulation,
                    p2G4_freq_to_d(req->radio_params.center_freq),
                    p2G4_power_to_d(req->antenna_gain),

                    req->acceptable_pre_truncation,
                    req->sync_threshold, req->header_threshold,
                    req->pream_and_addr_duration,
                    req->header_duration, req->error_calc_rate,
                    req->resp_type,
                    req->abort.abort_time, req->abort.recheck_time,

                    rx_st->tx_nbr,
                    resp->phy_address,
                    rx_st->biterrors,
                    rx_st->sync_end,
                    rx_st->header_end,

                    rx_st->payload_end,
                    resp->rx_time_stamp,
                    resp->status,
                    p2G4_RSSI_value_to_dBm(resp->rssi.RSSI),

                    resp->packet_size);

  if ( ( resp->packet_size > 0 ) && ( packet != NULL ) ) {
    char packetstr[resp->packet_size*3+1];
    bs_hex_dump(packetstr, packet, resp->packet_size);
    sprintf(&to_print[printed],"%s",packetstr);
  }

  print_or_compare(&rxv2_f[dev_nbr],
                   to_print, size, dev_nbr, "Rxv2",
                   &stats[dev_nbr].nbr_rxv2_er,
                   stats[dev_nbr].nbr_rxv2);
}

void dump_rx(rx_status_t *rx_st, uint8_t* packet, uint dev_nbr) {
  dump_rxv1(rx_st, packet, dev_nbr);
  dump_rxv2(rx_st, packet, dev_nbr);
}

void dump_RSSImeas(p2G4_rssi_t *RSSI_req, p2G4_rssi_done_t* RSSI_res, uint dev_nbr){
  if ( ( RSSI_f == NULL ) || ( RSSI_f[dev_nbr] == NULL ) ){
    return;
  }

  stats[dev_nbr].nbr_RSSI++;

  size_t size = 512; //size below adds to a max of 53 chars + commas + a lot of margin for careless expansion
  char to_print[size +1];

  snprintf(to_print, size,
      "%"PRItime"," //20 chars
      "%u,%.6f,"    //3+10
      "%.6f,"       //10
      "%.6f",       //10
      RSSI_req->meas_time,
      RSSI_req->radio_params.modulation,
      p2G4_freq_to_d(RSSI_req->radio_params.center_freq),
      p2G4_power_to_d(RSSI_req->antenna_gain),
      p2G4_RSSI_value_to_dBm(RSSI_res->RSSI));

  print_or_compare(&RSSI_f[dev_nbr],
                   to_print, size, dev_nbr, "RSSI",
                   &stats[dev_nbr].nbr_RSSI_er,
                   stats[dev_nbr].nbr_RSSI);
}


void dump_cca(cca_status_t *cca, uint dev_nbr) {
  if ( ( CCA_f == NULL ) || ( CCA_f[dev_nbr] == NULL ) ){
    return;
  }

  stats[dev_nbr].nbr_CCA++;

  size_t size = 512; //size below adds to a max of 180chars + commas + a lot of margin for careless expansion
  char to_print[size +1];

  snprintf(to_print, size,
      "%"PRItime"," //20
      "%u," //10
      "%u," //10

      "%u," //3
      "%.6f," //10
      "%.6f," //10

      "%.6f," //10
      "%.6f," //10
      "%u,"   //3

      "%"PRItime",%"PRItime"," //20+20

      "%"PRItime"," //20
      "%.6f," //10
      "%.6f," //10
      "%.6f," //10
      "%u,%u" //2+2
      ,

      cca->req.start_time,
      cca->req.scan_duration,
      cca->req.scan_period,

      cca->req.radio_params.modulation,
      p2G4_freq_to_d(cca->req.radio_params.center_freq),
      p2G4_power_to_d(cca->req.antenna_gain),

      p2G4_power_to_d(cca->req.mod_threshold),
      p2G4_power_to_d(cca->req.rssi_threshold),
      cca->req.stop_when_found,

      cca->req.abort.abort_time, cca->req.abort.recheck_time,

      cca->resp.end_time,
      p2G4_RSSI_value_to_dBm(cca->resp.RSSI_ave),
      p2G4_RSSI_value_to_dBm(cca->resp.RSSI_max),
      p2G4_RSSI_value_to_dBm(cca->resp.mod_rx_power),
      cca->resp.mod_found, cca->resp.rssi_overthreshold
      );



  print_or_compare(&CCA_f[dev_nbr],
                   to_print, size, dev_nbr, "CCA",
                   &stats[dev_nbr].nbr_CCA_er,
                   stats[dev_nbr].nbr_CCA);
}

void dump_ModemRx(bs_time_t CurrentTime, uint tx_nbr, uint dev_nbr, uint ndev, uint CalNotRecal, p2G4_radioparams_t *radio_p, rec_status_t *rx_st, tx_l_c_t *tx_l ){
  if ( ( modemrx_f == NULL ) || ( modemrx_f[dev_nbr] == NULL ) ) {
    return;
  }

  stats[dev_nbr].nbr_modemrx++;

#define _ModemRxStrSize 4096
  size_t size = _ModemRxStrSize;
  char to_print[_ModemRxStrSize +1];

  uint printed = snprintf(to_print, _ModemRxStrSize,
                          "%"PRItime",%u,"
                          "%u,%f,%u,"
                          "%e,%e,"
                          "%f,%f,%f",
                          CurrentTime,
                          tx_nbr,

                          CalNotRecal,
                          p2G4_freq_to_d(radio_p->center_freq),
                          radio_p->modulation,

                          rx_st->BER/(double)RAND_PROB_1,
                          rx_st->sync_prob/(double)RAND_PROB_1,

                          rx_st->SNR_total,
                          rx_st->SNR_analog_o,
                          rx_st->SNR_ISI);

  for ( uint tx = 0 ; tx< ndev; tx++){
    if ( tx_l->used[tx] ){
      printed += snprintf(&to_print[printed], _ModemRxStrSize-printed, ",%f,%f", rx_st->att[tx], rx_st->rx_pow[tx]);
    } else {
      printed += snprintf(&to_print[printed], _ModemRxStrSize-printed, ",NaN, NaN");
    }
    if (printed >= _ModemRxStrSize){
      bs_trace_warning_line("Too many devices, ModemRx dumping disabled\n");
      fclose(modemrx_f[dev_nbr]);
      modemrx_f[dev_nbr] = NULL;
    }
  }

  print_or_compare(&modemrx_f[dev_nbr],
                   to_print, size, dev_nbr, "ModemRx",
                   &stats[dev_nbr].nbr_modemrx_er,
                   stats[dev_nbr].nbr_modemrx_er);
}
