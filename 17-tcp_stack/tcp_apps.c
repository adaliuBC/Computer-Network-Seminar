#include "tcp_sock.h"

#include "log.h"

#include <stdlib.h>
#include <unistd.h>

pthread_mutex_t snd_rcv_lock;
extern pthread_mutex_t new_lock;
extern int server;
extern int client;
extern int rbuf_include;
extern int sleep_on_recv;

//send是在对方接受窗口为0的时候sleep_on，接收窗口这个信息在任何一个数据包里都包含，等到不为0时则wake_up
int tcp_sock_read(struct tcp_sock *tsk, char *buf, int len) {
    fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
    pthread_mutex_lock(&snd_rcv_lock);
    int i = 0, j = 0;
    printf("READ-1, %d\n", (int)tsk);
    struct tcp_sock *tsk_pos = (server==1)?(tsk->parent):tsk;
    if(ring_buffer_empty(tsk_pos->rcv_buf)) {
        pthread_mutex_unlock(&snd_rcv_lock);
        printf("READ-1.5\n");
        printf("sleep_on--tsk_addr: %d\n", (int)tsk_pos);
        sleep_on_recv = 1;
        sleep_on(tsk_pos->wait_recv);  //recv是等待ring_buffer不再empty，wake_up之前需要write_ring_buffer
        
        pthread_mutex_lock(&snd_rcv_lock);
    }
    i = read_ring_buffer(tsk_pos->rcv_buf, buf, len);
    printf("%s, %s\n", tsk_pos->rcv_buf->buf, buf);
    j = 0;
    printf("READ-2\n");
    if(i>=0) {
        printf("READ-choose-1:%d, %d\n", i, len);
        pthread_mutex_unlock(&snd_rcv_lock);
        printf("[READ END]\n");
        return i;
    } else {
        printf("READ-choose-3:%d, %d\n", i, len);
        pthread_mutex_unlock(&snd_rcv_lock);
        printf("[READ END]\n");
        return -1;
    }
}


int tcp_sock_write(struct tcp_sock *tsk, char *buf, int len){
    fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
    int i = 0;
    char *pos = buf;
    int record_len = len;
    pthread_mutex_lock(&snd_rcv_lock);
    printf("[SEND START]\n");
    struct tcp_sock *tsk_pos = (server==1)?(tsk->parent):tsk; //?????
    while(len > 0) {
        if(len > 1500 - ETHER_HDR_SIZE - IP_BASE_HDR_SIZE - TCP_BASE_HDR_SIZE) {
            //send max len包
            printf("WRITE-1, len = %d\n", len);
            char *packet = (char *)malloc(1500);
            char *data = (char *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + TCP_BASE_HDR_SIZE);
            i = 0;
            while(i < len) {
                data[i] = buf[i];
                i++;
            }
            buf = buf[i];  //?????
            tcp_send_packet(tsk_pos, packet, 1500);
            
            printf("TCP SEND DATA PACKET\n");
            printf("not last write packet:%s, %s\n", buf, data);
            len -= (1500 - ETHER_HDR_SIZE - IP_BASE_HDR_SIZE - TCP_BASE_HDR_SIZE);
        } else {  //len <= a packet
            printf("WRITE-2, len = %d\n", len);
            char *packet = (char *)malloc(len + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + TCP_BASE_HDR_SIZE);
            char *data = (char *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + TCP_BASE_HDR_SIZE);
            i = 0;
            while(i < len) {
                data[i] = buf[i];
                i++;
            }
            buf = buf[i];  //?????
            tcp_send_packet(tsk_pos, packet, len + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + TCP_BASE_HDR_SIZE);
            
            printf("TCP SEND DATA PACKET\n");
            //printf("last write packet:%s, %s\n", buf, data);
            len = 0;
        }
    }
    pthread_mutex_unlock(&snd_rcv_lock);
    printf("[SEND END]\n");
    return record_len;
}

// tcp server application, listens to port (specified by arg) and serves only one
// connection request
void *tcp_server(void *arg)
{
	u16 port = *(u16 *)arg;
	server = 1;
	struct tcp_sock *tsk = alloc_tcp_sock();
    pthread_mutex_init(&new_lock, NULL);
	struct sock_addr addr;
	addr.ip = htonl(0);
	addr.port = port;
	if (tcp_sock_bind(tsk, &addr) < 0) {
		log(ERROR, "tcp_sock bind to port %hu failed", ntohs(port));
		exit(1);
	}

	if (tcp_sock_listen(tsk, 3) < 0) {
		log(ERROR, "tcp_sock listen failed");
		exit(1);
	}

	log(DEBUG, "listen to port %hu.", ntohs(port));

	struct tcp_sock *csk = tcp_sock_accept(tsk);

	log(DEBUG, "accept a connection.");
    printf("tsk->addr: %d\ncsk->addr: %d\n", (int)tsk, (int)csk);
	char rbuf[1001];
	char wbuf[1024];
	int rlen = 0;
	while (1) {
		rlen = tcp_sock_read(csk, rbuf, 1000);
		if (rlen == 0) {
			log(DEBUG, "tcp_sock_read return 0, finish transmission.");
			break;
		} 
		else if (rlen > 0) {
			rbuf[rlen] = '\0';
			sprintf(wbuf, "server echoes: %s", rbuf);
			if (tcp_sock_write(csk, wbuf, strlen(wbuf)) < 0) {
				log(DEBUG, "tcp_sock_write return negative value, something goes wrong.");
				exit(1);
			}
			
		}
		else {
			log(DEBUG, "tcp_sock_read return negative value, something goes wrong.");
			exit(1);
		}
	}

	log(DEBUG, "close this connection.");
    sleep(5);
	tcp_sock_close(csk);
	
	return NULL;
}

// tcp client application, connects to server (ip:port specified by arg), each
// time sends one bulk of data and receives one bulk of data 
void *tcp_client(void *arg)
{
    client = 1;
    
	struct sock_addr *skaddr = arg;

	struct tcp_sock *tsk = alloc_tcp_sock();

	if (tcp_sock_connect(tsk, skaddr) < 0) {
		log(ERROR, "tcp_sock connect to server ("IP_FMT":%hu)failed.", \
				NET_IP_FMT_STR(skaddr->ip), ntohs(skaddr->port));
		exit(1);
	}

	char *wbuf = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	int wlen = strlen(wbuf);
	char rbuf[1001];
	int rlen = 0;

	int n = 10;
	for (int i = 0; i < n; i++) {
		if (tcp_sock_write(tsk, wbuf + i, wlen - n) < 0) 
			break;
        printf("tsk->seq: %d\n", tsk->snd_nxt);
		rlen = tcp_sock_read(tsk, rbuf, 1000);
		printf("~~~~~~1:%s\n", rbuf);
		if (rlen == 0) {
			log(DEBUG, "tcp_sock_read return 0, finish transmission.");
			break;
		}
		else if (rlen > 0) {
			rbuf[rlen] = '\0';
			fprintf(stdout, "%s\n", rbuf);
		}
		else {
			log(DEBUG, "tcp_sock_read return negative value, something goes wrong.");
			exit(1);
		}
		printf("tsk->seq: %d\n", tsk->snd_nxt);
		printf("~~~~~~2:%s\n", rbuf);
		sleep(1);
	}
    printf("tsk->seq: %d\n", tsk->snd_nxt);
	tcp_sock_close(tsk);

	return NULL;
}
