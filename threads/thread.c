#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include "thread.h"
#include "interrupt.h"
#include <stdbool.h>

typedef enum{
    thread_READY = 1,
    thread_RUN = 2,
    thread_SLEEP = 3,
    thread_EXITED = 4,
    thread_EMPTY = 5,
    thread_KILLED = 6
}thread_state;   /////////////////use to check the status of the thread


/* This is the wait queue structure */
typedef struct thread_node{
	Tid index;
	struct thread_node *next;
	struct thread_node *prev;
}thread_node;

/* This is the thread control block */


typedef struct wait_queue{
	thread_node *start_thread;
	thread_node *end_thread;
}thread_queue;

typedef struct {
    thread_state state;
    thread_queue *wait_queue;
    ucontext_t thread_context;
    void* start_sp; 
}thread;

volatile static Tid current_thread;        // to record the current thread
volatile static int total_Threads;    // to record total thread number

static thread thread_array[THREAD_MAX_THREADS];    // array to initilize thread
static thread_queue ready_q;


void thread_queue_push(Tid index, thread_queue *thread_q){
    int enable = interrupts_set(0);
	thread_node* new_node = (thread_node*)malloc(sizeof(thread_node));
	new_node->next = NULL;
	new_node->prev = thread_q->end_thread;
	new_node->index = index;

	if(thread_q->start_thread == NULL){
		thread_q->start_thread = new_node;
		thread_q->end_thread = new_node; // this line might not be used in the future
	}
	else{
		thread_q->end_thread->next = new_node;
                thread_q->end_thread = new_node;
	}
        interrupts_set(enable);
	return;
 } ////////////pushed the thread in the last;



Tid thread_pop_first(thread_queue *thread_q){
    int enable = interrupts_set(0);//////////////////////////////////////////////////////////
    
	if(thread_q->start_thread == NULL){
		return THREAD_NONE;
	}
	else{
		thread_node *return_thread = thread_q->start_thread;
		thread_q->start_thread = thread_q->start_thread->next;
		if(thread_q->start_thread == NULL){
			thread_q->end_thread = NULL;
		}
		else{
			thread_q->start_thread->prev = NULL;
		}
		Tid ans = return_thread->index;
		free(return_thread);
        interrupts_set(enable); ///////////////////////////////////////////
		return ans;                                          
	}
}

Tid thread_pop_index(Tid index, thread_queue *thread_q){
    int enable = interrupts_set(0);
    thread_node *rec_thread = thread_q->start_thread;
    Tid ans;
	 while(rec_thread != NULL){
	 	if(rec_thread->index == index){
	 		if(rec_thread->prev == NULL){
	 			thread_q->start_thread = rec_thread->next;
	 		}
	 		else{
	 			rec_thread->prev->next = rec_thread->next;
	 		}

	 		if(rec_thread->next == NULL){
	 			thread_q->end_thread = rec_thread->prev;
	 		}
	 		else{
	 			rec_thread->next->prev = rec_thread->prev;
	 		}
	 		ans = rec_thread->index;
	 		free(rec_thread);
                        interrupts_set(enable);
	 		return ans;
	 	}
	 	rec_thread = rec_thread->next;

	 }
         interrupts_set(enable);///////////////////
	 return THREAD_INVALID;
}






void thread_init(void)
{
    total_Threads = 1;
    current_thread = 0;
    ucontext_t record_context;
    for(int i = 0; i < THREAD_MAX_THREADS; ++i){
        thread_array[i].state = thread_EMPTY;
    }
    thread_array[current_thread].state = thread_RUN;
    record_context = thread_array[current_thread].thread_context;
    thread_array[current_thread].wait_queue = wait_queue_create();
    getcontext(&record_context);
	/* your optional code here */
}

Tid thread_id()
{
    return current_thread;
}

/* New thread starts by calling thread_stub. The arguments to thread_stub are
 * the thread_main() function, and one argument to the thread_main() function. 
 */
void thread_stub(void (*thread_main)(void *), void *arg)
{
        interrupts_on(); /////////////////////////////////////////////////////////
        thread_main(arg); // call thread_main() function with arg
        thread_exit(0);
        exit(0);
}

