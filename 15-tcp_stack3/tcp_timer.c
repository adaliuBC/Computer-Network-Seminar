#include "tcp.h"
#include "tcp_timer.h"
#include "tcp_sock.h"

#include <stdio.h>
#include <unistd.h>

static struct list_head timer_list;

// scan the timer_list, find the tcp sock which stays for at 2*MSL, release it
void tcp_scan_timer_list()
{
	//fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
	struct tcp_timer *tim_pos, *tim_q;
	list_for_each_entry_safe(tim_pos, tim_q, &timer_list, list) {  //?????
	    tim_pos->timeout += TCP_TIMER_SCAN_INTERVAL;
	    if(tim_pos->timeout > TCP_TIMEWAIT_TIMEOUT) {
	        printf("FINISHED!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	        list_delete_entry(&tim_pos->list);
	        //free(tim_pos);
	    }
	}
}

// set the timewait timer of a tcp sock, by adding the timer into timer_list
void tcp_set_timewait_timer(struct tcp_sock *tsk)
{
	fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
	struct tcp_timer *timp = &(tsk->timewait);
	timp->type = 0;
	timp->timeout = 0;
	timp->enable = 1;
	list_add_tail(&(timp->list), &timer_list);
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
