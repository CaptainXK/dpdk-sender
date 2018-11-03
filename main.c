#include <stdio.h>
#include <netinet/in.h>
#include <stddef.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <inttypes.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_timer.h>
#include <rte_eal.h>
#include <rte_debug.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_ethdev.h>

#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_tcp.h>


#include "pm.h"
#include "tload.h"
#include "tx_mp.h"
#include "my_ndn.h"

#define GEN_VXLAN

#define NB_MBUF 65535
#define MBUF_CACHE_SIZE 512
#define NB_BURST 32//debug , original value is 32
#define NB_TXQ 2
#define NB_RXQ 1

#define NB_RXD_DFT 4096//debug, original value is 128
#define NB_TXD_DFT 512//debug, original value is 512
#define NB_TEST 10000
const int nb_rxd = NB_RXD_DFT;
const int nb_txd = NB_TXD_DFT;
static int send_turn=0;//max send round
static int lcore_cur_turn[10]={0,0,0,0,0,0,0,0,0,0};//current send round for each lcore
static int lcore_end[10]={0,0,0,0,0,0,0,0,0,0};//signal of end sending for each lcore
FILE * res_fp;//res data file
const int max_test_nb = 10000;
static int __attribute__((unused)) cur_test_round=0;

//pp test var
#define PP_TEST 1
unsigned long long pp_tot_recv = 0;
unsigned long long pp_tot_error = 0;

#define NB_MAX_PM 1000001

#define TX_RATE_DFT 10000

#define PRINT_GAP 2


/* global data */
struct global_info
{
    uint32_t total_trace;
    char trace_file[256];
    uint64_t Mbps;
};

struct global_info ginfo =
{
	0,
	"",
    TX_RATE_DFT,
};

/* generate mbuf */
struct packet_model pms[NB_MAX_PM];

/* pkt length*/
extern uint32_t pkt_length;

static void usage()
{
    printf("Usage: pkt-sender <EAL options> -- -t <trace_file> -s <Mbps> -L <pkt_length> -T <send_rounds>\n");
    rte_exit(-1, "invalid arguments!\n");
}

static void parse_params(int argc, char **argv)
{
    char opt;
    int accept = 0;
#ifndef PP_TEST
		char res_path[256]="";
#endif
    while((opt = getopt(argc, argv, "t:s:L:T:")) != -1)
    {
        switch(opt)
        {
            case 't': rte_memcpy(ginfo.trace_file, optarg, strlen(optarg)+1); accept = 1; break;
            case 's': ginfo.Mbps = atoi(optarg);
                      if(ginfo.Mbps <= 0)
                      {
                          rte_exit(EINVAL, "tx rate (Mbps) is invalid!\n");
                      }
#ifndef PP_TEST
/*res_data file*/
					  sprintf(res_path, "res_%lu", ginfo.Mbps);
					  res_fp = fopen(res_path, "w");
					  if(res_fp == NULL)
					  {
						  perror("Can not open res file");
						  exit(1);
					  }
#endif
                      break;
            case 'L':
                      pkt_length = atoi(optarg);
                      if(pkt_length < 64)
                      {
                          rte_exit(EINVAL, "pkt_length should be no less than 64!\n");
                      }
                      break;
			case 'T': send_turn = atoi(optarg);break;
            default: usage();
        }
    }
    if(!accept)
    {
        usage();
    }

}
/**************************************************************/

/* port init */
struct rte_eth_conf port_conf;
//struct rte_eth_conf port_conf =
//{
//    .rxmode=
//    {
//        .max_rx_pkt_len=ETHER_MAX_LEN,
//    },
//};

/* don't care about NUMA */
static int all_port_setup(struct rte_mempool *mp)
{
    int ret;
    int nb_ports;
    int port_id;
    int queue_id;
    nb_ports = rte_eth_dev_count();
	port_conf.rxmode.max_rx_pkt_len = ETHER_MAX_LEN;
	
	//debug, tx & rx conf, dev_info
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf *tx_conf;
	struct rte_eth_rxconf *rx_conf;

    for(port_id = 0; port_id < nb_ports; port_id++)
    {
        ret = rte_eth_dev_configure(port_id, NB_RXQ, NB_TXQ, &port_conf);
        if(ret < 0)
        {
            rte_exit(-1, "port %d configure failure!\n", port_id);
        }
        for(queue_id = 0; queue_id < NB_TXQ; queue_id++)
        {
			//modify tx thresh
			rte_eth_dev_info_get(port_id, &dev_info);					
			tx_conf = &dev_info.default_txconf;
	//		tx_conf->tx_rs_thresh = 1024;
	//		tx_conf->tx_free_thresh = 1024;			

            ret = rte_eth_tx_queue_setup(port_id, queue_id, nb_txd, rte_eth_dev_socket_id(port_id), tx_conf);
            if(ret != 0)
            {
                rte_exit(-1, "port %d tx queue setup failure!\n", port_id);
            }
        }
        for(queue_id = 0; queue_id < NB_RXQ; queue_id++)
        {
			//modify rx thresh
			rte_eth_dev_info_get(port_id, &dev_info);
			rx_conf = &dev_info.default_rxconf;

            ret = rte_eth_rx_queue_setup(port_id, queue_id, nb_rxd, rte_eth_dev_socket_id(port_id), rx_conf, mp);
            if(ret != 0)
            {
                rte_exit(-1, "port %d rx queue setup failure!\n", port_id);
            }
        }
        ret = rte_eth_dev_start(port_id);
        if(ret < 0)
        {
            rte_exit(-1, "port %d start failure!\n", port_id);
        }
        rte_eth_promiscuous_enable(port_id);
    }
    return nb_ports;
}


