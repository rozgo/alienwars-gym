#ifndef ALIENWARS_ASTAR_H
#define ALIENWARS_ASTAR_H
#include <stdlib.h>
#include <math.h>
/* Graph-agnostic A*. Scratch storage is allocated by the caller at world/task
 * preparation, never in the simulation step. Positive edge costs; heuristics
 * must be admissible. Closed nodes can reopen when a cheaper path is found. */
#define AW_ASTAR_EDGES 32
typedef struct {int node;float cost;} AwAStarEdge;
typedef int (*AwAStarEdges)(void*,int,AwAStarEdge*);
typedef float (*AwAStarEstimate)(void*,int,int);
typedef struct {int capacity,count,expanded;int *heap,*slot,*parent;float *cost,*priority;} AwAStar;
static int aw_astar_init(AwAStar*s,int n){
    *s=(AwAStar){.capacity=n};if(n<1)return 0;
    s->heap=malloc((size_t)n*sizeof(int));s->slot=malloc((size_t)n*sizeof(int));s->parent=malloc((size_t)n*sizeof(int));
    s->cost=malloc((size_t)n*sizeof(float));s->priority=malloc((size_t)n*sizeof(float));
    return s->heap&&s->slot&&s->parent&&s->cost&&s->priority;
}
static void aw_astar_close(AwAStar*s){free(s->heap);free(s->slot);free(s->parent);free(s->cost);free(s->priority);*s=(AwAStar){0};}
static int aw_astar_less(const AwAStar*s,int a,int b){return s->priority[a]<s->priority[b]||(s->priority[a]==s->priority[b]&&a<b);}
static void aw_astar_up(AwAStar*s,int node){
    int p=s->slot[node];if(p<0){p=s->count++;s->heap[p]=node;}
    while(p){int q=(p-1)/2,other=s->heap[q];if(!aw_astar_less(s,node,other))break;s->heap[p]=other;s->slot[other]=p;p=q;}
    s->heap[p]=node;s->slot[node]=p;
}
static int aw_astar_pop(AwAStar*s){
    int node=s->heap[0],last=s->heap[--s->count];s->slot[node]=-2;
    if(s->count){int p=0;while(p*2+1<s->count){int c=p*2+1;if(c+1<s->count&&aw_astar_less(s,s->heap[c+1],s->heap[c]))c++;
        if(!aw_astar_less(s,s->heap[c],last))break;s->heap[p]=s->heap[c];s->slot[s->heap[p]]=p;p=c;}
        s->heap[p]=last;s->slot[last]=p;}
    return node;
}
static int aw_astar_path(AwAStar*s,int nodes,int start,int goal,void*ctx,AwAStarEdges edges,AwAStarEstimate estimate,int*out,int capacity){
    if(nodes<1||nodes>s->capacity||start<0||goal<0||start>=nodes||goal>=nodes||capacity<1||!out)return 0;
    s->count=s->expanded=0;
    for(int i=0;i<nodes;i++){s->cost[i]=INFINITY;s->parent[i]=-1;s->slot[i]=-1;}
    s->cost[start]=0;s->priority[start]=estimate(ctx,start,goal);aw_astar_up(s,start);
    while(s->count){int a=aw_astar_pop(s);s->expanded++;
        if(a==goal){int n=1;for(int p=goal;p!=start;p=s->parent[p])if(++n>capacity)return 0;
            for(int p=goal,i=n-1;i>=0;i--){out[i]=p;p=s->parent[p];}return n;}
        AwAStarEdge next[AW_ASTAR_EDGES];int count=edges(ctx,a,next);if(count<0||count>AW_ASTAR_EDGES)return 0;
        for(int i=0;i<count;i++){int b=next[i].node;float cost=next[i].cost;
            if(b<0||b>=nodes||!isfinite(cost)||cost<=0)continue;float value=s->cost[a]+cost;
            if(value>=s->cost[b])continue;s->cost[b]=value;s->parent[b]=a;s->priority[b]=value+estimate(ctx,b,goal);aw_astar_up(s,b);}
    }return 0;
}
#endif
