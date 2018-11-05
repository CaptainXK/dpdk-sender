#! /bin/bash

#echo "***Set RTE_SDK to:"
#sdk="/home/ubu/dpdk/dpdk-16.07.2"
#export RTE_SDK=$sdk
#echo $RTE_SDK
#export RTE_TARGET=x86_64-native-linuxapp-gcc
#echo $RTE_TARGET
#export PKTGEN_DPDK=/home/ubu/dpdk/pktgen-v3.1.0
#echo ""
#
#echo "***Current HugePage:"
#cat /proc/meminfo | grep Huge
#echo ""
#echo "ReSet HugePage to 1024*2M."
#umount /mnt/huge
#rm -rf /mnt/huge
#mkdir -p /mnt/huge
#echo 4096 >/sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
#mount -t hugetlbfs nodev /mnt/huge
#echo "***New HugePage:"
#cat /proc/meminfo | grep Huge
#echo ""
#
#echo "***Current netports:"
#python $RTE_SDK/tools/dpdk-devbind.py -s
#echo ""
#echo "bind netports to dpdk:"
#modprobe uio
#insmod $RTE_SDK/$RTE_TARGET/kmod/igb_uio.ko #build/kmod/igb_uio.ko
##grep -i numa /var/log/dmesg
#python $RTE_SDK/tools/dpdk-devbind.py -b igb_uio 42:00.0 42:00.1 42:00.2 42:00.3
#echo ""
#echo "show binded netports:"
#python $RTE_SDK/tools/dpdk-devbind.py -s
#echo ""

#t data file
#s speed Mbps
#L pkt length
TRACE_DIR="/home/xuke/git/gitlab/pkt-sender-ts/data"

./build/pkt-sender -c 0x1ff -n 2 -- -t $TRACE_DIR/pp/1_1000_match.traffic -s $1 -L 256
#./build/pkt-sender -c 0x7f -n 2 -- -t $TRACE_DIR/data/2_1000.ipv4.test.match -s $1 -L 256
#./build/pkt-sender -c 0x7f -n 2 -- -t $TRACE_DIR/name.trace -s $1 -L 256
