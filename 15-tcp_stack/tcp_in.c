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
	fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
	printf("State = %d, tsk_addr = %d\n", tsk->state, (int)tsk);
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
	    printf("process-8\n");
	    tcp_update_window_safe(tsk, cb);
	    if(tsk->state==TCP_SYN_RECV) {
	        printf("process-9\n");
	        struct tcp_sock *chi_tsk = tcp_sock_listen_dequeue(tsk);
	        //printf("process-9-1\n");
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
	        //wake_up(tsk->wait_recv); //?????
	        /*tsk->snd_nxt = cb->ack;
	        tsk->rcv_nxt = cb->seq+cb->pl_len;
	        tcp_send_control_packet()*/
	    } else if(tsk->state==TCP_FIN_WAIT_1) {
	        printf("process-10\n");
	        tsk->snd_nxt = cb->ack;
	        tsk->rcv_nxt = cb->seq+1;
	        tsk->state = TCP_FIN_WAIT_2;
	    } else if(tsk->state==TCP_LAST_ACK) {
	        printf("process-11\n");
	        tsk->state = TCP_CLOSED;
	        printf("FINISHED!!!\n");
	    } else {
	        printf("state:%d\n", tsk->state);
	    }
	}
	if (cb->pl_len!=0) {  //have data with pkt
	    if(tsk->state==TCP_ESTABLISHED) {
	        printf("process-data_recv\n");
	        pthread_mutex_lock(&snd_rcv_lock);
	        tsk->rcv_wnd -= cb->pl_len;
	        printf("cb--recv_data: %d, %s\n", cb->pl_len, cb->payload);
	        write_ring_buffer(tsk->rcv_buf, cb->payload, cb->pl_len);
	        //printf("IF_BUF_IS_EMPTY: %d in %d\n", ring_buffer_empty(tsk->rcv_buf), (int)tsk);
	        if(sleep_on_recv) {
	            wake_up(tsk->wait_recv);
	            sleep_on_recv = 0;
	        }
	        printf("wake_up--tsk_addr: %d\n", (int)tsk);
	        pthread_mutex_unlock(&snd_rcv_lock);
	        //send ACK
	        tsk->snd_nxt = cb->ack;
	        tsk->rcv_nxt = cb->seq + cb->pl_len;
	        
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
	//
	//pthread_mutex_unlock(&new_lock);
	//
}
