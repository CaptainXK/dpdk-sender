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

echo "Test Packt Size=$PKT_SIZE Bytes"

./build/pkt-sender -c 0x1fff -n 4 -w 3b:00.0 -w 3b:00.1 -- -t ${TRACE_DIR}/${TRACE_FILE} -s $SPEED -L $PKT_SIZE -P $TP_MAP
