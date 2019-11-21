#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAX_LINE 1024
int main()
{
    char buf[MAX_LINE];  /*缓冲区*/
    FILE *fp;            /*文件指针*/
    int len;             /*行字符个数*/
    if((fp = fopen("test.txt","r")) == NULL)
    {
        perror("fail to read");
        exit (1) ;
    }
    while(fgets(buf,MAX_LINE,fp) != NULL)
    {
        len = strlen(buf);
        buf[len-1] = '\0';  /*去掉换行符*/
        printf("%s %d \n",buf,len - 1);
    }
    return 0;
}

--------------------- 
作者：净无邪 
来源：CSDN 
原文：https://blog.csdn.net/naibozhuan3744/article/details/80610476 
版权声明：本文为博主原创文章，转载请附上博文链接！