/* lcore main */

struct port_stats_info
{
    struct
    {
        uint64_t tx_total_pkts;
        uint64_t tx_last_total_pkts;
    }txq_stats[NB_TXQ];

    struct
    {
        uint64_t rx_total_pkts;
        uint64_t rx_last_total_pkts;
    }rxq_stats[NB_RXQ];

    uint64_t tx_total;
    uint64_t tx_pps;
    uint64_t tx_mbps;
    uint64_t rx_total;
    uint64_t rx_pps;
    uint64_t rx_mbps;
}port_stats[RTE_MAX_ETHPORTS];

struct lcore_args
{
    uint32_t port_id;
    struct
    {
        struct rte_mbuf *m_table[NB_BURST] __rte_cache_aligned;
        struct rte_mempool *mp;
        uint32_t queue_id;
        struct rte_timer tim;
    }tx;
    struct
    {
        uint8_t is_rx_lcore;
    }rx;
    uint64_t speed;
    uint32_t trace_idx;
};

struct lcore_args lc_args[RTE_MAX_LCORE];

static void send_pkt_rate(__rte_unused struct rte_timer *timer, void *arg)
{
    struct lcore_args *largs = (struct lcore_args*)arg;
    struct rte_mempool *mp;
    uint32_t port_id;
    uint32_t queue_id;
    uint32_t count = 0;
    int ret;

    mp = largs->tx.mp;
    port_id = largs->port_id;
    queue_id = largs->tx.queue_id;
	
	//if(lcore_end[port_id]==1)
	//	return;
    for(; count < NB_BURST; count++)
    {
        if(largs->trace_idx == ginfo.total_trace)
        {
            largs->trace_idx = 0;
        }
        largs->tx.m_table[count] = generate_mbuf(pms[largs->trace_idx++], mp, pkt_length);
    }

    ret = rte_eth_tx_burst(port_id, queue_id, largs->tx.m_table, NB_BURST);
//	printf("sent %d pkts\n", ret);//debug for tx_burst
    port_stats[port_id].txq_stats[queue_id].tx_total_pkts += ret;
    while(ret < NB_BURST)
    {
        rte_pktmbuf_free(largs->tx.m_table[ret++]);
    }
	lcore_cur_turn[port_id] += 1;
	if(lcore_cur_turn[port_id]>=send_turn && send_turn>1){//if send_turn < 0, send till program ending, otherwise till cur_turn == send_turn
		lcore_end[port_id]=1;	
	}
}

static uint64_t calc_period(uint64_t speed)
{
    return (uint64_t) (((NB_BURST * (pkt_length + 20) * 8 * rte_get_tsc_hz()) / (double) speed) );
}

