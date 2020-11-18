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
#include <sched.h>//sched_getcpu()

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
#include "ndn_hdr.h"

#define GEN_VXLAN

// #define NB_MBUF 65535
#define NB_MBUF ((1 << 18) - 1)
#define MBUF_CACHE_SIZE 512
#define NB_BURST 32//debug , original value is 32
#define NB_TXQ 16
#define NB_RXQ 16
#define NB_MAX_PORTS 64
#define MAX_CORES 128

#define NB_RXD_DFT 4096//debug, original value is 128
#define NB_TXD_DFT 4096//debug, original value is 512
#define NB_TEST 10000
const int nb_rxd = NB_RXD_DFT;
const int nb_txd = NB_TXD_DFT;
FILE * res_fp; //per-pkt latency res data file

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

/* tot txq_nb*/
int tot_txq_nb = 0;

/* bitmap of ports transmiting pkts */
uint64_t t_ports_map = 0x1;

struct port_config{
  int port_id;
  int rxq_nb;
  int txq_nb;
} tot_port_conf[NB_MAX_PORTS];

static void usage()
{
  printf("Usage: pkt-sender <EAL options> -- -t <trace_file> -s <Mbps> -L <pkt_length> -T <send_rounds>\n");
  rte_exit(-1, "invalid arguments!\n");
}

static void parse_params(int argc, char **argv)
{
  char opt;
  int accept = 0;
  // char res_path[256]="";

  while((opt = getopt(argc, argv, "t:s:L:P:")) != -1)
  {
    switch(opt)
    {
      case 't': 
            printf("--->NDN-SENDER-DBG:Trace file=%s\n", optarg);
						rte_memcpy(ginfo.trace_file, optarg, strlen(optarg)+1); 
              accept = 1; 
						break;
      case 's': 
            ginfo.Mbps = atoi(optarg);
            if(ginfo.Mbps <= 0)
            {
              rte_exit(EINVAL, "tx rate (Mbps) is invalid!\n");
            }
						printf("--->NDN-SENDER-DBG:Send rate set to %lluMbps\n", (unsigned long long)ginfo.Mbps);
            // /*res_data file*/
            // sprintf(res_path, "res_%lu", ginfo.Mbps);
            // res_fp = fopen(res_path, "w");
            // if(res_fp == NULL)
            // {
            //   perror("Can not open res file");
            //   exit(1);
            // }
            break;
      case 'L':
            pkt_length = (uint32_t)atoi(optarg);
            if(pkt_length < sizeof(struct ndn_hdr))
            {
              rte_exit(EINVAL, "pkt_length should be no less than ndn hdr (%lu)!\n", 
                                                                      sizeof(struct ndn_hdr));
            }
            printf("--->NDN-SENDER-DBG:Pkt size is %uB\n", pkt_length);
            break;
      case 'P':
            /* bitmap of ports conducting transmit */
            t_ports_map = (uint64_t)strtol(optarg, NULL, 16);
            printf("--->NDN-SENDER-DBG:Transmit core map is 0x%lx\n", t_ports_map);
            break;
      default: usage();
						break;
    }
  }
  if(!accept)
  {
    usage();
  }

}
/**************************************************************/

static struct rte_eth_conf port_conf = {
	.rxmode = {
		.mq_mode = ETH_MQ_RX_RSS,
		.max_rx_pkt_len = RTE_ETHER_MAX_LEN,
		.split_hdr_size = 0,
		.offloads = DEV_RX_OFFLOAD_CHECKSUM,
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = NULL,
			.rss_hf = ETH_RSS_IP,
		},
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};

/* lcore main */
struct port_stats_info
{
  struct
  {
    uint64_t tx_total_pkts;
    uint64_t tx_last_total_pkts;
    uint64_t last_batch;
  }txq_stats[NB_TXQ];

  struct
  {
    uint64_t rx_total_pkts;
    uint64_t rx_last_total_pkts;
    uint64_t last_batch;
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
    uint8_t is_tx_lcore;
  }tx;
  struct
  {
    uint8_t is_rx_lcore;
    uint32_t queue_id;
  }rx;
  uint64_t speed;
  uint32_t trace_idx;
};

struct lcore_args lc_args[RTE_MAX_LCORE];

static void send_pkt_rate (__rte_unused struct rte_timer *timer, void *arg)
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
	
  for(; count < NB_BURST; count++)
  {
    if(largs->trace_idx == ginfo.total_trace)
    {
      largs->trace_idx = 0;
    }
    largs->tx.m_table[count] = generate_mbuf(pms[largs->trace_idx++], mp, pkt_length);
  }

  ret = rte_eth_tx_burst(port_id, queue_id, largs->tx.m_table, NB_BURST);
  
	port_stats[port_id].txq_stats[queue_id].tx_total_pkts += ret;
  while(ret < NB_BURST)
  {
    rte_pktmbuf_free(largs->tx.m_table[ret++]);
  }
}

