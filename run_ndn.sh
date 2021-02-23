#! /bin/bash

#t data file
#s speed Mbps
#L pkt length
TRACE_DIR=`pwd`"/data"
# TRACE_FILE=test.csv
TRACE_FILE=traces.csv
SPEED=$1
PKT_SIZE=$2
TP_MAP=0x1
CORE_LIST=16-31
P0_PCIE=81:00.0
P1_PCIE=81:00.1

echo "Test Packt Size=$PKT_SIZE Bytes"

sudo ./build/pkt-sender -l 16-31 -n 4 -w ${P0_PCIE} -w ${P1_PCIE} -- -t ${TRACE_DIR}/${TRACE_FILE} -s $SPEED -L $PKT_SIZE -P $TP_MAP