static int sender_lcore_main(void *args)
{
    struct rte_mbuf *rx_table[NB_BURST];
    struct rte_mbuf *m;
    struct lcore_args *largs;
    uint8_t is_rx;
    int ret;
#if PP_TEST
    struct pp_hdr * pp_hdr;//for pp test
#else
    struct my_ndn * ndn_hdr;//for ndn test
		double cur_latency;
		uint64_t cur_hz = rte_get_tsc_hz();
#endif
		uint64_t __attribute__((unused)) recv_tsc;

    largs = (struct lcore_args*)args;

    is_rx = largs->rx.is_rx_lcore;

    int j;
		int i;
    
		if(is_rx == 0)
    {
        printf("send packet from port %u - queue %u!\n", largs->port_id, largs->tx.queue_id);

        port_stats[largs->port_id].txq_stats[largs->tx.queue_id].tx_total_pkts = 0;
        port_stats[largs->port_id].txq_stats[largs->tx.queue_id].tx_last_total_pkts = 0;
        rte_timer_init(&largs->tx.tim);

        uint64_t period = calc_period(largs->speed);
        printf("period %lu\n", period);
        rte_timer_reset(&largs->tx.tim, period, PERIODICAL, rte_lcore_id(), send_pkt_rate, largs);
    }
    else
    {
        for(j = 0; j < NB_RXQ; j++)
        {
            port_stats[largs->port_id].rxq_stats[j].rx_total_pkts = 0;
            port_stats[largs->port_id].rxq_stats[j].rx_last_total_pkts = 0;
        }
    }

    for(;;) {
		if(lcore_end[largs->port_id]==1)
		{
			return 0;
		}
        if(is_rx) {
            for(j = 0; j < NB_RXQ; j++) {
                ret = rte_eth_rx_burst(largs->port_id, j, rx_table, NB_BURST*8);
                port_stats[largs->port_id].rxq_stats[j].rx_total_pkts += ret;

								recv_tsc = rte_rdtsc();

								//receive end analyze
								for(i=0; i<ret; i++){
									m = rx_table[i];
									rte_prefetch0(rte_pktmbuf_mtod(m,void *));
		
#if PP_TEST
									//pp test
									pp_hdr = rte_pktmbuf_mtod(m, struct pp_hdr*);
									
									//check id and action
									if(pp_hdr->id != pp_hdr->action){
										pp_tot_error++;
									}	
	
									pp_tot_recv++;

								}
								
#else
									//record test data for ndn test
									ndn_hdr = rte_pktmbuf_mtod(m,struct my_ndn *);
									cur_latency = (double)(recv_tsc - ndn_hdr->tsc)/(double)(cur_hz) * 1000;
									if(cur_test_round < NB_TEST){
										if(fprintf(res_fp, "%lf\n", cur_latency)){
											cur_test_round+=1;
										}
									}
									if(cur_test_round >= NB_TEST && res_fp != NULL)//test for NB_TEST pkts
									{	
										fclose(res_fp);
										res_fp = NULL;
									}
						//end test for ndn test
						}
#endif
        		while(ret > 0) {
                	rte_pktmbuf_free(rx_table[--ret]);
        		}
        	}
        }
        else
        {
            rte_timer_manage();
        }
    }
}

static void print_stats(int nb_ports)
{
    int i, j;
    uint64_t tx_total;
    uint64_t tx_last_total;
    uint64_t rx_total;
    uint64_t rx_last_total;
    uint64_t last_cyc, cur_cyc;
    uint64_t frame_len;
//#ifdef GEN_VXLAN
//    frame_len = pkt_length + 20 + 14 + 20 + 8 + 8;
//#else
//    frame_len = pkt_length + 20;
//#endif
		
		//7 bytes for preamble
		//1 byte for start of frame delimter
		//12 bytes for interpacket gap
    frame_len  = pkt_length + 20;
    double time_diff;
    last_cyc = rte_get_tsc_cycles();
    for(;;)
    {
        sleep(PRINT_GAP);
        i = system("clear");
        for(i = 0; i < nb_ports; i++)
        {
            tx_total = tx_last_total = 0;
            rx_total = rx_last_total = 0;
            for(j = 0; j < NB_TXQ; j++)
            {
                tx_total += port_stats[i].txq_stats[j].tx_total_pkts;
                tx_last_total += port_stats[i].txq_stats[j].tx_last_total_pkts;
                port_stats[i].txq_stats[j].tx_last_total_pkts = port_stats[i].txq_stats[j].tx_total_pkts;
            }
            for(j = 0; j < NB_RXQ; j++)
            {
                rx_total += port_stats[i].rxq_stats[j].rx_total_pkts;
                rx_last_total += port_stats[i].rxq_stats[j].rx_last_total_pkts;
                port_stats[i].rxq_stats[j].rx_last_total_pkts = port_stats[i].rxq_stats[j].rx_total_pkts;
            }
            cur_cyc = rte_get_tsc_cycles();
            time_diff = (cur_cyc - last_cyc) / (double)rte_get_tsc_hz();
            port_stats[i].tx_total = tx_total;
            port_stats[i].tx_pps = (uint64_t)((tx_total - tx_last_total) / time_diff);
            port_stats[i].tx_mbps = port_stats[i].tx_pps * (frame_len) * 8 / (1000000);
            port_stats[i].rx_total = rx_total;
            port_stats[i].rx_pps = (uint64_t)((rx_total - rx_last_total) / time_diff);
            port_stats[i].rx_mbps = port_stats[i].rx_pps * (frame_len) * 8 / (1000000);

        }
        last_cyc = rte_get_tsc_cycles();
        for(i = 0; i < nb_ports; i++)
        {
            printf("Port %d Statistics:\n", i);
            printf(">>>>>>>>>>>tx rate: %llupps\n", (unsigned long long)port_stats[i].tx_pps);
            printf(">>>>>>>>>>>tx rate: %lluMbps\n", (unsigned long long)port_stats[i].tx_mbps);
            printf(">>>>>>>>>>tx total: %llu\n", (unsigned long long)port_stats[i].tx_total);
            printf("\n");
            printf(">>>>>>>>>>>rx rate: %llupps\n", (unsigned long long)port_stats[i].rx_pps);
            printf(">>>>>>>>>>>rx rate: %lluMbps\n", (unsigned long long)port_stats[i].rx_mbps);
            printf(">>>>>>>>>>rx total: %llu\n", (unsigned long long)port_stats[i].rx_total);
            printf("============================\n");
            printf("nb_ports is %d\n", nb_ports);
        }

#if PP_TEST	
				//print pp test res
				printf("PP TEST STATISTIC: error rate = %llu/%llu\n", pp_tot_error, pp_tot_recv);
#endif
				
				for(int j=0; j<10; j++){
					if(lcore_end[j]==1)
						printf("lcore %d send %d times then stop\n", j, send_turn);
				}
    }
}


