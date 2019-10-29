#include "scheduling_simulator.h"

#define _XOPEN_SOURCE_EXTENDED 1

#ifdef _LP64
#define STACK_SIZE 2097152+16384	/* Large enough value for AMODE 64 */
#else
#define STACK_SIZE 16384			/* AMODE 31 addressing */
#endif

/* Task queue data structure */
struct Data {
    int pid;
    char task_name[10];
    ucontext_t context;
    enum TASK_STATE task_state;
    int time_quantum;
    int queueing_time;
    int waiting_time;
    char prior;
};

struct Node {
    struct Data data;
    struct Node *next;
};

static ucontext_t mcontext;				/* Main function context */
static ucontext_t signal_context;		/* Signal function context */
static ucontext_t scheduler_context;	/* Scheduler function context */
static ucontext_t terminator_context;	/* Terminator function context */
static ucontext_t newcontext;			/* New context for new task */
static void *signal_stack;				/* Stack pointer for signal function */
static void *scheduler_stack;			/* Stack pointer for scheduler function*/
static void *terminator_stack;			/* Stack pointer for scheduler function*/
static struct itimerval t;				/* Timer interval */

static struct sigaction p_act;
static struct sigaction t_act;

static struct Node* head = NULL;		/* Node pointer for head node */
static struct Node *current_node;		/* Node pointer for current node */
struct Node *newNode;
static int pid_counter = 1;
static int wait_exist = 0;



