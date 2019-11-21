char* buffer;
long lSize;
FILE *fp;

fp = fopen("someFile.txt","r");
if (fp==NULL) 
{
  printf("reading error\n");
  exit (1);
}

// 将光标停在文件的末尾
fseek (fp , 0 , SEEK_END);
//返回文件的大小（单位是bytes）
lSize = ftell (fp);
//将光标重新移回文件的开头
rewind (fp);
//将文件的内容读取到buffer中
buffer = malloc(lSize);
fread (buffer,1,lSize,fp);
fclose (fp);
--------------------- 
作者：lkxlaz 
来源：CSDN 
原文：https://blog.csdn.net/lkxlaz/article/details/51454473 
版权声明：本文为博主原创文章，转载请附上博文链接！
