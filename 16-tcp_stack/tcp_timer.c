#include "tcp.h"
#include "tcp_timer.h"
#include "tcp_sock.h"

#include <stdio.h>
#include <unistd.h>

extern int resend;

// scan the timer_list, find the tcp sock which stays for at 2*MSL, release it
void tcp_scan_timer_list()
{
	//fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
	struct tcp_timer *tim_pos, *tim_q;
	//printf("%d, %d, %d\n", &timer_list, (&timer_list)->next, list_empty(&timer_list));
	if(list_empty(&timer_list)) {
	    return;
	}
	list_for_each_entry_safe(tim_pos, tim_q, &timer_list, list) {  //?????

	    if(tim_pos->type == 0) {
	        tim_pos->timeout += TCP_TIMER_SCAN_INTERVAL;
	        if(tim_pos->timeout > TCP_TIMEWAIT_TIMEOUT) {
	            printf("FINISHED!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	            //list_delete_entry(&tim_pos->list);
	            struct tcp_timer *tim_pos2, *tim_q2;
	            list_for_each_entry_safe(tim_pos2, tim_q2, &timer_list, list) {
	                list_delete_entry(&tim_pos2->list);
	            }
	        }
	    } else {
	        //触发计时器时：
            //重传snd buf中第一个包
            //如果重传次数到达3，则发RST包，之后直接close
            //重传次数+1，计时器时间翻倍
            
	        tim_pos->timeout += TCP_TIMER_SCAN_INTERVAL;
	        int actual_retran_time = TCP_RETRANS_INTERVAL_INITIAL;
	        int i = 0;
	        for(i = 0; i < tim_pos->retran_cnt; i++) {
	            actual_retran_time *= 2;
	        }
	        if(tim_pos->timeout > actual_retran_time) {
	            printf("----------A PKT IS DROPPED!----------\n");
	            printf("%d, %d, %d\n", (&timer_list)->next, list_empty(&timer_list));
	            struct tcp_sock *tsk_pos = retranstimer_to_tcp_sock((char *)tim_pos);
	            /*printf("State = %d\n", tsk_pos->state);
	            if(tsk_pos->state==TCP_CLOSED) {
	                struct tcp_timer *tim_pos2, *tim_q2;
	                list_for_each_entry_safe(tim_pos2, tim_q2, &timer_list, list) {
	                    list_delete_entry(&tim_pos2->list);
	                }
	                break;
	            }*/
	            if(tim_pos->retran_cnt >= 3) {
	                tcp_send_control_packet(tsk_pos, TCP_RST);
	                printf("Already send 3 times\n");
	                printf("TCP SEND TCP_RST PACKET\n");
	                struct tcp_sock *tsk = retranstimer_to_tcp_sock((char *)tim_pos);
	                tcp_sock_close(tsk);
	            }
	            if(list_empty(&tsk_pos->send_buf)) {
	                printf("empty\n");
	                list_delete_entry(&(tsk_pos->retrans_timer.list));
	                tsk_pos->retrans_timer.type = 1;
	                tsk_pos->retrans_timer.timeout = 0;
	                init_list_head(&(tsk_pos->retrans_timer.list));
	                tsk_pos->retrans_timer.enable = 0;
	                tsk_pos->retrans_timer.retran_cnt = 0;
	                break;
	            }
	            //struct tcp_packet *first_packet = list_entry(tsk_pos->send_buf.next, struct tcp_packet, list);
	            struct tcp_packet *first_packet = (struct tcp_packet *)(tsk_pos->send_buf.next);
	            char *packet_pos = (char *)malloc(first_packet->len);
	            printf("old pkt seq: %d\nold packet data: %s\n", ntohl(((struct tcphdr *)((first_packet->packet)+ETHER_HDR_SIZE + IP_BASE_HDR_SIZE))->seq), (char *)((first_packet->packet)+ETHER_HDR_SIZE + IP_BASE_HDR_SIZE+TCP_BASE_HDR_SIZE));
	            memcpy(packet_pos, first_packet->packet, first_packet->len);
	            //list_delete_entry(&(first_packet->list));
	            resend = 1;
	            printf("TCP RESEND DROPPED DATA PACKET\n");
	            tcp_send_packet(tsk_pos, packet_pos, first_packet->len); 
	            printf("send end\n");
	            tim_pos->retran_cnt++;
	            tim_pos->timeout = 0;
	        }
	    }
	}
}

// set the timewait timer of a tcp sock, by adding the timer into timer_list
void tcp_set_timewait_timer(struct tcp_sock *tsk)
{
	fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
	struct tcp_timer *timp = &(tsk->timewait);
	init_list_head(&(timp->list));
	timp->type = 0;
	timp->timeout = 0;
	timp->enable = 1;
	timp->retran_cnt = 0;
	list_add_tail(&(timp->list), &timer_list);
}

void tcp_set_retrans_timer(struct tcp_sock *tsk)
{
	fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
	struct tcp_timer *timp = &(tsk->retrans_timer);
	init_list_head(&(timp->list));
	timp->type = 1;
	timp->timeout = 0;
	timp->enable = 1;
	timp->retran_cnt = 0;
	list_add_tail(&(timp->list), &timer_list);
}

void tcp_unset_retrans_timer(struct tcp_sock *tsk) {  //reset it to 0
    fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
    struct tcp_timer *timp = &(tsk->retrans_timer);
    timp->timeout = 0;
	timp->retran_cnt = 0;
}

// scan the timer_list periodically by calling tcp_scan_timer_list
void *tcp_timer_thread(void *arg)
{
	init_list_head(&timer_list);
	while (1) {
		usleep(TCP_TIMER_SCAN_INTERVAL);
		tcp_scan_timer_list();
	}

	return NULL;
}
