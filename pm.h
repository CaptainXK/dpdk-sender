#ifndef PM_H
#define PM_H

#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_tcp.h>
#include <rte_vxlan.h>
#include "my_ndn.h"

struct packet_model
{
    struct
    {
		  struct my_ndn ndn;
    }__attribute__((aligned(8))) ndn;
    
    int is_ndn;
};

#endif
