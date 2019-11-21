#include "tcp.h"
#include "tcp_sock.h"
#include "tcp_timer.h"

#include "log.h"
#include "ring_buffer.h"

#include <stdlib.h>

extern pthread_mutex_t snd_rcv_lock;
extern pthread_mutex_t new_lock;
int sleep_on_wnd = 0;
int rbuf_include = 0;
int sleep_on_recv = 0;
// update the snd_wnd of tcp_sock
//
// if the snd_wnd before updating is zero, notify tcp_sock_send (wait_send)
//extern struct list_head timer_list;
static inline void tcp_update_window(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	u16 old_snd_wnd = tsk->snd_wnd;
	tsk->snd_wnd = cb->rwnd;
	if (old_snd_wnd == 0)
		wake_up(tsk->wait_send);
}

// update the snd_wnd safely: cb->ack should be between snd_una and snd_nxt
static inline void tcp_update_window_safe(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	if (less_or_equal_32b(tsk->snd_una, cb->ack) && less_or_equal_32b(cb->ack, tsk->snd_nxt))
		tcp_update_window(tsk, cb);
}

#ifndef max
#	define max(x,y) ((x)>(y) ? (x) : (y))
#endif

// check whether the sequence number of the incoming packet is in the receiving
// window
static inline int is_tcp_seq_valid(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	u32 rcv_end = tsk->rcv_nxt + max(tsk->rcv_wnd, 1);
	if (less_than_32b(cb->seq, rcv_end) && less_or_equal_32b(tsk->rcv_nxt, cb->seq_end)) {
		return 1;
	}
	else {
		log(ERROR, "received packet with invalid seq, drop it.");
		return 0;
	}
}


void tcp_sock_listen_enqueue(struct tcp_sock *tsk)
{
	if (!list_empty(&tsk->list))
		list_delete_entry(&tsk->list);
	list_add_tail(&tsk->list, &tsk->parent->listen_queue);
	//tsk->parent->accept_backlog += 1;
}

// pop the first tcp sock of the accept_queue
struct tcp_sock *tcp_sock_listen_dequeue(struct tcp_sock *tsk)
{
	struct tcp_sock *new_tsk = list_entry(tsk->listen_queue.next, struct tcp_sock, list);
	//list_delete_entry(&new_tsk->list);
	//init_list_head(&new_tsk->list);
	//tsk->accept_backlog -= 1;

	return new_tsk;
}

// Process the incoming packet according to TCP state machine. 