static uint64_t calc_period(uint64_t speed)
{
  return (uint64_t) ( ( (NB_BURST * (pkt_length + 20) * 8 * rte_get_tsc_hz()) / (double) speed) * tot_txq_nb);
}

static int sender_lcore_main(void *args)
{
  struct rte_mbuf *rx_table[NB_BURST];
  // struct rte_mbuf *m;
  struct lcore_args *largs;
  uint8_t is_rx;
  uint8_t is_tx;
  uint16_t rxq_id;
  int ret;
  // struct ndn_hdr * ndn_hdr;//for ndn test
	uint64_t __attribute__((unused)) recv_tsc;

  largs = (struct lcore_args*)args;

  is_rx = largs->rx.is_rx_lcore;
  is_tx = largs->tx.is_tx_lcore;
  rxq_id = (uint16_t)(largs->rx.queue_id);
  
	printf("[CPU#%d is working, is_rx==%d, is_tx=%d]\n",sched_getcpu(), is_rx, is_tx);
	
	if(is_tx)
  {
    printf("lcore#%u send packet from port %u - queue %u!\n", rte_lcore_id(), largs->port_id, largs->tx.queue_id);

    port_stats[largs->port_id].txq_stats[largs->tx.queue_id].tx_total_pkts = 0;
    port_stats[largs->port_id].txq_stats[largs->tx.queue_id].tx_last_total_pkts = 0;
    rte_timer_init(&largs->tx.tim);

    uint64_t period = calc_period(largs->speed);
    printf("period %lu\n", period);
    rte_timer_reset(&largs->tx.tim, period, PERIODICAL, rte_lcore_id(), send_pkt_rate, largs);
  }
  
  port_stats[largs->port_id].rxq_stats[rxq_id].rx_total_pkts = 0;
  port_stats[largs->port_id].rxq_stats[rxq_id].rx_last_total_pkts = 0;

  for(;;) {
    if(is_rx) {
      ret = rte_eth_rx_burst(largs->port_id, largs->rx.queue_id, rx_table, NB_BURST*8);
      port_stats[largs->port_id].rxq_stats[rxq_id].rx_total_pkts += ret;

      recv_tsc = rte_rdtsc();

			// //receive end analyze
			// for(i=0; i<ret; i++){
			// 	m = rx_table[i];
			// 	rte_prefetch0(rte_pktmbuf_mtod(m,void *));
      
			// 	//record test data for ndn test
			// 	ndn_hdr = rte_pktmbuf_mtod(m,struct ndn_hdr *);
			// 	cur_latency = (double)(recv_tsc - ndn_hdr->tsc)/(double)(cur_hz) * 1000;
			// 	if(cur_test_round < NB_TEST){
			// 		if(fprintf(res_fp, "%lf\n", cur_latency)){
			// 			cur_test_round+=1;
			// 		}
			// 	}
			// 	if(cur_test_round >= NB_TEST && res_fp != NULL)//test for NB_TEST pkts
			// 	{	
			// 		fclose(res_fp);
			// 		res_fp = NULL;
			// 	}
			// }
      while(ret > 0) {
      rte_pktmbuf_free(rx_table[--ret]);
      }
    }
    if(is_tx)
    {
      rte_timer_manage();
    }
  }

		return 0;
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
  int cur_rxq_nb = 0;
  int cur_txq_nb = 0;

  /* framce size + premable + crc */
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
      cur_rxq_nb = tot_port_conf[i].rxq_nb;
      cur_txq_nb = tot_port_conf[i].txq_nb;

      /* txq */
      for(j = 0; j < cur_txq_nb; j++)
      {
        tx_total += port_stats[i].txq_stats[j].tx_total_pkts;
        tx_last_total += port_stats[i].txq_stats[j].tx_last_total_pkts;
        port_stats[i].txq_stats[j].last_batch = (port_stats[i].txq_stats[j].tx_total_pkts
                            - port_stats[i].txq_stats[j].tx_last_total_pkts);
        port_stats[i].txq_stats[j].tx_last_total_pkts = port_stats[i].txq_stats[j].tx_total_pkts;
      }

      /* rxq */
      for(j = 0; j < cur_rxq_nb; j++)
      {
        rx_total += port_stats[i].rxq_stats[j].rx_total_pkts;
        rx_last_total += port_stats[i].rxq_stats[j].rx_last_total_pkts;
        port_stats[i].rxq_stats[j].last_batch = (port_stats[i].rxq_stats[j].rx_total_pkts
                            - port_stats[i].rxq_stats[j].rx_last_total_pkts);
        port_stats[i].rxq_stats[j].rx_last_total_pkts = port_stats[i].rxq_stats[j].rx_total_pkts;
      }
      cur_cyc = rte_get_tsc_cycles();
      time_diff = (cur_cyc - last_cyc) / (double)rte_get_tsc_hz();
      port_stats[i].tx_total = tx_total;
      port_stats[i].tx_pps = (uint64_t)((tx_total - tx_last_total) / time_diff);
      port_stats[i].tx_mbps = port_stats[i].tx_pps * (frame_len) * 8 / (1<<20);
      port_stats[i].rx_total = rx_total;
      port_stats[i].rx_pps = (uint64_t)((rx_total - rx_last_total) / time_diff);
      port_stats[i].rx_mbps = port_stats[i].rx_pps * (frame_len) * 8 / (1<<20);
    }

    unsigned long long _pps = 0;
    unsigned long long _mbps = 0;
    last_cyc = rte_get_tsc_cycles();
    printf("nb_ports is %d\n", nb_ports);
    for(i = 0; i < nb_ports; i++)
    {
      printf("Port %d Statistics:\n", i);
      printf("tx rate: %llu pps\n", (unsigned long long)port_stats[i].tx_pps);
      printf("tx rate: %llu Mbps\n", (unsigned long long)port_stats[i].tx_mbps);
      printf("       : %.2lf Gbps\n", (double)port_stats[i].tx_mbps / 1024);
      if((1<<i) & t_ports_map){
        for(j = 0; j < tot_port_conf[i].txq_nb; ++j){
        _pps =  (unsigned long long)(port_stats[i].txq_stats[j].last_batch / time_diff);
        _mbps = (unsigned long long)(_pps * frame_len * 8 / (1<<20));

        printf("--->rxq#%d: %llu pps,\t%llu mbps\n", j, _pps, _mbps );
        }
      }
      printf("tx total: %llu\n", (unsigned long long)port_stats[i].tx_total);
      printf("\n");

      printf("rx rate: %llu pps\n", (unsigned long long)port_stats[i].rx_pps);
      printf("rx rate: %llu Mbps\n", (unsigned long long)port_stats[i].rx_mbps);
      printf("       : %.2lf Gbps\n", (double)port_stats[i].rx_mbps / 1024);
      for(j = 0; j < tot_port_conf[i].rxq_nb; ++j){
        _pps =  (unsigned long long)(port_stats[i].rxq_stats[j].last_batch / time_diff);
        _mbps = (unsigned long long)(_pps * frame_len * 8 / (1<<20));
        
        printf("--->rxq#%d: %llu pps,\t%llu mbps\n", j, _pps, _mbps);
      }
      printf("rx total: %llu\n", (unsigned long long)port_stats[i].rx_total);
      printf("============================\n");
    }
  }
}

