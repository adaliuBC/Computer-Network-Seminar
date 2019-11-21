#include "arpcache.h"
#include "arp.h"
#include "ether.h"
#include "packet.h"
#include "icmp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static arpcache_t arpcache;

// initialize IP->mac mapping, request list, lock and sweeping thread
void arpcache_init()
{
	bzero(&arpcache, sizeof(arpcache_t));

	init_list_head(&(arpcache.req_list));

	pthread_mutex_init(&arpcache.lock, NULL);

	pthread_create(&arpcache.thread, NULL, arpcache_sweep, NULL);
}

// release all the resources when exiting
void arpcache_destroy()
{
	pthread_mutex_lock(&arpcache.lock);

	struct arp_req *req_entry = NULL, *req_q;
	list_for_each_entry_safe(req_entry, req_q, &(arpcache.req_list), list) {
		struct cached_pkt *pkt_entry = NULL, *pkt_q;
		list_for_each_entry_safe(pkt_entry, pkt_q, &(req_entry->cached_packets), list) {
			list_delete_entry(&(pkt_entry->list));
			free(pkt_entry->packet);
			free(pkt_entry);
		}

		list_delete_entry(&(req_entry->list));
		free(req_entry);
	}

	pthread_kill(arpcache.thread, SIGTERM);

	pthread_mutex_unlock(&arpcache.lock);
}

// lookup the IP->mac mapping
//
// traverse the table to find whether there is an entry with the same IP
// and mac address with the given arguments
int arpcache_lookup(u32 ip4, u8 mac[ETH_ALEN])
{
	/*fprintf(stderr, "TODO: lookup ip address in arp cache.\n");*/
	pthread_mutex_lock(&arpcache.lock);
	for ( int i = 0; i != MAX_ARP_SIZE; ++i ){
		if (arpcache.entries[i].ip4 == ip4 && arpcache.entries[i].valid){
			memcpy(mac, arpcache.entries[i].mac, ETH_ALEN);
			pthread_mutex_unlock(&arpcache.lock);
			return 1;
		}
	}
	pthread_mutex_unlock(&arpcache.lock);
	return 0;
}

// append the packet to arpcache
//
// Lookup in the hash table which stores pending packets, if there is already an
// entry with the same IP address and iface (which means the corresponding arp
// request has been sent out), just append this packet at the tail of that entry
// (the entry may contain more than one packet); otherwise, malloc a new entry
// with the given IP address and iface, append the packet, and send arp request.
void arpcache_append_packet(iface_info_t *iface, u32 ip4, char *packet, int len)
{
	pthread_mutex_lock(&arpcache.lock);
	fprintf(stderr, "TODO: append the ip address if lookup failed, and send arp request if necessary.\n");
	//check req_list
	struct arp_req *req_pos, *req_q;
	struct cached_pkt *pkt_pos = (struct cached_pkt *)malloc(sizeof(struct cached_pkt));
	int found = 0;
	init_list_head(&(pkt_pos->list));
	pkt_pos->packet = packet;
	pkt_pos->len = len;
    list_for_each_entry_safe(req_pos, req_q, &(arpcache.req_list), list) {
        if(req_pos->iface==iface && req_pos->ip4==ip4 && found==0) {
            list_add_tail(&(pkt_pos->list), &(req_pos->cached_packets));
            found = 1;
        }
    }
    if(!found) {
        struct arp_req *req_new = (struct arp_req *)malloc(sizeof(struct arp_req));
        init_list_head(&(req_new->list));
        req_new->iface = iface;
        req_new->ip4 = ip4;
        req_new->sent = time(NULL);
        req_new->retries = 0;
        init_list_head(&(req_new->cached_packets));
        list_add_tail(&(pkt_pos->list), &(req_new->cached_packets));  //?????
        list_add_tail(&(req_new->list), &(arpcache.req_list));
        //void arp_send_request(iface_info_t *iface, u32 dst_ip)
        arp_send_request(iface, ip4);
    }
	
	
	pthread_mutex_unlock(&arpcache.lock);
}

