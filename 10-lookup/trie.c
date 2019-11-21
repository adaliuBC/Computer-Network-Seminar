#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<assert.h>
#include<time.h>
#include<sys/time.h>
#define FILENAME 20
#define IPLEN 15
#define ROWLEN 20
#define LE_IP_FMT_STR(ip) ((u8 *)&(ip))[3], \
						  ((u8 *)&(ip))[2], \
 						  ((u8 *)&(ip))[1], \
					      ((u8 *)&(ip))[0]
#define BE_IP_FMT_STR(ip) ((u8 *)&(ip))[0], \
						  ((u8 *)&(ip))[1], \
 						  ((u8 *)&(ip))[2], \
					      ((u8 *)&(ip))[3]
#define NET_IP_FMT_STR(ip)	LE_IP_FMT_STR(ip)


typedef int u32;
typedef char u8;

typedef struct trie_node {
    struct trie_node *father,*child0, *child1;
    u32 ip;  //needless?
    u32 port;
    u32 prefix_length;
}useless_specifier;
int memory_usage = 0;

void init_trie_node(struct trie_node *, struct trie_node *father);
void create_trie(char *, struct trie_node *);
u32 search_trie(u32, struct trie_node *);
void read_row(char *row, u32 *ip, int *port, int *prelen);
int  len_to_prefix(int prelen);

void init_trie_node(struct trie_node *node, struct trie_node *father) {
    node->father = father;
    node->child0 = NULL;
    node->child1 = NULL;
    node->ip = 0;
    node->port = 0;
    node->prefix_length = 0;
}


void read_row(char *row, u32 *ip, int *port, int *prelen) {
    int i = 0, j = 0;
    int ip_dec[4] = {0, 0, 0, 0};
    for(i = 0; row[i]!=' '; i++) {
        if(row[i]!='.') {
            ip_dec[j] = 10*ip_dec[j]+(row[i]-48);
        } else {
            j++;
        }
    }
    int ip_pos = 0;
    for(j = 0; j < 4; j++) {
        ip_pos = ip_pos*256 + ip_dec[j];
    }
    *ip = ip_pos;
    int len_pos = 0;
    i++;
    for( ; row[i]!=' '; i++) {
        len_pos = 10*len_pos+(row[i]-48);
    }
    *prelen = len_pos;
    int port_pos = 0;
    i++;
    port_pos = 10*port_pos+(row[i]-48);
    *port = port_pos;
    //printf("in:%s----out:ip=%d.%d.%d.%d, prelen = %d, port = %d\n", row, NET_IP_FMT_STR(*ip), *prelen, *port);
    return;
}

int len_to_prefix(int prelen) {
    int i = 0;
    int prefix = 0;
    for(i = 0; i < prelen; i++) {
        prefix = prefix | (0x1<<(31-i));
    }
    //printf("prelen = %d, prefix = %d.%d.%d.%d\n", prelen, NET_IP_FMT_STR(prefix));
    return prefix;
}


void create_trie(char *file_name, struct trie_node *root) {
    FILE *fp;
    char ch;
    char row[ROWLEN];
    int i = 0;
    if(NULL == (fp = fopen("forwarding-table.txt", "r"))) {
        printf("error\n");
    }
    while(1) {
       i = 0;
       //按行读入forwarding-table.txt文件
       while((ch=fgetc(fp))!='\n' && ch!=EOF) {
           row[i] = ch;
           i++;
       } 
       if(ch == EOF) {
           break;
       }
       u32 ip = 0;
       int port = 0;
       int prelen = 0;
       read_row(row, &ip, &port, &prelen);
       int prefix = len_to_prefix(prelen);
       int lor = 0; //ip第i位是0还是1
       struct trie_node *pos = root;
       struct trie_node *new_pos = NULL;
       //对于每一行，将其对应的节点插入到对应的前缀树节点上去(在相应时刻，创建新节点以满足前缀树的条件)
       //for(i < prefix_length) {
           //follow or create 新的node
       for(i = 0; i < prelen; i++) {
           lor = ip&(0x1<<(31-i));
           if(lor&&(pos->child1!=NULL)) {
               pos = pos->child1;
           } else if (!lor && (pos->child0!=NULL)) {
               pos = pos->child0;
           } else if (lor && (pos->child1==NULL)) {
               new_pos = (struct trie_node *)malloc(sizeof(struct trie_node));
               memory_usage += sizeof(struct trie_node);
               init_trie_node(new_pos, pos);
               pos->child1 = new_pos;
               pos = new_pos;
           } else if (!lor && (pos->child0==NULL)) {
               new_pos = (struct trie_node *)malloc(sizeof(struct trie_node));
               memory_usage += sizeof(struct trie_node);
               init_trie_node(new_pos, pos);
               pos->child0 = new_pos;
               pos = new_pos;
           }
       }
       //此时i=prelen
       pos->ip = ip;
       pos->port = port;
       pos->prefix_length = prelen;
       
       
       
       for(i = 0; i < ROWLEN; i++) {
           row[i] = 0;
       }
    }
    fclose(fp);
}