static int one_port_setup(struct rte_mempool *mp, int port_id, int core_for_port_list[], int tx_core_list[], int core_nb)
{
  int ret;
  int queue_id;
  int rxq_nb = 0;
  int txq_nb = 0;
		
		struct rte_eth_dev_info dev_info;
		struct rte_eth_txconf *tx_conf;
		struct rte_eth_rxconf *rx_conf;
  
  /* scan core_for_port and tx_core list */
  for(int i = 1; i < core_nb; ++i){
    if(core_for_port_list[i] == port_id){
      /* all cores are rx core */
      lc_args[i].port_id = port_id;
      lc_args[i].rx.is_rx_lcore = 1;
      lc_args[i].rx.queue_id = rxq_nb++;
      lc_args[i].tx.mp = mp;

      /* set tx core */
      if(tx_core_list[i] == 1){
        lc_args[i].tx.is_tx_lcore = 1;
        lc_args[i].tx.queue_id = txq_nb++;
        lc_args[i].speed = ginfo.Mbps * 1024 * 1024;
        lc_args[i].trace_idx = 0;
      }
    }
  }
  
  /* configure txq_nb and rxq_nb of port*/
  ret = rte_eth_dev_configure(port_id, rxq_nb, txq_nb, &port_conf);
  if(ret < 0)
  {
    rte_exit(-1, "port %d configure failure!\n", port_id);
  }

  /* set txq queue */
  for(queue_id = 0; queue_id < txq_nb; queue_id++)
  {
		//modify tx thresh
		rte_eth_dev_info_get(port_id, &dev_info);					
		tx_conf = &dev_info.default_txconf;

    ret = rte_eth_tx_queue_setup(port_id, queue_id, nb_txd, rte_eth_dev_socket_id(port_id), tx_conf);
    if(ret != 0)
    {
      rte_exit(-1, "port %d tx queue setup failure!\n", port_id);
    }
  }

  /* set rxq queue */
  for(queue_id = 0; queue_id < rxq_nb; queue_id++)
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

  /* update tot port conf */
  tot_port_conf[port_id].port_id = port_id;
  tot_port_conf[port_id].rxq_nb = rxq_nb;
  tot_port_conf[port_id].txq_nb = txq_nb;
  tot_txq_nb += txq_nb;
  
	ret = rte_eth_dev_start(port_id);
  
  if(ret < 0)
  {
    rte_exit(-1, "port %d start failure!\n", port_id);
  }
  
  printf("Set promiscuous enable for %d\n", port_id);
	rte_eth_promiscuous_enable(port_id);

	return ret;
}