// insert the IP->mac mapping into arpcache, if there are pending packets
// waiting for this mapping, fill the ethernet header for each of them, and send
// them out
void arpcache_insert(u32 ip4, u8 mac[ETH_ALEN]) //
{
	pthread_mutex_lock(&arpcache.lock);	
	fprintf(stderr, "TODO: insert ip->mac entry, and send all the pending packets.\n");
	int i = 0, j = 0, found = 0, arpca_num = -1;
	for(i = 0; i < MAX_ARP_SIZE; i++) {
	    if(arpcache.entries[i].valid==0 && found==0) {
	        arpcache.entries[i].valid = 1;
	        arpcache.entries[i].added = time(NULL);
	        arpcache.entries[i].ip4 = ip4;
	        for(j = 0; j < ETH_ALEN; j++) {
	            arpcache.entries[i].mac[j] = mac[j];
	        }
	        arpca_num = i;
	        found = 1;
	    } else if(arpcache.entries[i].valid==1 && arpcache.entries[i].ip4==ip4 && found==0) {
	        found = 1;
	    }
	}
	if(!found) {
	    arpca_num = rand()%32;
	    arpcache.entries[arpca_num].valid = 1;
	    arpcache.entries[arpca_num].added = time(NULL);
	    arpcache.entries[arpca_num].ip4 = ip4;
	    for(j = 0; j < ETH_ALEN; j++) {
	        arpcache.entries[arpca_num].mac[j] = mac[j];
	    }
	}
	struct arp_req *req_pos, *req_q;
	struct cached_pkt *cpkt_pos, *cpkt_q;
	list_for_each_entry_safe(req_pos, req_q, &(arpcache.req_list), list) {
	    if(req_pos->ip4 == ip4) {
	        list_for_each_entry_safe(cpkt_pos, cpkt_q, &(req_pos->cached_packets), list) {
	            //forward the packets and delete the packets
	            struct ether_header *eth_hdr;
	            eth_hdr = (struct ether_header *)(cpkt_pos->packet);
	            for(i = 0; i < ETH_ALEN; i++) {
	                eth_hdr->ether_dhost[i] = mac[i];
	            }
	            //iface_send_packet(iface_info_t *iface, char *packet, int len)
	            iface_send_packet(req_pos->iface, cpkt_pos->packet, cpkt_pos->len);
	            list_delete_entry(&(cpkt_pos->list));
	        }
	        //delete req_pos in req_list
	        list_delete_entry(&(req_pos->list));
	    }
	}
	
	pthread_mutex_unlock(&arpcache.lock);
}
/*
struct cached_pkt {
	struct list_head list;
	char *packet;
	int len;
};

struct arp_req {
	struct list_head list;
	iface_info_t *iface;
	u32 ip4;
	time_t sent;
	int retries;
	struct list_head cached_packets;
};

struct arp_cache_entry {
	u32 ip4; 	// stored in host byte order
	u8 mac[ETH_ALEN];
	time_t added;
	int valid;
};

typedef struct {
	struct arp_cache_entry entries[MAX_ARP_SIZE];
	struct list_head req_list;
	pthread_mutex_t lock;
	pthread_t thread;
} arpcache_t;
*/
// sweep arpcache periodically
//
// For the IP->mac entry, if the entry has been in the table for more than 15
// seconds, remove it from the table.
// For the pending packets, if the arp request is sent out 1 second ago, while 
// the reply has not been received, retransmit the arp request. If the arp
// request has been sent 5 times without receiving arp reply, for each
// pending packet, send icmp packet (DEST_HOST_UNREACHABLE), and drop these
// packets.
void *arpcache_sweep(void *arg) 
{
    int i = 0, j = 0;
	while (1) {
		sleep(1);
		pthread_mutex_lock(&arpcache.lock);
		fprintf(stderr, "TODO: sweep arpcache periodically: remove old entries, resend arp requests .\n");
		time_t tpos;
		//if the entry has been in the table for more than 15
        // seconds, remove it from the table.
		for(i = 0; i < MAX_ARP_SIZE; i++) {
		    tpos = time(NULL);
		    if(tpos-arpcache.entries[i].added > 15) {
		        arpcache.entries[i].valid = 0;
		        arpcache.entries[i].ip4 = 0;
		        for(j = 0; j < ETH_ALEN; j++) {
		        	arpcache.entries[i].mac[j] = 0;
		        }
		        arpcache.entries[i].added = 0;
		    }    
		}    
		struct arp_req *reqpos;
		struct arp_req *req_q;
		// For the pending packets
		list_for_each_entry_safe(reqpos, req_q, &(arpcache.req_list), list) {
		    tpos = time(NULL);
		    //If the arp request has been sent 5 times without receiving arp reply, for each
            // pending packet, send icmp packet (DEST_HOST_UNREACHABLE), and drop these packets.
            struct cached_pkt *cpacket_pos;
            struct cached_pkt *cpacket_q;
		    if(reqpos->retries > 5) {
		        list_for_each_entry_safe(cpacket_pos, cpacket_q, &(reqpos->cached_packets), list) {
		            icmp_send_packet(cpacket_pos->packet, cpacket_pos->len, 3,1);
		            //void icmp_send_packet(const char *in_pkt, int len, u8 type, u8 code)
		            list_delete_entry(&(cpacket_pos->list));
		        }
		        list_delete_entry(&(reqpos->list));
		    }
		    //if the arp request is sent out 1 second ago, while 
            // the reply has not been received, retransmit the arp request.
            tpos = time(NULL);
		    if(((tpos-reqpos->sent)>=1)) {
		        arp_send_request(reqpos->iface, reqpos->ip4);
		        reqpos->sent = time(NULL);
		        reqpos->retries++;
		    }
		}
		
		pthread_mutex_unlock(&arpcache.lock);
	}

	return NULL;
}
