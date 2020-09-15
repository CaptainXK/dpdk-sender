#! /bin/bash

#t data file
#s speed Mbps
#L pkt length
TRACE_DIR=`pwd`"/data"
SPEED=$1
PKT_SIZE=$2

echo "Test Packt Size=$PKT_SIZE Bytes"

./build/pkt-sender -c 0x1ff -n 2 -- -t $TRACE_DIR/name.csv -s $SPEED -L $PKT_SIZE