int main(int argc, char **argv)
{
    int ret;
    ret = rte_eal_init(argc, argv);
    if(ret < 0)
    {
        rte_exit(-1, "rte_eal_init failure!\n");
    }

    struct rte_mempool *mbuf_pool;
    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NB_MBUF, MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if(mbuf_pool == NULL)
    {
        rte_exit(-1, "create pktmbuf pool failure!\n");
    }
    int nb_ports;
    nb_ports = all_port_setup(mbuf_pool);
    if(nb_ports <= 0)
    {
        rte_exit(-1, "not detect any DPDK devices!\n");
    }

    int lcore_nb;
    lcore_nb = rte_lcore_count();

//    if(lcore_nb < nb_ports * (NB_TXQ + NB_RXQ) + 1)
    if(lcore_nb < nb_ports + 1)
    {
        rte_exit(-1, "lcore is less than needed! (should be %d)\n", nb_ports + 1);
    }
//    if(lcore_nb > nb_ports * (NB_TXQ + NB_RXQ) + 1)
    if(lcore_nb < nb_ports + 1)
    {
        rte_exit(-1, "lcore is too much! (should be %d)\n", nb_ports + 1);
    }

    parse_params(argc - ret, argv + ret);

    rte_timer_subsystem_init();

//#ifndef GEN_VXLAN
//    ret = load_trace(ginfo.trace_file, pms);
//#else
//    ret = load_vxlan_trace(ginfo.trace_file, pms);
//#endif
//	ret = load_ndn_trace(ginfo.trace_file, pms);//read data and put into pms
//	ret = load_trace(ginfo.trace_file, pms);//original test data
//	ret = load_test_trace(ginfo.trace_file,pms);//liguo test data 
		ret = load_pp_trace(ginfo.trace_file, pms);

    ginfo.total_trace = ret;//number of datas
    if(ret <= 0)
    {
        rte_exit(-1, "no invalid trace!\n");
    }

    int nb_rx_lcore = nb_ports;
//    int nb_tx_lcore = nb_ports * NB_TXQ;
    
		int lcore_idx = 0;
    uint32_t lcore_id;
    uint32_t port_id;
    uint32_t queue_id;
    port_id = queue_id = 0;
    RTE_LCORE_FOREACH_SLAVE(lcore_id)
    {
        if(lcore_idx < nb_rx_lcore)
        {
            lc_args[lcore_id].port_id = port_id++;
            lc_args[lcore_id].rx.is_rx_lcore = 1;
        }
        else
        {
            if(lcore_idx == nb_rx_lcore)
            {
                port_id = queue_id = 0;
            }
            lc_args[lcore_id].tx.mp = mbuf_pool;
            lc_args[lcore_id].tx.queue_id = queue_id % NB_TXQ;
            lc_args[lcore_id].rx.is_rx_lcore = 0;
            lc_args[lcore_id].port_id = queue_id++ / NB_TXQ;
            lc_args[lcore_id].speed = ginfo.Mbps * 1024 * 1024 / NB_TXQ;
            lc_args[lcore_id].trace_idx = 0;
        }
        rte_eal_remote_launch(sender_lcore_main, (void*)&lc_args[lcore_id], lcore_id);
        lcore_idx++;
    }
    print_stats(nb_ports);
    rte_eal_mp_wait_lcore();
    return 0;
}
