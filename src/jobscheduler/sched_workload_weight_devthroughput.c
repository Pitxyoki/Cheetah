/*
 * sched_workload_weight_devthroughput.c
 *
 * Workload Allocation Policy: Weighted Workload
 * Allocates resources on a Round-Robin fashion, takeing into account that faster machines get proportionally more jobs.
 *
 *  Created on: 13 Oct 2010
 *      Author: luis
 */


#include "scheduler.h"

#if SCHED_WORKLOAD_POLICY == SCHEDULING_RR_THROUGHPUT


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
//TODO: check the sched_workload_rr.c to get an improved version of this function
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





void setupPUMsStruct(PUMStruct *receivedStruct, double latencyResult, double throughputResult) {
//fprintf(stderr, "setting up latency %lf, throughput %lf\n", latencyResult, throughputResult);
  for (int i = 0; i < receivedStruct->nPUs; i++) {

    //store each PU by its processing throughput -> lower times for throughput go first
    //set PU's nTimesUsed to 0

    struct PUMs *newPUM = malloc(sizeof(struct PUMs));
    newPUM->next = NULL;
    newPUM->connLatency    = latencyResult;
    newPUM->connThroughput = throughputResult;
    newPUM->thisPUM = receivedStruct;

//    fprintf(stderr,">>>>>>>>>>>< receivedStruct (%i) type for id (%i) : %lu (isit CPU? %i)\n", receivedStruct->id, i, receivedStruct->availablePUs[i].device_type, CL_DEVICE_TYPE_CPU );
//    fprintf(stderr,">>>>>>>>>>>< receivedStruct (%i) nPUs: %i\n", receivedStruct->id, receivedStruct->nPUs);
    if (receivedStruct->availablePUs[i].device_type == CL_DEVICE_TYPE_CPU) {
      if (CPUsHead == NULL) {
        CPUsHead = newPUM;
        CPUsTail = newPUM;
        continue;
      }
      else {
        struct PUMs *prevPUM = NULL;
        struct PUMs *currPUM = CPUsHead;
        while (currPUM != NULL) {
          for (int j = 0; j < currPUM->thisPUM->nPUs; j++) {
            if (currPUM->thisPUM->availablePUs[j].device_type == CL_DEVICE_TYPE_CPU)
              if (currPUM->thisPUM->availablePUs[j].throughput > receivedStruct->availablePUs[i].throughput) { //if what is stored is slower than the new one
                newPUM->next = currPUM;
                if (prevPUM == NULL)
                  CPUsHead = newPUM;
                else
                  prevPUM->next = newPUM;
                currPUM = CPUsTail;
                break;
              }
          }
          prevPUM = currPUM;
          currPUM = currPUM->next;
        }
        CPUsTail->next = newPUM;
        CPUsTail = newPUM;
      }
    }
    else if (receivedStruct->availablePUs[i].device_type == CL_DEVICE_TYPE_GPU) {
      if (GPUsHead == NULL) {
        GPUsHead = newPUM;
        GPUsTail = newPUM;
        continue;
      }
      else {
        struct PUMs *prevPUM = NULL;
        struct PUMs *currPUM = GPUsHead;

        while (currPUM != NULL) {
          for (int j = 0; j < currPUM->thisPUM->nPUs; j++) {
            if (currPUM->thisPUM->availablePUs[j].device_type == CL_DEVICE_TYPE_GPU)
              if (currPUM->thisPUM->availablePUs[j].throughput > receivedStruct->availablePUs[i].throughput) {
                newPUM->next = currPUM;
                if (prevPUM == NULL)
                  GPUsHead = newPUM;
                else
                  prevPUM->next = newPUM;
                return;
              }
          }
          prevPUM = currPUM;
          currPUM = currPUM->next;
        }
        GPUsTail->next = newPUM;
        GPUsTail = newPUM;
      }
    }
    else
      cheetah_print_error("Received a PU with an unsupported device type (%li)", receivedStruct->availablePUs[i].device_type);
  }



  if (debug_JS)
    printSchedPUMs();
}







PUMStruct *pickPUWith (int nPossible, cl_device_type *possibleList, JobToPUM *jtp) {

  //percorrer lista de PUMs adequada, ao encontrar uma, selecciona-a e:
  //verifica throughput
  // divide o thgpt do último pelo deste.
  // incrementa nTimesUsed. Se == divisão do thgpt do último pelo deste, envia-o para reserve queue
  // se queue == NULL, queue = reserveQueue

/*
Para cada possible device:
- Percorre lista de PUMs adequada. Para cada uma das PUMs:
 - Percorre a lista de PUs. Para cada PU adequada:
  - se reserved:
    foundOneReserved = true;
    passa à próxima
  - se não reserved:
    incrementa nTimesUsed
    retorna este PUM

- Se percorreu todas as PUMs:
 - se foundOneReserved == true
   - percorre lista de reserveds, define-as como não reserveds
   - volta a percorrer todas as PUMs
 se não, retorna NULL
*/
  struct PUMs *currPUM = NULL, *prevPUM = NULL;
  PUMStruct *result = NULL;
  double worstThroughput = 0;
  bool foundOneReserved = false;
  int nIters = 0;
  PUProperties *currPU;
  double nTimesFaster;


  for (cl_uint i = 0; i < nPossible; i++) {

    if (possibleList[i] == CL_DEVICE_TYPE_CPU) {
      currPUM = CPUsHead;
      for (int j = 0; j<CPUsTail->thisPUM->nPUs; j++) {
        if (CPUsTail->thisPUM->availablePUs[j].device_type == CL_DEVICE_TYPE_CPU &&
            CPUsTail->thisPUM->availablePUs[j].throughput > worstThroughput) {
          worstThroughput = CPUsTail->thisPUM->availablePUs[j].throughput;
        }
      }

    }
    else {
      assert (possibleList[i] == CL_DEVICE_TYPE_GPU);
      currPUM = GPUsHead;
      for (int j = 0; j<GPUsTail->thisPUM->nPUs; j++) {
        if (GPUsTail->thisPUM->availablePUs[j].device_type == CL_DEVICE_TYPE_GPU &&
            GPUsTail->thisPUM->availablePUs[j].throughput > worstThroughput)
          worstThroughput = GPUsTail->thisPUM->availablePUs[j].throughput;
      }
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
            nTimesFaster = worstThroughput / currPU->throughput;
            currPU->nTimesUsed++;
            if (currPU->nTimesUsed >= nTimesFaster)
              addReservedPU (possibleList[i], currPU);

            //TODO: other tests (to compare latency, throughput provided by other PUMs)
            jtp->runOn = j;//currPU->device_type;

            return currPUM->thisPUM;
          }
        }
      }
      prevPUM = currPUM;
      currPUM = currPUM->next;
    }
  }
  return result;
}

//TODO
//job, to base the decision on which PUManager to send the job to
//jtp, to save the PU in which the job shall be run
PUMStruct *pickPUWithout (int nNotPossible, cl_device_type *notPossibleList, JobToPUM *jtp) {
  return NULL;
}

#endif //SCHED_WORKLOAD_POLICY == SCHEDULING_RR_THROUGHPUT
