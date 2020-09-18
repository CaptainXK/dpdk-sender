#ifndef NDN_H
#define NDN_H

#include <unistd.h>
#include <rte_ether.h>

struct my_ndn{
  struct rte_ether_hdr ether;
  uint32_t type;
  uint32_t name_len;
  uint64_t tsc;
  uint8_t name[256];
}__attribute__((aligned(8)));

struct test_pkt{
  uint16_t src_port;
  uint16_t dst_port;
  uint16_t fwd_port;
  uint16_t  proto_type;
  uint8_t src_ip[16];
  uint8_t dst_ip[16];
  uint8_t  name[100]; 
}__attribute__((packed));

#endif
