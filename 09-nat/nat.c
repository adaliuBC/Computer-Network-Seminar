#include "nat.h"
#include "ip.h"
#include "icmp.h"
#include "tcp.h"
#include "rtable.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#define MAXLEN 200

static struct nat_table nat;
static u32 getport();
static struct nat_mapping* insert_nat_entry(u32 internal_ip, u16 sport,u32 dest_ip,struct list_head *head);
struct nat_mapping *existing(iface_info_t *iface, char *packet, int len, int dir);
void read_file(char *intif, char *extif, char *rule_1, char *rule_2);
int strpass(char *stan, char *str, int inpos);
    

// get the interface from iface name
static iface_info_t *if_name_to_iface(const char *if_name)
{
	iface_info_t *iface = NULL;
	list_for_each_entry(iface, &instance->iface_list, list) {
		if (strcmp(iface->name, if_name) == 0)
			return iface;
	}

	log(ERROR, "Could not find the desired interface according to if_name '%s'", if_name);
	return NULL;
}

// determine the direction of the packet, DIR_IN / DIR_OUT / DIR_INVALID
static int get_packet_direction(char *packet)
{
	
	fprintf(stdout, "TODO: determine the direction of this packet.\n");
	//internal iface external iface read from example-config.txt
    struct iphdr *ip_hdr = (struct iphdr *)(packet + ETHER_HDR_SIZE);
    u32 src_ip = ntohl(ip_hdr->saddr);
    u32 dst_ip = ntohl(ip_hdr->daddr);
    printf("sip:%d.%d.%d.%d\n", NET_IP_FMT_STR(src_ip));            
    printf("dip:%d.%d.%d.%d\n", NET_IP_FMT_STR(dst_ip));
    rt_entry_t *pos;
	list_for_each_entry(pos, (&rtable), list) {
	    printf("rt:%d.%d.%d.%d, mask:%d\n", NET_IP_FMT_STR(pos->dest), pos->mask);
	}
	
	rt_entry_t *srt = longest_prefix_match(src_ip);
	rt_entry_t *drt = longest_prefix_match(dst_ip);
	//if(srt's iface is internal-iface && drt's iface is external iface)
	    //return DIR_OUT
	//else if(srt's iface is external-iface && dst is external iface)
	    //return DIR_IN 
	if(srt==NULL||drt==NULL) {
	    printf("NO!!!%d, %d\n", srt, drt);
	    return DIR_INVALID;
	}
	if((srt->iface==nat.internal_iface) && (drt->iface==nat.external_iface)) {
	    printf("get dir 4\n");
	    return DIR_OUT;
	} else if((srt->iface==nat.external_iface) && (ntohl(ip_hdr->daddr)==nat.external_iface->ip)) {
	    printf("get dir 5\n");
	    return DIR_IN;
	}
	printf("get dir 3\n");
	return DIR_INVALID;
}

struct nat_mapping *existing(iface_info_t *iface, char *packet, int len, int dir) {
	struct iphdr *ip_hdr = (struct iphdr *)(packet+ETHER_HDR_SIZE);
	struct tcphdr *tcp_hdr = (struct tcphdr *)(packet + ETHER_HDR_SIZE + IP_HDR_SIZE(ip_hdr));
	struct nat_mapping *map_pos;
	int hash_base = (dir==DIR_IN)?ntohl(ip_hdr->saddr):ntohl(ip_hdr->daddr);
    int hash_num = hash8((char *)&hash_base, 4);
    list_for_each_entry(map_pos, &(nat.nat_mapping_list[hash_num]), list) {
        if((dir==DIR_IN&&map_pos->external_ip==ntohl(ip_hdr->daddr)&&(map_pos->external_port==ntohs(tcp_hdr->dport)))||
           (dir==DIR_OUT&&map_pos->internal_ip==ntohl(ip_hdr->saddr)&&(map_pos->internal_port==ntohs(tcp_hdr->sport)))) {
            return map_pos;
        }
    }
    return NULL; 
}

int strpass(char *stan, char *str, int inpos) {
    int i = inpos;
    //printf("%c, %c\n", stan[i-inpos], str[i]);
    for(i = inpos; str[i] == stan[i-inpos]; ) {
        i++;
    }
    return i;
}

