#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

#include <error.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/stat.h>
#include <string.h>
//structures
typedef struct node{
    char  directory[PATH_MAX];
    struct node *next;
} node;
typedef struct queue{
    int size;
    node* head;
    node* tail;
}queue;
typedef struct thread_node{
    pthread_t * thread;
    struct thread_node *next;
}t_node;
typedef struct thread_queue{
    int size;
    t_node* head;
    t_node* tail;
} t_queue;

queue* dir_queue;
t_queue* thread_queue;
int * threads_waiting;
pthread_t * threads;
pthread_mutex_t lock_dequeue_dir;
pthread_mutex_t lock_enqueue_dir;
pthread_mutex_t lock_enqueue_t;
pthread_mutex_t lock_dequeue_t;
int numOfThreads;
char *serch_term;

//directory queue operation
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
//thread queue operation
int enqueue_t(t_queue* q, t_node* newNode){
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

t_node* dequeue_t(queue* q){
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
//helper function
void fillString(char *dest, char* string){
    int i = 0;
    while(string[i]!='\0'){
        dest[i]=string[i];
        i++;
    }
}
void init(){
    if(dir_queue = (queue*)malloc(sizeof(queue)) < 0){
        perror("there was a problem with the memory allocation");
    }
    
    if(thread_queue = (t_queue*)malloc(sizeof(t_queue)) < 0){
        perror("there was a problem with the memory allocation");
    }
    
    if(threads_waiting = calloc(numOfThreads,sizeof (int)) < 0){
        perror("there was a problem with the memory allocation");
    }
    
    if(threads = calloc(numOfThreads, sizeof(pthread_t)) < 0){
        perror("there was a problem with the memory allocation");
    }
}
void handle_regular_file(char * full_path,char *file_name){
    if(strstr(file_name,serch_term) == NULL){
        return;
    }
    printf(strcat(full_path,"\n"));
    if(dir_queue -> size == 0){
        return;
    }
}
void finish(){
    free(dir_queue);
    free(threads_waiting);
    free(threads);
    free(thread_queue);
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

    pthread_mutex_lock(&lock_enqueue_dir);
    node * new_node = malloc(sizeof(node));//Todo free the nodes
    fillString(new_node -> directory, path);
    enqueue(dir_queue, new_node);
}
void thread_func(int * index){
    int ind = *index;

    while(dir_queue-> size != 0 || (is_all_threads_waiting() < 0)){//if the queue is not empty and there is at least one thread that is not waiting
        if(dir_queue -> size == 0){
            if(threads_waiting[ind] == 0){
                pthread_mutex_lock(&lock_enqueue_t);
                
            }
            threads_waiting[ind] = 1;
            continue;
        }
        threads_waiting[ind] = 0;
        pthread_mutex_lock(&lock_dequeue_dir);
        node * node1 = dequeue(dir_queue);
        pthread_mutex_unlock(&lock_dequeue_dir);
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
                handle_regular_file(full_path,curr_dir->d_name);
            }else if(status.st_mode & S_IFDIR){
                handle_dir(full_path,curr_dir -> d_name);
            }
        }


    }



}
int main(int argc, char *argv[]){
    numOfThreads = atoi(argv[3]);
    serch_term = argv[2];
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
