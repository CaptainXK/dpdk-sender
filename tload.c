#include "tload.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <rte_memory.h>
#include <netinet/in.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "pm.h"
#include "common.h"

uint32_t pkt_length = 64;

#ifdef RSS_ON

static in_addr_t src_ip_base = 0;
static in_addr_t dst_ip_base = 0;
static uint8_t src_port_base = 0;
static uint8_t dst_port_base = 0;

void random_mac(struct rte_ether_hdr *eth_hdr){
    //ether header
    int i = 0;
    
    srand(time(NULL));
    memset(&(eth_hdr->d_addr), 0, sizeof(eth_hdr->d_addr));
    memset(&(eth_hdr->s_addr), 0, sizeof(eth_hdr->s_addr));
    for(i = 0; i < 6; ++i){
        eth_hdr->d_addr.addr_bytes[i] = (uint8_t)0x12;
        eth_hdr->s_addr.addr_bytes[i] = (uint8_t)0x34;
    }
    eth_hdr->ether_type = htons((uint16_t)0x0800);
    // eth_hdr->ether_type = (uint16_t)(pkt_length - sizeof(struct rte_ether_hdr));
}

void random_ipv4(struct rte_ipv4_hdr *ipv4){
    static int cnt = 0;

    //ipv4 header
    srand(time(NULL));
    ipv4->next_proto_id = 6;
    ipv4->version_ihl = (uint8_t)0x45;
    ipv4->type_of_service = (uint8_t)0;
    ipv4->total_length = htons((uint16_t)(pkt_length - 18));
    ipv4->packet_id = 0;
    ipv4->fragment_offset = 0x0040;//DF
    ipv4->time_to_live = 0xff;
    ipv4->hdr_checksum = 0;
    ipv4->src_addr = src_ip_base + (++cnt) % 10000;
    ipv4->dst_addr = dst_ip_base + (++cnt) % 10000;
    ipv4->hdr_checksum = rte_ipv4_cksum(ipv4);   
}

void random_tcp(struct rte_tcp_hdr *tcp, struct rte_ipv4_hdr *ipv4){
    static int cnt = 0;

    //l4 header
    srand(time(NULL));
    tcp->src_port = htons( src_port_base + (++cnt) % 10000 );
    tcp->dst_port = htons( dst_port_base + (++cnt) % 10000 );
    tcp->sent_seq = htonl(1);
    tcp->recv_ack = htonl(2);
    tcp->data_off = (uint8_t)(sizeof(struct rte_tcp_hdr)>>2)<<4;
    tcp->tcp_flags = (uint8_t)0x10;
    tcp->rx_win = htons(0xffff);
    tcp->cksum = 0;
    tcp->tcp_urp = 0;
    tcp->cksum = rte_ipv4_udptcp_cksum(ipv4, (void*)(tcp) );
}

#endif

int load_ndn_trace(const char *file, struct packet_model pms[])
{
    FILE *fp = fopen(file, "rb");
    int ret = 0;
    int count = 0;
    if(fp == NULL)
    {
        rte_exit(-1, "open trace file failure!\n");
    }

#ifdef RSS_ON
    src_ip_base = inet_addr("10.0.0.255");
    dst_ip_base = inet_addr("172.0.0.1");
#endif

    while((ret = load_ndn_trace_line(fp, &pms[count])) != END_LINE)
    {
        if(ret == VALID_LINE)
        {
            count++;
        }
    }
    printf("total trace %d, pkt_length=%u\n", count, pkt_length);
    return count;
}

int 
get_name_comp(const char * name)
{
    int count = 0;
    int i=0;
    int len = strlen(name);

    for(i=0; i < len; i++)
    {
        if(name[i] == '/')
            count++;
    }

    return count;
}

void
create_tlv_blk_vec(const char* name, int tot_comps, TBKLIST* name_tbkvec, uint8_t mid_comps)
{
    uint32_t offset = 0;
    const char *p = name;

    name_tbkvec->comps = tot_comps;
    
    if(mid_comps != 0) {
        name_tbkvec->cur_comps = mid_comps;
    }

    int idx = 0;
    while ( p && *p != '\0') {
        // beginning of a component
        if (*p == '/') {
            name_tbkvec->tlv_block[idx].offset = offset;

            // set the previous component's length
            if(idx > 0){
                name_tbkvec->tlv_block[idx - 1].length = name_tbkvec->tlv_block[idx].offset - 
                                                        name_tbkvec->tlv_block[idx - 1].offset;
            }
            
            // next component
            idx++;
        }

        offset++;
        p++;
    }

    name_tbkvec->tlv_block[tot_comps - 1].length = offset -
                                            name_tbkvec->tlv_block[tot_comps - 1].offset;
}

int load_ndn_trace_line(FILE *fp, struct packet_model *pm)
{
    int i = 0;
    char buff[256] = "";
    char *tok[7], *s, *sp;
    if(fgets(buff, 256, fp) == NULL)
    {
        return END_LINE;
    }
    for(i = 0, s = buff; i < NB_FIELD; i++, s = NULL)
    {
        tok[i] = strtok_r(s, " \t\n", &sp);
    }

#ifdef RSS_ON
    /* ether */
    random_mac(&pm->ndn.ndn.ether);

    /* ipv4 */
    random_ipv4(&pm->ndn.ndn.ipv4);

    /* tcp */
    random_tcp(&pm->ndn.ndn.tcp, &pm->ndn.ndn.ipv4 );
#else
    /* ether_type == frame length */
    pm->ndn.ndn.ether.ether_type = (uint16_t)(pkt_length - sizeof(struct rte_ether_hdr));
#endif

    /* ndn */
    memset(pm->ndn.ndn.name, 0, 256 * sizeof(char));
    memcpy(pm->ndn.ndn.name, tok[0], strlen(tok[0]));
    pm->ndn.ndn.name_len = strlen(tok[0]);
    pm->is_ndn=1; 

    // /* init ndn tbk list */
    // int tot_comps = 0;
    // tot_comps = get_name_comp(pm->ndn.ndn.name);
    // create_tlv_blk_vec(pm->ndn.ndn.name, tot_comps, &(pm->ndn.ndn.ndn_tbk_list), 0);
    
    return VALID_LINE;
}
