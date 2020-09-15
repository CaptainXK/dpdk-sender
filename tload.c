#include "tload.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <rte_memory.h>
#include <netinet/in.h>
#include <assert.h>
#include "pm.h"

uint32_t pkt_length = 64;

int load_trace_line(FILE *fp, struct packet_model *pm)
{
    int i;
    char buff[256];
    char *tok[7], *s, *sp;
    if(fgets(buff, 256, fp) == NULL)
    {
        return END_LINE;
    }
    for(i = 0, s = buff; i < NB_FIELD; i++, s = NULL)
    {
        tok[i] = strtok_r(s, " \t\n", &sp);
    }

    uint8_t proto;
    proto = (uint8_t)strtoul(tok[4], NULL, 0);
    if(proto != 6 && proto != 17)
    {
        return INVALID_LINE;
    }
    
    if(proto == 6)
    {
        //ether header
        memset(&(pm->tcp.eth.d_addr), 0, sizeof(pm->tcp.eth.d_addr));
        memset(&(pm->tcp.eth.s_addr), 0, sizeof(pm->tcp.eth.s_addr));
        pm->tcp.eth.d_addr.addr_bytes[5] = (uint8_t)0x02;
        pm->tcp.eth.s_addr.addr_bytes[5] = (uint8_t)0x01;
        pm->tcp.eth.ether_type = htons((uint16_t)0x0800);

        //ipv4 header
        pm->tcp.ip.next_proto_id = proto;
        pm->tcp.ip.version_ihl = (uint8_t)0x45;
        pm->tcp.ip.type_of_service = (uint8_t)0;
        pm->tcp.ip.total_length = htons((uint16_t)(pkt_length - 18));
        pm->tcp.ip.packet_id = 0;
        pm->tcp.ip.fragment_offset = 0x0040;//DF
        pm->tcp.ip.time_to_live = 0xff;
        pm->tcp.ip.hdr_checksum = 0;
        pm->tcp.ip.src_addr = htonl(strtoul(tok[0], NULL, 0));
        pm->tcp.ip.dst_addr = htonl(strtoul(tok[1], NULL, 0));
        pm->tcp.ip.hdr_checksum = rte_ipv4_cksum(&(pm->tcp.ip));

        //l4 header
        pm->tcp.tcp.src_port = htons((uint16_t)strtoul(tok[2], NULL, 0));
        pm->tcp.tcp.dst_port = htons((uint16_t)strtoul(tok[3], NULL, 0));
        pm->tcp.tcp.sent_seq = htonl(1);
        pm->tcp.tcp.recv_ack = htonl(2);
        pm->tcp.tcp.data_off = (uint8_t)(sizeof(struct rte_tcp_hdr)>>2)<<4;
        pm->tcp.tcp.tcp_flags = (uint8_t)0x10;
        pm->tcp.tcp.rx_win = htons(0xffff);
        pm->tcp.tcp.cksum = 0;
        pm->tcp.tcp.tcp_urp = 0;
        pm->tcp.tcp.cksum = rte_ipv4_udptcp_cksum(&(pm->tcp.ip), (void*)&(pm->tcp.tcp));
        pm->is_udp = 0;
    }
    else
    {
        //ether header
        memset(&(pm->udp.eth.d_addr), 0, sizeof(pm->udp.eth.d_addr));
        memset(&(pm->udp.eth.s_addr), 0, sizeof(pm->udp.eth.s_addr));
        pm->udp.eth.d_addr.addr_bytes[5] = (uint8_t)0x02;
        pm->udp.eth.s_addr.addr_bytes[5] = (uint8_t)0x01;
        pm->udp.eth.ether_type = htons((uint16_t)0x0800);

        //ipv4 header
        pm->udp.ip.next_proto_id = proto;
        pm->udp.ip.version_ihl = (uint8_t)0x45;
        pm->udp.ip.type_of_service = (uint8_t)0;
        pm->udp.ip.total_length = htons((uint16_t)(pkt_length - 18));
        pm->udp.ip.packet_id = 0;
        pm->udp.ip.fragment_offset = 0x0040;//DF
        pm->udp.ip.time_to_live = 0xff;
        pm->udp.ip.hdr_checksum = 0;
        pm->udp.ip.src_addr = htonl(strtoul(tok[0], NULL, 0));
        pm->udp.ip.dst_addr = htonl(strtoul(tok[1], NULL, 0));
        pm->udp.ip.hdr_checksum = rte_ipv4_cksum(&(pm->udp.ip));


        pm->udp.udp.src_port = htons((uint16_t)strtoul(tok[2], NULL, 0));
        pm->udp.udp.dst_port = htons((uint16_t)strtoul(tok[3], NULL, 0));
        pm->udp.udp.dgram_len = htons((uint16_t)(pkt_length - 18 - sizeof(struct rte_ipv4_hdr)));
        pm->udp.udp.dgram_cksum = 0;
        pm->udp.udp.dgram_cksum = rte_ipv4_udptcp_cksum(&(pm->udp.ip), (void*)&(pm->udp.udp));
        pm->is_udp = 1;
    }
    return VALID_LINE;
}