Tid thread_create(void (*fn) (void *), void *parg)
{
    int enable = interrupts_set(0);
    long int choosed_loc;
    if(total_Threads >= THREAD_MAX_THREADS){
        interrupts_set(enable);
        return THREAD_NOMORE;
    }
    
    for(unsigned i = 0; i < THREAD_MAX_THREADS; i++){
        if(thread_array[i].state == thread_EMPTY){
            getcontext(&thread_array[i].thread_context);
            choosed_loc = i;
            break;
        }
        
        if(thread_array[i].state == thread_EXITED){
            free(thread_array[i].start_sp);
            getcontext(&thread_array[i].thread_context);
            choosed_loc = i;
            break;
        }                                                  //if the thread state is exoted, free the stack pointer and choose it.  it might can be removed(try it later);
    }

	void *stack_pointer = malloc(THREAD_MIN_STACK);

	if(stack_pointer == NULL){
		return THREAD_NOMEMORY;
	}

        
        
	thread_array[choosed_loc].start_sp = stack_pointer;
	thread_array[choosed_loc].state = thread_READY;
        thread_array[choosed_loc].thread_context.uc_stack.ss_sp = stack_pointer;
        thread_array[choosed_loc].wait_queue = wait_queue_create();
        
        
        stack_pointer = stack_pointer + THREAD_MIN_STACK;
	//stack_pointer -=(unsigned long long)stack_pointer%16;
	stack_pointer = stack_pointer - 8;

	thread_array[choosed_loc].thread_context.uc_mcontext.gregs[REG_RSP] = (greg_t)stack_pointer;
	thread_array[choosed_loc].thread_context.uc_mcontext.gregs[REG_RIP] = (greg_t)&thread_stub;
	thread_array[choosed_loc].thread_context.uc_mcontext.gregs[REG_RDI] = (greg_t)fn;
	thread_array[choosed_loc].thread_context.uc_mcontext.gregs[REG_RSI] = (greg_t)parg;
        
	thread_queue_push(choosed_loc, &ready_q);
        total_Threads = total_Threads + 1;

        interrupts_set(enable);
	return choosed_loc;

}

Tid thread_yield(Tid want_tid){
    int enable = interrupts_set(0);
    bool already_pop = false;

    if(want_tid == THREAD_SELF){
        interrupts_set(enable);
        return current_thread;
    }

    else if(want_tid == THREAD_ANY){
       
        if(ready_q.start_thread == NULL){
                        interrupts_set(enable);
			return THREAD_NONE;
		}
		else{
			want_tid = thread_pop_first(&ready_q);
			already_pop = true;
		}
    }

	
	
	bool check_swap = false;
        if(want_tid >=0 && want_tid < THREAD_MAX_THREADS && ((thread_array[want_tid].state == thread_READY) || (thread_array[want_tid].state == thread_RUN) || (thread_array[want_tid].state == thread_KILLED))){
                    getcontext(&(thread_array[current_thread].thread_context));
                    for(int i = 1; i<THREAD_MAX_THREADS;i++){
                        if(thread_array[i].state == thread_EXITED && i != current_thread){
                            thread_array[i].state = thread_EMPTY;
                            free(thread_array[i].start_sp);
                            wait_queue_destroy(thread_array[i].wait_queue);
                            thread_array[i].wait_queue = NULL;
                        }
                    }   ///////////////////////////////this might need to rewrite into a function;
                        
			if(thread_array[current_thread].state ==thread_KILLED){
                            interrupts_set(enable);
                            thread_exit(0);
			}
                    
			if(check_swap == false){
				check_swap = true;
				if(already_pop == false){
                                    thread_pop_index(want_tid, &ready_q);
				}
                                
				if(thread_array[current_thread].state == thread_RUN && current_thread != want_tid){  
                                    thread_array[current_thread].state = thread_READY;
                                    thread_queue_push(current_thread, &ready_q);
				}
				if(thread_array[want_tid].state != thread_KILLED){
					thread_array[want_tid].state = thread_RUN;
				}
				current_thread = want_tid;
				setcontext(&(thread_array[current_thread].thread_context));
                                interrupts_set(enable);
				return want_tid;
			}
                interrupts_set(enable);
		return want_tid;
		}
                interrupts_set(enable);
		return THREAD_INVALID;
}

void thread_exit(int exit_code){
    
        int enable = interrupts_set(0);
	if(total_Threads <= 1 && thread_array[current_thread].wait_queue->start_thread == NULL){
            interrupts_set(enable);
            exit(0);
        }    
   
		thread_array[current_thread].state = thread_EXITED;
                total_Threads = total_Threads-1;
                thread_wakeup(thread_array[current_thread].wait_queue,1);
                interrupts_set(enable);
		thread_yield(THREAD_ANY);
	  
}

Tid thread_kill(Tid tid)
{
    int enable = interrupts_set(0);
	if(tid == current_thread || tid <0 || tid > THREAD_MAX_THREADS){
		interrupts_set(enable);
        return THREAD_INVALID;
	}
	else{
		if(thread_array[tid].state != thread_READY){
			interrupts_set(enable);
                        return THREAD_INVALID;
		}
		thread_array[tid].state = thread_KILLED;
	}
        interrupts_set(enable);
	return tid;
}

/*******************************************************************
 * Important: The rest of the code should be implemented in Lab 3. *
 *******************************************************************/

