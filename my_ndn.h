#ifndef NDN_H
#define NDN_H

#include <unistd.h>
#include <rte_ether.h>

struct my_ndn{
  uint32_t type;
  uint32_t name_len;
  uint64_t tsc;
  uint8_t name[100];
}__attribute__((__packed__));//size : 4+4+8+1*100=116

struct test_pkt{
  uint16_t src_port;
  uint16_t dst_port;
  uint16_t fwd_port;
  uint16_t  proto_type;
  uint8_t src_ip[16];
  uint8_t dst_ip[16];
  uint8_t  name[100]; 
}__attribute__((__packed__));//size : 16+16+2+2+2+1+1*100=139

struct pp_hdr{
	struct ether_hdr eth;//16
  uint8_t payload[32];//32
	uint32_t action;//4
	uint32_t id;//4
}__attribute__((__packed__));// size = 16 + 32 + 4 + 4 = 56

#endif