int load_trace(const char *file, struct packet_model pms[])
{
    FILE *fp = fopen(file, "rb");
    int ret = 0;
    int count = 0;
    if(fp == NULL)
    {
        rte_exit(-1, "open trace file failure!\n");
    }
    while((ret = load_trace_line(fp, &pms[count])) != END_LINE)
    {
        if(ret == VALID_LINE)
        {
            count++;
        }
    }
    printf("total trace %d\n", count);
    return count;
}


int load_vxlan_trace_line(FILE *fp, struct packet_model *pm)
{
    uint32_t vni;
    if(fscanf(fp, "%u%*c", &vni) == EOF)
    {
        return END_LINE;    
    }
    memset(pm, 0, sizeof(*pm));
    pm->vxlan.vx.vx_flags = htonl(0x08000000ULL);
    pm->vxlan.vx.vx_vni = htonl(vni << 8);
    //add udp hdr
    struct rte_udp_hdr *vx_rte_udp_hdr = (struct rte_udp_hdr*)(&(pm->vxlan.udp));
    vx_rte_udp_hdr->dst_port = htons(4789);
    vx_rte_udp_hdr->src_port = htons(9999);
    vx_rte_udp_hdr->dgram_len = htons(sizeof(struct rte_vxlan_hdr) + sizeof(struct rte_udp_hdr) + pkt_length - 4);
    //add ip hdr
    struct rte_ipv4_hdr *vx_ip_hdr = (struct rte_ipv4_hdr*)(&(pm->vxlan.ip));
    vx_ip_hdr->version_ihl = 0x45;
    vx_ip_hdr->total_length = htons(ntohs(vx_rte_udp_hdr->dgram_len) + sizeof(struct rte_ipv4_hdr));
    vx_ip_hdr->fragment_offset = htons(0x4000);
    vx_ip_hdr->next_proto_id = 17;
    vx_ip_hdr->hdr_checksum = rte_ipv4_cksum(vx_ip_hdr);

    vx_rte_udp_hdr->dgram_cksum = rte_ipv4_udptcp_cksum(vx_ip_hdr, (void*)vx_rte_udp_hdr);
    //add ether hdr
    //
    struct rte_vlan_hdr *vl_hdr;
    vl_hdr = (struct rte_vlan_hdr*)(&(pm->vxlan.vlan));
    vl_hdr->vlan_tci = 0;
    vl_hdr->eth_proto = htons(0x0800);

    struct rte_ether_hdr *vx_eth_hdr = (struct rte_ether_hdr*)(&(pm->vxlan.eth));
    vx_eth_hdr->ether_type = htons(0x8100);

    pm->is_vxlan = 1;
    return load_trace_line(fp, pm);
}

int load_vxlan_trace(const char *file, struct packet_model pms[])
{
    FILE *fp = fopen(file, "rb");
    int ret = 0;
    int count = 0;
    if(fp == NULL)
    {
        rte_exit(-1, "open trace file failure!\n");
    }
    while((ret = load_vxlan_trace_line(fp, &pms[count])) != END_LINE)
    {
        if(ret == VALID_LINE)
        {
            count++;
        }
    }
    printf("total trace %d\n", count);
    return count;

}

int load_ndn_trace(const char *file, struct packet_model pms[])
{
    FILE *fp = fopen(file, "rb");
    int ret = 0;
    int count = 0;
    if(fp == NULL)
    {
        rte_exit(-1, "open trace file failure!\n");
    }
    while((ret = load_ndn_trace_line(fp, &pms[count])) != END_LINE)
    {
        if(ret == VALID_LINE)
        {
            count++;
        }
    }
    printf("total trace %d\n", count);
    return count;
}

int load_ndn_trace_line(FILE *fp, struct packet_model *pm)
{
    int i;
    char buff[256];
    char *tok[7], *s, *sp;
    if(fgets(buff, 256, fp) == NULL)
    {
        return END_LINE;
    }
    for(i = 0, s = buff; i < NB_FIELD; i++, s = NULL)
    {
        tok[i] = strtok_r(s, " \t\n", &sp);
    }

    uint8_t proto;
    proto = (uint8_t)strtoul(tok[1], NULL, 0);

//    printf("proto=%d\n",proto); 

    if(proto != 66)
    {
        return INVALID_LINE;
    }
    
    memset(pm->ndn.ndn.name,0,400*sizeof(char));
    memcpy(pm->ndn.ndn.name,tok[0],strlen(tok[0]));    
    pm->ndn.ndn.name_len = strlen(tok[0]);
    pm->ndn.ndn.type=0;
    pm->is_ndn=1; 
    
    return VALID_LINE;
}

