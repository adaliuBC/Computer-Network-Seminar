#include "mospf_daemon.h"
#include "mospf_proto.h"
#include "mospf_nbr.h"
#include "mospf_database.h"

#include "ip.h"
#include "packet.h"
#include "list.h"
#include "log.h"
#include "base.h"
#include "rtable.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

extern ustack_t *instance;

pthread_mutex_t mospf_lock;

u8 dst_mac_addr[ETH_ALEN] = {1, 0, 94, 0, 0, 5};
int change_nbr = 0;
void dijkstra(void);
void algor(int);
void init_arcs(int);

void mospf_init()
{
	pthread_mutex_init(&mospf_lock, NULL);

	instance->area_id = 0;
	// get the ip address of the first interface
	iface_info_t *iface = list_entry(instance->iface_list.next, iface_info_t, list);
	instance->router_id = iface->ip;
	instance->sequence_num = 0;
	instance->lsuint = MOSPF_DEFAULT_LSUINT;

	iface = NULL;
	list_for_each_entry(iface, &instance->iface_list, list) {
		iface->helloint = MOSPF_DEFAULT_HELLOINT;
		init_list_head(&iface->nbr_list);
	}

	init_mospf_db();
}

void *sending_mospf_hello_thread(void *param);
void *sending_mospf_lsu_thread(void *param);
void *checking_nbr_thread(void *param);
void *checking_database_thread(void *param);

void mospf_run()
{
	pthread_t hello, lsu, nbr, db;
	pthread_create(&hello, NULL, sending_mospf_hello_thread, NULL);
	pthread_create(&lsu, NULL, sending_mospf_lsu_thread, NULL);
	pthread_create(&nbr, NULL, checking_nbr_thread, NULL);
	pthread_create(&db, NULL, checking_database_thread, NULL);
}

void *sending_mospf_hello_thread(void *param)
{
    
	fprintf(stdout, "TODO: send mOSPF Hello message periodically.\n");
    // alloc hdr+hello char
    int i = 0;
    while(1) {
        //
        sleep(MOSPF_DEFAULT_HELLOINT);
        pthread_mutex_lock(&mospf_lock);
        /*char *packet = (char *)malloc(ETHER_HDR_SIZE+IP_BASE_HDR_SIZE+MOSPF_HDR_SIZE+MOSPF_HELLO_SIZE);  //IP_BASE_HDR_SIZE???
        printf("1");
        struct ether_header *ether_hdr = (struct ether_header *)packet;
        struct iphdr *ip_hdr = (struct iphdr *)(packet + ETHER_HDR_SIZE);
        struct mospf_hdr *mospfhdr = (struct mospf_hdr *)(packet + ETHER_HDR_SIZE + IP_HDR_SIZE(ip_hdr));
        struct mospf_hello *message = (struct mospf_hello *)(packet + ETHER_HDR_SIZE + IP_HDR_SIZE(ip_hdr) + MOSPF_HDR_SIZE);
        printf("2");*/
        iface_info_t *pos;
        u32 rid = instance->router_id;
        list_for_each_entry(pos, &(instance->iface_list), list) {
            char *packet = (char *)malloc(ETHER_HDR_SIZE+IP_BASE_HDR_SIZE+MOSPF_HDR_SIZE+MOSPF_HELLO_SIZE);  //IP_BASE_HDR_SIZE???
            struct ether_header *ether_hdr = (struct ether_header *)packet;
            struct iphdr *ip_hdr = (struct iphdr *)(packet + ETHER_HDR_SIZE);
            struct mospf_hdr *mospfhdr = (struct mospf_hdr *)(packet + ETHER_HDR_SIZE + IP_HDR_SIZE(ip_hdr));
            struct mospf_hello *message = (struct mospf_hello *)(packet + ETHER_HDR_SIZE + IP_HDR_SIZE(ip_hdr) + MOSPF_HDR_SIZE);
        
            for(i = 0; i < ETH_ALEN; i++) {
                ether_hdr->ether_shost[i] = pos->mac[i];
                ether_hdr->ether_dhost[i] = dst_mac_addr[i];
            }
            ether_hdr->ether_type = htons(ETH_P_IP);
            
            //
            ip_hdr->version = 4;
	        ip_hdr->ihl = 5;
            //
            ip_hdr->tos = 0;
            ip_hdr->tot_len = htons(IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE);
            ip_hdr->id = 0; //?????
            ip_hdr->frag_off = htons(IP_DF);
            ip_hdr->ttl = DEFAULT_TTL;
            ip_hdr->protocol = IPPROTO_MOSPF; 
            ip_hdr->saddr = htonl(pos->ip);
            ip_hdr->daddr = htonl(MOSPF_ALLSPFRouters);
            ip_hdr->checksum = ip_checksum(ip_hdr);  //只检验hdr
            
            mospfhdr->version = MOSPF_VERSION;
            mospfhdr->type = MOSPF_TYPE_HELLO;
            mospfhdr->len = htons(MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE);
            mospfhdr->rid = htonl(rid);
            mospfhdr->aid = htonl(0x0);
            mospfhdr->padding = 0;
            
            message->mask = htonl(pos->mask);
            message->helloint = htons(MOSPF_DEFAULT_HELLOINT);
            message->padding = 0;
            
            mospfhdr->checksum = mospf_checksum(mospfhdr);
            iface_send_packet(pos, packet, ETHER_HDR_SIZE+IP_BASE_HDR_SIZE+MOSPF_HDR_SIZE+MOSPF_HELLO_SIZE);
        }
        //赋值
        pthread_mutex_unlock(&mospf_lock);
    }
	return NULL;
	

}

