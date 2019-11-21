#include "stp.h"

#include "base.h"
#include "ether.h"
#include "utils.h"
#include "types.h"
#include "packet.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sys/types.h>
#include <unistd.h>

#include <pthread.h>
#include <signal.h>

stp_t *stp;
int prior_con_port(stp_t *stp, stp_port_t *p, struct stp_config *config);
int prior_port_port(stp_port_t *p1, stp_port_t *p2);

const u8 eth_stp_addr[] = { 0x01, 0x80, 0xC2, 0x00, 0x00, 0x01 };

static bool stp_is_root_switch(stp_t *stp)
{
	return stp->designated_root == stp->switch_id;
}

static bool stp_port_is_designated(stp_port_t *p)
{
	return p->designated_switch == p->stp->switch_id &&
		p->designated_port == p->port_id;
}

static const char *stp_port_state(stp_port_t *p)
{
	if (p->stp->root_port && \
			p->port_id == p->stp->root_port->port_id)
		return "ROOT";
	else if (p->designated_switch == p->stp->switch_id &&
		p->designated_port == p->port_id)
		return "DESIGNATED";
	else
		return "ALTERNATE";
}

static void stp_port_send_packet(stp_port_t *p, void *stp_msg, int msg_len)
{
	int pkt_len = ETHER_HDR_SIZE + LLC_HDR_SIZE + msg_len;
	char *pkt = malloc(pkt_len);

	// ethernet header
	struct ether_header *eth = (struct ether_header *)pkt;
	memcpy(eth->ether_dhost, eth_stp_addr, 6);
	memcpy(eth->ether_shost, p->iface->mac, 6);
	eth->ether_type = htons(pkt_len - ETHER_HDR_SIZE);

	// LLC header
	struct llc_header *llc = (struct llc_header *)(pkt + ETHER_HDR_SIZE);
	llc->llc_dsap = LLC_DSAP_SNAP;
	llc->llc_ssap = LLC_SSAP_SNAP;
	llc->llc_cntl = LLC_CNTL_SNAP;

	memcpy(pkt + ETHER_HDR_SIZE + LLC_HDR_SIZE, stp_msg, msg_len);

	iface_send_packet(p->iface, pkt, pkt_len);
}

static void stp_port_send_config(stp_port_t *p)
{
	stp_t *stp = p->stp;
	bool is_root = stp_is_root_switch(stp);
	if (!is_root && !stp->root_port) {
		return;
	}

	struct stp_config config;
	memset(&config, 0, sizeof(config));
	config.header.proto_id = htons(STP_PROTOCOL_ID);
	config.header.version = STP_PROTOCOL_VERSION;
	config.header.msg_type = STP_TYPE_CONFIG;
	config.flags = 0;
	config.root_id = htonll(stp->designated_root);
	config.root_path_cost = htonl(stp->root_path_cost);
	config.switch_id = htonll(stp->switch_id);
	config.port_id = htons(p->port_id);
	config.msg_age = htons(0);
	config.max_age = htons(STP_MAX_AGE);
	config.hello_time = htons(STP_HELLO_TIME);
	config.fwd_delay = htons(STP_FWD_DELAY);

	// log(DEBUG, "port %s send config packet.", p->port_name);
	stp_port_send_packet(p, &config, sizeof(config));
}

static void stp_send_config(stp_t *stp)
{
	for (int i = 0; i < stp->nports; i++) {
		stp_port_t *p = &stp->ports[i];
		if (stp_port_is_designated(p)) {
			stp_port_send_config(p);
		}
	}
}

static void stp_handle_hello_timeout(void *arg)
{
	// log(DEBUG, "hello timer expired, now = %llx.", time_tick_now());

	stp_t *stp = arg;
	stp_send_config(stp);
	stp_start_timer(&stp->hello_timer, time_tick_now());
}

static void stp_port_init(stp_port_t *p)
{
	stp_t *stp = p->stp;

	p->designated_root = stp->designated_root;
	p->designated_switch = stp->switch_id;
	p->designated_port = p->port_id;
	p->designated_cost = stp->root_path_cost;
}

void *stp_timer_routine(void *arg)
{
	while (true) {
		long long int now = time_tick_now();

		pthread_mutex_lock(&stp->lock);

		stp_timer_run_once(now);

		pthread_mutex_unlock(&stp->lock);

		usleep(100);
	}

	return NULL;
}

