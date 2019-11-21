#include "tcp_sock.h"

#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

pthread_mutex_t snd_rcv_lock;
extern pthread_mutex_t new_lock;
extern int server;
extern int client;
extern int rbuf_include;
extern int sleep_on_recv;
int resend = 0;

int usleep(useconds_t usec);
//send是在对方接受窗口为0的时候sleep_on，接收窗口这个信息在任何一个数据包里都包含，等到不为0时则wake_up
int tcp_sock_read(struct tcp_sock *tsk, char *buf, int len) {
    fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
    pthread_mutex_lock(&snd_rcv_lock);
    int i = 0;
    struct tcp_sock *tsk_pos = (server==1)?(tsk->parent):tsk;
    if(ring_buffer_empty(tsk_pos->rcv_buf)) {
        pthread_mutex_unlock(&snd_rcv_lock);
        sleep_on_recv = 1;
        sleep_on(tsk_pos->wait_recv);  //recv是等待ring_buffer不再empty，wake_up之前需要write_ring_buffer
        
        pthread_mutex_lock(&snd_rcv_lock);
    }
    i = read_ring_buffer(tsk_pos->rcv_buf, buf, len);
    //printf("%s, %s\n", tsk_pos->rcv_buf->buf, buf);
    if(i>=0) {
        pthread_mutex_unlock(&snd_rcv_lock);
        //printf("[READ END]\n");
        return i;
    } else {
        pthread_mutex_unlock(&snd_rcv_lock);
        //printf("[READ END]\n");
        return -1;
    }
}


int tcp_sock_write(struct tcp_sock *tsk, char *buf, int len){
    fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
    
    int i = 0;
    int record_len = len;
    printf("len: %d\n", record_len);
    pthread_mutex_lock(&snd_rcv_lock);
    struct tcp_sock *tsk_pos = (server==1)?(tsk->parent):tsk;
    printf("[SEND START]\n");
    while(len > 0) {
        if(len > 1500 - ETHER_HDR_SIZE - IP_BASE_HDR_SIZE - TCP_BASE_HDR_SIZE) {  //send max len包
            char *packet = (char *)malloc(1514);
            char *data = (char *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + TCP_BASE_HDR_SIZE);
            i = 0;
            while(i < len) {
                data[i] = buf[i];
                i++;
            }
            buf = &buf[i];
            tcp_send_packet(tsk_pos, packet, 1500);
            printf("TCP SEND DATA PACKET\n");
            len -= (1500 - ETHER_HDR_SIZE - IP_BASE_HDR_SIZE - TCP_BASE_HDR_SIZE);
        } else {  //len < a packet
            char *packet = (char *)malloc(len + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + TCP_BASE_HDR_SIZE);
            char *data = (char *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + TCP_BASE_HDR_SIZE);
            i = 0;
            while(i < len) {
                data[i] = buf[i];
                i++;
            }
            buf = &buf[i];
            tcp_send_packet(tsk_pos, packet, len + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + TCP_BASE_HDR_SIZE);
            printf("TCP SEND DATA PACKET\n");
            len = 0;
        }
    }
    pthread_mutex_unlock(&snd_rcv_lock);
    printf("[SEND END], %d\n", record_len);
    return record_len;
    //return 0;
}

// tcp server application, listens to port (specified by arg) and serves only one
// connection request
void *tcp_server(void *arg)
{
    char *enter = "\n";
    int i = 0;
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
	int rlen = 0;
	FILE * fpw = fopen("server-output.dat","w");
	if(fpw == NULL) {
        printf("open filew error\n");
    }
	while (1) {
	    for(i = 0; i < 1001; i++) {
	        rbuf[i] = 0;
	    }
		rlen = tcp_sock_read(csk, rbuf, 1000);
		printf("RBUF: %s\n", rbuf);
		if (rlen == 0) {
			log(DEBUG, "tcp_sock_read return 0, finish transmission.");
			break;
		} 
		else if (rlen > 0) {
			rbuf[rlen] = '\0';
			fwrite(rbuf, sizeof(char), strlen(rbuf), fpw);  //应该写二进制数据
			fflush(fpw);
		}
		else {
			log(DEBUG, "tcp_sock_read return negative value, something goes wrong.");
			exit(1);
		}
	}
	fclose(fpw);

	log(DEBUG, "close this connection.");
    sleep(5);  //为了保证先收到包再进行close
	tcp_sock_close(csk);
	
	return NULL;
}

// tcp client application, connects to server (ip:port specified by arg), each
// time sends one bulk of data and receives one bulk of data 
void *tcp_client(void *arg)
{
    client = 1;
    
    FILE *fpr;
    char ch = 0;
    int i = 0;
    if(NULL == (fpr = fopen("client-input.dat", "r"))) {
        printf("open filer error\n");
    }
    
	struct sock_addr *skaddr = arg;

	struct tcp_sock *tsk = alloc_tcp_sock();

	if (tcp_sock_connect(tsk, skaddr) < 0) {
		log(ERROR, "tcp_sock connect to server ("IP_FMT":%hu)failed.", \
				NET_IP_FMT_STR(skaddr->ip), ntohs(skaddr->port));
		exit(1);
	}

	char wbuf[1024];
	int wlen = 0;

    while(1) {
        for(i = 0; i < 1024; i++) {
            wbuf[i] = 0;
        }
        printf("0\n");
        //按行读入client-input.dat文件
        i = 0;
        while((ch=fgetc(fpr))!='\n' && ch!= EOF) {
            wbuf[i] = ch;
            i++;
        }
        wbuf[i] = '\n';
        wlen = strlen(wbuf);
        printf("READ IN: %s, %d\n", wbuf, wlen);
		tcp_sock_write(tsk, wbuf, wlen);
		printf("1\n");
        if(wlen <= 74) {
            break;
        }
		printf("2\n");
		usleep(40000);
	}
    printf("3\n");
	tcp_sock_close(tsk);

	return NULL;
}