// do translation for the packet: replace the ip/port, recalculate ip & tcp
// checksum, update the statistics of the tcp connection
void do_translation(iface_info_t *iface, char *packet, int len, int dir)
{
	fprintf(stdout, "TODO: do translation for this packet.\n");
	pthread_mutex_lock(&nat.lock);
	struct iphdr *ip_hdr = (struct iphdr *)(packet+ETHER_HDR_SIZE);
	struct tcphdr *tcp_hdr = (struct tcphdr *)(packet + ETHER_HDR_SIZE + IP_HDR_SIZE(ip_hdr));
	struct nat_mapping *map;
	int i = 0;
	//printf("tranelate-1\n");
	map = existing(iface, packet, len, dir);
	struct dnat_rule *rule_pos;
    list_for_each_entry(rule_pos, &(nat.rules), list) {
        printf("internal_ip:%d internal_port:%d external_ip:%d external_port:%d\n", 
                rule_pos->internal_ip, rule_pos->internal_port, 
                rule_pos->external_ip, rule_pos->external_port);
    }
	//printf("tranelate-2, %d\n", (map==NULL)?0:1);
	if(dir==DIR_IN && map!=NULL) {
	    //printf("tranelate-3.1\n");
	    //daddr = rule->daddr;  dport = rule->dport;
	    ip_hdr->daddr = htonl(map->internal_ip);
	    tcp_hdr->dport = htons(map->internal_port);
	    ip_hdr->checksum = ip_checksum(ip_hdr);
	    tcp_hdr->checksum = tcp_checksum(ip_hdr, tcp_hdr);
	    // ~ nat_mapping->update_time = time(NULL);
	    //printf("tranelate-3.2\n");
	} else if(dir==DIR_OUT && map!=NULL) {
	    //printf("tranelate-4.1\n");
	    //saddr = external_iface->ip;  sport = assign_external_port();
	    ip_hdr->saddr = htonl(map->external_ip);
	    tcp_hdr->sport = htons(map->external_port);
	    ip_hdr->checksum = ip_checksum(ip_hdr);
	    tcp_hdr->checksum = tcp_checksum(ip_hdr, tcp_hdr);
	    // ~ nat_mapping->update_time = time(NULL);
	    //printf("tranelate-4.2\n");
	} else if(dir==DIR_IN && map==NULL) {
	    printf("tranelate-5\n");
	    struct nat_mapping *map_pos = (struct nat_mapping *)malloc(sizeof(struct nat_mapping));
        memset(map_pos,0,sizeof(struct nat_mapping));
        struct dnat_rule *rule_pos;
        int found = 0;
        printf("tranelate-5.1\n");
        list_for_each_entry(rule_pos, &(nat.rules), list) {
            u32 pos = ntohl(ip_hdr->daddr);
            printf("Is in DNAT checking rule");
            printf("ex:%c.%c.%c.%c\n", NET_IP_FMT_STR(pos));            
            printf("ex:%c.%c.%c.%c\n", NET_IP_FMT_STR((rule_pos->external_ip)));
            printf("in:%c.%c.%c.%c\n", NET_IP_FMT_STR((rule_pos->internal_ip)));
            printf("%d, %d, %d, %d\n", rule_pos->external_ip, rule_pos->internal_ip, rule_pos->external_port, rule_pos->internal_port);
            if(ntohl(ip_hdr->daddr)==rule_pos->external_ip && ntohs(tcp_hdr->dport)==rule_pos->external_port) {
                found = 1;
                break;
            }
        }
        if(found) {
            printf("tranelate-5.2\n");
            init_list_head(&(map_pos->list));
            map_pos->internal_ip = rule_pos->internal_ip;
            map_pos->internal_port = rule_pos->internal_port;
            map_pos->external_ip = rule_pos->external_ip;
            map_pos->external_port = rule_pos->external_port;
            
            int hash_base = (dir==DIR_IN)?ntohl(ip_hdr->saddr):ntohl(ip_hdr->daddr);
            int hash_num = hash8((char *)(&hash_base), 4);
            list_add_tail(&(map_pos->list), &nat.nat_mapping_list[hash_num]);
            u32 pos_2 = 0, pos_3 = 0;
            pos_2 = ntohl(ip_hdr->daddr);
            printf("ex:%c.%c.%c.%c\n", NET_IP_FMT_STR(pos_2));
            ip_hdr->daddr = htonl(rule_pos->internal_ip);
            pos_3 = ntohl(ip_hdr->daddr);
            printf("ex:%c.%c.%c.%c\n", NET_IP_FMT_STR(pos_3));
	        tcp_hdr->dport = htons(rule_pos->internal_port);
	        ip_hdr->checksum = ip_checksum(ip_hdr);
	        tcp_hdr->checksum = tcp_checksum(ip_hdr, tcp_hdr);
            
        } else {
            printf("tranelate-5.3\n");
        }
        
	} else if(dir==DIR_OUT && map==NULL) {
	    //printf("tranelate-6\n");
	    //saddr = external_iface->ip;  sport = assign_external_port();
        struct nat_mapping *map_pos = (struct nat_mapping *)malloc(sizeof(struct nat_mapping));
        memset(map_pos,0,sizeof(struct nat_mapping));
        map_pos->internal_ip = ntohl(ip_hdr->saddr);
	    map_pos->internal_port = ntohs(tcp_hdr->sport);
	    rt_entry_t *rt_pos = longest_prefix_match(ntohl(ip_hdr->daddr));
	    map_pos->external_ip = rt_pos->iface->ip;
	    //map_pos->external_port = 
	    for(i = NAT_PORT_MIN; i < NAT_PORT_MAX; i++) {
	        if(!nat.assigned_ports[i]) {
	            map_pos->external_port = i;
	            nat.assigned_ports[i] = 1;
	            break;
	        }
	    }
        //add the rule?
        ip_hdr->saddr = htonl(map_pos->external_ip);
	    tcp_hdr->sport = htons(map_pos->external_port);
	    ip_hdr->checksum = ip_checksum(ip_hdr);
	    tcp_hdr->checksum = tcp_checksum(ip_hdr, tcp_hdr);
        int hash_base = (dir==DIR_IN)?ntohl(ip_hdr->saddr):ntohl(ip_hdr->daddr);
        int hash_num = hash8((char *)(&hash_base), 4);
        list_add_tail(&(map_pos->list), &nat.nat_mapping_list[hash_num]);
        //printf("tranelate-6.end\n");
	}
	//replace the ip/port
	//recalculate ip & tcp checksum
	//update the statistics of the tcp connection
	ip_send_packet(packet, len);
	//printf("tranelate-7\n");
	pthread_mutex_unlock(&nat.lock);
}





