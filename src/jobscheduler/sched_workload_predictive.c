/*
 * sched_workload_rr.c
 *
 * Workload Allocation Policy: Predictive Scheduling
 * Allocates resources on a Round-Robin fashion, without taking into account any optimizations
 *
 *  Created on: 13 Oct 2010
 *      Author: luis
 */

//PU.seencats = sizeof(PU.accum) / (sizeof(int) * 2)
//PU.accum =
//int (*PU.accum)[2];

#include <limits.h>

#include "scheduler.h"


#if SCHED_WORKLOAD_POLICY == SCHEDULING_PREDICTIVE

#define FIXED true


struct PUList *PUsHead;
struct PUList *PUsTail;



int initSched() {
  PUsHead = NULL;
  PUsTail = NULL;

  return 0;
}


//TODO
void printSchedPUMs () {
/*  struct PUMs *iter = NULL;
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

  iter = NULL;
  fprintf(stderr,"GPUs:\n");
  while (iter != NULL) {
    fprintf(stderr,"  PUM %i: \n", iter->thisPUM->id);
    for (int l = 0; l < iter->thisPUM->nPUs; l++) {
      if (iter->thisPUM->availablePUs[l].device_type == 4)
        fprintf(stderr,"    %i: latency: %f, throughput: %f\n", l, iter->thisPUM->availablePUs[l].latency, iter->thisPUM->availablePUs[l].throughput);
    }
  iter = iter->next;
  }
  fprintf(stderr,"EOGPUs\n");*/
}



/*
 * PU Setup
 */


void setupPUMsStruct(PUMStruct *receivedStruct, double latencyResult, double throughputResult) {


  FILE *f = fopen("uselessPUMs.txt","r");
  if (f == NULL) {
    perror("fopen");
    exit(EXIT_FAILURE);
  }


  int testPU, testPUM;
  float nil;
  bool useless = false;

  for (int i = 0; i < receivedStruct->nPUs; i++) {

    while (fscanf(f,"%i %i %f\n",&testPU, &testPUM, &nil) != EOF) {
      if (testPU == i && testPUM == receivedStruct->id) {
	fprintf(stderr,"JS  (%i): PU %i, PUM %i marked as useless. Ignoring PU.\n",myid, testPU, testPUM);
	useless = true;
	break;
      }
    }
    rewind(f);
    if (useless) {
      useless = false;
      continue;
    }




    struct PUList *newPU = malloc(sizeof (struct PUList));
    newPU->next = NULL;
    newPU->connLatency = latencyResult;
    newPU->connThroughput = throughputResult;
    newPU->PUid = i;
    newPU->PUMid = receivedStruct->id;
    newPU->thisPUProps = &(receivedStruct->availablePUs[i]);

    newPU->nCategsSeen = 0;
    newPU->allCategsSeen = NULL;
    newPU->nTimesUsed = NULL;
    newPU->categAverageDuration = NULL;
    newPU->alreadyMeasured = NULL;

    newPU->nJobsAtQueue = 0;
    newPU->currentQueue = NULL;
    newPU->startedAtInstant = 0.0;
    if (pthread_mutex_init(&(newPU->accessMutex), NULL) != 0)
      fprintf(stderr, "JS  (%i): ERROR INITIALIZING ACCESS MUTEX for PU %i of PUM %i\n", myid, i,receivedStruct->id);
//    if(pthread_cond_init(&(newPU->accessCondition), NULL) != 0)
//      fprintf(stderr, "PUM(%i) ERROR INITIALIZING COND for PU %i of PUM %i\n", myid, i, receivedStruct->id);

    if (PUsHead == NULL)
      PUsHead = newPU;
    else
      PUsTail->next = newPU;

    PUsTail = newPU;
  }

  free(receivedStruct);
  if (debug_JS)
    printSchedPUMs();

  fclose(f);

}



