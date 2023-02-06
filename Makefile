# Copyright 2018 Oticon A/S
# SPDX-License-Identifier: Apache-2.0

BSIM_BASE_PATH?=$(abspath ../ )
include ${BSIM_BASE_PATH}/common/pre.make.inc

2G4_libPhyComv1_COMP_PATH?=$(abspath ${BSIM_COMPONENTS_PATH}/ext_2G4_libPhyComv1)

EXE_NAME:=bs_2G4_phy_v1
SRCS:= src/p2G4_func_queue.c \
       src/p2G4_main.c \
       src/p2G4_v1_v2_remap.c \
       src/p2G4_com.c \
       src/p2G4_args.c \
       src/p2G4_pending_tx_list.c \
       src/p2G4_dump.c \
       src/p2G4_channel_and_modem.c \

A_LIBS:=${BSIM_LIBS_DIR}/libUtilv1.a \
        ${BSIM_LIBS_DIR}/libPhyComv1.a \
        ${BSIM_LIBS_DIR}/libRandv2.a \
        ${BSIM_LIBS_DIR}/lib2G4PhyComv1.a
SO_LIBS:=

INCLUDES:=-I${libUtilv1_COMP_PATH}/src/ \
          -I${libPhyComv1_COMP_PATH}/src/ \
          -I${libRandv2_COMP_PATH}/src/ \
          -I${2G4_libPhyComv1_COMP_PATH}/src

DEBUG:=-g
OPT:= -O2 -fno-strict-aliasing
ARCH:=
WARNINGS:=-Wall -pedantic
COVERAGE:=
CFLAGS:=${ARCH} ${DEBUG} ${OPT} ${WARNINGS} -MMD -MP -std=c99 ${INCLUDES}
LDFLAGS:=${ARCH} ${COVERAGE} -ldl -rdynamic -lm
#-ldl : link to the dl library: we will use the dinamic runtime library linking (for the selected channel and modems)
#-rdynamic : the global symbols in the executable will also be used to resolve references in dynamically loaded libraries. 
#-z now: When generating an executable or shared library, mark it to tell the dynamic linker to resolve all symbols when the program is started
CPPFLAGS:=-D_XOPEN_SOURCE=700

include ${BSIM_BASE_PATH}/common/make.device.inc
