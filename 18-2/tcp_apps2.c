#include "tcp_sock.h"

#include "log.h"

#include <stdlib.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>
//#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#define  WORKER_NUM 3
#define  BUF_LEN    20
#define  NAME_LEN   17
#define  FILE_BUF_SIZE 1024

pthread_mutex_t snd_rcv_lock;
extern pthread_mutex_t new_lock;
extern int server;
extern int client;
extern int rbuf_include;
extern int sleep_on_recv;

//send是在对方接受窗口为0的时候sleep_on，接收窗口这个信息在任何一个数据包里都包含，等到不为0时则wake_up
int tcp_sock_read(struct tcp_sock *tsk, char *buf, int len) {
    //fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
    pthread_mutex_lock(&snd_rcv_lock);
    int i = 0;
    //printf("READ-1, %d\n", (int)tsk);
    struct tcp_sock *tsk_pos = (server==1)?(tsk->parent):tsk;
    if(ring_buffer_empty(tsk_pos->rcv_buf)) {
        pthread_mutex_unlock(&snd_rcv_lock);
        //printf("READ-1.5\n");
        //printf("sleep_on--tsk_addr: %d\n", (int)tsk_pos);
        sleep_on_recv = 1;
        sleep_on(tsk_pos->wait_recv);  //recv是等待ring_buffer不再empty，wake_up之前需要write_ring_buffer
        
        pthread_mutex_lock(&snd_rcv_lock);
    }
    i = read_ring_buffer(tsk_pos->rcv_buf, buf, len);
    //printf("%s, %s\n", tsk_pos->rcv_buf->buf, buf);
    if(i>=0) {
        //printf("READ-choose-1:%d, %d\n", i, len);
        pthread_mutex_unlock(&snd_rcv_lock);
        //printf("[READ END]\n");
        return i;
    } else {
        //printf("READ-choose-3:%d, %d\n", i, len);
        pthread_mutex_unlock(&snd_rcv_lock);
        //printf("[READ END]\n");
        return -1;
    }
}


int tcp_sock_write(struct tcp_sock *tsk, char *buf, int len){
    //fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
    int i = 0;
    int record_len = len;
    pthread_mutex_lock(&snd_rcv_lock);
    struct tcp_sock *tsk_pos = (server==1)?(tsk->parent):tsk; //?????
    //printf("[SEND START]\n");
    while(len > 0) {
        if(len > 1500 - ETHER_HDR_SIZE - IP_BASE_HDR_SIZE - TCP_BASE_HDR_SIZE) {
            //send max len包
            //printf("WRITE-1, len = %d\n", len);
            char *packet = (char *)malloc(1514);
            char *data = (char *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + TCP_BASE_HDR_SIZE);
            i = 0;
            while(i < len) {
                data[i] = buf[i];
                i++;
            }
            buf = buf[i];  //?????
            tcp_send_packet(tsk_pos, packet, 1500);
            printf("TCP SEND DATA PACKET\n");
            //printf("not last write packet:%s, %s\n", pos, data);
            len -= (1500 - ETHER_HDR_SIZE - IP_BASE_HDR_SIZE - TCP_BASE_HDR_SIZE);
        } else {  //len < a packet
            //printf("WRITE-2, len = %d\n", len);
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
            //printf("last write packet:%s, %s\n", pos, data);
            len = 0;
        }
    }
    pthread_mutex_unlock(&snd_rcv_lock);
    //printf("[SEND END]\n");
    return record_len;
}