bool acceptablePUForJob (PUProperties *PUProps, JobToPUM *jtp) {

  /////Verifications/////
  long unsigned int totalArgSize = 0;

  for (int i = 0; i < jtp->nTotalArgs; i++) {
    totalArgSize += jtp->argSizes[i];

    //1. Total arguments size
    if (totalArgSize > PUProps->global_mem_size) {
      if (!SILENT)
      fprintf(stderr,"JS  (%i):  PU unnacceptable because global_mem_size (%lu) < totalArgSize (%lu).\n",myid,PUProps->global_mem_size, totalArgSize);
      return false;
    }

    //2. Size of each argument
    if (jtp->argSizes[i] > PUProps->max_memory_alloc) {
      if (!SILENT)
      fprintf(stderr,"JS  (%i):  PU unnacceptable because max_memory_alloc < jtp->argSizes[%i] (%i)\n",myid, i, jtp->argSizes[i]);
      return false;
    }

  }

  //3. NDRange dimensions
  // 3.1 number of dimensions
  if (jtp->nDimensions > PUProps->max_work_item_dimensions) {
    if (!SILENT)
    fprintf(stderr,"JS  (%i):  PU unnacceptable because nDimensions (%i) > PUProps->max_work_item_dimensions (%i)\n",myid, jtp->nDimensions, PUProps->max_work_item_dimensions);
    return false;
  }

  int totalWorkGroupSize = 1;
  for (int i = 0; i < jtp->nDimensions; i++) {
    // 3.2 number of items per dimension
    if (jtp->nItems[i] > PUProps->max_work_item_sizes[i]) {
      if (!SILENT)
      fprintf(stderr,"JS  (%i):  PU unnacceptable because max_work_item_sizes[i] < jtp->nItems[%i] (%i)\n",myid, i, jtp->nItems[i]);
      return false;
    }

    totalWorkGroupSize *= jtp->nGroupItems[i];
  }

  // 3.3 number of total items per group
  if (totalWorkGroupSize > PUProps->max_work_group_size) {
    if (!SILENT)
    fprintf(stderr,"JS  (%i):  PU unnacceptable because totalWorkGroupSize (%i)> PUProps->max_work_group_size\n",myid, totalWorkGroupSize);
    return false;
  }


  return true;
}

double categoryDurationEstimate (struct PUList *PU, int category) {
  for (int i = 0; i < PU->nCategsSeen ; i++) {
    if (PU->allCategsSeen[i] == category)
      return PU->categAverageDuration[i];//TODO: multiply by something on the job (data size?)
  }

  //if we are here, it means that this PU never saw this category.
  fprintf(stderr, "JS  (%i): BUG<<<<<<<<<<<<<<<<<<<<\n", myid);
  fprintf(stderr, "JS  (%i): BUG<<<<<<<<<<<<<<<<<<<<: We should not have reached this. Trying to estimate time for a never-before seen category?\n", myid);
  fprintf(stderr, "JS  (%i): BUG<<<<<<<<<<<<<<<<<<<<\n", myid);
  return PU->thisPUProps->throughput; //TODO: replace with (Num of FLOPs)/(FLOP/sec)
}

