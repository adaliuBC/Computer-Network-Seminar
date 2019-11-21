#include <stdio.h>
#include <string.h>
//#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include "/home/cod/Net/18-/include/tcp_sock.h"
#define  WORKER_NUM 3
#define  BUF_LEN    20
#define  NAME_LEN   17
#define  FILE_BUF_SIZE 1024

void *master_thread(void *arg) {
    int i = 0, j = 0, k = 0;
    struct tcp_sock *soc[WORKER_NUM];
    struct sock_addr saddr[WORKER_NUM];
    int txtname_len = NAME_LEN;
    char name[] = {'w','a','r','_','a','n','d','_','p','e','a','c','e','.','t','x','t','\0'};
    int start[WORKER_NUM], end[WORKER_NUM];
    int count_msg[WORKER_NUM][26];
    int total_count[26];
    char addr[WORKER_NUM][BUF_LEN];
    int workeread_len[WORKER_NUM];
    for(i = 0; i < WORKER_NUM; i++) {
        start[i] = 0;
        end[i] = 0;
        workeread_len[i] = 0;
        for(j = 0; j < BUF_LEN; j++) {
            addr[i][j] = 0;
        }
        for(j = 0; j < 26; j++) {
            count_msg[i][j]=0;
        }
    }
    struct sockaddr_in master, worker[3];
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
    printf("%s, %s, %s\n", addr[0], addr[1], addr[2]);
    //read .conf get IP for 3 workers
    //for 3 workers, set IP as dest.sin_addr.s_addr, connect
    for(i = 0; i < WORKER_NUM; i++) {
        //--worker[i].sin_addr.s_addr = inet_addr(addr[i]);
        //--worker[i].sin_family = AF_INET;
        //--worker[i].sin_port = htons(12345);
        saddr[i].ip = inet_addr(addr[i]);
        saddr[i].port = htons(12345);
    }
    
    for(i=0; i<WORKER_NUM; i++) {
        //--if (tcp_sock_connect(soc[i], (struct sockaddr *)&worker[i], sizeof(worker[i])) < 0) {
        if (tcp_sock_connect(soc[i], &saddr[i]) != 0) {
            perror("connect failed\n");
        }
    }
    printf("connected\n");
    
    //prepare
        //read .txt, get length, /WORKER_NUM get the avg, get the start and end
    int book_len = 0;

    if((fp = fopen("war_and_peace.txt","r")) == NULL) {
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
        //start[i] = htonl(start[i]);
        //end[i] = htonl(end[i]);
        printf("%d, %d\n", start[i], end[i]);
    }
    printf("%d, %d, %d, %d, %d, %d, %d\n", book_len, start[0], start[1], start[2], end[0], end[1], end[2]);
    //send:message
    char *startc, *endc;
    for(i = 0; i < 3; i++) {
        startc = (char *)&start[i];
        endc = (char *)&end[i];
        tcp_sock_write(soc[i], &txtname_len, sizeof(int));
        tcp_sock_write(soc[i], &name, sizeof(name));
        tcp_sock_write(soc[i], (void *)&i, sizeof(int));
        //send(soc[i], &endc, sizeof(int), 0);
    } // zijiexu?????
        //4 time for each connect, 3 connects?
    //wait
    
    //recv:count
    for(i = 0; i < 3; i++) {
        tcp_sock_read(soc[i], (void *)(count_msg[i]), 26*4);
    }
    
    
    //add
    //printf
    for(i = 0; i < 26; i++) {
        total_count[i] = count_msg[0][i] + count_msg[1][i] + count_msg[2][i];
        printf("count_%c: %d\t", i+97, total_count[i]);
    }
    
    for(i = 0; i < WORKER_NUM; i++) {
        tcp_sock_close(soc[i]);
    }
}



void *worker_thread(void *arg) {
    //create fd
    int i=0, j=0;
    struct tcp_sock *wor, *cworker;
    struct sock_addr caddr;
    struct sockaddr_in master, worker;
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
    caddr.ip = htonl(0);
    caddr.port = htons(12345);
    
    if (tcp_sock_bind(wor, &caddr) != 0) {
        perror("bind failed\n");
    }
    printf("bind done\n");
      
        
    //listen
    tcp_sock_listen(wor, 3);
    printf("listening...\n");
    //1 time!!!
    // backlog num == 1?????
    
    //accept   //recv an fd
    //--int slen = sizeof(struct sockaddr_in);
    if ((cworker = tcp_sock_accept(wor)) == NULL) {
        perror("accept failed\n");
    }
    printf("connection accepted\n"); 
        
    //recv:message
    tcp_sock_read(cworker, &txtname_len, sizeof(int));
    char name[txtname_len];
    for(i = 0; i < txtname_len; i++) {
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
        //start[i] = htonl(start[i]);
        //end[i] = htonl(end[i]);
        printf("%d, %d\n", start[i], end[i]);
    }
    printf("%d, %d, %d, %d, %d, %d, %d\n", wor_num, start[0], start[1], start[2], end[0], end[1], end[2]);
    
    if(wor_num==256) {  //num2
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
    /*for(i = 0; i < 26; i++) {
        count_msg[i] = 20;
    }*/
    tcp_sock_write(cworker, (void *)count_msg, 26*4);
    printf("sent msg!\n");
    
    
    tcp_sock_close(cworker);
    
}

int main(int argc, char **argv) {
    pthread_t thread;
    printf("%s\n", (argv+1)[0]);
    if (strcmp((argv+1)[0], "worker") == 0) {
        pthread_create(&thread, NULL, worker_thread, NULL);
    } else if (strcmp((argv+1)[0], "master") == 0) {
        pthread_create(&thread, NULL, master_thread, NULL);
    } else {
        printf("Wrong input\n");
    }
}
