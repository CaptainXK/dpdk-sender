#ifndef PM_H
#define PM_H

#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_tcp.h>
#include "my_ndn.h"

struct packet_model
{
    struct
    {
        struct ether_hdr eth;
        struct vlan_hdr vlan;
        struct ipv4_hdr ip;
        struct udp_hdr udp;
        struct vxlan_hdr vx;
    }__attribute__((__packed__)) vxlan;
    struct
    {
        struct ether_hdr eth;
        struct ipv4_hdr ip;
        struct tcp_hdr tcp;
    }__attribute__((__packed__)) tcp;
    struct
    {
        struct ether_hdr eth;
        struct ipv4_hdr ip;
        struct udp_hdr udp;
    }__attribute__((__packed__)) udp;
    struct
    {
		struct my_ndn ndn;
    }__attribute__((__packed__)) ndn;
    struct
    {
		struct test_pkt test;
    }__attribute__((__packed__)) test;
    struct
    {
		struct pp_hdr pp_hdr;
    }__attribute__((__packed__)) pp;
    
    int is_tcp;
    int is_udp;
    int is_vxlan;
    int is_ndn;
    int is_test;
	  int is_pp;
};

#endif