double predictIdleTime (struct PUList *PU, JobToPUM *jtp) {
  double availableTime;

  pthread_mutex_lock(&(PU->accessMutex));//we need to ensure nJobsAtQueue donesn't change
  double timeNow = time(NULL);
  if (PU->nJobsAtQueue == 0)
    availableTime = timeNow;
  else
    availableTime = PU->startedAtInstant;
  if (!SILENT)
  fprintf(stderr,"JS  (%i):    idle time prediction: started first job on queue at %f.\n",myid, PU->startedAtInstant);
  if (PU->nJobsAtQueue == 0) {
    if (!SILENT)
    fprintf(stderr,"JS  (%i):    idle time prediction: ...BUT! nJobsAtQueue==0, so the starting time will be _now_ (%f).\n",myid, availableTime);
  }

  if (FIXED)
    for (int i = 0; i<PU->nJobsAtQueue; i++) {
      availableTime+=PU->thisPUProps->throughput;
    }
  //1. predict availability time (counts with currently running job's time)
  // - includes computation time
  else
    for (int i = 0; i<PU->nJobsAtQueue; i++)
      availableTime += categoryDurationEstimate(PU, PU->currentQueue[i]);
  pthread_mutex_unlock(&(PU->accessMutex));
  if (!SILENT)
  fprintf(stderr,"JS  (%i):    idle time prediction: after each category is done, will be available at: %f\n",myid, availableTime);



  //2. predict duration of this job on this PU
  // - includes (latency + transfer time) + compilation + data transfer time + computation time
  int totalTransferSize = 0;
  for (int i = 0; i < jtp->nTotalArgs; i++) {
    if ((jtp->argTypes[i] == INPUT) ||
        (jtp->argTypes[i] == INPUT_OUTPUT))
      totalTransferSize += jtp->argSizes[i];
  }



  // 2.1 network data transfer
  bool hideTransf = false;
  double timeToTransfer = PU->connLatency + (totalTransferSize / PU->connThroughput);
  if ((timeNow + timeToTransfer) > availableTime)
    availableTime = timeNow + timeToTransfer;
  else {
    //if the PU is doing computations, the data transfer time will be hidden
    //by the computation time of the previous job
    hideTransf = true;
  }

  // 2.2 computation time
  availableTime += PU->thisPUProps->latency;
  double categEst;
  if (FIXED)
    categEst = PU->thisPUProps->throughput;
  else
    categEst = categoryDurationEstimate(PU, jtp->category);
  availableTime += categEst;//categoryDurationEstimate(PU, jtp->category);


  if (!SILENT)
  fprintf(stderr,"JS  (%i):    idle time prediction: Latency of PU connect. : %f\n"
                 "                                   Throughput of PU conn. : %f\n"
                 "                                   Transfer time for job  : %f (size = %i / thgput)\n"
                 "                                   Latency of this PU     : %f\n"
                 "                                   Process. hides transfer? %s\n"
                 "                                   Category estimated time: %f\n"
                 "                                   Predicted Availability : %f\n",myid,
                                                     PU->connLatency,
                                                     PU->connThroughput,
                                                     totalTransferSize / PU->connThroughput, totalTransferSize,
                                                     PU->thisPUProps->latency,
                                                     hideTransf == true ? "true" : "false",
                                                     categEst,
                                                     availableTime);



  return availableTime;
}

double scorePU (struct PUList *PU, JobToPUM *jtp) {
  if (FIXED)
    return predictIdleTime(PU,jtp);

  double result = 0.0;
  //1. detect if this PU has already computed a job of the same category as jtp's
  bool alreadySeenCategory = false;


  for (int i = 0; i < PU->nCategsSeen; i++)
    if (PU->allCategsSeen[i] == jtp->category)
      alreadySeenCategory = true;

  if (!alreadySeenCategory)
    return result;

  result = predictIdleTime(PU,jtp);


  return result;
  //TODO: weight other factors such as preferred, allow and avoid PU types
  //result += something;
}



//TODO: preferred, allow and avoid should also be weighted

/*
  For each Job:
  Type/Category
  Preference
 *
 */

//job, to base the decision on which PUManager to send the job to
//jtp, to save the PU in which the job shall be run at
//if nPossible = 0, will pick any available PUM and any PU