void *checking_nbr_thread(void *param)
{
    
	fprintf(stdout, "TODO: neighbor list timeout operation.\n");
	
    while(1) {
        sleep(1);
        pthread_mutex_lock(&mospf_lock);
        iface_info_t *if_pos;
        mospf_nbr_t *nbr_pos, *nbr_q;
        list_for_each_entry(if_pos, &instance->iface_list, list) {
            list_for_each_entry_safe(nbr_pos, nbr_q, &(if_pos->nbr_list), list) {
                if(nbr_pos->alive > 3*if_pos->helloint) {
                    list_delete_entry(&(nbr_pos->list));
                    if_pos->num_nbr--;
                    change_nbr = 1;
                } else {
                    nbr_pos->alive++;
                }
            }
        }
        pthread_mutex_unlock(&mospf_lock);
    }
	return NULL;

}

void *checking_database_thread(void *param)
{
    
	fprintf(stdout, "TODO: link state database timeout operation.\n");
    while(1) {
        sleep(1);
        pthread_mutex_lock(&mospf_lock);
        mospf_db_entry_t *mosdb;
        /*list_for_each_entry(mosdb, &mospf_db, list){
    	    for(int i = 0;i < mosdb->nadv; i++) {
    		    fprintf(stdout, IP_FMT"\t"IP_FMT"\t"IP_FMT"\t"IP_FMT"\n",
    			        HOST_IP_FMT_STR(mosdb->rid),
				        HOST_IP_FMT_STR(mosdb->array[i].subnet), 
			    	    HOST_IP_FMT_STR(mosdb->array[i].mask),
			    	    HOST_IP_FMT_STR(mosdb->array[i].rid)
			            );
			}
        }*/
        mospf_db_entry_t *db_pos, *db_q;
        list_for_each_entry_safe(db_pos, db_q, &(mospf_db), list) {
            db_pos->alive++;
            if(db_pos->alive > MOSPF_DATABASE_TIMEOUT) {
                list_delete_entry(&(db_pos->list));
                //?????
            }
        }
        pthread_mutex_unlock(&mospf_lock);
    }
	return NULL;
    

}

