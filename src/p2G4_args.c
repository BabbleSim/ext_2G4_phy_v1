/*
 * Copyright 2018 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "p2G4_args.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "bs_tracing.h"
#include "bs_types.h"
#include "bs_oswrap.h"

char executable_name[] = "bs_2G4_phy_v1";

void component_print_post_help(){
  fprintf(stdout,"\n"
" Note regarding multiple modems: the default modem (and the default modem arguments)\n"
" are assigned to any modem which does not get assigned any other modem with\n"
" -modem<nbr>=<modem>.\n"
"\n"
"bs_2G4_phy_v1: Emulates the physical layer, including all devices modem's and the radio channel\n");
}

#define MAXPARAMS_LIBRARIES 1024

static char default_channel[] = "NtNcable";
static char default_modem[]   = "Magic";

static inline void allocate_modems_params(p2G4_args_t *args){
  args->modem_name = (char **)bs_calloc(args->n_devs, sizeof(char*));
  args->modem_argc = (uint *)bs_calloc(args->n_devs, sizeof(uint));
  args->modem_argv = (char ***)bs_calloc(args->n_devs, sizeof(char**));
}

p2G4_args_t *args_g;

static void cmd_trace_lvl_found(char * argv, int offset){
  bs_trace_set_level(args_g->verb);
}
static void phy_id_found(char * argv, int offset){
  bs_trace_set_prefix_phy(args_g->p_id);
}
double sim_length;
static void sim_length_found(char * argv, int offset){
  args_g->sim_length = sim_length;
  bs_trace_raw(9,"cmdarg: sim_length set to %"PRItime"\n", args_g->sim_length);
}
static void stop_found(char * argv, int offset){
  args_g->compare = true;
}
static void dump_found(char * argv, int offset){
  args_g->dont_dump = false;
  bs_trace_raw(9,"cmdarg: dump mode set\n");
}
static void channel_found(char * argv, int offset){
  bs_trace_raw(9,"cmdarg: channel set to libChannel_%s.so\n",args_g->channel_name);
}
static void defmodem_found(char * argv, int offset){
  bs_trace_raw(9,"cmdarg: defmodem set to libModem_%s.so\n",args_g->defmodem_name);
}
/**
 * Check the arguments provided in the command line: set args based on it
 * or defaults, and check they are correct
 */
