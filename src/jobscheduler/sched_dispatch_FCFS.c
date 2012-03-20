/*
 * sched_dispatch_FCFS.c
 *
 * Job Dispatching Strategy: First-Come, First-Serve
 * Selects jobs for dispatching by the order they arrived at the scheduler
 *
 *  Created on: 13 Oct 2010
 *      Author: luis
 */

#include "scheduler.h"


struct JQueueElem {
  struct JQueueElem *next;
  Job *job;
};

pthread_mutex_t JQueMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_cond = PTHREAD_COND_INITIALIZER;
struct JQueueElem *JQueueHead = NULL;
struct JQueueElem *JQueueTail = NULL;



//TODO: keep variable with queue size for later polling by JM/other JSs?
void FIFOenqueue (Job *job) {
  struct JQueueElem *newElem = malloc(sizeof(struct JQueueElem));

  pthread_mutex_lock(&JQueMutex);
  newElem->next = NULL;
  newElem->job = job;

  if (JQueueHead == NULL) { //the queue is empty
    assert(JQueueTail == NULL);
    JQueueHead = newElem;
  }
  else {
    assert(JQueueTail != NULL && JQueueTail->next == NULL);
    JQueueTail->next = newElem;
  }

  JQueueTail = newElem;

  pthread_cond_signal(&condition_cond);
  pthread_mutex_unlock(&JQueMutex);
}


//Dequeues the job at the head of the queue
Job *dequeue () {
  Job *result;
  struct JQueueElem *oldHead;

  pthread_mutex_lock(&JQueMutex);

  //if the queue is empty wait until a job is added
  while (JQueueHead == NULL && !shutdown) {//shutdown check... Ugly hack.
    assert(JQueueTail == NULL);
    pthread_cond_wait(&condition_cond, &JQueMutex);
  }

  if (shutdown) {
    finalizeThread();
  }

  result = JQueueHead->job;

  oldHead = JQueueHead;
  JQueueHead = JQueueHead->next;


  if (JQueueHead == NULL) {//if this was the only element on the list, delete the reference to the tail
    assert(JQueueTail == oldHead);
    JQueueTail = NULL;
  }
  else
    assert(JQueueTail != NULL);

  free(oldHead);

  pthread_mutex_unlock(&JQueMutex);
  return result;
}
