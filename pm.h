#ifndef PM_H
#define PM_H

#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_tcp.h>
#include <rte_vxlan.h>
#include "ndn_hdr.h"

struct packet_model
{
    struct
    {
        struct ndn_hdr ndn;
    } ndn;

    int is_ndn;
};

#endif
