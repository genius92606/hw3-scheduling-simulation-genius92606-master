#ifndef SCHEDULING_SIMULATOR_H
#define SCHEDULING_SIMULATOR_H

#include <stdio.h>
#include <ucontext.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include "task.h"

enum TASK_STATE {
    TASK_RUNNING,
    TASK_READY,
    TASK_WAITING,
    TASK_TERMINATED
};

void hw_suspend(int msec_10);
void hw_wakeup_pid(int pid);
int hw_wakeup_taskname(char *task_name);
int hw_task_create(char *task_name);
void scheduler(void);
void waiting(void);
void terminator(void);
void signal_function(void);
static void timer_handler(int sig);
static void pause_handler(int sig);
void hw_suspend(int msec_10);
void hw_wakeup_pid(int pid);
int hw_wakeup_taskname(char *task_name);
int hw_task_create(char *task_name);
void task1(void);
void task2(void);
void task3(void);
void task4(void);
void task5(void);
void task6(void);
void add_task(char *task_name, int time_quantum,char prior);
void remove_task(int pid);
void start_simulation(void);
void process_status(void);
void free_all(void);

#endif
