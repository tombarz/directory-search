#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

#include <error.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/stat.h>
#include <string.h>

typedef struct node{
    char [PATH_MAX] directory;
    struct node *next;
} node;
typedef struct queue{
    int size;
    node* head;
    node* tail;
}queue;

queue* queue1;
int * threads_waiting;
pthread_t * threads;
pthread_mutex_t lock_dequeue;
pthread_mutex_t lock_enqueue;
int numOfThreads;


int enqueue(queue* q, node* newNode){

    if(newNode == NULL){
        perror('newNode is null');
        return -1;
    }
    q -> size++;
    if(q -> head != NULL) {
        q -> tail -> next = newNode;
    }else{
        q->head = newNode;
    }
    q -> tail = newNode;
    return 0;
}
node* dequeue(queue* q){
    if(q -> head == NULL){
        q->size = 0;
        perror("the queue is empty");
        return NULL;
    }
    q -> size--;
    node* last = q -> head;
    if(q -> size > 0){
        q -> head = last -> next;
    }else{
        q -> head = NULL;
        q -> tail = NULL;
    }
    return last;
}
void fillString(char *dest, char* string){
    int i = 0;
    while(string[i]!='\0'){
        dest[i]=string[i];
        i++;
    }
}
void init(){
    queue1 = (queue*)malloc(sizeof(queue));
    threads_waiting = calloc(numOfThreads,sizeof (int));
    threads = calloc(numOfThreads, sizeof(pthread_t));
}
void handle_regular_file(char * full_path){
    printf(strcat(full_path,"\n"));
    if(queue1 -> size==0){
        return;
    }
}
void finish(){
    free(queue1);
    free(threads_waiting);
    free(threads);
}
int is_all_threads_waiting(){
    int is_waiting = threads[0];
    for (int i=1; i < numOfThreads; i++){
        is_waiting = (is_waiting & threads[i]);
    }
    if(is_waiting==1){
        return 0;
    }
    return -1;
}
void handle_dir(char * path,  char *dir_name){
    if((strcmp(dir_name,".") == 0) || (strcmp(dir_name,"..") == 0)){
        return;
    }
    
    if(opendir(path)==NULL){
        printf("Directory %s: Permission denied.\n",path);
        return;
    }

    pthread_mutex_lock(&lock_enqueue);
    node * new_node = malloc(sizeof(node));//Todo free the nodes
    fillString(new_node -> directory, path);
    enqueue(queue1,new_node);
}
void thread_func(int * index){
    int ind = *index;

    while( queue1-> size!=0 || (is_all_threads_waiting() < 0)){//if the queue is not empty and there is at least one thread that is not waiting
        if(queue1 -> size == 0){
            threads_waiting[ind] = 1;//TODO make sure no busy waiting and make a thread queue
            continue;
        }
        threads_waiting[ind] = 0;
        pthread_mutex_lock(&lock_dequeue);
        node * node1 = dequeue(queue1);
        pthread_mutex_unlock(&lock_dequeue);
        char* path = node1 -> directory;
        DIR *dir = opendir(path);
        if(dir == NULL){
            perror('there was a problem with opening the directory');
        }
        struct dirent *curr_dir;
        while(curr_dir = readdir(dir) != NULL){
            struct stat status;
            char* full_path = strcat(path  ,(curr_dir -> d_name));
            if(stat(full_path,&status) < 0){
                perror('there was a problem');
                exit(-1);
            }
            if(status.st_mode & S_IFREG){
                handle_regular_file(full_path);
            }else if(status.st_mode & S_IFDIR){
                handle_dir(full_path,curr_dir -> d_name);
            }
        }


    }



}
int main(int argc, char *argv[]){
    numOfThreads = argv[3];
    init();
    for (int i = 0; i < argv[3]; i++){
        pthread_create(&threads[i],NULL,thread_func,&i);
    }
    for (int i = 0; i < argv[3]; i++){
        pthread_join(threads[i],NULL);
    }
    finish();
}//
// Created by student on 5/17/22.
//