int main()
{
    /* Activate the pause handler */
    p_act.sa_handler = &pause_handler;
    p_act.sa_flags = SA_RESTART | SA_SIGINFO;
    sigfillset(&p_act.sa_mask);  // Block every signal during the handler
    if (sigaction(SIGTSTP, &p_act, NULL) == -1) { // Intercept SIGTSTP
        perror("Error: cannot handle SIGTSTP");
        exit(1);
    }

    /* Allocate the global signal function stack */
    signal_stack = malloc(STACK_SIZE);
    if (signal_stack == NULL) {
        perror("malloc");
        exit(1);
    }

    /* Allocate the global scheduler function stack */
    scheduler_stack = malloc(STACK_SIZE);
    if (scheduler_stack == NULL) {
        perror("malloc");
        exit(1);
    }

    /* Allocate the global terminator function stack */
    terminator_stack = malloc(STACK_SIZE);
    if (terminator_stack == NULL) {
        perror("malloc");
        exit(1);
    }

    while (1) {
        /* Get the main function context */
        getcontext(&mcontext);

        /* Make the scheduler function context for the fist time */
        getcontext(&scheduler_context);
        scheduler_context.uc_stack.ss_sp = scheduler_stack;
        scheduler_context.uc_stack.ss_size = STACK_SIZE;
        scheduler_context.uc_stack.ss_flags = 0;
        scheduler_context.uc_link = &mcontext;
        makecontext(&scheduler_context, scheduler, 0);

        /* Make the terminator function context for the fist time */
        getcontext(&terminator_context);
        terminator_context.uc_stack.ss_sp = terminator_stack;
        terminator_context.uc_stack.ss_size = STACK_SIZE;
        terminator_context.uc_stack.ss_flags = 0;
        terminator_context.uc_link = &scheduler_context;
        makecontext(&terminator_context, terminator, 0);


        printf("$ ");
        char buf[512];
        fgets(buf,512,stdin);
        if(strcmp(buf,"\n")==0)
            ;
        char command[100], TASK_NAME[100],t[100],TIME_QUANTUM[100],p[100],PRIOR[100];
        int pid;
        int quantum;
        sscanf(buf,"%s",command);
        if(strcmp(command,"add")==0) {
            sscanf(buf,"%s %s %s %s %s %s",command, TASK_NAME, t, TIME_QUANTUM, p, PRIOR);
            if(TASK_NAME!=NULL) {
                if(strcmp(TIME_QUANTUM,"L")==0) {
                    quantum=20;
                } else if (strcmp(TIME_QUANTUM,"S")==0) {
                    quantum=10;
                } else {
                    quantum=10;
                }
                if(strcmp(PRIOR,"H")==0) {
                    add_task(TASK_NAME,quantum,'H');
                } else if(strcmp(PRIOR,"L")==0) {
                    add_task(TASK_NAME,quantum,'L');
                } else {
                    add_task(TASK_NAME,quantum,'L');
                }
            } else {
                printf("the task name should be entered!\n");
            }

        } else if(strcmp(command,"remove")==0) {
            sscanf(buf, "%s %d",command,&pid);
            remove_task(pid);

        } else if(strcmp(command,"start")==0) {
            printf("simulating:...\n");
            swapcontext(&mcontext, &scheduler_context);
        } else if(strcmp(command,"ps")==0) {
            process_status();
        } else printf("Command is unvailable\n");
    }
    free_all();
    free(signal_stack);
    free(scheduler_stack);
    free(terminator_stack);
    return 0;
}
/* The RR scheduling algorithm; selects the next ready task to run and swaps to it's context to start it; if the task terminates, it will swap back and the scheduler will reschedule */
void scheduler(void)
{
    t.it_interval.tv_sec = 0;
    t.it_interval.tv_usec = 0;
    t.it_value = t.it_interval;
    if (setitimer(ITIMER_REAL, &t, NULL) < 0) {
        printf("settimer error.\n");
        exit(1);
    }
    t_act.sa_handler = &timer_handler;
    t_act.sa_flags = SA_RESTART | SA_SIGINFO;
    sigfillset(&t_act.sa_mask);
    if (sigaction(SIGALRM, &t_act, NULL) == -1) { // Intercept SIGALRM
        perror("Error: cannot handle SIGALRM");
        exit(1);
    }

    if(current_node==NULL) { // No task
        printf("No task in the queue.\n");
        return;
    }
    int wait = 0, terminate = 1;
    struct Node *original_node = current_node;
    while (current_node->data.task_state != TASK_READY
            &&current_node->data.task_state !=TASK_RUNNING) {
        if(current_node->data.task_state != TASK_TERMINATED) {
            terminate = 0;
        }
        if(current_node->next==NULL) {
            current_node = head;
        } else {
            current_node = current_node->next;
        }
        if(original_node==current_node) {
            if(terminate) {
                t.it_interval.tv_sec = 0;
                t.it_interval.tv_usec = 0;
                t.it_value = t.it_interval;
                if (setitimer(ITIMER_REAL, &t, NULL) < 0) {
                    printf("settimer error.\n");
                    exit(1);
                }
                t_act.sa_handler = &timer_handler;
                t_act.sa_flags = SA_RESTART | SA_SIGINFO;
                sigfillset(&t_act.sa_mask);
                printf("All tasks were terminated.\n");
                return;
            } else if(current_node->data.task_state == TASK_WAITING) {
                wait = 1;
                break;
            }
        }
    }

    t.it_interval.tv_sec = 0;
    t.it_interval.tv_usec = current_node->data.time_quantum * 1000;
    t.it_value = t.it_interval;
    if (setitimer(ITIMER_REAL, &t, NULL) < 0) {
        printf("settimer error.\n");
        exit(1);
    }
    t_act.sa_handler = &timer_handler;
    t_act.sa_flags = SA_RESTART | SA_SIGINFO;
    sigfillset(&t_act.sa_mask);
    if (sigaction(SIGALRM, &t_act, NULL) == -1) { // Intercept SIGALRM
        perror("Error: cannot handle SIGALRM");
        exit(1);
    }
    if (wait) {
        wait_exist = 1;
        add_task("waiting", 10,'L');
        wait = 0;
        if(current_node->next==NULL) {
            current_node = head;
        } else {
            current_node = current_node->next;
        }
    }
    //printf("Schedule in task's PID\t:\t%d\n", current_node->data.pid);
    current_node->data.task_state = TASK_RUNNING;
    swapcontext(&scheduler_context, &current_node->data.context);
}
void waiting(void)
{
    while(1) {
        ;
    }
}
void terminator(void)
{
    t.it_interval.tv_sec = 0;
    t.it_interval.tv_usec = 0;
    t.it_value = t.it_interval;
    if (setitimer(ITIMER_REAL, &t, NULL) < 0) {
        printf("settimer error.\n");
        exit(1);
    }
    t_act.sa_handler = &timer_handler;
    t_act.sa_flags = SA_RESTART | SA_SIGINFO;
    sigfillset(&t_act.sa_mask);
    if (sigaction(SIGALRM, &t_act, NULL) == -1) { // Intercept SIGALRM
        perror("Error: cannot handle SIGALRM");
        exit(1);
    }
    if(wait_exist) {
        remove_task(0);
        wait_exist = 0;
    }
    struct Node *current = head;
    while (current!=NULL) {
        if(current!=current_node&&current->data.task_state == TASK_READY) {
            current->data.queueing_time += current_node->data.time_quantum;
        }
        if(current->data.task_state == TASK_WAITING
                && current->data.waiting_time > 0) {
            current->data.waiting_time -= current_node->data.time_quantum;
        }
        if(current->data.task_state == TASK_WAITING
                && current->data.waiting_time == 0) {
            current->data.task_state = TASK_READY;
        }
        current = current->next;
    }
    //printf("Terminated task's PID\t:\t%d\n", current_node->data.pid);
    current_node->data.task_state=TASK_TERMINATED;
    getcontext(&scheduler_context);
    scheduler_context.uc_stack.ss_sp = scheduler_stack;
    scheduler_context.uc_stack.ss_size = STACK_SIZE;
    scheduler_context.uc_stack.ss_flags = 0;
    scheduler_context.uc_link = &mcontext;
    makecontext(&scheduler_context, scheduler, 0);
    setcontext(&scheduler_context);
}

