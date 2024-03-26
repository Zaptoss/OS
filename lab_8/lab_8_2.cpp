#include <iostream>
#include <string>

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>

#include <queue>

#include <semaphore.h>
#include <pthread.h>

#include "sha256.h"

using namespace std;


pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock_stdout = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;
SHA256 sha256;
queue<string> items_queue;
int buffer_size;

void unlock( void * arg ) {
   pthread_mutex_unlock( (pthread_mutex_t*)arg );
}

// function to get now timestamp
string timestamp() {
    timeval tv;
    time_t nowtime;
    tm *nowtm;
    char tmbuf[20], buf[40];

    gettimeofday(&tv, NULL);
    nowtime = tv.tv_sec;
    nowtm = localtime(&nowtime);
    strftime(tmbuf, sizeof tmbuf, "%T", nowtm);
    snprintf(buf, sizeof buf, "%s.%06ld", tmbuf, tv.tv_usec);

    return (string)buf;
}

string produce() {
    string rand_str;

    // generating random string with 10 chars in lowercase
    for (int i = 0; i < 10; i++) {
        rand_str += (rand() % 26) + 97;
    }

    // returning sha256 sum of random string
    return "P " + to_string(gettid()) + ": " + sha256(rand_str);
}

void *producer(void *arg) {
    string str_hash;

    while (1) {
        usleep((rand()%1000)*1000);

        str_hash = produce();

        pthread_mutex_lock(&lock);    // critical section entry
        pthread_cleanup_push(unlock, (void*)&lock);

        while (items_queue.size() == buffer_size) { pthread_cond_wait(&not_full, &lock); }
        items_queue.push(str_hash);
        pthread_cond_signal(&not_empty);

        pthread_cleanup_pop(1);    //critical section exit
    }

    pthread_exit(0);
}

void consume(string str_hash) {
    cout << "[" + timestamp() + "] C " << gettid() << ": " + str_hash << endl;
}

void *consumer(void *arg) {
    string str_hash;

    while (1) {
        usleep((rand()%1000)*1000);

        pthread_mutex_lock(&lock);    // critical section entry
        pthread_cleanup_push(unlock, (void*)&lock);

        while (items_queue.size() == 0) { pthread_cond_wait(&not_empty, &lock); }
        str_hash = items_queue.front();
        items_queue.pop();
        pthread_cond_signal(&not_full);
        
        pthread_cleanup_pop(1);    //critical section exit


        pthread_mutex_lock(&lock_stdout); // critical section entry
        pthread_cleanup_push(unlock, (void*)&lock_stdout);
        consume(str_hash);
        pthread_cleanup_pop(1); //critical section exit
    }

    pthread_exit(0);
}

int main(int argc, char *argv[]) {
    // argument parsing
    if (argc != 5) {
        cout << "Usage: " << argv[0] << " <buffer_size> <producer_count> <consumer_count> <work_time>" << endl;
        exit(1);
    }

    buffer_size = stoi(argv[1]);
    if (buffer_size < 1) {
        cout << "buffer_size must be greater then zero" << endl;
        exit(1);
    }

    int producers_count = stoi(argv[2]);
    if (producers_count < 1) {
        cout << "producer_count must be greater then zero" << endl;
        exit(1);
    }

    int consumers_count = stoi(argv[3]);
    if (consumers_count < 1) {
        cout << "consumer_count must be greater then zero" << endl;
        exit(1);
    }

    int work_time = stoi(argv[4]);
    if (work_time < 1) {
        cout << "work_time must be greater then zero" << endl;
        exit(1);
    }

    // init
    srand(time(0));

    pthread_t producersTID[producers_count];
    pthread_t consumersTID[consumers_count];
    
    // start producer and consumer
    for (int i = 0; i < producers_count; i++) { pthread_create(&producersTID[i], NULL, producer, NULL); }
    for (int i = 0; i < consumers_count; i++) { pthread_create(&consumersTID[i], NULL, consumer, NULL); }

    // sleep
    sleep(work_time);

    // stop producer and consumer
    for (int i = 0; i < producers_count; i++) { pthread_cancel(producersTID[i]); }
    for (int i = 0; i < consumers_count; i++) { pthread_cancel(consumersTID[i]); }
    for (int i = 0; i < producers_count; i++) { pthread_join(producersTID[i], NULL); }
    for (int i = 0; i < consumers_count; i++) { pthread_join(consumersTID[i], NULL); }

    // queue clear
    while ( !items_queue.empty() ) { items_queue.pop(); }

    pthread_exit(0);   
}
