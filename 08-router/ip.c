#include "ip.h"
#include "icmp.h"
#include "rtable.h"
#include "arp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// handle ip packet
// If the packet is ICMP echo request and the destination IP address is equal to
// the IP address of the iface, send ICMP echo reply; otherwise, forward the
// packet.
void handle_ip_packet(iface_info_t *iface, char *packet, int len)
{
	fprintf(stderr, "TODO: handle ip packet.\n");
	//ether header + ip header + icmp header
	struct ether_header *eth_hdr = (struct ether_header *)packet;
	struct iphdr *ip_hdr = (struct iphdr *)(packet+sizeof(struct ether_header));
	int j = 0;
	if(ip_hdr->protocol==1) { //icmp
	    struct icmphdr *icmp_hdr = (struct icmphdr *)(packet+sizeof(struct ether_header)+IP_HDR_SIZE(ip_hdr));
	    //send ICMP reply
	    if(icmp_hdr->type == 8 && ntohl(ip_hdr->daddr)==iface->ip) {
	        icmp_send_packet(packet, len, ICMP_ECHOREPLY, 0);
	    } else {  //zhuanfa
	        ip_hdr->ttl--;
	        if(ip_hdr->ttl < 0) {
	            icmp_send_packet(packet, len, ICMP_TIME_EXCEEDED, 0);
	            return;
	        }
	        ip_hdr->checksum = ip_checksum(ip_hdr);
	        for(j = 0; j < ETH_ALEN; j++) {
	            eth_hdr->ether_shost[j] = iface->mac[j];
	        }
	        rt_entry_t *drt = longest_prefix_match(ntohl(ip_hdr->daddr));
	        if(!drt) {
	            icmp_send_packet(packet, len, 3, 0);
	            return;
	        }
	        u32 dgw = drt->gw;
	        if(!dgw) {
	            dgw = ntohl(ip_hdr->daddr);
	        }
	        iface_send_packet_by_arp(drt->iface, dgw, packet, len);
	    }
	} else {  //zhuanfa
	        ip_hdr->ttl--;
	        if(ip_hdr->ttl <= 0) {
	            icmp_send_packet(packet, len, ICMP_TIME_EXCEEDED, 0);
	            return;
	        }
	        rt_entry_t *drt = longest_prefix_match(ntohl(ip_hdr->daddr));
	        ip_hdr->checksum = ip_checksum(ip_hdr);
	        if(drt) {
	            u32 dgw = drt->gw;
	            if(!dgw) {
	                dgw = ntohl(ip_hdr->daddr);
	            }
	            iface_send_packet_by_arp(drt->iface, dgw, packet, len);
	        
	        } else {
	            icmp_send_packet(packet, len, 3, 0);
	        }
	        //cannote use the same code like above???
	}
}  //????? where is the new dst mac addr?