/* The signal function; updates the current node, and
  makes and sets to new scheduler context to run the scheduler in */
void signal_function(void)
{
    t.it_interval.tv_sec = 0;
    t.it_interval.tv_usec = 0;
    t.it_value = t.it_interval;
    if (setitimer(ITIMER_REAL, &t, NULL) < 0) {
        printf("settimer error.\n");
        exit(1);
    }
    t_act.sa_handler = &timer_handler;
    t_act.sa_flags = SA_RESTART | SA_SIGINFO;
    sigfillset(&t_act.sa_mask);
    if (sigaction(SIGALRM, &t_act, NULL) == -1) { // Intercept SIGALRM
        perror("Error: cannot handle SIGALRM");
        exit(1);
    }

    struct Node *current = head;
    while (current!=NULL) {
        if(current!=current_node&&current->data.task_state == TASK_READY) {
            current->data.queueing_time += current_node->data.time_quantum;
        }
        if(current->data.task_state == TASK_WAITING
                && current->data.waiting_time > 0) {
            current->data.waiting_time -= current_node->data.time_quantum;
        }
        if(current->data.task_state == TASK_WAITING
                && current->data.waiting_time == 0) {
            current->data.task_state = TASK_READY;
        }
        current = current->next;
    }
    if(current_node->data.task_state == TASK_RUNNING) {
        //printf("Schedule out task's PID\t:\t%d\n", current_node->data.pid);
        current_node->data.task_state = TASK_READY;
    }
    if(current_node->next==NULL) {
        current_node = head;
    } else {
        current_node = current_node->next;
    }
    getcontext(&scheduler_context);
    scheduler_context.uc_stack.ss_sp = scheduler_stack;
    scheduler_context.uc_stack.ss_size = STACK_SIZE;
    scheduler_context.uc_stack.ss_flags = 0;
    scheduler_context.uc_link = &mcontext;
    makecontext(&scheduler_context, scheduler, 0);
    setcontext(&scheduler_context);
}

/* Timer interrupt handler; makes the new signal function context, saves the running task and swaps to signal function */
void timer_handler(int j)
{
    getcontext(&signal_context);
    signal_context.uc_stack.ss_sp = signal_stack;
    signal_context.uc_stack.ss_size = STACK_SIZE;
    signal_context.uc_stack.ss_flags = 0;
    makecontext(&signal_context, signal_function, 0);
    swapcontext(&current_node->data.context, &signal_context);
}