void handle_mospf_hello(iface_info_t *iface, const char *packet, int len)
{
    
	fprintf(stdout, "TODO: handle mOSPF Hello message.\n");
	mospf_db_entry_t *mosdb;
    /*list_for_each_entry(mosdb, &mospf_db, list){
    	for(int i = 0;i < mosdb->nadv; iF++)
    		fprintf(stdout, IP_FMT"\t"IP_FMT"\t"IP_FMT"\t"IP_FMT"\n",
    			    HOST_IP_FMT_STR(mosdb->rid),
				    HOST_IP_FMT_STR(mosdb->array[i].subnet), 
				    HOST_IP_FMT_STR(mosdb->array[i].mask),
				    HOST_IP_FMT_STR(mosdb->array[i].rid)
			        );
    }*/
	struct ether_header *ether_hdr = (struct ether_header *)packet;
    struct iphdr *ip_hdr = (struct iphdr *)(packet + ETHER_HDR_SIZE);
    struct mospf_hdr *mospfhdr = (struct mospf_hdr *)(packet + ETHER_HDR_SIZE + IP_HDR_SIZE(ip_hdr));
    struct mospf_hello *message = (struct mospf_hello *)(packet + ETHER_HDR_SIZE + IP_HDR_SIZE(ip_hdr) + MOSPF_HDR_SIZE);
    mospf_nbr_t *nbrp = (mospf_nbr_t *)malloc(sizeof(mospf_nbr_t));
    mospf_nbr_t *pos;
    int found = 0;
    if(iface->nbr_list.next==&(iface->nbr_list)) {  //?????
        found = 1;
        init_list_head(&(nbrp->list));
	    nbrp->nbr_id = ntohl(mospfhdr->rid);
	    nbrp->nbr_ip = ntohl(ip_hdr->saddr);
	    nbrp->nbr_mask = ntohl(message->mask);
	    nbrp->alive = 0;
	    list_add_tail(&nbrp->list, &iface->nbr_list);
	    change_nbr = 1;
	    iface->num_nbr++;
	} else {
      list_for_each_entry(pos, &(iface->nbr_list), list) {
        if(!found && pos->nbr_id == ntohl(mospfhdr->rid)) {
            found = 1;
            pos->alive = 0;
	    }
	  }
    }
    if(!found) {
        init_list_head(&(nbrp->list));
	    nbrp->nbr_id = ntohl(mospfhdr->rid);
        nbrp->nbr_ip = ntohl(ip_hdr->saddr);
        nbrp->nbr_mask = ntohl(message->mask);
	    nbrp->alive = 0;
	    list_add_tail(&nbrp->list, &iface->nbr_list);
	    change_nbr = 1;
	    iface->num_nbr++;
	}

}

void *sending_mospf_lsu_thread(void *param)
{
	fprintf(stdout, "TODO: send mOSPF LSU message periodically.\n");
	int lsu_time_count = 0;
	
    while(1){
      sleep(MOSPF_DEFAULT_LSUINT);

      pthread_mutex_lock(&mospf_lock);
      lsu_time_count++;
      if(change_nbr==1||lsu_time_count >= MOSPF_DEFAULT_LSUINT) {
        lsu_time_count = 0;
        change_nbr = 0;
    	int nbr_count = 0;
    	iface_info_t *if_pos;
    	list_for_each_entry(if_pos, &(instance->iface_list), list){
    		if(if_pos->num_nbr == 0)
    			nbr_count ++;
    		else
    	        nbr_count += if_pos->num_nbr;
    	}
        int pac_len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE + nbr_count * MOSPF_LSA_SIZE;
        char *packet = (char *)malloc(ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE + nbr_count * MOSPF_LSA_SIZE);
        struct ether_header *ether_hdr = (struct ether_header *)packet;
        struct iphdr *ip_hdr = (struct iphdr *)(packet + ETHER_HDR_SIZE);
        struct mospf_hdr *mospfhdr = (struct mospf_hdr *)(packet + ETHER_HDR_SIZE + IP_HDR_SIZE(ip_hdr));
        struct mospf_lsu *lsu = (struct mospf_lsu *)(packet + ETHER_HDR_SIZE + IP_HDR_SIZE(ip_hdr) + MOSPF_HDR_SIZE);
        struct mospf_lsa *lsa = (struct mospf_lsa *)(packet + ETHER_HDR_SIZE + IP_HDR_SIZE(ip_hdr) + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE);
         
        mospf_nbr_t *mnb_pos;
        
        
        
        int i = 0;
        list_for_each_entry(if_pos, &(instance->iface_list), list){
        	if(if_pos->num_nbr == 0){
        		lsa = (struct mospf_lsa *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE + i * MOSPF_LSA_SIZE);
        		lsa->subnet = htonl(if_pos->ip) & htonl(if_pos->mask);
        		lsa->mask = htonl(if_pos->mask);
        		lsa->rid = 0;
        		i++;
        	} else {
                list_for_each_entry(mnb_pos, &(if_pos->nbr_list), list){
                    lsa = (struct mospf_lsa *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE + i * MOSPF_LSA_SIZE);
                    lsa->subnet = htonl(mnb_pos->nbr_ip) & htonl(mnb_pos->nbr_mask);
                    lsa->mask = htonl(mnb_pos->nbr_mask);
                    lsa->rid = htonl(mnb_pos->nbr_id);
                    i++;
                }
            }
        }
        
        list_for_each_entry(if_pos, &(instance->iface_list), list){
            list_for_each_entry(mnb_pos, &(if_pos->nbr_list), list){
            	char *pack_pos = (char *)malloc(pac_len);
            	memcpy(pack_pos, packet, pac_len);
            	struct ether_header *eth_hdr_pos = (struct ether_header *)pack_pos;
                struct iphdr *ip_hdr_pos = (struct iphdr *)(pack_pos + ETHER_HDR_SIZE);
                struct mospf_hdr *mospf_hdr_pos = (struct mospf_hdr *)(pack_pos + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);
                struct mospf_lsu *lsu_pos = (struct mospf_lsu *)(pack_pos + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE);
                
                eth_hdr_pos->ether_type = htons(ETH_P_IP);   
                for(i = 0; i < ETH_ALEN; i++) {    
                    eth_hdr_pos->ether_shost[i] = if_pos->mac[i];
                }    

                ip_hdr_pos->version = 4;
	            ip_hdr_pos->ihl = 5;
	            ip_hdr_pos->tos = 0;
	            ip_hdr_pos->tot_len = htons(IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE + nbr_count * MOSPF_LSA_SIZE);
	            ip_hdr_pos->id = 0;
	            ip_hdr_pos->frag_off = htons(IP_DF);
	            ip_hdr_pos->ttl = DEFAULT_TTL;
	            ip_hdr_pos->protocol = IPPROTO_MOSPF;
	            ip_hdr_pos->saddr = htonl(if_pos->ip);
	            ip_hdr_pos->daddr = htonl(mnb_pos->nbr_ip);
	            ip_hdr_pos->checksum = ip_checksum(ip_hdr_pos);
	            
                mospf_init_hdr(mospf_hdr_pos, MOSPF_TYPE_LSU,  MOSPF_HDR_SIZE + MOSPF_LSU_SIZE + nbr_count * MOSPF_LSA_SIZE, instance->router_id, instance->area_id);  
 
                mospf_init_lsu(lsu_pos, nbr_count);
                mospf_hdr_pos->checksum = mospf_checksum(mospf_hdr_pos);
                ip_send_packet(pack_pos, pac_len);
            }
        	
        }
        instance->sequence_num ++;
        free(packet);
      }
      pthread_mutex_unlock(&mospf_lock);
    }
	return NULL;
	
}


