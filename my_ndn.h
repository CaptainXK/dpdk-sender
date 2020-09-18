#ifndef NDN_H
#define NDN_H

#include <unistd.h>
#include <rte_ether.h>

struct __attribute__((aligned)) my_ndn{
  struct rte_ether_hdr ether;
  uint32_t name_len;
  uint8_t name[256];
};

#endif