void nat_translate_packet(iface_info_t *iface, char *packet, int len)
{
	int dir = get_packet_direction(packet);
	if (dir == DIR_INVALID) {
		log(ERROR, "invalid packet direction, drop it.");
		icmp_send_packet(packet, len, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH);
		free(packet);
		return ;
	}

	struct iphdr *ip = packet_to_ip_hdr(packet);
	if (ip->protocol != IPPROTO_TCP) {
		log(ERROR, "received non-TCP packet (0x%0hhx), drop it", ip->protocol);
		free(packet);
		return ;
	}

	do_translation(iface, packet, len, dir);
}

// check whether the flow is finished according to FIN bit and sequence number
// XXX: seq_end is calculated by `tcp_seq_end` in tcp.h
static int is_flow_finished(struct nat_connection *conn)
{
    return (conn->internal_fin && conn->external_fin) && \
            (conn->internal_ack >= conn->external_seq_end) && \
            (conn->external_ack >= conn->internal_seq_end);
}

// nat timeout thread: find the finished flows, remove them and free port
// resource
void *nat_timeout()
{
    int i = 0;
	while (1) {
		fprintf(stdout, "TODO: sweep finished flows periodically.\n");
		sleep(1);
		pthread_mutex_lock(&nat.lock);
		//if link end/more than 60s
		/*
		struct iphdr *ip_hdr = (struct iphdr *)(packet+ETHER_HDR_SIZE);
		struct tcphdr *tcp_hdr = (struct tcphdr *)(packet + ETHER_HDR_SIZE + IP_HDR_SIZE(ip_hdr));
		*/
		struct nat_mapping *nat_node_pos, *nat_node_q;
		for(i = 0; i < HASH_8BITS; i++) {
		    list_for_each_entry_safe(nat_node_pos, nat_node_q, &(nat.nat_mapping_list[i]), list) {
		        time_t pos_time = time(NULL);
		        if(pos_time - (nat_node_pos->update_time) > 60) {
		            list_delete_entry(&(nat_node_pos->list));
		        } else if(is_flow_finished(&(nat_node_pos->conn))) {
		            list_delete_entry(&(nat_node_pos->list));
		        }
		    //free
		    }
		}
        pthread_mutex_unlock(&nat.lock);
	}

	return NULL;
}