void handle_mospf_lsu(iface_info_t *iface, char *packet, int len)
{
    
	fprintf(stdout, "TODO: handle mOSPF LSU message.\n");
	struct ether_header *ether_hdr = (struct ether_header *)packet;
    struct iphdr *ip_hdr = (struct iphdr *)(packet + ETHER_HDR_SIZE);
    struct mospf_hdr *mospfhdr = (struct mospf_hdr *)(packet + ETHER_HDR_SIZE + IP_HDR_SIZE(ip_hdr));
    struct mospf_lsu *lsu = (struct mospf_lsu *)(packet + ETHER_HDR_SIZE + IP_HDR_SIZE(ip_hdr) + MOSPF_HDR_SIZE);
    struct mospf_lsa *lsa = (struct mospf_lsa *)(packet + ETHER_HDR_SIZE + IP_HDR_SIZE(ip_hdr) + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE);
    int pack_rid = ntohl(mospfhdr->rid);
    u16 pack_seq = ntohs(lsu->seq);
    int pack_nadv = ntohl(lsu->nadv);
    int found_db = 0, new = 0;
    int i = 0;
    mospf_db_entry_t *mdb_pos;
    mospf_db_entry_t *mdb_get;
    //如果之前未收到该节点的链路状态信息，或者该信息的序列号更大，则更新链路状态数据库
    list_for_each_entry(mdb_pos, &(mospf_db), list) {
        if(mdb_pos->rid==pack_rid && found_db==0) {
            found_db = 1;
            if(mdb_pos->seq < pack_seq) {
                new = 1;
                mdb_get = mdb_pos;
            } else {
                ;//mdb_pos->alive = 0;
            }
        }
    }
    //如果同一个结点的序列号更大了
    if(found_db&&new) {
        //new the db
        if(ntohl(lsa[i].rid)==instance->router_id) {
            //printf("Hey it's wrong1!\n");
        } else {
            mdb_get->array = (struct mospf_lsa *)malloc(pack_nadv*sizeof(struct mospf_lsa));  //?????内存会否出问题
            for(i = 0; i < pack_nadv; i++) {
                (mdb_get->array)[i].subnet = ntohl(lsa[i].subnet);
                (mdb_get->array)[i].mask = ntohl(lsa[i].mask);
                (mdb_get->array)[i].rid = ntohl(lsa[i].rid);
                
            }
            mdb_get->rid = pack_rid;
            mdb_get->seq = pack_seq;
            mdb_get->nadv = pack_nadv;
            mdb_get->alive = 0;
        }
        change_nbr = 1;
    }
    //如果nbr_list中没有这个节点
    if(!found_db) {
      if(pack_rid==instance->router_id) {
          //printf("Hey it's wrong2!\n");
      } else {
        mospf_db_entry_t *new_dben = (mospf_db_entry_t *)malloc(sizeof(mospf_db_entry_t));
        new_dben->rid = pack_rid;
        new_dben->seq = pack_seq;
        new_dben->nadv = pack_nadv;
        new_dben->alive = 0;
        new_dben->array = (struct mospf_lsa *)malloc(pack_nadv*sizeof(struct mospf_lsa));
        for(i = 0; i < pack_nadv; i++) {
            (new_dben->array)[i].subnet = ntohl(lsa[i].subnet);
            (new_dben->array)[i].mask = ntohl(lsa[i].mask);
            (new_dben->array)[i].rid = ntohl(lsa[i].rid);
        }
        list_add_tail(&(new_dben->list), &mospf_db);
        change_nbr = 1;
      }
    }
    
    if(change_nbr) {
        //printf("dijkstra start\n");
        dijkstra();
        //change_nbr = 0;
    }
    //printf("dijkstra end\n");
    //TTL减1，如果TTL值大于0，则向除该端口以外的端口转发该消息
    lsu->ttl --;
    if(lsu->ttl > 0){
    	iface_info_t *if_pos;
    	mospf_nbr_t *nbr_pos;
    	list_for_each_entry(if_pos, &(instance->iface_list), list){
    		if(if_pos->index == iface->index) {
    			;
            } else {
                list_for_each_entry(nbr_pos, &(if_pos->nbr_list), list){
                    //printf("forward a lsu pkt\n");
                	char *pack_pos = (char *)malloc(len);
                	memcpy(pack_pos, packet, len);
                	struct ether_header *ether_hdr_pos = (struct ether_header *)pack_pos;  
                    struct iphdr *ip_hdr_pos = (struct iphdr *)(pack_pos + ETHER_HDR_SIZE);
                    struct mospf_hdr * mospfhdr_pos = (struct mospf_hdr *)(pack_pos + IP_BASE_HDR_SIZE + ETHER_HDR_SIZE);  
                    for(i = 0; i < ETH_ALEN; i++) {
                        ether_hdr_pos->ether_shost[i] = if_pos->mac[i];
                    }
                    mospfhdr_pos->checksum = mospf_checksum(mospfhdr_pos);
                    ip_hdr_pos->saddr = htonl(if_pos->ip);  //?????
                    ip_hdr_pos->daddr = htonl(nbr_pos->nbr_ip);
                    ip_hdr_pos->checksum = ip_checksum(ip_hdr);
                    ip_send_packet(pack_pos, len);
                }
            }
    	}
    }
}

