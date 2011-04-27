/*
 * sched_workload_rr.c
 *
 * Workload Allocation Policy: Round-Robin
 * Allocates resources on a Round-Robin fashion, without taking into account any optimizations
 *
 *  Created on: 13 Oct 2010
 *      Author: luis
 */


#include "scheduler.h"

#if SCHED_WORKLOAD_POLICY == SCHEDULING_RR

struct PUMs *CPUsHead;
struct PUMs *CPUsTail;
struct PUMs *GPUsHead;
struct PUMs *GPUsTail;
struct reservedPUs *reservedCPUs;
struct reservedPUs *reservedGPUs;



int initSched() {
  CPUsHead = NULL;
  CPUsTail = NULL;
  GPUsHead = NULL;
  GPUsTail = NULL;
  reservedCPUs = NULL;
  reservedGPUs = NULL;
  return 0;
}


//TODO: better organize this file

void printSchedPUMs () {
  struct PUMs *iter = CPUsHead;
  fprintf(stderr,"CPUs:\n");

  while (iter != NULL) {
    fprintf(stderr,"  PUM %i: \n", iter->thisPUM->id);
    for (int l = 0; l < iter->thisPUM->nPUs; l++) {
      if (iter->thisPUM->availablePUs[l].device_type == 2)
        fprintf(stderr,"    %i: latency: %f, throughput: %f\n", l, iter->thisPUM->availablePUs[l].latency, iter->thisPUM->availablePUs[l].throughput);
    }
    iter = iter->next;
  }
  fprintf(stderr,"EOCPUs\n");

  iter = GPUsHead;
  fprintf(stderr,"GPUs:\n");
  while (iter != NULL) {
    fprintf(stderr,"  PUM %i: \n", iter->thisPUM->id);
    for (int l = 0; l < iter->thisPUM->nPUs; l++) {
      if (iter->thisPUM->availablePUs[l].device_type == 4)
        fprintf(stderr,"    %i: latency: %f, throughput: %f\n", l, iter->thisPUM->availablePUs[l].latency, iter->thisPUM->availablePUs[l].throughput);
    }
  iter = iter->next;
  }
  fprintf(stderr,"EOGPUs\n");
}



/*
 * Reserved PUs' functions
 * TODO: document this better
 */

void cleanupReservedPUs (cl_device_type devType) {
  struct reservedPUs *head = NULL;
  struct reservedPUs *freeme = NULL;

  if (devType == CL_DEVICE_TYPE_CPU) {
    head = reservedCPUs;
    reservedCPUs = NULL;
  }
  else {
    head = reservedGPUs;
    reservedGPUs = NULL;
  }

  freeme = head;

  while (head != NULL) {
    head->thisPU->reserved = false;
    head->thisPU->nTimesUsed = 0;
    freeme = head;
    head = head->next;
    free(freeme);
  }
}


void addReservedPU(cl_device_type devType, PUProperties *PU) {

  PU->reserved = true;

  struct reservedPUs *head = NULL;
  if (devType == CL_DEVICE_TYPE_CPU)
    head = reservedCPUs;
  else
    head = reservedGPUs;

  if (head == NULL) {
    head = malloc(sizeof(struct reservedPUs));
    head->next = NULL;
    head->thisPU = PU;

    if (devType == CL_DEVICE_TYPE_CPU)
      reservedCPUs = head;
    else if (devType == CL_DEVICE_TYPE_GPU)
      reservedGPUs = head;
  }
  else {
    while (head->next != NULL)
      head = head->next;
    head->next = malloc(sizeof(struct reservedPUs));
    head->next->next = NULL;
    head->next->thisPU = PU;
  }
}




/*
 * PU Setup
 */

