#include <iostream>
#include <string>

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>

#include <semaphore.h>
#include <pthread.h>

using namespace std;

pthread_mutex_t lock_stdout = PTHREAD_MUTEX_INITIALIZER;    // mutex for restrict stdout
pthread_mutex_t *forks; // mutexes for forks

int N;  // philosopher count
int eat_time_range[2];  // random eat time range
int think_time_range[2];    // random think time range
int busy_waiting_time_range[2]; // random wait time range
int time_before_faint;  // wait time for faint
int eating_time;    // time for eat portion
int *portion_left;  // left time for portion

sem_t thread_queue; // semaphore for correct initialize philosophers
pthread_t *tids;    // philosophers thread id

// function for get now timestamp
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

// function for unlock mutex
void unlock( void * arg ) {
   pthread_mutex_unlock( (pthread_mutex_t*)arg );
}

// function for restrict stdout from writing by more then 1 thread
void mutex_stdout(string str) {
    pthread_mutex_lock(&lock_stdout);
    pthread_cleanup_push(unlock, &lock_stdout);

    cout << str;

    pthread_cleanup_pop(1);
}

// return forks state, where O - free, X - busy;
string forks_state() {
    string str = "";

    for (int i = 0; i < N; i++) {
        if (pthread_mutex_trylock(&forks[i]) == 0) {
            pthread_cleanup_push(unlock, &forks[i]);
            str += "O";
            pthread_cleanup_pop(1);
        } else {
            str += "X";
        }
    }

    return str;
}

// get rand number from range
int rand_range(int range[2]) {
    return ((range[1]-range[0] != 0) ? rand()%(range[1]-range[0]) : 0) + range[0];
}

// eat with busy waiting
void eat(int i) {
    bool eated = false;
    int eating_time = 0;
    int elapsed_time = 0;
    int busy_wait = 0;
    while (!eated) {
        if (pthread_mutex_trylock(&forks[(i+N-1)%N]) == 0) { 
            pthread_cleanup_push(unlock, &forks[(i+N-1)%N]);

            mutex_stdout("[" + timestamp() + "] Forks state: " + forks_state() + " | Philosopher " + to_string(i) + " take fork " + to_string((i+N-1)%N) + " (left)\n");

            if (pthread_mutex_trylock(&forks[i]) == 0) {
                pthread_cleanup_push(unlock, &forks[i]);
                mutex_stdout("[" + timestamp() + "] Forks state: " + forks_state() + " | Philosopher " + to_string(i) + " take fork " + to_string(i) + " (right)\n");
                mutex_stdout("[" + timestamp() + "] Forks state: " + forks_state() + " | Philosopher " + to_string(i) + " start eat\n");

                eating_time = rand_range(eat_time_range)*10000;
                usleep(eating_time);
                eated = true;

                mutex_stdout("[" + timestamp() + "] Forks state: " + forks_state() + " | Philosopher " + to_string(i) + " end eat\n");
                mutex_stdout("[" + timestamp() + "] Forks state: " + forks_state() + " | Philosopher " + to_string(i) + " put fork " + to_string(i) + " (right)\n");
                pthread_cleanup_pop(1);
            } else {
                mutex_stdout("[" + timestamp() + "] Forks state: " + forks_state() + " | Philosopher " + to_string(i) + " don't take fork " + to_string(i) + " (right)\n");
            }

            mutex_stdout("[" + timestamp() + "] Forks state: " + forks_state() + " | Philosopher " + to_string(i) + " put fork " + to_string((i+N-1)%N) + " (left)\n");
            pthread_cleanup_pop(1);
        } else {
            mutex_stdout("[" + timestamp() + "] Forks state: " + forks_state() + " | Philosopher " + to_string(i) + " don't take fork " + to_string((i+N-1)%N) + " (left)\n");
        }

        if (!eated) {
            busy_wait = rand_range(busy_waiting_time_range)*10000;
            elapsed_time += busy_wait;
            if (elapsed_time > time_before_faint*1000000) {
                mutex_stdout("[" + timestamp() + "] Forks state: " + forks_state() + " | Philosopher " + to_string(i) + " faint\n");
                pthread_exit(0);
            }
            usleep(busy_wait);
        } else {
            portion_left[i] -= eating_time;
            if (portion_left[i] <= 0) {
                mutex_stdout("[" + timestamp() + "] Forks state: " + forks_state() + " | Philosopher " + to_string(i) + " ate the whole portion\n");
                pthread_exit(0);
            }
        }
    }
}