PUMStruct *pickPUWith(int nPossible, cl_device_type *possibleList, JobToPUM *jtp) {
  if (!SILENT){
  fprintf(stderr,"JS  (%i): Going to pick PU for jtp number %i \n", myid, jtp->jobID);
  fprintf(stderr,"JS  (%i): nPossible: %i\n", myid, nPossible);
  }

  int i = 0;
  int totalPossible = 0;
  struct PUList *allPUsIter = NULL;
  struct PUList **possiblePUs = NULL; //an array of pointers to the selected PUs for this job
                                      //{PUList *, PUList *, ...}


  //TODO: deprecate this (also, currently leaking)
  PUMStruct *result = malloc(sizeof (PUMStruct));
  struct PUList *resultPU = NULL;

  if (!SILENT)
  fprintf(stderr,"JS  (%i): 1. Going to build the list of possible PUs\n", myid);
  //1. Build the list of PUs to which this job may be submitted to
  // -> filtering those that can't run the job due to certain limitations
  for (i = 0; i < nPossible ; i++) {
    allPUsIter = PUsHead;

    while (allPUsIter != NULL) { //assuming the PU list doesn't change while we iterate it
      if (allPUsIter->thisPUProps->device_type == possibleList[i]) {
        if (!SILENT)
        fprintf(stderr,"JS  (%i):  Found a PU with the same device type (PU %i, PUM %i). Filtering it...\n", myid, allPUsIter->PUid, allPUsIter->PUMid);
        if (acceptablePUForJob(allPUsIter->thisPUProps, jtp)) { //filtering...
          if (!SILENT)
          fprintf(stderr,"JS  (%i):  Accepted!\n", myid);
          totalPossible++;
          possiblePUs = realloc(possiblePUs, sizeof(struct PUList *)*totalPossible);
          possiblePUs[totalPossible-1] = allPUsIter;
        }
        else
          if (!SILENT)
          fprintf(stderr,"JS  (%i): PU REJECTED!\n", myid);
      }
      allPUsIter = allPUsIter->next;
    } // endwhile

  } // endfor
  if (!SILENT){
  fprintf(stderr,"JS  (%i): Finished building the list of possible PUs.\n", myid);
  ////////////End of 1.

  fprintf(stderr,"JS  (%i): 2. Going to determine the finishing times for the computation on each of them (will leave here on first job of each category for each PU)\n", myid);
  fprintf(stderr,"JS  (%i): This job's category: %i\n", myid, jtp->category);
  }
  //2. Determining the finishing times for the computations on each of them
  // -> and choosing the best one
  double bestScore = DBL_MAX;//FLT_MAX;
  double currScore = DBL_MAX;//FLT_MAX;

  for (i = 0 ; i < totalPossible; i++) {
    //2.1 get PU's score
    if (!SILENT)
    fprintf(stderr,"JS  (%i):   Going to get score for PU %i (PU %i of PUM %i)\n", myid, i, possiblePUs[i]->PUid, possiblePUs[i]->PUMid);
    currScore = scorePU(possiblePUs[i], jtp);

    if (!SILENT)
    fprintf(stderr,"JS  (%i):   Got score: %f (best was %f)\n", myid, currScore, (bestScore == DBL_MAX) ? -999 : bestScore);

    assert(currScore >= 0.0);
    if (!SILENT)
    fprintf(stderr,"JS  (%i):   Asserted successfully.\n", myid);
    //2.2 if this score is lower than the recorded low, keep this one
    if (currScore == 0.0) {
      if (!SILENT)
      fprintf(stderr,"JS  (%i):   score is 0 for PU %i, PUM: %i!!!\n", myid, possiblePUs[i]->PUid, possiblePUs[i]->PUMid);
      resultPU = possiblePUs[i];
      break;
    }
    else if (currScore < bestScore) {
      if (!SILENT)
      fprintf(stderr,"JS  (%i):   score is > 0 for PU %i, PUM %i\n", myid, possiblePUs[i]->PUid, possiblePUs[i]->PUMid);
      bestScore = currScore;
      resultPU = possiblePUs[i];
    }
    if (!SILENT)
    fprintf(stderr,"JS  (%i):   re-cycling....?\n", myid);
  }

  free(possiblePUs);

  if (resultPU == NULL) {//it might happen that no PU is adequate (none has enough resources for this job)
    fprintf(stderr,"JS  (%i): No PU is adequate! :(\n", myid);
    return NULL;
  }

  if (!SILENT)
  fprintf(stderr,"JS  (%i): Ok, got a PU.\n", myid);
  jtp->runOn = resultPU->PUid;
  result->id = resultPU->PUMid;

  fprintf(stderr,"JS  (%i): Selected PU %i @ PUM %i\n", myid, jtp->runOn, result->id);
  ///////////End of 2.


  //3. update variable, stateful information
  // - nCategsSeen++
  // - nTimesUsed[] == {Cat, ntimes++}
  // - jobsAtQueue++
  // - currentQueue = currentQueue{Cat}
  // - if (jobsAtQueue == 1) startedAtInstant = now + latency + jobSize/throughput
  if (!SILENT)
  fprintf(stderr,"JS  (%i): 3. Updating category execution tracking.\n", myid);
  for (i = 0; i < resultPU->nCategsSeen; i++) {
    if (resultPU->allCategsSeen[i] == jtp->category) {
      if (!SILENT)
      fprintf(stderr,"JS  (%i): Current PU had already seen this category. Increasing nTimesUsed.\n", myid);
      (resultPU->nTimesUsed[i])++;
      if (!SILENT)
      fprintf(stderr,"JS  (%i): Successfully increased nTimesUsed. Breaking now.\n", myid);
      break;
    }
  }
  if (i == resultPU->nCategsSeen) {//then this PU hasn't ran a job of this category yet
    if (!SILENT)
    fprintf(stderr,"JS  (%i): this PU hadn't ran a job of this category yet. Creating data containers.\n", myid);

    if (pthread_mutex_lock(&(resultPU->accessMutex)))
      perror("pthread_mutex_lock");

    resultPU->nCategsSeen++;

    resultPU->allCategsSeen =        realloc(resultPU->allCategsSeen,        sizeof(int) *(resultPU->nCategsSeen));
    resultPU->nTimesUsed =           realloc(resultPU->nTimesUsed,           sizeof(int) *(resultPU->nCategsSeen));
    resultPU->categAverageDuration = realloc(resultPU->categAverageDuration, sizeof(int) *(resultPU->nCategsSeen));
    resultPU->alreadyMeasured =      realloc(resultPU->alreadyMeasured,      sizeof(bool)*(resultPU->nCategsSeen));
    if (pthread_mutex_unlock(&(resultPU->accessMutex)))
      perror( "pthread_mutex_unlock");
    if (!SILENT)
    fprintf(stderr,"JS  (%i): created ok.\n", myid);

    //we don't need a lock here because this is the last item that has been created, which will never correspond to something being edited
    resultPU->allCategsSeen[i] = jtp->category;
    resultPU->nTimesUsed[i] = 1;
    resultPU->categAverageDuration[i] = resultPU->thisPUProps->throughput;//TODO: replace with FLOPS
    resultPU->alreadyMeasured[i] = false;
    if (!SILENT)
    fprintf(stderr,"JS  (%i): assigned ok\n", myid);
  }
  ////////////End of 3.

  if (pthread_mutex_lock(&(resultPU->accessMutex)))
    perror("pthread_mutex_lock");
  if (!SILENT)
  fprintf(stderr,"JS  (%i): 4. Updating job queue monitor for this PU...\n", myid);
  (resultPU->nJobsAtQueue)++;
  resultPU->currentQueue = realloc(resultPU->currentQueue, sizeof(int)*(resultPU->nJobsAtQueue));
  if (!SILENT)
  fprintf(stderr,"JS  (%i):  Resized ok\n", myid);

  resultPU->currentQueue[(resultPU->nJobsAtQueue)-1] = jtp->category;
  if (!SILENT)
  fprintf(stderr,"JS  (%i):  Assigned category to last job ok\n", myid);

  if ((resultPU->nJobsAtQueue) == 1) { //there are no more jobs on this PU's queue -> we can predict when the next (this) job will start running
    if (!SILENT)
    fprintf(stderr,"JS  (%i): There are no more jobs running on this PU. Setting predicted start time\n", myid);
    int totalTransferSize = 0;
    for (i = 0; i < jtp->nTotalArgs; i++) {
      if ((jtp->argTypes[i] == INPUT)||
          (jtp->argTypes[i] == INPUT_OUTPUT))
        totalTransferSize += jtp->argSizes[i];
    }
    resultPU->startedAtInstant = time(NULL) + (resultPU->connLatency) + (totalTransferSize/(resultPU->connThroughput));

    if (!SILENT){
    fprintf(stderr,"JS  (%i):  Calculations: connLat: %f, transfSize: %i, throughput: %f\n", myid, resultPU->connLatency, totalTransferSize, resultPU->connThroughput);
    fprintf(stderr,"JS  (%i):  Results: div: %f, sum: %f\n", myid, (totalTransferSize/(resultPU->connThroughput)), resultPU->connLatency + (totalTransferSize/(resultPU->connThroughput)));
    fprintf(stderr,"JS  (%i):  Will start at instant: %f (curr time is %li)\n", myid, resultPU->startedAtInstant, time(NULL));
    }
  }
  else
    if (!SILENT)
    fprintf(stderr,"JS  (%i): There are more jobs on this PU's queue (the running one must have started at %f (curr time is %li).\n", myid, resultPU->startedAtInstant, time(NULL));

  if (pthread_mutex_unlock(&(resultPU->accessMutex)))
    perror("pthread_mutex_unlock");
  /////////////End of 4.

  return result;
}


