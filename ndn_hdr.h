#ifndef NDN_H
#define NDN_H

#include <unistd.h>
#include <rte_ether.h>

#include "common.h"

struct __attribute__((aligned)) ndn_hdr{
  struct rte_ether_hdr ether;

#ifdef RSS_ON
  struct rte_ipv4_hdr ipv4;
  struct rte_tcp_hdr tcp;
#endif

  uint32_t name_len;
  char name[256];
};

#endif