static void stp_handle_config_packet(stp_t *stp, stp_port_t *p,
		struct stp_config *config)
{
	// TODO: handle config packet here
	//judge the youxianji   
	//high/low?
	int config_or_port = 2;  //==1:config is higher; ==2:port is higher
	//==0:all the same
	int i = 0, j = 0;
	int root_port_found = 0, root_port_finding = 1, root_port_num = -1;
	int prev_root = 0, new_root = 0;
	config_or_port = prior_con_port(stp, p, config);
	stp_port_t *port_p;
	prev_root = (stp->switch_id==stp->designated_root)?1:0;
	if(config_or_port == 0) {
	    ;//printf("ERROR: Very same config between two ports on one link!\n");
	} else if(config_or_port == 2) {  //me is the designated port
        stp_send_config(stp);
    } else {  //the other port is the designated port
        //new the port config
        p->designated_root = ntohll(config->root_id);
        p->designated_switch = ntohs(config->switch_id);
        p->designated_port = ntohs(config->port_id);
        p->designated_cost = ntohl(config->root_path_cost)+1;
        //new the state
        for(i = 0; i < stp->nports; i++) {
            if(root_port_found == 0) {
                port_p = &stp->ports[i];
		        if (!stp_port_is_designated(port_p)) {  //for undesig port i
		            for(j = i+1; j < stp->nports; j++) {  //check all other unde ports, if always lower than i, then root_port_found.
		                if(prior_port_port(port_p, &stp->ports[j])==2) {//if higher than i, then break
		                    root_port_finding = 0;
		                }
		            }
		        }
		        if(root_port_finding == 1) {
		            root_port_found = 1;
		            root_port_num = i;
		        }    
		    }
		    root_port_finding = 1;
		}
		if(root_port_num == -1) {
		    stp->root_port = NULL;
            stp->designated_root = stp->switch_id;
            stp->root_path_cost = 0;
		} else {
		    stp_port_t *root_port = &stp->ports[root_port_num];
		    stp->root_port = root_port;
            stp->designated_root = root_port->designated_root;
            stp->root_path_cost = root_port->designated_cost + root_port->path_cost;
		}
            //go through the interfaces, get the highest not destinated port
            //and set it as the root port
        //new the config of the interfaces
        for(i = 0; i < stp->nports; i++) {
            port_p = &stp->ports[i];
            if(!stp_port_is_designated(port_p) && 
               (stp->root_path_cost < (port_p->designated_cost+1))) {
                port_p->designated_switch = stp->switch_id;
                port_p->designated_port = port_p->port_id;
                port_p->designated_root = stp->designated_root;
                port_p->designated_cost = stp->root_path_cost;
            }
            if(stp_port_is_designated(port_p)) {
                port_p->designated_root = stp->designated_root;
                port_p->designated_cost = stp->root_path_cost;
            }
        }
            //set destinated port in the new environment
            //new the  root and cost  for the new destinated port
        //if root-->!root, stop the timer
        new_root = (stp->switch_id==stp->designated_root)?1:0;
        if(prev_root&&!new_root) {
            stp_stop_timer(&stp->hello_timer);
        }
        //send_config
        stp_send_config(stp);
    }
    
	//Qs:what is timer?where to join the thread?
	//receive the config, new the node state need to judge youxianji
	
	//fprintf(stdout, "TODO: handle config packet here.\n");
}

int prior_con_port(stp_t *stp, stp_port_t *p, struct stp_config *config) {
    int priority = 0;
    if((ntohll(config->root_id)) != p->designated_root) {
        if((ntohll(config->root_id)) < p->designated_root) {
            priority = 1;
        } else {
            priority = 2;
        }
    } else if ((ntohl(config->root_path_cost)+1)!=p->designated_cost){
        if((ntohl(config->root_path_cost)+1) < p->designated_cost) {
            priority = 1;
        } else {
            priority = 2;
        }
    } else if ((ntohs(config->switch_id)) != p->designated_switch) {
        if((ntohs(config->switch_id)) < p->designated_switch) {
            priority = 1;
        } else {
            priority = 2;
        }
    } else if ((ntohs(config->port_id)) != p->designated_port) {
        if((ntohs(config->port_id)) < p->designated_port) {
            priority = 1;
        } else {
            priority = 2;
        }
    } else {
            priority = 0;
    }
    //if root1!=root2   config:root_id, port:designnated_root
	    //choose the smaller one
	// else (root==) if cost1!=cost2  config:root_path_cost + 1, port:designnated_cost
	    //choose the smaller one
	//else (cost==) if last_node1!=last_node2 config:switch_id,  port:designated_switch
	    //choose the smaller one
	//else (last_node==) if last_port1!=last_port2 config:port_id,  port:designated_port
	    //choose the smaller one
	//else (last_port==)
        //do nothing
    return priority;  //==1config, ==2port
}