// think...
void think(int i) {
    mutex_stdout("[" + timestamp() + "] Forks state: " + forks_state() + " | Philosopher " + to_string(i) + " start think\n");
    usleep(rand_range(think_time_range)*10000);
    mutex_stdout("[" + timestamp() + "] Forks state: " + forks_state() + " | Philosopher " + to_string(i) + " end think\n");
}

void *philosopher(void *arg) {
    // init philosopher number
    int i = *(int*)arg;

    // helper
    sem_post(&thread_queue);

    while (1) {
        think(i);
        eat(i);
    }

    pthread_exit(0);
}

int main(int argc, char *argv[]) {

    // argument parsing
    if (argc != 10) {
        cout << "Usage: " << argv[0] << " <philosopher_count> <eat_time_range_min> <eat_time_range_max>\n<think_time_range_min> <think_time_range_max>\n<busy_waiting_time_range_min> <busy_waiting_time_range_max>\n<time_before_faint> <eating_time>\n\nTime ranges: 1*10000us\nTime: 1*1s" << endl;
        exit(1);
    }

    N = stoi(argv[1]);
    if (N < 2) {
        cout << "philosopher count must be greater then one" << endl;
        exit(1);
    }

    eat_time_range[0] = stoi(argv[2]);
    if (eat_time_range[0] < 1) {
        cout << "eat_time_range_min must be greater then zero" << endl;
        exit(1);
    }

    eat_time_range[1] = stoi(argv[3]);
    if (eat_time_range[1] < 1 && eat_time_range[0] <= eat_time_range[1]) {
        cout << "eat_time_range_max must be greater then zero and eat_time_range_min" << endl;
        exit(1);
    }

    think_time_range[0] = stoi(argv[4]);
    if (think_time_range[0] < 1) {
        cout << "think_time_range_min must be greater then zero" << endl;
        exit(1);
    }

    think_time_range[1] = stoi(argv[5]);
    if (think_time_range[1] < 1 && think_time_range[0] <= think_time_range[1]) {
        cout << "think_time_range_max must be greater then zero and think_time_range_min" << endl;
        exit(1);
    }

    busy_waiting_time_range[0] = stoi(argv[6]);
    if (busy_waiting_time_range[0] < 1) {
        cout << "busy_waiting_time_range_min must be greater then zero" << endl;
        exit(1);
    }

    busy_waiting_time_range[1] = stoi(argv[7]);
    if (busy_waiting_time_range[1] < 1 && busy_waiting_time_range[0] <= busy_waiting_time_range[1]) {
        cout << "busy_waiting_time_range_max must be greater then zero and busy_waiting_time_range_min" << endl;
        exit(1);
    }
    
    time_before_faint = stoi(argv[8]);
    if (time_before_faint < 1) {
        cout << "time_before_faint must be greater then zero" << endl;
        exit(1);
    }

    eating_time = stoi(argv[9]);
    if (eating_time < 1) {
        cout << "eating_time must be greater then zero" << endl;
        exit(1);
    }

    // init
    srand(time(0)); // generator seed
    sem_init(&thread_queue, 0, 0);
    forks = new pthread_mutex_t[N];
    tids = new pthread_t[N];
    portion_left = new int[N];
    for (int i = 0; i < N; i++) { forks[i] = PTHREAD_MUTEX_INITIALIZER; };  // mutex initialize
    for (int i = 0; i < N; i++) { portion_left[i] = eating_time*1000000; }; // protion_left array initialize

    // start
    for (int i = 0; i < N; i++) { pthread_create(&tids[i], NULL, philosopher, (void*)&i); sem_wait(&thread_queue); }
    for (int i = 0; i < N; i++) { pthread_join(tids[i], NULL); }

    return 0;
}