void pause_handler(int sig)
{
    t.it_interval.tv_sec = 0;
    t.it_interval.tv_usec = 0;
    t.it_value = t.it_interval;
    if (setitimer(ITIMER_REAL, &t, NULL) < 0) {
        printf("settimer error.\n");
        exit(1);
    }
    t_act.sa_handler = &timer_handler;
    t_act.sa_flags = SA_RESTART | SA_SIGINFO;
    sigfillset(&t_act.sa_mask);
    if (sigaction(SIGALRM, &t_act, NULL) == -1) { // Intercept SIGALRM
        perror("Error: cannot handle SIGALRM");
        exit(1);
    }
    if(wait_exist) {
        remove_task(0);
        wait_exist = 0;
    }
    struct Node *current = head;
    while (current!=NULL) {
        if(current!=current_node&&current->data.task_state == TASK_READY) {
            current->data.queueing_time += current_node->data.time_quantum;
        }
        if(current->data.task_state == TASK_WAITING
                && current->data.waiting_time > 0) {
            current->data.waiting_time -= current_node->data.time_quantum;
        }
        if(current->data.task_state == TASK_WAITING
                && current->data.waiting_time == 0) {
            current->data.task_state = TASK_READY;
        }
        current = current->next;
    }

    //printf(" Your input is Ctrl + Z\n");
    printf("\n");

    swapcontext(&scheduler_context, &mcontext);
    getcontext(&scheduler_context);
    scheduler_context.uc_stack.ss_sp = scheduler_stack;
    scheduler_context.uc_stack.ss_size = STACK_SIZE;
    scheduler_context.uc_stack.ss_flags = 0;
    scheduler_context.uc_link = &mcontext;
    makecontext(&scheduler_context, scheduler, 0);
    setcontext(&scheduler_context);
    swapcontext(&mcontext,&scheduler_context);
}

void hw_suspend(int msec_10)
{
    struct Node *current = head;
    while (current!=NULL) {
        if(current!=current_node&&current->data.task_state == TASK_READY) {
            current->data.queueing_time += current_node->data.time_quantum;
        }
        if(current->data.task_state == TASK_WAITING
                && current->data.waiting_time > 0) {
            current->data.waiting_time -= current_node->data.time_quantum;
        }
        if(current->data.task_state == TASK_WAITING
                && current->data.waiting_time == 0) {
            current->data.task_state = TASK_READY;
        }
        current = current->next;
    }
    //printf("Suspend task's PID\t:\t%d\n", current_node->data.pid);
    current_node->data.task_state = TASK_WAITING;
    current_node->data.waiting_time = msec_10 * 10 ;
    getcontext(&scheduler_context);
    scheduler_context.uc_stack.ss_sp = scheduler_stack;
    scheduler_context.uc_stack.ss_size = STACK_SIZE;
    scheduler_context.uc_stack.ss_flags = 0;
    scheduler_context.uc_link = &mcontext;
    makecontext(&scheduler_context, scheduler, 0);
    swapcontext(&current_node->data.context, &scheduler_context);
    return;
}

void hw_wakeup_pid(int pid)
{
    struct Node *current = head;
    while (current!=NULL) {
        if(current->data.pid==pid&&current->data.task_state==TASK_WAITING) {
            current->data.task_state = TASK_READY;
        }
        current = current->next;
    }
    return;
}

int hw_wakeup_taskname(char *task_name)
{
    int num = 0;
    struct Node *current = head;
    while (current!=NULL) {
        if(strcmp(current->data.task_name,task_name)==0
                &&current->data.task_state==TASK_WAITING) {
            current->data.task_state = TASK_READY;
            num++;
        }
        current = current->next;
    }
    return num;
}

