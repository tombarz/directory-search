#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
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
    int  thread_ind;
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
pthread_mutex_t *lock;
int counter = 0;
pthread_t * threads;
pthread_mutex_t lock_queue_dir;
pthread_mutex_t lock_queue_t;
pthread_mutex_t sleep_lock;
pthread_mutex_t start_lock;
pthread_mutex_t main_lock;
pthread_mutex_t if_lock;
pthread_mutex_t queue_t_empty_lock;
pthread_cond_t * cond_var_arr;
pthread_cond_t  sig_start_cond;
pthread_cond_t main_cond;
pthread_cond_t queue_t_empty_cond;
int sig_start = 0;
int numOfThreads;
char *search_term;


//directory queue operation
int enqueue(queue* q, node* newNode){

    if(newNode == NULL){
        perror("newNode is null");
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
        perror("newNode is null");
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

t_node* dequeue_t(t_queue* q){
    if(q -> head == NULL){
        q -> size = 0;
        perror("the queue is empty");
        return NULL;
    }
    q -> size--;
    t_node* last = q -> head;
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
    if((dir_queue = (queue*)malloc(sizeof(queue))) == NULL){
        perror("there was a problem with the memory allocation");
        exit(1);
    }
    dir_queue -> size = 0;

    if((thread_queue = (t_queue*)malloc(sizeof(t_queue))) == NULL){
        perror("there was a problem with the memory allocation");
        exit(1);
    }

    if((threads_waiting = calloc(numOfThreads,sizeof (int))) == NULL){
        perror("there was a problem with the memory allocation");
        exit(1);
    }

    if((threads = calloc(numOfThreads, sizeof(pthread_t))) == NULL){
        perror("there was a problem with the memory allocation");
        exit(1);
    }
    if(pthread_mutex_init(&lock_queue_dir,NULL) != 0){
        perror("there was a problem with the lock initialization");
        exit(1);
    }
    if(pthread_mutex_init(&if_lock,NULL) != 0){
        perror("there was a problem with the lock initialization");
        exit(1);
    }

    if(pthread_mutex_init(&lock_queue_t,NULL) != 0){
        perror("there was a problem with the lock initialization");
        exit(1);
    }

    if(pthread_mutex_init(&queue_t_empty_lock,NULL) != 0){
        perror("there was a problem with the lock initialization");
        exit(1);
    }

    if(pthread_mutex_init(&sleep_lock,NULL) != 0){
        perror("there was a problem with the lock initialization");
        exit(1);
    }

    if(pthread_mutex_init(&start_lock,NULL) != 0){
        perror("there was a problem with the lock initialization");
        exit(1);
    }

    if(pthread_mutex_init(&main_lock,NULL) != 0){
        perror("there was a problem with the lock initialization");
        exit(1);
    }

    if((cond_var_arr = calloc(numOfThreads, sizeof(pthread_cond_t))) == NULL){
        perror("there was a problem with the memory allocation");
        exit(1);
    }

    if(pthread_cond_init(&sig_start_cond, NULL) != 0){
        perror("there was a problem with condition variable initialization");
        exit(1);
    }

    if(pthread_cond_init(&queue_t_empty_cond, NULL) != 0){
        perror("there was a problem with condition variable initialization");
        exit(1);
    }

    if(pthread_cond_init(&main_cond, NULL) != 0){
        perror("there was a problem with condition variable initialization");
        exit(1);
    }



}
void handle_regular_file(char * full_path,char *file_name){
    if(strstr(file_name, search_term) == NULL){
        return;
    }
    printf("%s %c",full_path,'\n');
    counter++;
}
void finish(){
    free(dir_queue);
    free(threads_waiting);
    free(threads);
    free(thread_queue);
    free(cond_var_arr);
}
int is_all_threads_waiting(){
    int is_waiting = threads_waiting[0];
    for (int i=1; i < numOfThreads; i++){
        is_waiting = (is_waiting & threads_waiting[i]);
    }
    if(is_waiting == 1){
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

    pthread_mutex_lock(&lock_queue_dir);
    node * new_node = malloc(sizeof(node));
    fillString(new_node -> directory, path);
    enqueue(dir_queue, new_node);
    pthread_cond_signal(&main_cond);
    pthread_mutex_unlock(&lock_queue_dir);
}
void *thread_func(void * index){

    int *ptr = (int *) index;
    int ind = *ptr;

    pthread_mutex_lock(&start_lock);
    pthread_cond_wait(&sig_start_cond,&start_lock);
    pthread_mutex_unlock(&start_lock);
    printf(" thread %d started\n",ind);

    while(1){

        pthread_mutex_lock(&lock_queue_dir);
        if(dir_queue -> size > 0){
            node * node1 = dequeue(dir_queue);
            char* path = calloc(PATH_MAX, sizeof(char));
            strcpy(path,node1 -> directory);
            free(node1);
            pthread_mutex_unlock(&lock_queue_dir);
            DIR *dir = opendir(path);
            if(dir == NULL){
                perror("there was a problem with opening the directory");
            }
            struct dirent *curr_dir;
            while((curr_dir = readdir(dir)) != NULL){
                struct stat status;
                char* full_path = calloc(PATH_MAX, sizeof(char));
                strcpy(full_path,path);
                strcpy(full_path,strcat(full_path  ,"/"));
                strcpy(full_path, strcat(full_path  ,(curr_dir -> d_name)));
                if(stat(full_path,&status) < 0){
                    perror("there was a problem with the file");
                    exit(-1);
                }
                if(S_ISREG(status.st_mode)){
                    handle_regular_file(full_path,curr_dir->d_name);
                }else if(S_ISDIR(status.st_mode)){
                    handle_dir(full_path,curr_dir -> d_name);
                }
                free(full_path);
        }
            free(path);
        }else{
            pthread_mutex_unlock(&lock_queue_dir);
            pthread_mutex_lock(&lock_queue_t);
            t_node *new_node = malloc(sizeof(t_node));
            new_node -> thread_ind = ind;
            enqueue_t(thread_queue, new_node);
            threads_waiting[ind]=1;
            pthread_cond_signal(&queue_t_empty_cond);
            pthread_mutex_unlock(&lock_queue_t);
            pthread_mutex_lock(&sleep_lock);
            pthread_cond_wait(&cond_var_arr[ind],&sleep_lock);
            pthread_mutex_unlock(&sleep_lock);
        }

    }

}

int main(int argc, char *argv[]){
    if(argc != 4){
        perror("wrong number of arguments");
        exit(1);
    }
    numOfThreads = atoi(argv[3]);
    search_term = argv[2];
    init();
    int * vars = calloc(numOfThreads,sizeof(int));
    node * dir_node = malloc(sizeof(node));
    fillString(dir_node -> directory,argv[1]);
    enqueue(dir_queue,dir_node);
    for (int i = 0; i < numOfThreads; i++){
        vars[i] = i;
        pthread_cond_init(&cond_var_arr[i],NULL);
        pthread_create(&threads[vars[i]],NULL,thread_func,&vars[i]);
    }

    pthread_cond_broadcast(&sig_start_cond);
    while((is_all_threads_waiting() != 0) || (dir_queue -> size) > 0){
        pthread_cond_broadcast(&sig_start_cond);
        if(dir_queue -> size != 0){
            if(thread_queue -> size > 0){
                pthread_mutex_lock(&lock_queue_t);
                t_node  *thread = dequeue_t(thread_queue);
                pthread_cond_signal(&cond_var_arr[ thread -> thread_ind]);
                threads_waiting[thread -> thread_ind] = 0;
                free(thread);
                pthread_mutex_unlock(&lock_queue_t);
            }else{
                pthread_mutex_lock(&queue_t_empty_lock);
                pthread_cond_wait(&queue_t_empty_cond, &queue_t_empty_lock);
                pthread_mutex_unlock(&queue_t_empty_lock);
            }

        } else{
            pthread_mutex_lock(&main_lock);
            if (is_all_threads_waiting() != 0){
                pthread_cond_wait(&queue_t_empty_cond,&main_lock);
            }
            pthread_mutex_unlock(&main_lock);
        }

    }

    printf("Done searching, found %d files\n",counter);

    free(vars);
    finish();
    return 0;
}
// Created by student on 5/17/22.
//
