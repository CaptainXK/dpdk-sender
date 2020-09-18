#include "tx_mp.h"
#include <rte_lcore.h>
#include <stdio.h>
#include <string.h>
#include <rte_eal.h>
#include <rte_common.h>
#include <rte_mempool.h>
#include <rte_memory.h>
#include <rte_mbuf.h>
#include <rte_cycles.h>/*timestamp*/
#include "my_ndn.h"

struct rte_mbuf* generate_mbuf(struct packet_model pm, struct rte_mempool *mp, uint32_t pkt_length)
{
    struct rte_mbuf *m;

    m = rte_pktmbuf_alloc(mp);
    if(m == NULL)
    {
        rte_exit(-1, "mempool is empty!\n");
    }
    char *data;
    
    if(pm.is_ndn)
    {
        data = rte_pktmbuf_append(m, sizeof(pm.ndn));
        if(data == NULL)
        {
            rte_exit(-1, "mbuf append ndn hdr failed!\n");
        }
        rte_memcpy(data, &(pm.ndn), sizeof(pm.ndn));

		    /*rest part of pkt*/
        data = rte_pktmbuf_append(m, pkt_length - sizeof(pm.ndn));
		    if(data == NULL)
        {
               rte_exit(-1, "mbuf append ndn data failed!\n");
        }
    }
    
    return m;
}

struct rte_mempool* tx_mempool_create(int n, int lcore_id)
{
    char name[64];
    struct rte_mempool *mp;
    
    snprintf(name, 64, "tx_mempool_lcore(%d)", lcore_id);
    mp = rte_pktmbuf_pool_create(name, n, MAX_TX_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_DATAROOM, rte_lcore_to_socket_id(lcore_id));
    if(mp == NULL)
    {
        rte_exit(EINVAL, "create mempool for lcore %d failed\n", lcore_id);
    }
    rte_mempool_dump(stdout, mp);
    return mp;
}