/* make sure to fill the wait_queue structure defined above */
struct wait_queue *
wait_queue_create()
{
	struct wait_queue *wq;

	wq = malloc(sizeof(struct wait_queue));
	assert(wq);

	wq->start_thread = NULL;
    wq->end_thread = NULL;

	return wq;
}

void
wait_queue_destroy(struct wait_queue *wq)
{    
   while(wq->start_thread != NULL){
       thread_pop_first(wq);
   }
	free(wq);
}

Tid
thread_sleep(struct wait_queue *queue)
{
        int enable = interrupts_set(0);
        if(queue == NULL){
            interrupts_set(enable);
            return THREAD_INVALID;
        }
        
        if(ready_q.start_thread == NULL){
            interrupts_set(enable);
            return THREAD_NONE;
        } ///////////////////// this condition might can be removed; 
        
        else{
            thread_array[current_thread].state = thread_SLEEP;
            thread_queue_push(current_thread,queue);
            total_Threads = total_Threads - 1;
            interrupts_set(enable);
            return thread_yield(THREAD_ANY);
        }
        interrupts_set(enable);
	return THREAD_FAILED;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int
thread_wakeup(struct wait_queue *queue, int all)
{
    int enable = interrupts_set(0);
    if(queue == NULL){
        interrupts_set(enable);
        return 0;
    }
    if(all == 1){
        int counter = 0;
        while(queue->start_thread != NULL){
            Tid ret = thread_pop_first(queue);
            if(ret == THREAD_NONE){
                interrupts_set(enable);
                return counter;
            }
            else{
                if(thread_array[ret].state != thread_KILLED){
                    thread_array[ret].state = thread_READY;
                }
                total_Threads = total_Threads + 1;
                thread_queue_push(ret, &ready_q);
                counter = counter+1;
            }
        }
        interrupts_set(enable);
        return counter;  
    }
    
    else{
            Tid ret = thread_pop_first(queue);
            if(ret == THREAD_NONE){
                interrupts_set(enable);
                return 0;
            }
            else{
                if(thread_array[ret].state != thread_KILLED){
                    thread_array[ret].state = thread_READY;
                }
                total_Threads = total_Threads + 1;
                thread_queue_push(ret, &ready_q);
                interrupts_set(enable);
                return 1;
            }
    }
}

/* suspend current thread until Thread tid exits */
Tid
thread_wait(Tid tid, int *exit_code)
{
	if (tid <= 0 || tid > THREAD_MAX_THREADS || tid == current_thread || thread_array[tid].state == thread_EMPTY || thread_array[tid].state == thread_EXITED || thread_array[tid].wait_queue->start_thread != NULL){
		return THREAD_INVALID;
	}
	Tid ret = thread_sleep(thread_array[tid].wait_queue);
	if (ret == THREAD_NONE) exit(0);
	return tid;
}




struct lock {
	int lock_state;
    struct wait_queue *lock_wq;
};

struct lock *
lock_create()
{
    int enable = interrupts_set(0);
	struct lock *lock;

	lock = malloc(sizeof(struct lock));
	assert(lock);

	lock->lock_state = 0;
    lock->lock_wq = wait_queue_create();

    assert(lock->lock_wq);
    interrupts_set(enable);
	return lock;
}

void
lock_destroy(struct lock *lock)
{
    int enable = interrupts_set(0);
	assert(lock != NULL);
    
	wait_queue_destroy(lock->lock_wq);
    interrupts_set(enable);
	free(lock);
}

void
lock_acquire(struct lock *lock)
{
	
    int enable = interrupts_set(0);
    assert(lock != NULL);

    while(lock->lock_state == 1){                 // if the lock has already been locked, then just sleep it.
        thread_sleep(lock->lock_wq);
    }
    lock->lock_state = 1;

    interrupts_set(enable);
	
}

void
lock_release(struct lock *lock)
{
    int enable = interrupts_set(0);
	assert(lock != NULL);
    lock->lock_state = 0;
    thread_wakeup(lock->lock_wq, 1);
    interrupts_set(enable);
}

struct cv {
	struct wait_queue *cv_wq;
};

struct cv *
cv_create()
{
    int enable = interrupts_set(0);
	struct cv *cv;

	cv = malloc(sizeof(struct cv));
	assert(cv);

	cv->cv_wq = wait_queue_create();
    interrupts_set(enable);
	return cv;
}

void
cv_destroy(struct cv *cv)
{
	assert(cv != NULL);

	wait_queue_destroy(cv->cv_wq);

	free(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);
        int enable = interrupts_set(0);
        lock_release(lock);
        thread_sleep(cv->cv_wq);
        interrupts_set(enable);
        lock_acquire(lock);
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
    assert(cv != NULL);
    assert(lock != NULL);
	    thread_wakeup(cv->cv_wq,0);
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);
	    thread_wakeup(cv->cv_wq,1);
}
