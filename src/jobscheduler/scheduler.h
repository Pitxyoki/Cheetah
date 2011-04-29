/*
 * scheduler.h
 *
 *  Created on: 7 Apr 2010
 *      Author: luis
 */

#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#include "cheetah/cheetah-common.h"

#define OLD_COMPUTATION_WEIGHT (1.0f/2.0f)


#define SCHEDULING_RR 1
#define SCHEDULING_RR_THROUGHPUT 2
#define SCHEDULING_PREDICTIVE 3
#define SCHED_WORKLOAD_POLICY SCHEDULING_PREDICTIVE


struct PUList{
  struct PUList *next;
  double connLatency;
  double connThroughput;
  int PUid;
  int PUMid;
  PUProperties *thisPUProps;
  pthread_mutex_t accessMutex;
//  pthread_cond_t accessCondition;

  //Variable, stateful information:
  int nCategsSeen;
  int *allCategsSeen;             //{categ,  categ,  categ}
  int *nTimesUsed;             //{ntimes, ntimes, ntimes}
  double *categAverageDuration;//{avrg,   avrg,   avrg}
  bool *alreadyMeasured;

  int nJobsAtQueue;
  int *currentQueue;//{categ, categ}
  double startedAtInstant; //time since Epoch when this PU started computing a job

};


//TODO: deprecate this:
struct PUMs{
  struct PUMs *next;
  double connLatency;
  double connThroughput;
  PUMStruct *thisPUM;
};


struct reservedPUs {
  struct reservedPUs *next;
  PUProperties *thisPU;
};


//PUM-receiving thread
void *PUMReceiver();


void FIFOenqueue (Job *job);
Job *dequeue ();


void *jobReceiver();
void *jobDispatcher();

PUMStruct *selectPUM (Job *job, JobToPUM *jtp);

void sendJobToPUM (int toRank, JobToPUM *job);

void returnFailure (Job *job);



//Initialize the scheduler's state
int initSched();

void setupPUMsStruct(PUMStruct *receivedStruct, double latencyResult, double throughputResult);
//TODO: this should only return the PUM's ID
PUMStruct *pickPUWith   (int nPossible,    cl_device_type *possibleList, JobToPUM *jtp);
PUMStruct *pickPUWithout(int nPossible,    cl_device_type *possibleList, JobToPUM *jtp);


/*void setupPUMsStruct_RR(PUMStruct *receivedStruct, double latencyResult, double throughputResult);
PUMStruct *pickPUWith_RR(int nPossible,    cl_device_type *possibleList, JobToPUM *jtp);


void setupPUMsStruct_RR_Thrghpt(PUMStruct *receivedStruct, double latencyResult, double throughputResult);
PUMStruct *pickPUWith_RR_Thrghpt (int nPossible, cl_device_type *possibleList, JobToPUM *jtp);


void addReservedPU(cl_device_type devType, PUProperties *PU);
void cleanupReservedPUs (cl_device_type devType);
*/

void *PUStatusReceiver ();

void treatFinishNotification(finishedJobNotification *fjn, int fromPUM);


#endif /* SCHEDULER_H_ */
