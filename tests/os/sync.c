/* Stage 2: semaphores, a mutex under contention, message queues, and
   the console read a line at a time. */
#include "kernel.h"

static struct sem ready;
static struct mutex lock;
static struct mq queue;
static int shared;

static int producer(void *arg)
{
    int i, id = (int)arg;

    for (i = 0; i < 5; i++) {
        int msg = id * 100 + i;

        mq_send(&queue, &msg, 1);
        thread_yield();
    }
    return 0;
}

static int consumer(void *arg)
{
    int i, msg, sum = 0;

    (void)arg;
    for (i = 0; i < 10; i++) {
        mq_receive(&queue, &msg, 1);
        sum += msg;
    }
    kprintf("consumer: 10 messages, sum %d\n", sum);
    sem_post(&ready);
    return 0;
}

/* many increments under a mutex, with yields inside the critical
   section to invite trouble */
static int counter(void *arg)
{
    int i;

    (void)arg;
    for (i = 0; i < 50; i++) {
        int v;

        mutex_lock(&lock);
        v = shared;
        thread_yield();
        shared = v + 1;
        mutex_unlock(&lock);
    }
    sem_post(&ready);
    return 0;
}

static int echo(void *arg)
{
    char line[80];
    int n, lines = 0;

    (void)arg;
    while ((n = console_gets(line, sizeof line)) >= 0) {
        kprintf("line %d: [%s] %d\n", ++lines, line, n);
    }
    kprintf("echo: end of input after %d lines\n", lines);
    sem_post(&ready);
    return 0;
}

void init_main(void)
{
    int i;

    sem_init(&ready, 0);
    mutex_init(&lock);
    mq_init(&queue, sizeof(int), 3);
    thread_create("consumer", consumer, NULL, 4, STACK_DEFAULT);
    thread_create("producer1", producer, (void *)1, 4, STACK_DEFAULT);
    thread_create("producer2", producer, (void *)2, 4, STACK_DEFAULT);
    thread_create("count1", counter, NULL, 3, STACK_DEFAULT);
    thread_create("count2", counter, NULL, 3, STACK_DEFAULT);
    thread_create("count3", counter, NULL, 3, STACK_DEFAULT);
    thread_create("echo", echo, NULL, 5, STACK_DEFAULT);
    for (i = 0; i < 5; i++) {
        sem_wait(&ready);
    }
    kprintf("shared = %d, all done at tick %u\n", shared, ticks_now());
    kernel_halt(0);
}