// tcp server application, listens to port (specified by arg) and serves only one
// connection request
//void *tcp_server(void *arg)
void *tcp_server()
{
	server = 1;
    int i=0, j=0;
    struct tcp_sock *wor, *cworker;
    struct sock_addr caddr;
    int count_msg[26];
    int txtname_len;
    int start[3], end[3];
    int wor_num = 0, book_len = 0, workeread_len[3];
    char ch;
    for(i = 0; i < 26; i++) {
        count_msg[i] = 0;
    }
    for(i = 0; i < WORKER_NUM; i++) {
        start[i] = 0;
        end[i] = 0;
        workeread_len[i] = 0;
    }
    
    
    wor = alloc_tcp_sock();
    if (wor == NULL) {
        printf("create socket failed\n");
    }
    //bind
    //set master IP as mas.sin_addr.s_addr, bind(master IP????)
    //--worker.sin_family = AF_INET;
    //--worker.sin_addr.s_addr = INADDR_ANY;  //mark, ?
    //--worker.sin_port = htons(12345);
    //caddr.ip = htonl(0);
    //caddr.ip = inet_addr("10.0.0.1");
    caddr.port = htons(12345);
    
    if (tcp_sock_bind(wor, &caddr) != 0) {
        perror("bind failed\n");
    }
    printf("bind done\n");
      
        
    //listen
    tcp_sock_listen(wor, 3);
    printf("listening...\n");
    
    //accept   //recv an fd
    //--int slen = sizeof(struct sockaddr_in);
    if ((cworker = tcp_sock_accept(wor)) == NULL) {
        perror("accept failed\n");
    }
    printf("connection accepted\n"); 
        
    //recv:message
    tcp_sock_read(cworker, &txtname_len, sizeof(int));
    char name[txtname_len+1];
    for(i = 0; i < txtname_len+1; i++) {
        name[i] = 0;
    }
    tcp_sock_read(cworker, &name, NAME_LEN);
    tcp_sock_read(cworker, &wor_num, sizeof(int));
    //recv(wors, &end, sizeof(int), 0);
    //*(int *)((int *)msg + 1)
    printf("recv:%d, %s, %d\n", txtname_len, name, wor_num/*, *((int *)end)*/);
    
    
    //count
    FILE *fp;
    if((fp = fopen((char *)name,"r")) == NULL) {
        printf("fail to read\n");
    }
    fseek (fp , 0 , SEEK_END);
    book_len = ftell (fp);
    workeread_len[0] = book_len/3;
    workeread_len[1] = book_len/3;
    workeread_len[2] = book_len - workeread_len[0] - workeread_len[1];
    printf("%d, %d, %d, %d\n", book_len, workeread_len[0], workeread_len[1], workeread_len[2]);
    
    
    for(i = 0; i < WORKER_NUM; i++) {
        for(j = 0; j < i; j++) {
            start[i] += workeread_len[j];
        }
        for(j = 0; j <= i; j++) {
            end[i] += workeread_len[j];
        }
        printf("%d, %d\n", start[i], end[i]);
        printf("%d, %d\n", start[i], end[i]);
    }
    printf("%d, %d, %d, %d, %d, %d, %d\n", wor_num, start[0], start[1], start[2], end[0], end[1], end[2]);
    
    if(wor_num==256) {  //num2
        printf("server1\n");
        fseek (fp, start[1], SEEK_SET);
        for(i = 0; i < end[1]-start[1]; i++) {
            ch = fgetc(fp);
            if(ch >= 'a' && ch <= 'z') {
                count_msg[ch-'a']++;
            } else if(ch >= 'A' && ch <= 'Z') {
                count_msg[ch-'A']++;
            }
        }
    } else if(wor_num==512) {
        printf("server2\n");
        fseek (fp, start[2], SEEK_SET);
        for(i = 0; i < end[2]-start[2]; i++) {
            ch = fgetc(fp);
            if(ch >= 'a' && ch <= 'z') {
                count_msg[ch-'a']++;
            } else if(ch >= 'A' && ch <= 'Z') {
                count_msg[ch-'A']++;
            }
        }
    } else {
        printf("server3\n");
        fseek (fp, start[0], SEEK_SET);
        for(i = 0; i < end[0]-start[0]; i++) {
            ch = fgetc(fp);
            if(ch >= 'a' && ch <= 'z') {
                count_msg[ch-'a']++;
            } else if(ch >= 'A' && ch <= 'Z') {
                count_msg[ch-'A']++;
            }
        }
    }
    
    //send:count
    for(i = 0; i < 26; i++) {
        printf("count_%c: %d\t", i+97, (int)count_msg[i]);
    }
    tcp_sock_write(cworker, (void *)count_msg, 26*4);
    printf("sent msg!\n");
    
    
    tcp_sock_close(cworker);
    
    return NULL;
}

