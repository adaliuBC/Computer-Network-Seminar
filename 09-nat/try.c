#include <stdio.h>
#include <string.h>
#define MAXLEN 200
int strpass(char *stan, char *str, int inpos);

int main() {
    FILE *fp; 
    char input[MAXLEN];
    int i = 0;
    char intif_name[16], extif_name[16];
    char rule1[36], rule2[36];
    if(NULL == (fp = fopen("example-config.txt", "r")))  
    {  
        printf("error\n");
    }  
  
    char ch;  
    while(EOF != (ch=fgetc(fp)))  
    {  
        input[i] = ch;
        i++;
    }
    printf("%s\n", input);
    int intpos = strpass("internal-iface: ", input, 0);
    i = intpos;
    while(input[i] != '\n') {
        i++;
    }
    int intend = i;
    memcpy(intif_name, &input[intpos], intend-intpos);
    
    int extpos = strpass("external-iface: ", input, intend+1);
    i = extpos;
    while(input[i] != '\n') {
        i++;
    }
    int extend = i;
    memcpy(extif_name, &input[extpos], extend-extpos);
    
    printf("1:%s, %s\n", intif_name, extif_name);
    
    
    int rulepos1 = strpass("dnat-rules: ", input, extend+2);
    i = rulepos1;
    while(input[i] != '\n') {
        i++;
    }
    int rulend1 = i;
    memcpy(rule1, &input[rulepos1], rulend1-rulepos1);
    
    int rulepos2 = strpass("dnat-rules: ", input, rulend1+1);
    i = rulepos2;
    while(input[i] != '\n' && input[i] != EOF) {
        i++;
    }
    int rulend2 = i;
    memcpy(rule2, &input[rulepos2], rulend2-rulepos2);
    printf("%s\n%s\n", rule1, rule2);
    
    fclose(fp); 
}

int strpass(char *stan, char *str, int inpos) {
    int i = inpos;
    //printf("%c, %c\n", stan[i-inpos], str[i]);
    for(i = inpos; str[i] == stan[i-inpos]; ) {
        i++;
    }
    return i;
}