#define MAX_STR_SIZE 100
int get_next_line(FILE *input, char (*strs)[MAX_STR_SIZE], int *num_strings)
{
	const char *delim = " \t\n";
	char buffer[120];
	int ret = 0;
	if (fgets(buffer, sizeof(buffer), input)) {
		char *token = strtok(buffer, delim);
		*num_strings = 0;
		while (token) {
			strcpy(strs[(*num_strings)++], token);
			token = strtok(NULL, delim);
		}

		ret = 1;
	}

	return ret;
}

int read_ip_port(const char *str, u32 *ip, u16 *port)
{
	int i1, i2, i3, i4;
	int ret = sscanf(str, "%d.%d.%d.%d:%hu", &i1, &i2, &i3, &i4, port);
	if (ret != 5) {
		log(ERROR, "parse ip-port string error: %s.", str);
		exit(1);
	}

	*ip = (i1 << 24) | (i2 << 16) | (i3 << 8) | i4;

	return 0;
}

int parse_config(const char *filename)
{
	FILE *input;
	char strings[10][MAX_STR_SIZE];
	int num_strings;

	input = fopen(filename, "r");
	if (input) {
		while (get_next_line(input, strings, &num_strings)) {
			if (num_strings == 0)
				continue;
            
			if (strcmp(strings[0], "internal-iface:") == 0)
				nat.internal_iface = if_name_to_iface(strings[1]);
			else if (strcmp(strings[0], "external-iface:") == 0)
				nat.external_iface = if_name_to_iface(strings[1]);
			else if (strcmp(strings[0], "dnat-rules:") == 0) {
			    printf("%s----%s----%s\n", strings[0], strings[1], strings[3]);
				struct dnat_rule *rule = (struct dnat_rule*)malloc(sizeof(struct dnat_rule));
				read_ip_port(strings[1], &rule->external_ip, &rule->external_port);
				read_ip_port(strings[3], &rule->internal_ip, &rule->internal_port);
				
				list_add_tail(&rule->list, &nat.rules);
			}
			else {
				log(ERROR, "incorrect config file, exit.");
				exit(1);
			}
		}

		fclose(input);
	}
	else {
		log(ERROR, "could not find config file '%s', exit.", filename);
		exit(1);
	}
	
	if (!nat.internal_iface || !nat.external_iface) {
		log(ERROR, "Could not find the desired interfaces for nat.");
		exit(1);
	}

	return 0;
}

// initialize
void nat_init(const char *config_file)
{
	memset(&nat, 0, sizeof(nat));

	for (int i = 0; i < HASH_8BITS; i++)
		init_list_head(&nat.nat_mapping_list[i]);

	init_list_head(&nat.rules);

	// seems unnecessary
	memset(nat.assigned_ports, 0, sizeof(nat.assigned_ports));

	parse_config(config_file);

	pthread_mutex_init(&nat.lock, NULL);

	pthread_create(&nat.thread, NULL, nat_timeout, NULL);
}

void nat_exit()
{
	fprintf(stdout, "TODO: release all resources allocated.\n");
	pthread_mutex_lock(&nat.lock);
	int i = 0;
	struct nat_mapping *nat_node_pos, *nat_node_q;
	for(i = 0; i < HASH_8BITS; i++) {
		list_for_each_entry_safe(nat_node_pos, nat_node_q, &(nat.nat_mapping_list[i]), list) {
            list_delete_entry(&(nat_node_pos->list));
            free(nat_node_pos);
		}
	}
	pthread_join(nat.thread, NULL);
	pthread_mutex_unlock(&nat.lock);
	
}
