#include<stdio.h>
#define MAX_INT 65535

int arcs[4][4];
int prev[4];
int visited[4];
int dist[4];

void algor(int node_num) {
    int i = 0, j = 0, k = 0;
    for(i = 0; i < node_num; i++) {
        dist[i] = MAX_INT;
        visited[i] = 0;
        prev[i] = -1;
    }
    dist[0] = 0;
    //--visited[0] = 1;
    
    printf("prev:");
    for(i = 0; i <node_num; i++) {
        printf("%d ", prev[i]);
    }
    printf("\n");
    
    
    
    /*--for(i = 1; i < node_num; i++) {
        dist[i] = arcs[0][i];
    }*/
    
    printf("dist:");
    for(i = 0; i <node_num; i++) {
        printf("%d ", dist[i]);
    }
    printf("\n");
    
    int visit_num = 1;
    int mindis = MAX_INT;
    int min_no = -1;
    int pre = 0;
    while(visit_num < node_num) {
      for(i = 0; i < node_num; i++) {
        if(visited[i]==0 && dist[i] < mindis) {
            mindis = dist[i];
            min_no = i;
        }
      }
      printf("min_no:%d\n", min_no);
      visited[min_no] = 1;
      /*--if(visit_num==1) {
          prev[min_no] = 0;
      }*/
      for(j = 0; j < node_num; j++) {
        if(visited[j]==0 && arcs[min_no][j]==1 && dist[min_no]+arcs[min_no][j] < dist[j]) {
            dist[j] = dist[min_no] + arcs[min_no][j];
            prev[j] = min_no;
        }
      }
      visit_num++;
      pre = min_no;
      mindis = MAX_INT;
      min_no = -1;
      
    }
    /*for i in range(num):
    u = min_dist(dist, visited, num)
    visited[u] = true
    
        for v in range(num):
            if visited[v] == false && graph[u][v] > 0 && dist[u] + graph[u][v] < dist[v]:
                dist[v] = dist[u] + graph[u][v]
                prev[v] = u*/

}


int main() {

    int i = 0, j = 0;
    for(i = 0; i < 4; i++) {
        prev[i] = -1;
        visited[i] = 0;
        dist[i] = 0;
        for(j = 0; j < 4; j++) {
            arcs[i][j] = MAX_INT;
        }
    }
    arcs[0][1] = 1;
    arcs[1][0] = 1;
    //arcs[0][2] = 1;
    //arcs[2][0] = 1;
    arcs[1][3] = 1;
    arcs[3][1] = 1;
    arcs[2][3] = 1;
    arcs[3][2] = 1;
    //arcs[3][4] = 1;
    //arcs[4][3] = 1;
    //arcs[4][5] = 1;
    //arcs[5][4] = 1;
    for(i = 0; i < 4; i++) {
        for(j = 0; j < 4; j++) {
            printf("%d ", arcs[i][j]);
        }
        printf("\n");
    }
    algor(4);
    for(i = 0; i < 4; i++) {
        printf("%d ", prev[i]);
    }
}