void p2G4_argsparse(int argc, char *argv[], p2G4_args_t *args)
{

  args_g = args;
  bs_args_struct_t args_struct[] = {
      ARG_TABLE_S_ID,
      { false, false , false,  "p_id",       "phy_id",       's', (void*)&args->p_id,  phy_id_found,   "(2G4) String which uniquely identifies the phy inside the simulation"},
      /*manual,mandatory,switch,option,     name ,           type,       destination,         callback,             , description*/
      { true,  true  , false,  "D",        "number_devices", 'u', (void*)&args->n_devs,  NULL,         "Number of devices which will connect in this phy"},
      { false, false  , false, "sim_length",  "sim_length",  'f', (void*)&sim_length, sim_length_found,"In us, length of the simulation"},
      ARG_TABLE_VERB,
      ARG_TABLE_SEED,
      ARG_TABLE_COLOR,
      ARG_TABLE_NOCOLOR,
      ARG_TABLE_FORCECOLOR,
      { false, false  , true,  "nodump",    "no_dump",  'b', (void*)&args->dont_dump,      NULL,         "Will not dump (or compare) any files"},
      { false, false  , true,  "dump",      "dump",     'b', (void*)NULL,                 dump_found,    "Revert -nodump option (note that the last -nodump/dump set in the command line prevails)"},
      { false, false  , true,  "c",          "compare", 'b', (void*)&args->compare,        NULL,         "Run in compare mode: will compare instead of dumping"},
      { false, false  , true,  "stop_on_diff","stop",   'b', (void*)&args->stop_on_diff,  stop_found,    "Run in compare mode, but stop as soon as a difference is found"},
      { false, false  , false, "channel",    "channel", 's', (void*)&args->channel_name,  channel_found, "Which channel will be used ( lib/lib_2G4Channel_<channel>.so ). By default NtNcable"},
      { false, false  , false, "defmodem",   "modem",   's', (void*)&args->defmodem_name, defmodem_found,"Which modem will be used by default for all devices ( lib/lib_2G4Modem_<modem>.so ). By default Magic"},
      { true,  false  , false, "modem<nbr>", "modem",   's', (void*)NULL,                  NULL,         "Which modem will be used for the device <nbr> ( lib/lib_2G4Modem_<modem>.so )"},
      { true,  false  , false, "argschannel","arg",     'l', (void*)NULL,                  NULL,         "Following arguments (until end or new -args*) will be passed to the channel"},
      { true,  false  , false, "argsdefmodem","arg",    'l', (void*)NULL,                  NULL,         "Following arguments (until end or new -args*) will be passed to the modems set to be the default modem"},
      { true,  false  , false, "argsmodem<nbr>","arg",  'l', (void*)NULL,                  NULL,         "Following arguments (until end or new -args*) will be passed to the modem of the device <nbr>"},
      { true,  false  , false, "argsmain","arg",        'l', (void*)NULL,                  NULL,         "Following arguments (until end or new -args*) will be passed to the phy itself (default)"},
      ARG_TABLE_ENDMARKER
  };

#define MAIN       0
#define CHANNEL    1
#define DEFMODEM   2
#define MODEM   1000

  uint parsing = MAIN;
  static char default_phy[] ="2G4";

  args->verb       = 2;
  bs_trace_set_level(args->verb);
  args->rseed      = 0xFFFF;
  args->sim_length = TIME_NEVER - 1000000000 ; //1Ksecond before never by default

  args->channel_argv    = bs_calloc(MAXPARAMS_LIBRARIES*2, sizeof(char *));
  args->channel_argc    = 0;
  args->channel_name    = default_channel;

  args->defmodem_argv   = args->channel_argv + MAXPARAMS_LIBRARIES;
  args->defmodem_argc   = 0;
  args->defmodem_name   = default_modem;

  args->modem_argv = NULL;
  args->modem_argc = NULL;
  args->modem_name = NULL;

  char trace_prefix[] = "cmdarg: ";
  bs_args_set_trace_prefix(trace_prefix);

  int offset;
  uint modem_nbr;

  for (int i=1; i<argc; i++){ 

    //First we check if we are getting the option to switch the type of args:
    if ((strncmp(argv[i], "-args",5)==0) || (strncmp(argv[i], "--args",6)==0)) {
      if (bs_is_option(argv[i], "argschannel", 0)) {
        parsing = CHANNEL;
      } else if ( bs_is_option(argv[i], "argsdefmodem", 0)) {
        parsing = DEFMODEM;
      } else if ( bs_is_option(argv[i], "argsmain", 0)) {
        parsing = MAIN;
      } else if (bs_is_multi_opt(argv[i], "argsmodem", &modem_nbr ,0)) {
        if ( args->n_devs == 0 ) {
          bs_trace_error_line("cmdarg: tried to set the modem args for a device"
                              " (%i) before setting the number of devices (-D="
                              "<nbr>) (%s)\n\n""\n",
                              modem_nbr, args->n_devs, argv[i]);
        }
        if ( modem_nbr >= args->n_devs ) {
          bs_trace_error_line("cmdarg: tried to set the modem args for a device "
                              "%i >= %i number of avaliable devices (%s)\n\n""\n",
                              modem_nbr, args->n_devs, argv[i]);
        }
        if ( args->modem_name[modem_nbr] == NULL ) {
          bs_trace_error_line("cmdarg: tried to set the modem args for a modem "
                              "(%i) not yet specified (%s)\n\n""\n",
                              modem_nbr, argv[i]);
        }
        parsing = MODEM + modem_nbr;
      } else {
        bs_args_print_switches_help(args_struct);
        bs_trace_error_line("Incorrect args option '%s'\n",argv[i]);
      }
      continue;
    }

    //Otherwise, depending on what we are parsing now, we handle things:
    if ( parsing == MAIN ){
      if ( !bs_args_parse_one_arg(argv[i], args_struct) ){
        if ((offset = bs_is_option(argv[i], "D", 1)) > 0) {
          if (args->n_devs != 0) {
            bs_trace_error_line("The number of devices (-D) can only be "
                                "specified once, found as argument %i: %s\n",
                                i, argv[i]);
          }
          bs_read_optionparam(&argv[i][offset], (void*)&(args->n_devs), 'u', "nbr_devices");
          allocate_modems_params(args);

        } else if ((offset = bs_is_multi_opt(argv[i], "modem", &modem_nbr, 1))>0) {
          if ( args->n_devs == 0 ) {
            bs_trace_error_line("cmdarg: tried to set the modem for a device "
                                "(%i) before setting the number of devices (-D"
                                "=<nbr>) (%s)\n",
                                modem_nbr, args->n_devs, argv[i]);
          }
          if ( modem_nbr >= args->n_devs ) {
            bs_trace_error_line("cmdarg: tried to set the modem for a device "
                                "%i >= %i number of avaliable devices (%s)\n",
                                modem_nbr, args->n_devs, argv[i]);
          }
          args->modem_name[modem_nbr] = &argv[i][offset];
          bs_trace_raw(9, "cmdarg: modem[%u] set to libModem_%s.so\n",
                       modem_nbr, args->modem_name[modem_nbr]);
        }
        else {
          bs_args_print_switches_help(args_struct);
          bs_trace_error_line("Incorrect phy main option %s\n",argv[i]);
        }
      }

    } else if ( parsing == CHANNEL ){
      if ( args->channel_argc >= MAXPARAMS_LIBRARIES ) {
        bs_trace_error_line("Too many channel command line parameters (at '%s'), maximum is %i\n", argv[i],MAXPARAMS_LIBRARIES);
      } else {
        args->channel_argv[ args->channel_argc ] = argv[i];
        bs_trace_raw(9,"cmdarg: adding '%s' to channel args[%i]\n", argv[i], args->channel_argc);
        args->channel_argc++;
      }

    } else if ( parsing == DEFMODEM ){
      if ( args->channel_argc >= MAXPARAMS_LIBRARIES ) {
        bs_trace_error_line("Too many defmodem command line parameters (at '%s'), maximum is %i\n", argv[i],MAXPARAMS_LIBRARIES);
      } else {
        args->defmodem_argv[ args->defmodem_argc ] = argv[i];
        bs_trace_raw(9,"cmdarg: adding '%s' to defmodem args[%i]\n", argv[i], args->defmodem_argc);
        args->defmodem_argc++;
      }

    } else if ( parsing >= MODEM ){
      uint modem_nbr = parsing - MODEM;
      if ( args->modem_argc[modem_nbr] >= MAXPARAMS_LIBRARIES ) {
        bs_trace_error_line("Too many modem%i command line parameters (at '%s'), maximum is %i\n",modem_nbr, argv[i],MAXPARAMS_LIBRARIES);
      } else {
        if ( args->modem_argc[modem_nbr] == 0 ){
          args->modem_argv[modem_nbr]  = bs_calloc(MAXPARAMS_LIBRARIES, sizeof(char *));
        }
        args->modem_argv[modem_nbr][ args->modem_argc[modem_nbr] ] = argv[i];
        bs_trace_raw(9,"cmdarg: adding '%s' to modem%i args[%i]\n", argv[i], modem_nbr, args->modem_argc[modem_nbr]);
        args->modem_argc[modem_nbr]++;
      }
    }
  }

  if ( args->s_id == NULL ) {
    bs_args_print_switches_help(args_struct);
    bs_trace_error_line("TThe command line option <simulation ID> needs to be set\n");
  }
  if ( args->p_id == NULL ){
    args->p_id = default_phy;
    bs_trace_set_prefix_phy(args->p_id);
  }
  if ( args->n_devs == 0 ) {
    bs_args_print_switches_help(args_struct);
    bs_trace_error_line("The command line option <number_devices> needs to be set\n");
  }
  //for all modems which didn't get set to something else, we set them to the default modem
  for (uint dev = 0; dev < args->n_devs ; dev ++){
    if (args->modem_name[dev] == NULL) { //if we didnt set for this modem already
      args->modem_name[dev] = args->defmodem_name;
      args->modem_argv[dev] = args->defmodem_argv;
      args->modem_argc[dev] = args->defmodem_argc;
    }
  }
}

void p2G4_clear_args_struct(p2G4_args_t *args){
  for (uint n = 0; n < args->n_devs ; n++) {
    if ((args->modem_argv[n] != NULL) && (args->modem_argv[n] != args->defmodem_argv)) {
      free(args->modem_argv[n]);
    }
  }

  if ( args->channel_argv != NULL ){
    free(args->channel_argv);
  }
  if (args->modem_name != NULL) {
    free(args->modem_name);
  }

  if (args->modem_argc != NULL) {
      free(args->modem_argc);
  }

  if (args->modem_argv != NULL) {
      free(args->modem_argv);
  }
}