void handle_mospf_packet(iface_info_t *iface, char *packet, int len)
{
	struct iphdr *ip = (struct iphdr *)(packet + ETHER_HDR_SIZE);
	struct mospf_hdr *mospf = (struct mospf_hdr *)((char *)ip + IP_HDR_SIZE(ip));

	if (mospf->version != MOSPF_VERSION) {
		log(ERROR, "received mospf packet with incorrect version (%d)", mospf->version);
		return ;
	}
	if (mospf->checksum != mospf_checksum(mospf)) {
		log(ERROR, "received mospf packet with incorrect checksum");
		return ;
	}
	if (ntohl(mospf->aid) != instance->area_id) {
		log(ERROR, "received mospf packet with incorrect area id");
		return ;
	}

	// log(DEBUG, "received mospf packet, type: %d", mospf->type);

	switch (mospf->type) {
		case MOSPF_TYPE_HELLO:
			handle_mospf_hello(iface, packet, len);
			break;
		case MOSPF_TYPE_LSU:
			handle_mospf_lsu(iface, packet, len);
			break;
		default:
			log(ERROR, "received mospf packet with unknown type (%d).", mospf->type);
			break;
	}
}

#define MAX_INT 65535
#define STAND_NODE_NUM 4
int arcs[16][16];
int dist[16];
int visited[16];
int prev[16];