int prior_port_port(stp_port_t *p1, stp_port_t *p2) {
//return 1 port p1 higher, 2 p2 higher
    int priority = 0;
    if(p1->designated_root != p2->designated_root) {
        if(p1->designated_root < p2->designated_root) {
            priority = 1;
        } else {
            priority = 2;
        }
    } else if (p1->designated_cost!=p2->designated_cost){
        if(p1->designated_cost < p2->designated_cost) {
            priority = 1;
        } else {
            priority = 2;
        }
    } else if (p1->designated_switch != p2->designated_switch) {
        if(p1->designated_switch < p2->designated_switch) {
            priority = 1;
        } else {
            priority = 2;
        }
    } else if (p1->designated_port != p2->designated_port) {
        if(p1->designated_port < p2->designated_port) {
            priority = 1;
        } else {
            priority = 2;
        }
    } else {
        priority = 0;
    }
    return priority;
}


static void *stp_dump_state(void *arg)
{
#define get_switch_id(switch_id) (int)(switch_id & 0xFFFF)
#define get_port_id(port_id) (int)(port_id & 0xFF)

	pthread_mutex_lock(&stp->lock);

	bool is_root = stp_is_root_switch(stp);
	if (is_root) {
		log(INFO, "this switch is root."); 
	}
	else {
		log(INFO, "non-root switch, designated root: %04x, root path cost: %d.", \
				get_switch_id(stp->designated_root), stp->root_path_cost);
	}

	for (int i = 0; i < stp->nports; i++) {
		stp_port_t *p = &stp->ports[i];
		log(INFO, "port id: %02d, role: %s.", get_port_id(p->port_id), \
				stp_port_state(p));
		log(INFO, "\tdesignated ->root: %04x, ->switch: %04x, " \
				"->port: %02d, ->cost: %d.", \
				get_switch_id(p->designated_root), \
				get_switch_id(p->designated_switch), \
				get_port_id(p->designated_port), \
				p->designated_cost);
	}

	pthread_mutex_unlock(&stp->lock);

	exit(0);
}

static void stp_handle_signal(int signal)
{
	if (signal == SIGTERM) {
		log(DEBUG, "received SIGTERM, terminate this program.");
		
		pthread_t pid;
		pthread_create(&pid, NULL, stp_dump_state, NULL);
	}
}

void stp_init(struct list_head *iface_list)
{
	stp = malloc(sizeof(*stp));

	// set switch ID
	u64 mac_addr = 0;
	iface_info_t *iface = list_entry(iface_list->next, iface_info_t, list);
	for (int i = 0; i < sizeof(iface->mac); i++) {
		mac_addr <<= 8;
		mac_addr += iface->mac[i];
	}
	stp->switch_id = mac_addr | ((u64) STP_BRIDGE_PRIORITY << 48);

	stp->designated_root = stp->switch_id;
	stp->root_path_cost = 0;
	stp->root_port = NULL;

	stp_init_timer(&stp->hello_timer, STP_HELLO_TIME, \
			stp_handle_hello_timeout, (void *)stp);

	stp_start_timer(&stp->hello_timer, time_tick_now());

	stp->nports = 0;
	list_for_each_entry(iface, iface_list, list) {
		stp_port_t *p = &stp->ports[stp->nports];

		p->stp = stp;
		p->port_id = (STP_PORT_PRIORITY << 8) | (stp->nports + 1);
		p->port_name = strdup(iface->name);
		p->iface = iface;
		p->path_cost = 1;

		stp_port_init(p);

		// store stp port in iface for efficient access
		iface->port = p;

		stp->nports += 1;
	}

	pthread_mutex_init(&stp->lock, NULL);
	pthread_create(&stp->timer_thread, NULL, stp_timer_routine, NULL);

	signal(SIGTERM, stp_handle_signal);
}

void stp_destroy()
{
	pthread_kill(stp->timer_thread, SIGKILL);

	for (int i = 0; i < stp->nports; i++) {
		stp_port_t *port = &stp->ports[i];
		port->iface->port = NULL;
		free(port->port_name);
	}

	free(stp);
}

void stp_port_handle_packet(stp_port_t *p, char *packet, int pkt_len)
{
	stp_t *stp = p->stp;

	pthread_mutex_lock(&stp->lock);
	
	// protocol insanity check is omitted
	struct stp_header *header = (struct stp_header *)(packet + ETHER_HDR_SIZE + LLC_HDR_SIZE);

	if (header->msg_type == STP_TYPE_CONFIG) {
		stp_handle_config_packet(stp, p, (struct stp_config *)header);
	}
	else if (header->msg_type == STP_TYPE_TCN) {
		log(ERROR, "TCN packet is not supported in this lab.");
	}
	else {
		log(ERROR, "received invalid STP packet.");
	}

	pthread_mutex_unlock(&stp->lock);
}