#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<assert.h>

int main() {
    FILE *fpr, *fpw;
    char rbuf[128];
    char wbuf[128];
    char ch = 0;
    char *enter = "\n";
    int i = 0;
    if(NULL == (fpr = fopen("client-input.dat", "r"))) {
        printf("open filer error\n");
    }
    if(NULL == (fpw = fopen("server-output.dat", "w"))) {
        printf("open filew error\n");
    }
    while(1) {
       for(i = 0; i < 128; i++) {
           wbuf[i] = 0;
       }
       //按行读入client-input.dat文件
       i = 0;
       while((ch=fgetc(fpr))!='\n' && ch!=EOF) {
           wbuf[i] = ch;
           i++;
       }
       
       //复制行
       for(i = 0; i < 128; i++) {
           rbuf[i] = wbuf[i];
       }
       
       //写入srever_output.dat
       fputs(rbuf, fpw);
       if(ch!=EOF) {
           fputs(enter, fpw);
       } else {
           break;
       }
       
    }
    fclose(fpr);
    fclose(fpw);
}
