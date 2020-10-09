#! /bin/bash

#t data file
#s speed Mbps
#L pkt length
TRACE_DIR=`pwd`"/data"
SPEED=$1
PKT_SIZE=$2

echo "Test Packt Size=$PKT_SIZE Bytes"

./build/pkt-sender -c 0x1ff -n 4 -w 3b:00.0 -w 3b:00.1 -- -t $TRACE_DIR/name.csv -s $SPEED -L $PKT_SIZE -P 0x1