u32 search_trie(u32 ip, struct trie_node *root) {
    int port = -1;
    int lor = 0;
    int i = 0, j = 0;
    
    struct trie_node *pos = root;
    for(i = 0; i < 32; i++) {
        lor = ip&(0x1<<(31-i));
        if(pos->ip==ip) { //找到了ip-port对，返回
            //printf("yes!\n");
            port = pos->port;
            //return port;
        }
        int ipp = (int)pos->ip;
        //printf("[[%d]], ", lor);
        if(lor && (pos->child1!=NULL)) {  //还在找
            //printf("pos->ip:%d.%d.%d.%d\n", NET_IP_FMT_STR(ipp));
            pos = pos->child1;
        } else if(!lor && pos->child0!=NULL) {  //还在找
            //printf("pos->ip:%d.%d.%d.%d\n", NET_IP_FMT_STR(ipp));
            pos = pos->child0;
        } else if((lor&&(pos->child1==NULL)) || (!lor&&(pos->child0==NULL))) {  //发现走不下去了，返回-1
        
            //printf("no!\n");
            /*if(pos->child0==NULL&&pos->child1==NULL) {
                printf("???\n");
            }*/
            //port = -1;
            return port;
        }
    }
    return port; //走到32了，返回-1
}

int main() {
    //第一阶段：创建前缀树
    char file[FILENAME] = "forwarding-table.txt";
    char ip_addr[IPLEN];
    u32 port;
    int i = 0;
    struct timeval time1, time2;
    long int time_int = 0;
    for(i = 0; i < IPLEN; i++) {
        ip_addr[i] = 0;
    }
    //建立前缀树的根节点

    struct trie_node *root = (struct trie_node *)malloc(sizeof(struct trie_node));
    memory_usage += sizeof(struct trie_node);
    printf("1\n");
    init_trie_node(root, NULL);
    printf("2\n");
    create_trie(file, root);
    printf("3\n");
    //前缀树的创建完成
    printf("trie_create_finished.\ntotal_size = %d\n", memory_usage);
    
    
    
    //
    FILE *fp;
    char row[ROWLEN];
    char ch;
    
    if(NULL == (fp = fopen("forwarding-table.txt", "r"))) {
        printf("error\n");
    }
    while(1) {
       for(i = 0; i < ROWLEN; i++) {
           row[i] = 0;
       }
       i = 0;
       //按行读入forwarding-table.txt文件
       while((ch=fgetc(fp))!='\n' && ch!=EOF) {
           row[i] = ch;
           i++;
       } 
       if(ch == EOF) {
           break;
       }
       u32 ip_test = 0;
       int port_test = 0, port_get = 0;
       int useless2 = 0;
       gettimeofday(&time1, NULL);
       read_row(row, &ip_test, &port_test, &useless2);
       gettimeofday(&time2, NULL);
       time_int += time2.tv_usec-time1.tv_usec;
       port_get = search_trie(ip_test, root);
       /*if(port_get!=port_test) {
           printf("wrong! %d, %d, %d\n", ip_test, port_test, port_get);
       }*/
       //assert(port_get==port_test);
    }
    printf("asserted, time = %ld\n", time_int);
    
    //
    //第二阶段：查找前缀树
    //以字符串形式输入要查找的IP地址
    int ip_dec[4] = {0, 0, 0, 0};
    int ip = 0, j = 0;
    while(1) {
        for(i = 0; i < IPLEN; i++) {
            ip_addr[i] = 0;
        }
        for(i = 0; i < 4; i++) {
            ip_dec[i] = 0;
        }
        i = 0;
        j = 0;
        printf("IP_addr(end with Enter):");
        scanf("%s", ip_addr);
        printf("ip_addr:%s\n", ip_addr);
        //printf("searching-0\n");
        if(strcmp(ip_addr, "quit")==0) {
            break;
        }
        printf("ip_addr:%s\n", ip_addr);
        for(i = 0; ip_addr[i]!=0; i++) {
            if(ip_addr[i]!='.') {
                ip_dec[j] = 10*ip_dec[j]+(ip_addr[i]-48);
            } else {
                j++;
            }
        }
        printf("ip_addr:%s\n", ip_addr);
        ip_dec[3] = 0;
        for(j = 0; j < 4; j++) {
            ip = ip*256 + ip_dec[j];
        }
        printf("ip_addr:%s\n", ip_addr);
        
        
        port = search_trie(ip, root);
        //根据IP地址的位数搜索前缀树，直到找到了IP地址，或者前缀树没有子节点了，或者搜索到了最后一位
        //返回NULL或者前缀树相应的port
        //输出port或者报错信息。
        for(i = 0; i < IPLEN; i++) {
            ip_addr[i] = 0;
        }
        if(port==-1) {
            printf("ERROR: no such ip addr\n");
        } else {
            printf("%d\n", port);
            
        }
    }

}