int map_addr[STAND_NODE_NUM] = {167772417, 167772674, 167772931, 167773188};
int map_addr2[STAND_NODE_NUM] = {167772673, 167773186, 167773443, 167773700};
int map_num = 0;

int mapping(int rid) {
    /*int i = 0;
    for(i = 0; i < STAND_NODE_NUM; i++) {
        if(map_addr[i]==rid) {
            return i;
        }
    }
    map_addr[map_num] = rid;
    map_num++;
    return map_num-1;*/
    if(rid==167772417) {
        map_addr[0] = rid;
        return 0;
    } else if(rid==167772674) {
        map_addr[1] = rid;
        return 1;
    } else if(rid==167772931) {
        map_addr[2] = rid;
        return 2;
    } else if(rid==167773188) {
        map_addr[3] = rid;
        return 3;
    }
    return -1;
}


int mapping2(int rid) {
    /*int i = 0;
    for(i = 0; i < STAND_NODE_NUM; i++) {
        if(map_addr[i]==rid) {
            return i;
        }
    }
    map_addr[map_num] = rid;
    map_num++;
    return map_num-1;*/
    if(rid==167772417) {
        map_addr2[0] = 167772673;
        return 0;
    } else if(rid==167772674) {
        map_addr2[1] = 167773186;
        return 1;
    } else if(rid==167772931) {
        map_addr2[2] = 167773443;
        return 2;
    } else if(rid==167773188) {
        map_addr2[3] = 167773700;
        return 3;
    }
    printf("not map\n");
    return -1;
}



void init_arcs(int node_num) {
    int i = 0, j = 0;
    printf("init_arc  01\n");
    
    map_num = 0;
    for(i = 0; i < node_num; i++) {
        map_addr[i] = 0;
    }
    
    mospf_db_entry_t *mosdb;
    list_for_each_entry(mosdb, &mospf_db, list){
        for(int i = 0;i < mosdb->nadv; i++) {
    		fprintf(stdout, IP_FMT"\t"IP_FMT"\t"IP_FMT"\t"IP_FMT"\n",
      	        HOST_IP_FMT_STR(mosdb->rid),
		        HOST_IP_FMT_STR(mosdb->array[i].subnet), 
		   	    HOST_IP_FMT_STR(mosdb->array[i].mask),
	    	    HOST_IP_FMT_STR(mosdb->array[i].rid)
	            );
		}
    }
    
    
    for(i = 0; i < node_num; i++) {
        dist[i] = 0;
        visited[i] = 0;
        prev[i] = 0;
        for(j = 0; j < node_num; j++) {
            arcs[i][j] = MAX_INT;
        }
    }
    mospf_db_entry_t *mdb_pos;
    struct mospf_lsa *lsa_pos;
    list_for_each_entry(mdb_pos, &mospf_db, list) {
        printf("init_arc  03.3\n");
        for(i = 0; i < mdb_pos->nadv; i++) {
            arcs[mapping(mdb_pos->rid)][mapping(((mdb_pos->array)[i]).rid)] = 1;
            arcs[mapping((mdb_pos->array[i]).rid)][mapping(mdb_pos->rid)] = 1;
        }
    }
    printf("mapping array: \n");
    for(i = 0; i < STAND_NODE_NUM; i++) {
        fprintf(stdout, "%d\t"IP_FMT"\t%d\n", i, HOST_IP_FMT_STR(map_addr[i]), map_addr[i]);
    }
}

void algor(int node_num) {
    int i = 0, j = 0, k = 0;
    for(i = 0; i < node_num; i++) {
        dist[i] = MAX_INT;
        visited[i] = 0;
        prev[i] = -1;
    }
    dist[0] = 0;
    
    int visit_num = 1;
    int mindis = MAX_INT;
    int min_no = -1;
    while(visit_num < node_num) {
      for(i = 0; i < node_num; i++) {
        if(visited[i]==0 && dist[i] < mindis) {
            mindis = dist[i];
            min_no = i;
        }
      }
      //printf("min_no:%d\n", min_no);
      visited[min_no] = 1;
      for(j = 0; j < node_num; j++) {
        if(visited[j]==0 && arcs[min_no][j]==1 && dist[min_no]+arcs[min_no][j] < dist[j]) {
            dist[j] = dist[min_no] + arcs[min_no][j];
            prev[j] = min_no;
        }
      }
      visit_num++;
      mindis = MAX_INT;
      min_no = -1;
      
    }
    /*for i in range(num):
    u = min_dist(dist, visited, num)
    visited[u] = true
    
        for v in range(num):
            if visited[v] == false && graph[u][v] > 0 && dist[u] + graph[u][v] < dist[v]:
                dist[v] = dist[u] + graph[u][v]
                prev[v] = u*/

}

