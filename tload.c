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

    memset(pm->ndn.ndn.name,0,400*sizeof(char));
    memcpy(pm->ndn.ndn.name,tok[0],strlen(tok[0]));    
    pm->ndn.ndn.name_len = strlen(tok[0]);
    pm->ndn.ndn.type=0;
    pm->is_ndn=1; 
    
    return VALID_LINE;
}