int load_test_trace(const char *file, struct packet_model pms[])
{
	FILE *fp = fopen(file,"rb");
	int ret=0;
	int count=0;
	if(fp==NULL)
	{
		rte_exit(-1,"test trace file open error!\n");	
	}
	while( (ret=load_test_trace_line(fp, &pms[count])) != END_LINE){
		if(ret==VALID_LINE){
			count++;
		}
	}
	return count;	
}

int load_test_trace_line(FILE *fp, struct packet_model *pm){
	int i;
	char buff[256];
	char *tok[7],*s,*sp;
	if(	fgets(buff,256,fp) == NULL	)
	{
		return END_LINE;
	}
	for(i=0,s=buff ; i<NB_FIELD; i++ , s = NULL)
	{	
		tok[i]=strtok_r(s, " \t\n", &sp);
	}
	printf("\n");
	printf("split data done..!\n");
	uint8_t proto = (uint8_t)strtoul(tok[6],NULL,10);
	pm->test.test.proto_type=proto;
	printf("type:%d\n",proto);

	if(proto != 99)
		return INVALID_LINE;

  memset(pm->test.test.src_ip, 0, 16*sizeof(char));	
	memset(pm->test.test.dst_ip, 0, 16*sizeof(char));	
  memset(pm->test.test.name , 0, 100*sizeof(char));	
  memcpy(pm->test.test.name ,tok[2],strlen(tok[2]));
	memcpy(pm->test.test.src_ip ,tok[0],strlen(tok[0]));
	memcpy(pm->test.test.dst_ip ,tok[1],strlen(tok[1]));
	pm->test.test.src_port=(uint16_t)strtoul(tok[3],NULL,10);
  pm->test.test.dst_port=(uint16_t)strtoul(tok[4],NULL,10);
	pm->test.test.fwd_port=(uint16_t)strtoul(tok[5],NULL,10);
  pm->is_test=1;      	
	
	return VALID_LINE;  
}

int load_pp_trace(const char *file, struct packet_model pms[])
{
    FILE *fp = fopen(file, "rb");
    int ret = 0;
    int count = 0;
    if(fp == NULL)
    {
        rte_exit(-1, "open trace file failure!\n");
    }
    while((ret = load_pp_trace_line(fp, &pms[count])) != END_LINE)
    {
        if(ret == VALID_LINE)
        {
            count++;
        }
    }
    printf("--->PP-DBG:total trace %d\n", count);
    return count;
}

int load_pp_trace_line(FILE *fp, struct packet_model *pm)
{
	//read one packet
  int j, k;

	//pp bench sender
	//read fields_nb and packets_nb
	static int nFields = 0;
	static int nPackets = 0;
	static uint32_t pkt_id = 1;
	
	if(nFields == 0 && nPackets ==0){
		assert(fscanf(fp, "%d %d", &nFields, &nPackets));
		printf("--->PP-DBG:fields = %d, packets = %d\n", nFields, nPackets);
	}
	
	//check eof
	if(feof(fp))
		return END_LINE;

	int payload_pos = 0;
	uint8_t* cur_byte;
	uint32_t cur_field;

#if DEBUG_PKT
	printf("Current line:");
#endif

	for(j = 0, payload_pos = 0; j < nFields; ++j, payload_pos += 4){
		//read one 4Bytes data
		if(fscanf(fp, "%x", &cur_field) == EOF)
			return END_LINE;
		
		//put into payload one by one
		//little end order, so start from last byte in uint32_t data
		cur_byte = (uint8_t*)(&cur_field);
		for(k = 3; k>=0; --k){
			pm->pp.pp_hdr.payload[payload_pos + k] = *(cur_byte+k);
#if DEBUG_PKT
			printf("%x", pm->pp.pp_hdr.payload[payload_pos + k]);
#endif
		}	
	}
	
#if DEBUG_PKT
	printf("\n");		
#endif
	//set packet id
	pm->pp.pp_hdr.id = pkt_id++;
	
	pm->pp.pp_hdr.action = 0;

	//set ether type to pass NIC hardware check, otherwise can not receive packets as expected	
	pm->pp.pp_hdr.eth.ether_type = (uint16_t)(pkt_length - sizeof(struct rte_ether_hdr));
	
	pm->is_pp=1; 
  
	return VALID_LINE;
}