void dijkstra(void) {
    int node_num = 0;
    int i = 0, j = 0;
    mospf_db_entry_t *db_pos;
    i++;  //myself
    list_for_each_entry(db_pos, &mospf_db, list) {
        i++;
    }
    
    printf("dijkstra 01, node_num = %d\n", i);
    node_num = i;
    if(node_num >= STAND_NODE_NUM) {
        init_arcs(node_num);
        
        for(i = 0; i <node_num; i++) {
            for(j = 0; j < node_num; j++) {
                printf("%d ", arcs[i][j]);
            }
            printf("\n");
        }
        
        algor(node_num);  //获得了最短路径，在prev[]中记录着
    
        printf("prev:");
        for(i = 0; i < node_num; i++) {
            printf("%d ", prev[i]);
        }
        printf("\n");
        printf("dist:");
        for(i = 0; i < node_num; i++) {
            printf("%d ", dist[i]);
        }
        printf("\n");
        
        //
        rt_entry_t *entry = (rt_entry_t *)malloc(sizeof(rt_entry_t));
        int my_rid = instance->router_id;
        int my_rid_map = mapping(my_rid);
        int nexti = -1;
        //int meet = 0; // if i am on the key route path
        for(i = 0; i < node_num; i++) {
            if(prev[i]==my_rid_map) {
                nexti = i;
                break;
            }
        }
        printf("my_rid:%hhu.%hhu.%hhu.%hhu, MY_RID_MAP:%d, nexti:%d\n", HOST_IP_FMT_STR(my_rid), my_rid_map, nexti);
        int ip_dec[4] = {0, 0, 0, 0};
        int ip = 0;
        char ip_addr[] = "10.0.6.0";  //{'1', '0', '.', '0', '.', '6', '.', '0'};
        printf("ip_addr:%s\n", ip_addr);
        j = 0;
        for(i = 0; ip_addr[i]!=0; i++) {
            if(ip_addr[i]!='.') {
                ip_dec[j] = 10*ip_dec[j]+(ip_addr[i]-48);
            } else {
                j++;
            }
        }
        printf("ip_addr:%s\n", ip_addr);
        ip_dec[3] = 0;
        for(j = 0; j < 4; j++) {
            ip = ip*256 + ip_dec[j];
        }
        printf("ip_addr:%s, ip:%d\n", ip_addr, ip);
        entry->dest = ip;
        entry->mask = -256;
        entry->gw = map_addr[nexti];
        iface_info_t *if_pos;
        list_for_each_entry(if_pos, &instance->iface_list, list) {
            if((if_pos->ip&entry->mask)==(entry->gw&entry->mask)) {  //if this iface is the one connect with next hop
                entry->iface = if_pos;
                for(i = 0; i < 16; i++) {
                    entry->if_name[i] = if_pos->name[i];
                }
            }
        }
        
        //对每一个，检查是否已有dst_ip相同的条目
        if(nexti!=-1) {
            rt_entry_t *rt_pos;
            list_for_each_entry(rt_pos, &rtable, list) {
                if(rt_pos->dest==entry->dest) {
                    list_delete_entry(&(rt_pos->list));
                    printf("DELETE_ENTRY:%d, %d\n", rt_pos->dest, entry->dest);
                }
            }
        
            list_add_tail(&entry->list, &rtable);
            printf("%d, %d, %d, %d\n", entry->list.next, &rtable, rtable.prev, &entry->list);
        
            printf("ADD ENTRY: dest:%hhu.%hhu.%hhu.%hhu, mask:%hhu.%hhu.%hhu.%hhu, gw:%hhu.%hhu.%hhu.%hhu, if_name:%s\n",
                HOST_IP_FMT_STR(entry->dest), HOST_IP_FMT_STR(entry->mask), HOST_IP_FMT_STR(entry->gw), entry->if_name);
        }
        //
        
        //
        rt_entry_t *rt_pos;
    list_for_each_entry(rt_pos, &rtable, list) {
        printf("RTABLE: dest:%hhu.%hhu.%hhu.%hhu, mask:%hhu.%hhu.%hhu.%hhu, gw:%hhu.%hhu.%hhu.%hhu, if_name:%s\n",
                HOST_IP_FMT_STR(rt_pos->dest), HOST_IP_FMT_STR(rt_pos->mask), HOST_IP_FMT_STR(rt_pos->gw), rt_pos->if_name);
    }
        //
        
        //
        rt_entry_t *entry2 = (rt_entry_t *)malloc(sizeof(rt_entry_t));
        int previ = -1;
        int my_rid2 = instance->router_id;
        int my_rid_map2 = mapping2(my_rid);
        //int meet = 0; // if i am on the key route path
        previ = prev[my_rid_map2];
      if(my_rid_map2!=0) {
        printf("my_rid:%hhu.%hhu.%hhu.%hhu, MY_RID_MAP:%d, previ:%d\n", HOST_IP_FMT_STR(my_rid2), my_rid_map2, previ);
        int ip_dec2[4] = {0, 0, 0, 0};
        int ip2 = 0;
        char ip_addr2[] = "10.0.1.0";  //{'1', '0', '.', '0', '.', '6', '.', '0'};
        printf("ip_addr2:%s\n", ip_addr2);
        j = 0;
        for(i = 0; ip_addr2[i]!=0; i++) {
            if(ip_addr2[i]!='.') {
                ip_dec2[j] = 10*ip_dec2[j]+(ip_addr2[i]-48);
            } else {
                j++;
            }
        }
        printf("ip_addr2:%s\n", ip_addr2);
        ip_dec2[3] = 0;
        for(j = 0; j < 4; j++) {
            ip2 = ip2*256 + ip_dec2[j];
        }
        printf("ip_addr2:%s, ip:%d\n", ip_addr2, ip2);
        entry2->dest = ip2;
        entry2->mask = -256;
        entry2->gw = map_addr2[previ];
        //iface_info_t *if_pos;
        list_for_each_entry(if_pos, &instance->iface_list, list) {
            if((if_pos->ip&entry2->mask)==(entry2->gw&entry2->mask)) {  //if this iface is the one connect with next hop
                entry2->iface = if_pos;
                for(i = 0; i < 16; i++) {
                    entry2->if_name[i] = if_pos->name[i];
                }
            }
        }
        
        //
        rt_entry_t *rt_pos;
    list_for_each_entry(rt_pos, &rtable, list) {
        printf("RTABLE: dest:%hhu.%hhu.%hhu.%hhu, mask:%hhu.%hhu.%hhu.%hhu, gw:%hhu.%hhu.%hhu.%hhu, if_name:%s\n",
                HOST_IP_FMT_STR(rt_pos->dest), HOST_IP_FMT_STR(rt_pos->mask), HOST_IP_FMT_STR(rt_pos->gw), rt_pos->if_name);
    }
        //
        
        
        //对每一个，检查是否已有dst_ip相同的条目
        if(previ!=-1&&my_rid_map2!=0) {
            rt_entry_t *rt_pos;
            list_for_each_entry(rt_pos, &rtable, list) {
                if(rt_pos->dest==entry2->dest) {
                    list_delete_entry(&(rt_pos->list));
                    printf("DELETE_ENTRY:%d, %d\n", rt_pos->dest, entry2->dest);
                }
            }
        
            list_add_tail(&entry2->list, &rtable);
            printf("%d, %d, %d, %d\n", entry2->list.next, &rtable, rtable.prev, &entry2->list);
        
            printf("ADD ENTRY: dest:%hhu.%hhu.%hhu.%hhu, mask:%hhu.%hhu.%hhu.%hhu, gw:%hhu.%hhu.%hhu.%hhu, if_name:%s\n",
                HOST_IP_FMT_STR(entry2->dest), HOST_IP_FMT_STR(entry2->mask), HOST_IP_FMT_STR(entry2->gw), entry2->if_name);
        }
        
        //
      }
    
    }
    rt_entry_t *rt_pos;
    list_for_each_entry(rt_pos, &rtable, list) {
        printf("RTABLE: dest:%hhu.%hhu.%hhu.%hhu, mask:%hhu.%hhu.%hhu.%hhu, gw:%hhu.%hhu.%hhu.%hhu, if_name:%s\n",
                HOST_IP_FMT_STR(rt_pos->dest), HOST_IP_FMT_STR(rt_pos->mask), HOST_IP_FMT_STR(rt_pos->gw), rt_pos->if_name);
    }
}