int hw_task_create(char *task_name)
{
    void * stack;
    getcontext(&newcontext);
    stack = malloc(STACK_SIZE);
    if (stack == NULL) {
        perror("malloc");
        exit(1);
    }
    newcontext.uc_stack.ss_sp = stack;
    newcontext.uc_stack.ss_size = STACK_SIZE;
    newcontext.uc_stack.ss_flags = 0;
    newcontext.uc_link = &terminator_context;

    int is_waiting = 0;
    /* setup the function we're going to. */
    if(strcmp(task_name,"task1")==0) {
        makecontext(&newcontext, task1, 0);
        //printf("context is %p\n", &newcontext);
    } else if(strcmp(task_name,"task2")==0) {
        makecontext(&newcontext, task2, 0);
        //printf("context is %p\n", &newcontext);
    } else if(strcmp(task_name,"task3")==0) {
        makecontext(&newcontext, task3, 0);
        //printf("context is %p\n", &newcontext);
    } else if(strcmp(task_name,"task4")==0) {
        makecontext(&newcontext, task4, 0);
        //printf("context is %p\n", &newcontext);
    } else if(strcmp(task_name,"task5")==0) {
        makecontext(&newcontext, task5, 0);
        //printf("context is %p\n", &newcontext);
    } else if(strcmp(task_name,"task6")==0) {
        makecontext(&newcontext, task6, 0);
        //printf("context is %p\n", &newcontext);
    } else if(strcmp(task_name,"waiting")==0) {
        makecontext(&newcontext, waiting, 0);
        //printf("context is %p\n", &newcontext);
        is_waiting = 1;
    } else {
        return -1;
    }

    struct Node *last = head;
    newNode = malloc(sizeof(struct Node));
    strcpy(newNode->data.task_name, task_name);
    newNode->data.context = newcontext;
    if(is_waiting) {
        newNode->data.pid=0;
    } else {
        newNode->data.pid=pid_counter++;
    }
    newNode->data.task_state=TASK_READY;
    newNode->data.time_quantum=10;
    newNode->data.queueing_time=0;
    newNode->data.waiting_time = 0;
    newNode->data.prior = 'L';
    newNode->next = NULL;
    if (head == NULL) {
        head = newNode;
        current_node=head; /* Allocate the head to current node when head is builded */
        return newNode->data.pid;
    }
    while (last->next != NULL) {
        last = last->next;
    }
    last->next = newNode;
    return newNode->data.pid;
}

void add_task(char *task_name, int time_quantum,char prior)
{
    //printf("Added task:\n");
    //printf("task name: %s\n", task_name);
    //printf("time quantum (ms): %d\n", time_quantum);
    int pid = hw_task_create(task_name);
    if(pid==-1) {
        printf("No such task name to create.\n");
        return;
    }
    newNode->data.time_quantum=time_quantum;
    newNode->data.prior=prior;
    return;
}
void remove_task(int pid)
{
    struct Node *current = head;
    struct Node *prev;
    /* If head node itself holds the pid to be deleted */
    if (current != NULL && current->data.pid == pid) {
        if(current_node==head) {
            head = current->next;
            current_node = head;
        } else {
            head = current->next;
        }
        free(current);
        return;
    }

    /* Search for the pid to be deleted, keep track of the previous node as we need to change 'prev->next' */
    while (current != NULL && current->data.pid != pid) {
        prev = current;
        current = current->next;
    }

    /* If pid was not present in linked list */
    if (current == NULL) {
        printf("No such pid in the queue.\n");
        return;
    }

    /* Unlink the node from linked list */
    if(current_node==current) {
        prev->next = current->next;
        if(current->next==NULL) {
            current_node = head;
        } else {
            current_node = current->next;
        }
    } else {
        prev->next = current->next;
    }
    free(current);
    return;
}
void process_status()
{
    struct Node *current = head;
    if(current==NULL) {
        printf("No task in the queue.\n");
        return;
    }

    while(current != NULL) {
        //printf("%d\t%s\t%d\t%d\n", current->data.pid, current->data.task_name,
        //       current->data.task_state, current->data.time_quantum);
        char *state="";
        switch(current->data.task_state) {
        case TASK_RUNNING:
            state = "TASK_RUNNING";
            break;
        case TASK_READY:
            state = "TASK_READY";
            break;
        case TASK_WAITING:
            state = "TASK_WAITING";
            break;
        case TASK_TERMINATED:
            state = "TASK_TERMINATED";
            break;
        default:
            ;
        }
        char c;
        if(current->data.time_quantum==20)
            c='L';
        else c='S';
        printf("%d\t%s\t%s\t%d\t%c\t%c\n", current->data.pid, current->data.task_name,
               state, current->data.queueing_time,current->data.prior,c);
        current = current->next;
    }
}

void free_all()
{
    struct Node* current = head;
    struct Node* next;
    while (current != NULL) {
        next = current->next;
        free(current);
        current = next;
    }
    head = NULL;
    current_node = head;
}
