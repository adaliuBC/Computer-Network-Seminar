#include "icmp.h"
#include "ip.h"
#include "rtable.h"
#include "arp.h"
#include "base.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// send icmp packet

void icmp_send_packet(const char *in_pkt, int len, u8 type, u8 code)
{
	/*fprintf(stderr, "TODO: malloc and send icmp packet.\n");*/
	struct ether_header *eth_hdr = (struct ether_header *)in_pkt;
    struct iphdr *ip_hdr = (struct iphdr *)(in_pkt + sizeof(struct ether_header));
	int pkt_len;
	int i = 0;
	pkt_len = (type==8)?len:(ETHER_HDR_SIZE+IP_HDR_SIZE(ip_hdr)+ICMP_HDR_SIZE+IP_HDR_SIZE(ip_hdr) + 8);
    char *packet = (char *)malloc(pkt_len);
	
	struct ether_header *new_eth_hdr = (struct ether_header *)(packet);
	struct iphdr *new_ip_hdr = (struct iphdr *)(packet + ETHER_HDR_SIZE);
	struct icmphdr *new_icmp_hdr = (struct icmphdr *)(packet + ETHER_HDR_SIZE + IP_HDR_SIZE(ip_hdr));
    for(i = 0; i < ETH_ALEN; i++) {
        new_eth_hdr->ether_dhost[i] = eth_hdr->ether_dhost[i];
        new_eth_hdr->ether_shost[i] = eth_hdr->ether_shost[i];
    }
    //mark the IP packet
	new_eth_hdr->ether_type = htons(ETH_P_IP);

	rt_entry_t *entry = longest_prefix_match(ntohl(ip_hdr->saddr));
	ip_init_hdr(new_ip_hdr, entry->iface->ip, ntohl(ip_hdr->saddr), pkt_len-ETHER_HDR_SIZE, 1);
    //set icmp hdr
    new_icmp_hdr->type = (type==8)?0:type;
    new_icmp_hdr->code = (type==8)?0:code;
    //rest of ICMP Header
	if (type == 8){
		memcpy((char *)packet + sizeof(struct ether_header) + IP_HDR_SIZE(ip_hdr) + ICMP_HDR_SIZE - 4, (char *)(in_pkt + sizeof(struct ether_header) + IP_HDR_SIZE(ip_hdr) + ICMP_HDR_SIZE - 4), len - sizeof(struct ether_header) - IP_HDR_SIZE(ip_hdr) - ICMP_HDR_SIZE + 4);
		//cannot use new_icmp_hdr instead of (char *)packet+...
		//?????
	}
	else {
	    int pos = 0;
		memcpy((char *)(packet + sizeof(struct ether_header) + IP_HDR_SIZE(ip_hdr) + ICMP_HDR_SIZE - 4), &pos, 4);
		memcpy((char *)(packet + sizeof(struct ether_header) + IP_HDR_SIZE(ip_hdr) + ICMP_HDR_SIZE), ip_hdr, IP_HDR_SIZE(ip_hdr) + 8);
	}
	new_icmp_hdr->checksum = icmp_checksum(new_icmp_hdr, 
	        (type==8?(len - sizeof(struct ether_header) - IP_HDR_SIZE(ip_hdr)):(IP_HDR_SIZE(ip_hdr)+ICMP_HDR_SIZE+8)));
	
	ip_send_packet(packet, pkt_len);
	printf("sent packet\n");
}


