#ifndef TX_MP
#define TX_MP

#include <rte_eal.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include "pm.h"

#define MIN_TX_POOL_SIZE 2048
#define MAX_TX_CACHE_SIZE 512

#ifdef __cplusplus
extern "C"{
#endif

struct rte_mempool* tx_mempool_create(int n, int lcore_id);

struct rte_mbuf* generate_mbuf(struct packet_model pm, struct rte_mempool *mp, uint32_t pkt_length);

#ifdef __cplusplus
}
#endif

#endif