void tcp_process(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
    //SYN-ACK-data-FIN串行处理，可以共存
	//fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
	//printf("State = %d, tsk_addr = %d\n", tsk->state, (int)tsk);
	//
	//pthread_mutex_lock(&new_lock);
	//
	//printf("in_pkt sip:%d, dip:%d\n", cb->saddr, cb->daddr);
	if(cb->rwnd==0&&!sleep_on_wnd) {
	    sleep_on(tsk->wait_send);
	    sleep_on_wnd = 1;
	} else if(cb->rwnd!=0&&sleep_on_wnd) {
	    wake_up(tsk->wait_send);
	    sleep_on_wnd = 0;
	}
	if(cb->flags&TCP_RST) {  //检查是否为RST包，如果是，直接结束连接
	    printf("process-1\n");
	    tsk->state = TCP_CLOSED;
	    tcp_unhash(tsk);  //freed
	}
	if (cb->flags&TCP_SYN) {  //检查是否为SYN包，如果是，进行建立连接管理
	    //检查ack字段，对方是否确认了新的数据
	    //printf("process-2\n");
	    if(tsk->state==TCP_LISTEN) {
	        printf("process-2\n");
	        tsk->state = TCP_SYN_RECV;
	        struct tcp_sock *new_sock = alloc_tcp_sock();
	        new_sock->parent = tsk;
	        tcp_sock_listen_enqueue(new_sock);
	        
	        tsk->sk_sip = cb->daddr;
	        tsk->sk_dip = cb->saddr;
	        tsk->sk_sport = cb->dport;
	        tsk->sk_dport = cb->sport;
	        //tsk->snd_nxt, tsk->rcv_nxt, flags, tsk->rcv_wnd
	        tsk->iss = rand();
	        tsk->snd_nxt = tsk->iss;
	        tsk->rcv_nxt = cb->seq+1;
	        
	        tcp_send_control_packet(tsk, TCP_ACK|TCP_SYN);
	        //tcp_set_retrans_timer(tsk);
	        printf("TCP SEND TCP_ACK|TCP_SYN PACKET\n");
	        
	    } else if(tsk->state==TCP_SYN_SENT) {
	        printf("process-3\n");
	        tsk->state = TCP_ESTABLISHED;
	        tsk->snd_nxt = cb->ack;
	        tsk->rcv_nxt = cb->seq+1;
	        tcp_send_control_packet(tsk, TCP_ACK);
	        printf("TCP SEND TCP_ACK PACKET\n");
	        tcp_hash(tsk);
	        wake_up(tsk->wait_connect);
	    }
	}
	if(cb->flags&TCP_ACK) {
	    //printf("process-8\n");
	    tcp_update_window_safe(tsk, cb);
	    //1. ACK处理：
        //计算ACK
        //如果ACK确认了所有包，说明暂时完成了，就关掉定时器（把定时器从timer list中移除）
        //如果ACK确认了部分包，就重置定时器的计时，把snd buf中已经被ACK的包删除掉；
        //如果ACK的值和上一次相等，也就是说没有新确认包，则说明ACK是重发过来的，……
        printf("rcv pkt ack: %d\n", ntohl(((struct tcphdr *)(packet+ETHER_HDR_SIZE + IP_BASE_HDR_SIZE))->ack)/*, list_empty(&timer_list)*/);
	    if(cb->ack == tsk->snd_nxt) {
	        list_delete_entry(&(tsk->retrans_timer.list));
	        tsk->retrans_timer.type = 1;
	        tsk->retrans_timer.timeout = 0;
	        init_list_head(&(tsk->retrans_timer.list));
	        tsk->retrans_timer.enable = 0;
	        tsk->retrans_timer.retran_cnt = 0;
	        struct tcp_packet *pkt_list_pos, *pkt_list_q;
	        list_for_each_entry_safe(pkt_list_pos, pkt_list_q, &(tsk->send_buf), list) {
	            list_delete_entry(&(pkt_list_pos->list));
	            printf("DELETE 1 PKT\n");
	        }
	        printf("ALL PKT ACKED POS\n");
	    } else if(cb->ack < tsk->snd_nxt) {
	        //printf("ack:%d, snd_nxt: %d\n", cb->ack, tsk->snd_nxt);
	        
	        struct tcp_packet *pkt_list_pos, *pkt_list_q;
	        list_for_each_entry_safe(pkt_list_pos, pkt_list_q, &(tsk->send_buf), list) {
	            char *packet_content = pkt_list_pos->packet;
	            struct tcphdr *tcp_hdr = packet_to_tcp_hdr(packet_content);
	            if((ntohl(tcp_hdr->seq) + (tcp_hdr->flags&(TCP_SYN|TCP_FIN)) + pkt_list_pos->len - ETHER_HDR_SIZE - IP_BASE_HDR_SIZE - TCP_BASE_HDR_SIZE) <= cb->ack) {
	                printf("DELETE 1 PKT\n");
	                printf("NEW PKT ACKED\n");
	                tcp_unset_retrans_timer(tsk);
	                printf("delete pkt seq: %d\ndelete pkt seq_end: %d\ndelete pkt data: %s", ntohl(((struct tcphdr *)((pkt_list_pos->packet)+ETHER_HDR_SIZE + IP_BASE_HDR_SIZE))->seq), (ntohl(tcp_hdr->seq) + (tcp_hdr->flags&(TCP_SYN|TCP_FIN)) + pkt_list_pos->len - ETHER_HDR_SIZE - IP_BASE_HDR_SIZE - TCP_BASE_HDR_SIZE), packet_content+ETHER_HDR_SIZE + IP_BASE_HDR_SIZE+TCP_BASE_HDR_SIZE);
	                list_delete_entry(&(pkt_list_pos->list));
	            }
	        }
	    }
	    
	    
	    if(tsk->state==TCP_SYN_RECV) {
	        printf("process-9\n");
	        struct tcp_sock *chi_tsk = tcp_sock_listen_dequeue(tsk);
	        chi_tsk->sk_sip = cb->daddr;
	        chi_tsk->sk_dip = cb->saddr;
	        chi_tsk->sk_sport = cb->dport;
	        chi_tsk->sk_dport = cb->sport;
	        printf("save_accept: tsk->saddr:%d, tsk->daddr:%d\n", chi_tsk->sk_sip, chi_tsk->sk_dip);
	        tcp_sock_accept_enqueue(chi_tsk);
	        tsk->state = TCP_ESTABLISHED;
	        wake_up(tsk->wait_accept);
	        //
	        //pthread_mutex_lock(&new_lock);
	        //
	        printf("process-9.end\n");
	    } else if(tsk->state == TCP_ESTABLISHED){
	        printf("process-data_ACK\n");
	    } else if(tsk->state==TCP_FIN_WAIT_1) {
	        printf("process-10\n");
	        tsk->snd_nxt = cb->ack;
	        tsk->rcv_nxt = cb->seq+1;
	        tsk->state = TCP_FIN_WAIT_2;
	    } else if(tsk->state==TCP_LAST_ACK) {
	        printf("process-11\n");
	        tsk->state = TCP_CLOSED;
	        printf("FINISHED!!!\n");
	        printf("Is HERE?\n");
	        struct tcp_timer *tim_pos2, *tim_q2;
	        printf("%d, %d, %d\n", &timer_list, (&timer_list)->next, list_empty(&timer_list));
	        if((&timer_list)->next) {
	            printf("Is at HERE?\n");
	            
	            list_for_each_entry_safe(tim_pos2, tim_q2, &timer_list, list) {
	                list_delete_entry(&tim_pos2->list);
	            }
	        }
	        printf("Is HERE?\n");
	        init_list_head(&timer_list);
	    } else {
	        printf("state:%d\n", tsk->state);
	    }
	}
	if (cb->pl_len!=0) {  //have data with pkt
	    if(tsk->state==TCP_ESTABLISHED) {
	        printf("process-data_recv\n");
	        pthread_mutex_lock(&snd_rcv_lock);
	        //tsk->rcv_wnd -= cb->pl_len;
	        printf("cb--recv_data: %d, %d, %s\n", cb->pl_len, cb->seq, cb->payload);
	        /*write_ring_buffer(tsk->rcv_buf, cb->payload, cb->pl_len);
	        if(sleep_on_recv) {
	            wake_up(tsk->wait_recv);
	            sleep_on_recv = 0;
	        }*/
	        struct tcp_packet *pkt_list = (struct tcp_packet *)malloc(sizeof(struct tcp_packet));
	        struct tcphdr *tcp_hdr_pos = packet_to_tcp_hdr(packet);
	        pkt_list->len = cb->pl_len + ETHER_HDR_SIZE+IP_BASE_HDR_SIZE+TCP_BASE_HDR_SIZE;
	        pkt_list->packet = (char *)malloc(pkt_list->len+1);
	        memcpy(pkt_list->packet, packet, pkt_list->len);
	        (pkt_list->packet)[pkt_list->len] = 0;
	        list_add_tail(&(pkt_list->list), &(tsk->rcv_ofo_buf));
	        int flag = 0;
	        while(1) {
	            struct tcp_packet *pkt_list_pos, *pkt_list_q;
	            flag = 0;
	            list_for_each_entry_safe(pkt_list_pos, pkt_list_q, &(tsk->rcv_ofo_buf), list) {
	                char *packet_content = pkt_list_pos->packet;
	                struct tcphdr *tcp_hdr = packet_to_tcp_hdr(packet_content);
	                if(tsk->rcv_nxt == ntohl(tcp_hdr->seq)) {
	                    printf("HIT! rcv_nxt: %d seq: %d\n", tsk->rcv_nxt, ntohl(tcp_hdr->seq));
	                    flag = 1;
	                    write_ring_buffer(tsk->rcv_buf, packet_content+ETHER_HDR_SIZE+IP_BASE_HDR_SIZE+TCP_BASE_HDR_SIZE, pkt_list_pos->len-ETHER_HDR_SIZE-IP_BASE_HDR_SIZE-TCP_BASE_HDR_SIZE);
	                    tsk->rcv_nxt = ntohl(tcp_hdr->seq) + pkt_list_pos->len-ETHER_HDR_SIZE-IP_BASE_HDR_SIZE-TCP_BASE_HDR_SIZE;
	                    if(sleep_on_recv) {
	                        wake_up(tsk->wait_recv);
	                        sleep_on_recv = 0;
	                    }
	                    printf("HIT2! rcv_nxt: %d seq: %d\n", tsk->rcv_nxt, ntohl(tcp_hdr->seq));
	                }
	            }
	            if(flag==0) {
	                break;
	            }
	        }
	        
	        // printf("wake_up--tsk_addr: %d\n", (int)tsk);
	        pthread_mutex_unlock(&snd_rcv_lock);
	        //send ACK
	        /*
	        tsk->snd_nxt = cb->ack;
	        tsk->rcv_nxt = cb->seq + cb->pl_len;
	        */
	        tcp_send_control_packet(tsk, TCP_ACK);
	        printf("TCP SEND TCP_ACK PACKET\n");
	        printf("process-data_recv.end\n");
	    } else {
	        printf("ERROR:have playload but not ESTABLISHED\n");
	    }
	}
	if (cb->flags&TCP_FIN) {
	    //检查是否为FIN包，如果是，进行断开连接管理
	    //printf("process-5, state:%d\n", tsk->state);
	    if(tsk->state==TCP_ESTABLISHED) {
	        printf("process-6\n");
	        if(tsk->rcv_nxt == cb->seq) {
	            tsk->state = TCP_CLOSE_WAIT;
    	        tsk->sk_sip = cb->daddr;
	            tsk->sk_dip = cb->saddr;
	            tsk->sk_sport = cb->dport;
	            tsk->sk_dport = cb->sport;
	            printf("recv FIN1: tsk->saddr:%d, tsk->daddr:%d\n", tsk->sk_sip, tsk->sk_dip);
	            tsk->rcv_nxt = cb->seq+1;
	            tcp_send_control_packet(tsk, TCP_ACK);
	            printf("TCP SEND TCP_ACK PACKET\n");
	            wake_up(tsk->wait_recv);
	        }
	    } else if(tsk->state==TCP_FIN_WAIT_2) {
	        printf("process-7\n");
	        tsk->state = TCP_TIME_WAIT;
	        tsk->sk_sip = cb->daddr;
	        tsk->sk_dip = cb->saddr;
	        tsk->sk_sport = cb->dport;
	        tsk->sk_dport = cb->sport;
	        printf("recv FIN1: tsk->saddr:%d, tsk->daddr:%d\n", tsk->sk_sip, tsk->sk_dip);
	        tsk->snd_nxt = cb->ack;
	        tsk->rcv_nxt = cb->seq+1;
	        printf("WAITING!!!\n");
	        tcp_set_timewait_timer(tsk);
	        tcp_send_control_packet(tsk, TCP_ACK);
	        printf("TCP SEND TCP_ACK PACKET\n");
	    } else if(tsk->state==TCP_FIN_WAIT_1) {
	        printf("process-bug\n");
	        tsk->state = TCP_TIME_WAIT;
	        tsk->sk_sip = cb->daddr;
	        tsk->sk_dip = cb->saddr;
	        tsk->sk_sport = cb->dport;
	        tsk->sk_dport = cb->sport;
	        printf("recv FIN1: tsk->saddr:%d, tsk->daddr:%d\n", tsk->sk_sip, tsk->sk_dip);
	        tsk->snd_nxt = cb->ack;
	        tsk->rcv_nxt = cb->seq+1;
	        printf("WAITING!!!\n");
	        tcp_set_timewait_timer(tsk);
	        tcp_send_control_packet(tsk, TCP_ACK);
	        printf("TCP SEND TCP_ACK PACKET\n");
	    }
	}
	if (cb->flags&TCP_RST) {
	    //RST:关闭连接，直接close？
	    printf("process-rst_close\n");
	    tcp_sock_close(tsk);
	}
	//
	//pthread_mutex_unlock(&new_lock);
	//
}