// tcp client application, connects to server (ip:port specified by arg), each
// time sends one bulk of data and receives one bulk of data 
//void *tcp_client(void *arg)
void *tcp_client()
{
    client = 1;
    int i = 0, j = 0, k = 0;
    struct tcp_sock *soc[WORKER_NUM];
    struct sock_addr saddr[WORKER_NUM];
    int txtname_len = NAME_LEN;
    char name1[] = {'w','a','r','_','a','n','d','_','p','e','a','c','e','.','t','x','t','\0'};
    char name2[] = {'w','b','r','_','a','n','d','_','p','e','a','c','e','.','t','x','t','\0'};
    char name3[] = {'w','c','r','_','a','n','d','_','p','e','a','c','e','.','t','x','t','\0'};
    int count_msg[WORKER_NUM][26];
    int total_count[26];
    char addr[WORKER_NUM][BUF_LEN];
    for(i = 0; i < WORKER_NUM; i++) {
        for(j = 0; j < BUF_LEN; j++) {
            addr[i][j] = 0;
        }
        for(j = 0; j < 26; j++) {
            count_msg[i][j]=0;
        }
    }
    FILE *fp;
    char buf[FILE_BUF_SIZE];
    for(i = 0; i < FILE_BUF_SIZE; i++) {
        buf[i] = 0;
    }
    char ch;

    
    //create fd
    for(i=0; i<WORKER_NUM; i++) {
        //--soc[i] = socket(AF_INET, SOCK_STREAM, 0);
        soc[i] = alloc_tcp_sock();
        if (soc[i] == NULL) {
            printf("create master failed\n");
        }
    }
    //connect
    if((fp = fopen("workers.conf","r")) == NULL) {
        printf("fail to read\n");
    }
    i = 0;
    while(EOF != (ch=fgetc(fp)))  
    {  
        buf[i] = ch;
        i++;
    }
    buf[i] = 0;
    fclose(fp); 
    printf("conf:%s", buf);
    for(i = 0, j = 0, k = 0; buf[i] != 0; i++) {
        if(buf[i]=='\n') {
            j++;
            k = 0;
        } else {
            addr[j][k] = buf[i];
            k++;
        }
    
    }
    printf("addr: %s, %s, %s\n", addr[0], addr[1], addr[2]);
    char name[18];
    //read .conf get IP for 3 workers
    //for 3 workers, set IP as dest.sin_addr.s_addr, connect
    for(i = 0; i < WORKER_NUM; i++) {
        saddr[i].ip = inet_addr(addr[i]);
        saddr[i].port = htons(12345);
        if (tcp_sock_connect(soc[i], &saddr[i]) != 0) {
            perror("connect failed\n");
        }
        printf("connected\n");
        for(j = 0; j < 18; j++) {
            if(i==0) {
                name[j] = name1[j];
            } else if(i==1) {
                name[j] = name2[j];
            } else {
                name[j] = name3[j];
            }
        }
        tcp_sock_write(soc[i], &txtname_len, sizeof(int));
        tcp_sock_write(soc[i], &name, sizeof(name));
        tcp_sock_write(soc[i], (void *)&i, sizeof(int));
        printf("RECV_DATA\n");
        tcp_sock_read(soc[i], (void *)(count_msg[i]), 26*4);
        for(j = 0; j < 26; j++) {
            printf("count_%c: %d\t", j+97, (int)(ntohl(count_msg[j])));
        }
        tcp_sock_close(soc[i]);
        printf("CLOSED\n");
    }
    
    //add
    //printf
    for(i = 0; i < 26; i++) {
        total_count[i] = count_msg[0][i] + count_msg[1][i] + count_msg[2][i];
        printf("count_%c: %d\t", i+97, total_count[i]);
    }
    
    
    return NULL;
}
