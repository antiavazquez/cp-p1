#include <pthread.h>

pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
int count = 0;

void inc_count() {
    pthread_mutex_lock(&m);
    count ++;
    pthread_mutex_unlock(&m);
}


int get_count() {
    int cnt;
    pthread_mutex_lock(&m);
    cnt = count;
    pthread_mutex_unlock(&m);

    return cnt;
}