void setupPUMsStruct(PUMStruct *receivedStruct, double latencyResult, double throughputResult) {

  for (cl_uint i = 0; i < receivedStruct->nPUs; i++) {
    struct PUMs *newPUM = malloc(sizeof(struct PUMs));
    newPUM->next = NULL;
    newPUM->connLatency    = latencyResult;
    newPUM->connThroughput = throughputResult;
    newPUM->thisPUM = receivedStruct;

    if (receivedStruct->availablePUs[i].device_type == CL_DEVICE_TYPE_CPU) {
      if (CPUsHead == NULL) {
        CPUsHead = newPUM;
        CPUsTail = newPUM;
      }
      else {
        CPUsTail->next = newPUM;
        CPUsTail = newPUM;
      }
    }
    else if (receivedStruct->availablePUs[i].device_type == CL_DEVICE_TYPE_GPU) {
      if (GPUsHead == NULL) {
        GPUsHead = newPUM;
        GPUsTail = newPUM;
      }
      else {
        GPUsTail->next = newPUM;
        GPUsTail = newPUM;
      }
    }
    else
      fprintf(stderr,"JS (%i): received a PU with an unsupported device type (%li)", myid, receivedStruct->availablePUs[i].device_type);
  }


  if (debug_JS)
    printSchedPUMs();

}





//job, to base the decision on which PUManager to send the job to
//jtp, to save the PU in which the job shall be run at
//if nPossible = 0, will pick any available PUM and any PU

PUMStruct *pickPUWith(int nPossible, cl_device_type *possibleList, JobToPUM *jtp) {
  struct PUMs *currPUM = NULL, *prevPUM = NULL;
  PUProperties *currPU;
  bool foundOneReserved = false;
  int nIters = 0;

  for (cl_uint i = 0; i < nPossible; i++) {
    if (possibleList[i] == CL_DEVICE_TYPE_CPU) {
      currPUM = CPUsHead;
    }
    else {
      assert (possibleList[i] == CL_DEVICE_TYPE_GPU);
      currPUM = GPUsHead;
    }


    while(currPUM != NULL || foundOneReserved) {
      if (currPUM == NULL) {                //this means that we have already looked at all PUMs
        nIters++;
        if (nIters > 1)
          break;
        cleanupReservedPUs(possibleList[i]);//but all were already used the appropriate number of

        if (possibleList[i] == CL_DEVICE_TYPE_CPU)
          currPUM = CPUsHead;
        else {
          assert (possibleList[i] == CL_DEVICE_TYPE_GPU);
          currPUM = GPUsHead;
        }
      }

      for (int j = 0; j < currPUM->thisPUM->nPUs; j++) {
        currPU = &(currPUM->thisPUM->availablePUs[j]);

        if (currPU->device_type == possibleList[i]) {
          if (currPU->reserved == true) {
            foundOneReserved = true;
          }
          else {
            currPU->nTimesUsed++;
            addReservedPU(possibleList[i], currPU);

            jtp->runOn = j;


            //send this PUM to the end of the queue *only if* all of it's PUs have been round-robined
            if (j == currPUM->thisPUM->nPUs) {
              if (currPU->device_type == CL_DEVICE_TYPE_CPU) {
                if (currPUM != CPUsTail) {

                  if (currPUM == CPUsHead)
                    CPUsHead = currPUM->next;
                  else
                    prevPUM->next = currPUM->next;

                  currPUM->next = NULL;
                  CPUsTail->next = currPUM;
                  CPUsTail = currPUM;
                }
              }
              else { //currPU->device_type == CL_DEVICE_TYPE_GPU
                if (currPUM != GPUsTail) {

                    if (currPUM == GPUsHead)
                      GPUsHead = currPUM->next;
                    else
                      prevPUM->next = currPUM->next;

                    currPUM->next = NULL;
                    GPUsTail->next = currPUM;
                    GPUsTail = currPUM;
                  }
              }
            }

            return currPUM->thisPUM;
          }
        }
      }
      prevPUM = currPUM;
      currPUM = currPUM->next;
    }
  }
  return NULL;
}


//TODO
//job, to base the decision on which PUManager to send the job to
//jtp, to save the PU in which the job shall be run
PUMStruct *pickPUWithout (int nNotPossible, cl_device_type *notPossibleList, JobToPUM *jtp) {
  return NULL;
}

#endif //SCHED_WORKLOAD_POLICY == SCHEDULING_RR
