#include "arp.h"
#include "base.h"
#include "types.h"
#include "packet.h"
#include "ether.h"
#include "arpcache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #include "log.h"

#if 0
struct ether_arp {
    u16 arp_hrd;		/* Format of hardware address.  */
    u16 arp_pro;		/* Format of protocol address.  */
    u8	arp_hln;		/* Length of hardware address.  */
    u8	arp_pln;		/* Length of protocol address.  */
    u16 arp_op;			/* ARP opcode (command).  */
	u8	arp_sha[ETH_ALEN];	/* sender hardware address */
	u32	arp_spa;		/* sender protocol address */
	u8	arp_tha[ETH_ALEN];	/* target hardware address */
	u32	arp_tpa;		/* target protocol address */
} __attribute__ ((packed));

struct ether_header {
	u8 ether_dhost[ETH_ALEN];
	u8 ether_shost[ETH_ALEN];
	u16 ether_type;
};

typedef struct {
	struct list_head list;
	int fd;
	int index;
	u8	mac[ETH_ALEN];
	u32 ip;
	u32 mask;
	char name[16];
	char ip_str[16];
} iface_info_t;
#endif

// send an arp request: encapsulate an arp request packet, send it out through
// iface_send_packet
void arp_send_request(iface_info_t *iface, u32 dst_ip)
{
	fprintf(stderr, "TODO: send arp request when lookup failed in arpcache.\n");
	char *packet_hdr;
	int i = 0;
	packet_hdr = (char *)malloc(sizeof(struct ether_header)+sizeof(struct ether_arp));
	struct ether_header *eth_hdr = (struct ether_header *)packet_hdr;
	struct ether_arp *arp_hdr = (struct ether_arp *)(packet_hdr+sizeof(struct ether_header));
    for(i = 0; i < ETH_ALEN; i++) {
        eth_hdr->ether_dhost[i] = 0xff;
        eth_hdr->ether_shost[i] = iface->mac[i];
    }
    eth_hdr->ether_type = htons(ETH_P_ARP);
    arp_hdr->arp_hrd = htons(0x01);
    arp_hdr->arp_pro = htons(0x0800);
    arp_hdr->arp_hln = 6;
    arp_hdr->arp_pln = 4;
	arp_hdr->arp_op  = htons(0x01);
	arp_hdr->arp_spa = htonl(iface->ip);
	arp_hdr->arp_tpa = htonl(dst_ip);
	for(i = 0; i < ETH_ALEN; i++) {
	    arp_hdr->arp_sha[i] = iface->mac[i];
	    arp_hdr->arp_tha[i] = 0xff;
	}
	iface_send_packet(iface, packet_hdr, sizeof(struct ether_header)+sizeof(struct ether_arp));
}

// send an arp reply packet: encapsulate an arp reply packet, send it out
// through iface_send_packet
void arp_send_reply(iface_info_t *iface, struct ether_arp *req_hdr)
{
	fprintf(stderr, "TODO: send arp reply when receiving arp request.\n");
	char *packet_hdr;
	int i = 0;
	packet_hdr = (char *)malloc(sizeof(struct ether_header)+sizeof(struct ether_arp));
	struct ether_header *eth_hdr = (struct ether_header *)packet_hdr;
	struct ether_arp *arp_hdr = (struct ether_arp *)(packet_hdr+sizeof(struct ether_header));
	for(i = 0; i < ETH_ALEN; i++) {
	    eth_hdr->ether_dhost[i] = req_hdr->arp_sha[i];
	    eth_hdr->ether_shost[i] = iface->mac[i];
	}
	eth_hdr->ether_type = htons(0x0806);
	arp_hdr->arp_hrd = htons(ARPHRD_ETHER);
	arp_hdr->arp_pro = htons(0x0800);
	arp_hdr->arp_hln = 6;
	arp_hdr->arp_pln = 4;
	arp_hdr->arp_op  = htons(ARPOP_REPLY);
	arp_hdr->arp_spa = htonl(iface->ip);
	arp_hdr->arp_tpa = htonl(ntohl(req_hdr->arp_spa));
	for(i = 0; i < ETH_ALEN; i++) {
	    arp_hdr->arp_sha[i] = iface->mac[i];
	    arp_hdr->arp_tha[i] = req_hdr->arp_sha[i];
	}
	iface_send_packet(iface, packet_hdr, sizeof(struct ether_header)+sizeof(struct ether_arp));
}

#if 0
struct ether_arp {
    u16 arp_hrd;		/* Format of hardware address.  */
    u16 arp_pro;		/* Format of protocol address.  */
    u8	arp_hln;		/* Length of hardware address.  */
    u8	arp_pln;		/* Length of protocol address.  */
    u16 arp_op;			/* ARP opcode (command).  */
	u8	arp_sha[ETH_ALEN];	/* sender hardware address */
	u32	arp_spa;		/* sender protocol address */
	u8	arp_tha[ETH_ALEN];	/* target hardware address */
	u32	arp_tpa;		/* target protocol address */
} __attribute__ ((packed));
#endif


void handle_arp_packet(iface_info_t *iface, char *packet, int len)
{
	fprintf(stderr, "TODO: process arp packet: arp request & arp reply.\n");
	//struct ether_header *eth_hdr = (struct ether_header *)packet;
	struct ether_arp *arp_hdr = (struct ether_arp *)(packet + ETHER_HDR_SIZE);
	if(ntohs(arp_hdr->arp_op)==0x01) { //is arp request
	    if(ntohl(arp_hdr->arp_tpa)==iface->ip) {
	        arp_send_reply(iface, arp_hdr);
	    } else {
	        iface_send_packet(iface,packet,len);
	    }
	} else if(ntohs(arp_hdr->arp_op)==0x02) { //is arp reply
	    arpcache_insert(ntohl(arp_hdr->arp_spa), arp_hdr->arp_sha);
	} else {
	    printf("ERROR: ARP pkt with wrong op code!\n");
	}
	    	
}

// send (IP) packet through arpcache lookup 
//
// Lookup the mac address of dst_ip in arpcache. If it is found, fill the
// ethernet header and emit the packet by iface_send_packet, otherwise, pending 
// this packet into arpcache, and send arp request.
void iface_send_packet_by_arp(iface_info_t *iface, u32 dst_ip, char *packet, int len)
{
	struct ether_header *eh = (struct ether_header *)packet;
	memcpy(eh->ether_shost, iface->mac, ETH_ALEN);
	eh->ether_type = htons(ETH_P_IP);

	u8 dst_mac[ETH_ALEN];
	int found = arpcache_lookup(dst_ip, dst_mac);
	if (found) {
		// log(DEBUG, "found the mac of %x, send this packet", dst_ip);
		memcpy(eh->ether_dhost, dst_mac, ETH_ALEN);
		iface_send_packet(iface, packet, len);
	}
	else {
		// log(DEBUG, "lookup %x failed, pend this packet", dst_ip);
		arpcache_append_packet(iface, dst_ip, packet, len);
	}
}