void configure_core_port_map(int port_ids[], int is_tx_core[], int nb_cores, int nb_ports)
{
  int i = 0;
  const int cores_per_port = (int)((nb_cores - 1) / nb_ports);
  int cur_port = 0;
  int cores_count_one_port = 0;

  for(i = 1; i < nb_cores; ++i){
    port_ids[i] = cur_port;
    cores_count_one_port += 1;
    if(cores_count_one_port == cores_per_port){
      cores_count_one_port = 0;
      cur_port += 1;
    }
  }

  printf("t_ports_map: 0x%lx\n", t_ports_map);
  for(i = 1; i < nb_cores; ++i){
    if( (1UL<<port_ids[i]) & t_ports_map ){
      is_tx_core[i] = 1;
    }
    else{
      is_tx_core[i] = 0;
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
  
  int nb_ports = rte_eth_dev_count_avail();
	printf("--->NDN-SENDER-DBG:%d ports load\n", nb_ports);
  
	if(nb_ports <= 0)
  {
    rte_exit(-1, "not detect any DPDK devices!\n");
  }

  int lcore_nb;
  lcore_nb = rte_lcore_count();
	printf("--->NDN-SENDER-DBG:%d cores load\n", lcore_nb);
  
  /* parse cmdline */
  parse_params(argc - ret, argv + ret);

	printf("--->NDN-SENDER-DBG:Cmd parse done!\n");

  rte_timer_subsystem_init();

  /* load ndn traces from file */
  ret = load_ndn_trace(ginfo.trace_file, pms);

	if(ret <= 0){
		printf("Sender exit for no trace loaded...\n");
		exit(1);		
	}

  ginfo.total_trace = ret;//number of datas

  if(ret <= 0)
  {
    rte_exit(-1, "no invalid trace!\n");
  }
  
  uint32_t lcore_id;
	uint32_t core_white_list = (1 << lcore_nb) - 1;

  /* core#idx for port_ids[idx] port, core#0 is master core and never conduct tx/rx */
	// int port_ids[MAX_CORES]   = {0, 0, 0, 0, 0, 1, 1, 1, 1};
	int port_ids[MAX_CORES]   = {0};

  /* idx core is tx core whtn is_tx_core[idx] is 1 */
	// int is_tx_core[MAX_CORES] = {0, 1, 1, 1, 1, 0, 0, 0, 0};
	int is_tx_core[MAX_CORES] = {0};

  configure_core_port_map(port_ids, is_tx_core, lcore_nb, nb_ports);

  for(int port_id = 0; port_id < nb_ports; ++port_id){
    one_port_setup(mbuf_pool, port_id, port_ids, is_tx_core, lcore_nb);
  }

  /* show core config */
  RTE_LCORE_FOREACH_SLAVE(lcore_id){
    printf("Core#%d, port_id=%d, rxq_id=%d, ", lcore_id, lc_args[lcore_id].port_id,
                          lc_args[lcore_id].rx.queue_id);
    if(is_tx_core[lcore_id]){
    printf("txq_id=%d\n", lc_args[lcore_id].tx.queue_id);
    }
    else{
    printf("txq_id=None\n");
    }
  }

	//final confirm
	char start_cmd;
	printf("[Press any key but 'q' to start sender...]\n");
	start_cmd = getchar();
	if(start_cmd == 'q'){
		printf("Sender quit...\n");
		return 0;
	}
	else{
		printf("Sender ready to go...\n");
	}
	
	//lunch task onto lcores and run
	RTE_LCORE_FOREACH_SLAVE(lcore_id)
	{
		if( ( ((uint32_t)1<<lcore_id) & core_white_list ) == 0  ){
			continue;			  
		}
  rte_eal_remote_launch(sender_lcore_main, (void*)&lc_args[lcore_id], lcore_id);
	}

  print_stats(nb_ports);
  rte_eal_mp_wait_lcore();
  return 0;
}