//TODO
//job, to base the decision on which PUManager to send the job to
//jtp, to save the PU in which the job shall be run
PUMStruct *pickPUWithout (int nNotPossible, cl_device_type *notPossibleList, JobToPUM *jtp) {
  return NULL;
}



  /*
   * lock stuff
   *
   * Actualiza dados:
   *      actualizar categAverageDuration == categoria do 1º job na queue da PU indicada demora X segundos nesta PU
   *      eliminar (categoria do) 1º job na queue
   *      reduzir nJobsAtQueue
   *      (só relevante se ainda há jobs na queue):
   *         indicar startedAtInstant __agora__
   *      TODO: actualiza latência + throughput para o resultCollector respectivo
   *
   * unlock stuff
   */
void treatFinishNotification (finishedJobNotification *fjn, int fromPUM) {

  struct PUList *allPUsIter = PUsHead;

  //we assume the PU list does change while we iterate it (no new PUs connecting/leaving).
  while (allPUsIter != NULL) {

    if ((fromPUM == allPUsIter->PUMid) &&
        (fjn->ranAtPU == allPUsIter->PUid)) { //we found the right PU



      pthread_mutex_lock(&(allPUsIter->accessMutex));
      printf("RECEIVED AN EXECUTION TIME from PU %i, PUM %i: %fs (overheads: %fs)\n", fjn->ranAtPU, fromPUM, fjn->executionTime, fjn->overheads);
/*      if ((fjn->executionTime > 5000) && (fjn->executionTime != DBL_MAX)) {
	FILE *f = fopen("uselessPUMs.txt","a");
	if (f == NULL) {
	  perror("fopen");
	  exit(EXIT_FAILURE);
	}
	fprintf(f,"%i %i %f\n",fjn->ranAtPU, fromPUM, fjn->executionTime);
	fclose(f);
	fprintf(stderr,"JS  (%i): PU %i, PUM %i deemed useless.\n",myid,fjn->ranAtPU, fromPUM);
      }
  */    for (int i = 0 ; i < allPUsIter->nCategsSeen; i++) {
        if (allPUsIter->allCategsSeen[i] == fjn->category) { //we found the right category

          //For the respective category, on the PU the job ran on:

          //1. update execution time prediction
          if ( !(allPUsIter->alreadyMeasured[i]) ) //this is the first time we estimate this job's category execution time
            allPUsIter->categAverageDuration[i] = fjn->executionTime;
          else
            allPUsIter->categAverageDuration[i] =   (      OLD_COMPUTATION_WEIGHT  * allPUsIter->categAverageDuration[i])
                                                  + ( (1 - OLD_COMPUTATION_WEIGHT) * fjn->executionTime                 );

          //2. remove current job from the head of the queue
          (allPUsIter->nJobsAtQueue)--;
          allPUsIter->currentQueue = memmove(allPUsIter->currentQueue, (allPUsIter->currentQueue)+1, sizeof(int)*(allPUsIter->nJobsAtQueue));
          allPUsIter->currentQueue = realloc(allPUsIter->currentQueue, sizeof(int)*(allPUsIter->nJobsAtQueue));

          //3. update startedAtInstant to _now_
          allPUsIter->startedAtInstant = time(NULL);

          pthread_mutex_unlock(&(allPUsIter->accessMutex));
          return;
        }
      }
      pthread_mutex_unlock(&(allPUsIter->accessMutex));
    }
    allPUsIter = allPUsIter->next;
  }

  if (allPUsIter == NULL) {
    fprintf(stderr, "JS  (%i): PROGRAMMING ERROR saving finish notification. did not find the PU corresponding to this finished job!\n", myid);
    exit(EXIT_FAILURE);
  }

}

#endif //SCHED_WORKLOAD_POLICY == SCHEDULING_PREDICTIVE
