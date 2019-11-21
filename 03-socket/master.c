#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#define  WORKER_NUM 3
#define  BUF_LEN    20
#define  NAME_LEN   17
#define  FILE_BUF_SIZE 1024

int main() {
    int soc[WORKER_NUM], i = 0, j = 0, k = 0;
    int txtname_len = NAME_LEN;
    char name[] = {'w','a','r','_','a','n','d','_','p','e','a','c','e','.','t','x','t','\0'};
    int start[WORKER_NUM], end[WORKER_NUM];
    int count_msg[WORKER_NUM][26];
    int total_count[26];
    char addr[WORKER_NUM][BUF_LEN];
    int workeread_len[WORKER_NUM];
    for(i = 0; i < WORKER_NUM; i++) {
        soc[i] = 0;
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
        soc[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (soc[i] == -1) {
            printf("create master failed\n");
	    return -1;
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
        worker[i].sin_addr.s_addr = inet_addr(addr[i]);
        worker[i].sin_family = AF_INET;
        worker[i].sin_port = htons(12345);
    }
    
    for(i=0; i<WORKER_NUM; i++) {
        if (connect(soc[i], (struct sockaddr *)&worker[i], sizeof(worker[i])) < 0) {
            perror("connect failed\n");
            return 1;
        }
    }
    printf("connected\n");
    
    //prepare
        //read .txt, get length, /WORKER_NUM get the avg, get the start and end
    //!!!!!!!
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
    /*for(i = 0; i < 3; i++) {
        //memcpy((void *)startc[i], &start[i], 4);
        //memcpy((void *)endc[i], &end[i], 4);
        startc[i] = (char *)&start[i];
        endc[i] = (char *)&end[i];
        printf("char: %s, %s\n", startc[i], endc[i]);
    }*/
    for(i = 0; i < 3; i++) {
        startc = (char *)&start[i];
        endc = (char *)&end[i];
        send(soc[i], &txtname_len, sizeof(int), 0);
        send(soc[i], &name, sizeof(name), 0);
        send(soc[i], (void *)&i, sizeof(int), 0);
        //send(soc[i], &endc, sizeof(int), 0);
    } // zijiexu?????
        //4 time for each connect, 3 connects?
    //wait
    
    //recv:count
    for(i = 0; i < 3; i++) {
        recv(soc[i], (void *)(count_msg[i]), 26*4, 0);
    }
    
    
    //add
    //printf
    for(i = 0; i < 26; i++) {
        total_count[i] = count_msg[0][i] + count_msg[1][i] + count_msg[2][i];
        printf("count_%c: %d\t", i+97, total_count[i]);
    }

}
