#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "op_count.h"
#include "options.h"

struct buffer {
    int *data;
    int size;
};

struct thread_info {
    pthread_t       thread_id;        // id returned by pthread_create()
    int             thread_num;       // application defined thread #
};


struct args {
    int				thread_num;       // application defined thread #
    int				delay;			  // delay between operations
    int				iterations;
    struct buffer	*buffer;		  // Shared buffer
    pthread_mutex_t *mutex;
};

struct print_args {
    struct buffer	*buffer;
    int             print_wait;
    int             *print_check;
};

void *swap(void *ptr)
{
    struct args *args =  ptr;

    while(args->iterations--) {
        int i,j, tmp;
        i=rand() % args->buffer->size;
        j=rand() % args->buffer->size;

        if (i==j){
            inc_count();
            continue;
        }

        while (1) {

            pthread_mutex_lock(&args->mutex[i]);
            if (pthread_mutex_trylock(&args->mutex[j])) { //If locking m2 fails, free m1
                pthread_mutex_unlock(&args->mutex[i]);
                continue;
            }

            printf("Thread %d swapping positions %d (== %d) and %d (== %d)\n",
                   args->thread_num, i, args->buffer->data[i], j, args->buffer->data[j]);

            tmp = args->buffer->data[i];
            if (args->delay) usleep(args->delay); // Force a context switch

            args->buffer->data[i] = args->buffer->data[j];
            if (args->delay) usleep(args->delay);

            args->buffer->data[j] = tmp;
            if (args->delay) usleep(args->delay);
            inc_count();

            pthread_mutex_unlock(&args->mutex[i]);
            pthread_mutex_unlock(&args->mutex[j]);
            break;
        }
    }
    return NULL;
}

int cmp(int *e1, int *e2) {
    if(*e1==*e2) return 0;
    if(*e1<*e2) return -1;
    return 1;
}

void print_buffer(struct buffer buffer) {
    int i;

    for (i = 0; i < buffer.size; i++)
        printf("%i ", buffer.data[i]);
    printf("\n");
}

void* print_periodically(void* ptr){
    struct print_args *args = ptr;
    while (*args->print_check){
        print_buffer(*args->buffer);
        usleep(args->print_wait);
    }
    return NULL;
}

void start_threads(struct options opt)
{
    int i;
    struct thread_info *threads;
    struct args *args;
    struct buffer buffer;
    pthread_mutex_t *mutex;

    struct thread_info *print_thread;
    struct print_args *print_args;

    srand(time(NULL));

    if((buffer.data=malloc(opt.buffer_size*sizeof(int)))==NULL) {
        printf("Out of memory\n");
        exit(1);
    }
    buffer.size = opt.buffer_size;

    mutex= malloc(sizeof(pthread_mutex_t)* opt.buffer_size);

    for(i=0; i<buffer.size; i++){
        pthread_mutex_init(&mutex[i],NULL);
        buffer.data[i]=i;
    }

    printf("creating %d threads\n", opt.num_threads);
    threads = malloc(sizeof(struct thread_info) * opt.num_threads);
    args = malloc(sizeof(struct args) * opt.num_threads);
    print_thread = malloc(sizeof(struct thread_info));
    print_args = malloc(sizeof(struct print_args));

    if (threads == NULL || args==NULL || print_thread==NULL || print_args==NULL) {
        printf("Not enough memory\n");
        exit(1);
    }

    printf("Buffer before: ");
    print_buffer(buffer);

    print_thread->thread_num= 0;
    print_args->buffer      = &buffer;
    print_args->print_wait  = opt.print_wait;
    print_args->print_check = malloc(sizeof(int));
    if (print_args->print_check == NULL) {
        printf("Memory allocation failed for print_check\n");
        exit(1);
    }
    *print_args->print_check = 1;

    if ( 0 != pthread_create(&print_thread->thread_id, NULL,
                             print_periodically, print_args)) {
        printf("Could not create thread #%d", i);
        exit(1);
    }

    // Create num_thread threads running swap()
    for (i = 0; i < opt.num_threads; i++) {
        threads[i].thread_num = i;

        args[i].thread_num = i;
        args[i].buffer     = &buffer;
        args[i].delay      = opt.delay;
        args[i].iterations = opt.iterations;
        args[i].mutex      = mutex;

        if ( 0 != pthread_create(&threads[i].thread_id, NULL,
                     swap, &args[i])) {
            printf("Could not create thread #%d", i);
            exit(1);
        }
    }

    // Wait for the threads to finish
    for (i = 0; i < opt.num_threads; i++){
        pthread_join(threads[i].thread_id, NULL);
    }

    *print_args->print_check=0;

    pthread_join(print_thread->thread_id, NULL);

    // Print the buffer
    printf("Buffer after:  ");
    qsort(buffer.data, opt.buffer_size, sizeof(int), (int (*)(const void *, const void *)) cmp);
    print_buffer(buffer);

    printf("iterations: %d\n", get_count());

    for (i = 0; i < opt.buffer_size; i++) {
        pthread_mutex_destroy(&mutex[i]);
    }

    free(mutex);
    free(print_args->print_check);
    free(print_thread);
    free(print_args);
    free(args);
    free(threads);
    free(buffer.data);

    pthread_exit(NULL);
}

int main (int argc, char **argv)
{
    struct options opt;

    // Default values for the options
    opt.num_threads = 10;
    opt.buffer_size = 10;
    opt.iterations  = 100;
    opt.delay       = 10;
    opt.print_wait  = 10;

    read_options(argc, argv, &opt);

    start_threads(opt);

    exit (0);
}
