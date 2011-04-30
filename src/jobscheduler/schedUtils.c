/*
 * schedUtils.c
 *
 *  Created on: 7 Apr 2010
 *      Author: luis
 * TODO: free() PUMStructs when scheduler is shutting down
 */

#include "scheduler.h"


void *PUMReceiver() {

  int pos,
      dataSize;

  MPI_Status status;
  PUMStruct *receivedStruct;
  struct timeval start, end, result;

  while (true) {

    receivedStruct = malloc(sizeof(PUMStruct));
    pos = 0;
    dataSize = 0;

    cheetah_print("Probing PUMs...");

    receiveMsg(&dataSize,    1, MPI_INT, MPI_ANY_SOURCE, COMM_TAG_PUONLINE, &status);

    /*** A new PU Manager is online ***/
    cheetah_print("A PU-M was detected. Setting up...");
    char *recvb = malloc(dataSize);
    assert(recvb != NULL);


    receiveMsg(recvb, dataSize, MPI_PACKED, status.MPI_SOURCE, COMM_TAG_PUONLINE, MPI_STATUS_IGNORE);

    MPI_Unpack(recvb, dataSize, &pos, &(receivedStruct->id),   1, MPI_INT, MPI_COMM_WORLD);
    MPI_Unpack(recvb, dataSize, &pos, &(receivedStruct->nPUs), 1, MPI_INT, MPI_COMM_WORLD);

    receivedStruct->availablePUs = calloc(receivedStruct->nPUs, sizeof(PUProperties));
    assert(receivedStruct->availablePUs != NULL);


    for (int i = 0; i < receivedStruct->nPUs; i++) {
      MPI_Unpack(recvb, dataSize, &pos, &(receivedStruct->availablePUs[i].device_type),              1, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);
      MPI_Unpack(recvb, dataSize, &pos, &(receivedStruct->availablePUs[i].clock_frequency),          1, MPI_UNSIGNED,      MPI_COMM_WORLD);
      MPI_Unpack(recvb, dataSize, &pos, &(receivedStruct->availablePUs[i].max_memory_alloc),         1, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);
      MPI_Unpack(recvb, dataSize, &pos, &(receivedStruct->availablePUs[i].cache_type),               1, MPI_UNSIGNED,      MPI_COMM_WORLD);
      MPI_Unpack(recvb, dataSize, &pos, &(receivedStruct->availablePUs[i].global_mem_size),          1, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);
      MPI_Unpack(recvb, dataSize, &pos, &(receivedStruct->availablePUs[i].local_mem_type),           1, MPI_UNSIGNED,      MPI_COMM_WORLD);
      MPI_Unpack(recvb, dataSize, &pos, &(receivedStruct->availablePUs[i].local_mem_size),           1, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);
      MPI_Unpack(recvb, dataSize, &pos, &(receivedStruct->availablePUs[i].max_compute_units),        1, MPI_UNSIGNED,      MPI_COMM_WORLD);
      MPI_Unpack(recvb, dataSize, &pos, &(receivedStruct->availablePUs[i].max_work_item_dimensions), 1, MPI_UNSIGNED,      MPI_COMM_WORLD);
      MPI_Unpack(recvb, dataSize, &pos, &(receivedStruct->availablePUs[i].max_work_item_sizes),      3, MPI_LONG,          MPI_COMM_WORLD);
      MPI_Unpack(recvb, dataSize, &pos, &(receivedStruct->availablePUs[i].max_work_group_size),      1, MPI_LONG,          MPI_COMM_WORLD);
      MPI_Unpack(recvb, dataSize, &pos, &(receivedStruct->availablePUs[i].max_parameter_size),       1, MPI_LONG,          MPI_COMM_WORLD);
      MPI_Unpack(recvb, dataSize, &pos, &(receivedStruct->availablePUs[i].latency),                  1, MPI_DOUBLE,        MPI_COMM_WORLD);
      MPI_Unpack(recvb, dataSize, &pos, &(receivedStruct->availablePUs[i].throughput),               1, MPI_DOUBLE,        MPI_COMM_WORLD);
      MPI_Unpack(recvb, dataSize, &pos, &(receivedStruct->availablePUs[i].bandwidth),               1, MPI_DOUBLE,        MPI_COMM_WORLD);
      receivedStruct->availablePUs[i].nTimesUsed = 0;
      receivedStruct->availablePUs[i].reserved = false;
      
      char devType[4] = "???";

      switch(receivedStruct->availablePUs[i].device_type) {
        case CL_DEVICE_TYPE_CPU: strcpy(devType,"CPU");
                                 break;
        case CL_DEVICE_TYPE_GPU: strcpy(devType,"GPU");
                                 break;
      }

      cheetah_info_print("Device %i (%s) @ PU-M %i:\n"
                              "\tLocal Job submission LATENCY:      %fs\n"
                              "\tProcessing THROUGHPUT (~FLOPS^-1): %fs\n"
                              "\tHost RAM-to-Device bus BANDWIDTH:  %fMB/s",
                               i, devType, status.MPI_SOURCE,
                               receivedStruct->availablePUs[i].latency,
                               receivedStruct->availablePUs[i].throughput,
                               receivedStruct->availablePUs[i].bandwidth);
    }


    if (debug_JS)
      printPUMStruct(receivedStruct);

    free(recvb);

    /*** Test 1: determine communication latency ***/
    cheetah_print("Performing link tests with PU-M %i.", status.MPI_SOURCE);

    //First ping can take much longer, so we ignore it
    MPI_Isend (NULL, 0, MPI_BYTE, status.MPI_SOURCE, COMM_TAG_PUONLINE, MPI_COMM_WORLD, NULL_REQUEST);
    receiveMsg(NULL, 0, MPI_BYTE, status.MPI_SOURCE, COMM_TAG_PUONLINE, MPI_STATUS_IGNORE);

    if (gettimeofday(&start, NULL)) {
      perror("gettimeofday");
      return CL_FALSE;
    }

    for (int i = 0; i < NUM_JS_LATENCY_TESTS-1; i++) {
      MPI_Isend (NULL, 0, MPI_BYTE, status.MPI_SOURCE, COMM_TAG_PUONLINE, MPI_COMM_WORLD, NULL_REQUEST);
      receiveMsg(NULL, 0, MPI_BYTE, status.MPI_SOURCE, COMM_TAG_PUONLINE, MPI_STATUS_IGNORE);
    }

    if (gettimeofday(&end, NULL)) {
      perror("gettimeofday");
      return CL_FALSE;
    }

    timeval_subtract(&result, &end, &start);

    double latencyResult = result.tv_sec + (result.tv_usec / 1000000.0);
    latencyResult = latencyResult / ((NUM_JS_LATENCY_TESTS-1)*2);

    cheetah_info_print("Link test result from PU-M %i: Latency: %.6f seconds.", status.MPI_SOURCE, latencyResult);


    /*** Test 2: determine communication throughput ***/
    char buffer_throughput[PUM_THROUGHPUT_TEST_SIZE / sizeof(char)];

    if (gettimeofday(&start, NULL)) { //TODO?: check also clock_getres
      perror("gettimeofday");
      return CL_FALSE;
    }

    MPI_Isend(buffer_throughput, PUM_THROUGHPUT_TEST_SIZE, MPI_BYTE, status.MPI_SOURCE, COMM_TAG_PUONLINE, MPI_COMM_WORLD, NULL_REQUEST);
    receiveMsg(NULL, 0, MPI_BYTE, status.MPI_SOURCE, COMM_TAG_PUONLINE, MPI_STATUS_IGNORE);

    if (gettimeofday(&end, NULL)) {
      perror("gettimeofday");
      return CL_FALSE;
    }

    timeval_subtract(&result, &end, &start);

    double throughputResult = result.tv_sec + (result.tv_usec / 1000000.0);
    throughputResult = (PUM_THROUGHPUT_TEST_SIZE / throughputResult) - latencyResult;
    cheetah_info_print("Link test result from PU-M %i: Throughput: %.6f MByte/second.", status.MPI_SOURCE, throughputResult /(1024*1024));


    setupPUMsStruct(receivedStruct, latencyResult, throughputResult);
/*    TODO: safe to deprecate?
      switch (SCHED_WORKLOAD_POLICY) {
      case SCHEDULING_RR:
        setupPUMsStruct_RR(receivedStruct, latencyResult, throughputResult);
      break;
      case SCHEDULING_RR_THROUGHPUT:
        setupPUMsStruct_RR_Thrghpt(receivedStruct, latencyResult, throughputResult);
      break;
    }*/


  }
}


void *jobReceiver() {
  int pos,
      dataSize;

  MPI_Status status;
  Job *job;

  while (true) {
    job = malloc(sizeof(Job));
    pos = 0;
    dataSize = 0;

    /*** Receive a job ***/
    receiveMsg(&dataSize, 1, MPI_INT, MPI_ANY_SOURCE, COMM_TAG_JOBSUBMIT, &status);

    char *receivedData = malloc(dataSize);

    //TODO?: This could be treated by a new thread
    receiveMsg(receivedData, dataSize, MPI_PACKED, status.MPI_SOURCE, COMM_TAG_JOBSUBMIT, MPI_STATUS_IGNORE);

    MPI_Unpack(receivedData, dataSize, &pos, &(job->probID),               1, MPI_INT,           MPI_COMM_WORLD);
    MPI_Unpack(receivedData, dataSize, &pos, &(job->jobID),                1, MPI_INT,           MPI_COMM_WORLD);
    MPI_Unpack(receivedData, dataSize, &pos, &(job->required_is_set),      1, MPI_BYTE,          MPI_COMM_WORLD);
    MPI_Unpack(receivedData, dataSize, &pos, &(job->requirePU),            1, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);
    MPI_Unpack(receivedData, dataSize, &pos, &(job->nPreferPUs),           1, MPI_INT,           MPI_COMM_WORLD);
    MPI_Unpack(receivedData, dataSize, &pos, &(job->nAllowPUs),            1, MPI_INT,           MPI_COMM_WORLD);
    MPI_Unpack(receivedData, dataSize, &pos, &(job->nAvoidPUs),            1, MPI_INT,           MPI_COMM_WORLD);
    MPI_Unpack(receivedData, dataSize, &pos, &(job->nForbidPUs),           1, MPI_INT,           MPI_COMM_WORLD);

    job->preferPUs = calloc(job->nPreferPUs, sizeof(cl_device_type));
    job->allowPUs  = calloc(job->nAllowPUs,  sizeof(cl_device_type));
    job->avoidPUs  = calloc(job->nAvoidPUs,  sizeof(cl_device_type));
    job->forbidPUs = calloc(job->nForbidPUs, sizeof(cl_device_type));
    MPI_Unpack(receivedData, dataSize, &pos, job->preferPUs, job->nPreferPUs, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);
    MPI_Unpack(receivedData, dataSize, &pos, job->allowPUs,   job->nAllowPUs, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);
    MPI_Unpack(receivedData, dataSize, &pos, job->avoidPUs,   job->nAvoidPUs, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);
    MPI_Unpack(receivedData, dataSize, &pos, job->forbidPUs, job->nForbidPUs, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);

    MPI_Unpack(receivedData, dataSize, &pos, &(job->category),             1, MPI_INT,          MPI_COMM_WORLD);
    MPI_Unpack(receivedData, dataSize, &pos, &(job->nDimensions),          1, MPI_INT,          MPI_COMM_WORLD);
    MPI_Unpack(receivedData, dataSize, &pos, job->nItems,                  3, MPI_INT,          MPI_COMM_WORLD);
    MPI_Unpack(receivedData, dataSize, &pos, job->nGroupItems,             3, MPI_INT,          MPI_COMM_WORLD);
    MPI_Unpack(receivedData, dataSize, &pos, &(job->startingNameSize),     1, MPI_INT,          MPI_COMM_WORLD);

    job->startingKernel = calloc(job->startingNameSize, sizeof(char));
    MPI_Unpack(receivedData, dataSize, &pos, job->startingKernel, job->startingNameSize, MPI_CHAR, MPI_COMM_WORLD);

    MPI_Unpack(receivedData, dataSize, &pos, &(job->taskSourceSize),       1, MPI_INT,          MPI_COMM_WORLD);

    job->taskSource = calloc(job->taskSourceSize, sizeof(char));
    MPI_Unpack(receivedData, dataSize, &pos, job->taskSource, job->taskSourceSize, MPI_CHAR,     MPI_COMM_WORLD);

    MPI_Unpack(receivedData, dataSize, &pos, &(job->nTotalArgs),           1, MPI_INT,          MPI_COMM_WORLD);

    job->argTypes  = calloc(job->nTotalArgs, sizeof(argument_type));
    job->argSizes  = calloc(job->nTotalArgs, sizeof(int));
    job->arguments = calloc(job->nTotalArgs, sizeof(void *));
    MPI_Unpack(receivedData, dataSize, &pos, job->argTypes,  job->nTotalArgs, MPI_INT,            MPI_COMM_WORLD);
    MPI_Unpack(receivedData, dataSize, &pos, job->argSizes,  job->nTotalArgs, MPI_INT,            MPI_COMM_WORLD);

    for (cl_uint i = 0; i < job->nTotalArgs; i++) {
      if ((job->argTypes[i] != OUTPUT) &&
          (job->argTypes[i] != EMPTY_BUFFER)) {
        job->arguments[i] = malloc(job->argSizes[i]);
        MPI_Unpack(receivedData, dataSize, &pos, job->arguments[i], job->argSizes[i], MPI_BYTE,    MPI_COMM_WORLD);
      }
    }

    MPI_Unpack(receivedData, dataSize, &pos, &(job->returnTo),             1, MPI_INT,           MPI_COMM_WORLD);

    free(receivedData);

    /*** Enqueue the job on the queue ***/
    //TODO: Other dispatching policies
    FIFOenqueue(job);
  }
}

void cleanupJob(Job *job) {
  free(job->preferPUs);
  free(job->allowPUs);
  free(job->avoidPUs);
  free(job->forbidPUs);
  free(job->startingKernel);
  free(job->taskSource);
  free(job->argTypes);
  free(job->argSizes);
  for (int i = 0; i < job->nTotalArgs; i++)
    free(job->arguments[i]);
  free(job->arguments);
  free(job);
}

void *jobDispatcher() {

  while(true) {
    Job *job = dequeue();
    JobToPUM jobToSend;

    ////Scheduling algorithms may need this information, so we will fill it out beforehand////
    jobToSend.probID = job->probID;
    jobToSend.jobID = job->jobID;
    //jobToSend.runOn; //will be chosen by the selectPUM function

    jobToSend.category = job->category;

    jobToSend.startingNameSize = job->startingNameSize;
    jobToSend.startingKernel = job->startingKernel;
    jobToSend.taskSourceSize = job->taskSourceSize;
    jobToSend.taskSource = job->taskSource;
    jobToSend.nTotalArgs = job->nTotalArgs;
    jobToSend.nDimensions = job->nDimensions;
    if (jobToSend.nDimensions > 0) {
      for (unsigned int i = 0; i < job->nDimensions; i++) {
        jobToSend.nItems[i] = job->nItems[i];
        jobToSend.nGroupItems[i] = job->nGroupItems[i];
      }
    }
    jobToSend.argTypes = job->argTypes;
    jobToSend.argSizes = job->argSizes;
    jobToSend.arguments = job->arguments;
    jobToSend.returnTo = job->returnTo;
    PUMStruct *selectedPUM = selectPUM(job, &jobToSend);


    if (selectedPUM == NULL) { //No PUM is available to run this job. Return a failure value
      returnFailure(job);
      cleanupJob(job);
      continue;
    }


    sendJobToPUM(selectedPUM->id, &jobToSend);

    cleanupJob(job);

  }
}




/*
PUMStruct *pickPUWith (int nPossible, cl_device_type *possibleList, JobToPUM *jtp) {
  switch(SCHED_WORKLOAD_POLICY) {
    case SCHEDULING_RR:
      return pickPUWith_RR(nPossible, possibleList, jtp);
    case SCHEDULING_RR_THROUGHPUT:
      return pickPUWith_RR_Thrghpt(nPossible, possibleList, jtp);
  }
}

//TODO
//job, to base the decision on which PUManager to send the job to
//jtp, to save the PU in which the job shall be run
PUMStruct *pickPUWithout (int nNotPossible, cl_device_type *notPossibleList, JobToPUM *jtp) {
  return NULL;
}*/

//job, to base the decision on which PUManager to send the job to
//jtp, to save the PU in which the job shall be run
PUMStruct *selectPUM (Job *job, JobToPUM *jtp) {
  PUMStruct *result = NULL;

  if (job->required_is_set)
      result = pickPUWith(1, &job->requirePU, jtp);

  else {
    if (job->nPreferPUs > 0)
      result = pickPUWith(job->nPreferPUs, job->preferPUs, jtp);

    if (result == NULL && job->nAllowPUs > 0)
      result = pickPUWith(job->nAllowPUs, job->allowPUs, jtp);

    if (result == NULL && job->nAvoidPUs > 0)
      result = pickPUWith(job->nAvoidPUs, job->avoidPUs, jtp);

    if (result == NULL && job->nForbidPUs > 0)
      result = pickPUWithout(job->nForbidPUs, job->forbidPUs, jtp);
  }

  return result;
}


void sendJobToPUM (int toRank, JobToPUM *job) {

  cheetah_debug_print("Sending job to PU %i @ PU-M %i).", job->runOn, toRank);
  if (debug_JS)
    printJobToPUM(job);


  /*** Preparing data transfer ***/
  cl_uint totalArgumentsSize = 0;
  for (cl_uint i = 0; i < job->nTotalArgs; i++ )
    if ((job->argTypes[i] != OUTPUT) &&
        (job->argTypes[i] != EMPTY_BUFFER))
      totalArgumentsSize += job->argSizes[i];

  //Packing the struct to a contiguous memory zone
  int pos = 0;
  int sizeofData = sizeof(JobToPUM)
      + sizeof(char)*(job->startingNameSize)   //startingKernel
      + sizeof(char)*(job->taskSourceSize)     //taskSource
      + sizeof(argument_type)*(job->nTotalArgs)//argTypes
      + sizeof(int)*(job->nTotalArgs)          //argSizes
      + totalArgumentsSize;                    //arguments


  char *buff = malloc(sizeofData);
  MPI_Pack(&(job->probID),                     1,      MPI_INT,  buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(&(job->jobID),                      1,      MPI_INT,  buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(&(job->runOn),                      1,      MPI_INT,  buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(&(job->category),                   1,      MPI_INT,  buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(&(job->nDimensions),                1,      MPI_INT,  buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(job->nItems,                        3,      MPI_INT,  buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(job->nGroupItems,                   3,      MPI_INT,  buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(&(job->startingNameSize),           1,      MPI_INT,  buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(job->startingKernel, job->startingNameSize, MPI_CHAR, buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(&(job->taskSourceSize),             1,      MPI_INT,  buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(job->taskSource,  job->taskSourceSize,      MPI_CHAR, buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(&(job->nTotalArgs),                 1,      MPI_INT,  buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(job->argTypes,        job->nTotalArgs,      MPI_INT,  buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(job->argSizes,        job->nTotalArgs,      MPI_INT,  buff, sizeofData, &pos, MPI_COMM_WORLD);

  for (cl_uint i = 0; i < job->nTotalArgs; i++)
    if ((job->argTypes[i] != OUTPUT) &&
        (job->argTypes[i] != EMPTY_BUFFER))
      MPI_Pack(job->arguments[i], job->argSizes[i],    MPI_BYTE, buff, sizeofData, &pos, MPI_COMM_WORLD);

  MPI_Pack(&(job->returnTo),                     1,    MPI_INT,  buff, sizeofData, &pos, MPI_COMM_WORLD);

  cheetah_debug_print("When seding job to PUM, the returnTo argument is: %i", job->returnTo);

//  printf("JS  (%i): Job size: %i bytes.\n", myid, pos);
  /*** Sending it ***/
  MPI_Isend(&pos, 1, MPI_INT,    toRank, COMM_TAG_JOBSEND, MPI_COMM_WORLD, NULL_REQUEST);
  //sendMsg(&pos,   1, MPI_INT,    toRank, COMM_TAG_JOBSEND, MPI_STATUS_IGNORE);
  sendMsg(buff, pos, MPI_PACKED, toRank, COMM_TAG_JOBSEND, MPI_STATUS_IGNORE);

  free(buff);
  cheetah_debug_print("Sent job to PU-M at %i (%i bytes).\n", toRank, pos);


}


void returnFailure (Job *job) {

  JobResults jobResults;
  jobResults.probID = job->probID;
  jobResults.jobID = job->jobID;
  jobResults.nTotalResults = 0;
  jobResults.returnStatus = JOB_RETURN_STATUS_FAILURE;


  cheetah_info_print_error("Sending failure result to RC at %i", job->returnTo);


  /*** Preparing the data transfer ***/
  cl_uint totalResultsSize = 0;

  int pos = 0;
  int sizeofData = sizeof(int)*3
      + sizeof(int)*jobResults.nTotalResults
      + totalResultsSize //0
      + sizeof(int);

  char *buff = malloc(sizeofData);

  MPI_Pack(&(jobResults.probID),                            1, MPI_INT, buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(&(jobResults.jobID),                             1, MPI_INT, buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(&(jobResults.nTotalResults),                     1, MPI_INT, buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(jobResults.resultSizes,   jobResults.nTotalResults, MPI_INT, buff, sizeofData, &pos, MPI_COMM_WORLD);

  for (cl_uint i = 0; i < jobResults.nTotalResults; i++ )
    MPI_Pack(jobResults.results[i], jobResults.resultSizes[i], MPI_BYTE,buff, sizeofData, &pos, MPI_COMM_WORLD);

  MPI_Pack(&(jobResults.returnStatus),                      1, MPI_INT, buff, sizeofData, &pos, MPI_COMM_WORLD);

  /*** Sending ***/
  cheetah_debug_print_error("Results' packed size: %i, returning to: %i", pos, job->returnTo);
  //MPI_Isend(&pos,   1, MPI_INT,    job->returnTo, COMM_TAG_RESULTSEND, MPI_COMM_WORLD, &NULL_REQ);

  sendMsg(&pos, 1, MPI_INT, job->returnTo, COMM_TAG_RESULTSEND, MPI_STATUS_IGNORE);

  //MPI_Recv(NULL, 0, MPI_BYTE, job->returnTo, COMM_TAG_RESULTSEND, MPI_COMM_WORLD, &NULL_STATUS);
  receiveMsg(NULL, 0, MPI_BYTE, job->returnTo, COMM_TAG_RESULTSEND, MPI_STATUS_IGNORE);

  cheetah_debug_print_error("Sent return size. Sending packed result NOW.");

  sendMsg(buff, pos, MPI_PACKED, job->returnTo, COMM_TAG_RESULTSEND, MPI_STATUS_IGNORE);

  free(buff);

  cheetah_info_print_error("Sent packed result (failure).");


}




//TODO: support other kinds of shared data
void *PUStatusReceiver() {

  int dataSize = sizeof(finishedJobNotification);
  MPI_Status status;
  finishedJobNotification *fjn = malloc(sizeof(finishedJobNotification));
  char *recvb = malloc(dataSize);
  int pos;

  while (true) {
    pos = 0;
    receiveMsg(recvb, dataSize, MPI_PACKED, MPI_ANY_SOURCE, COMM_TAG_PUSTATUS, &status);

    /*recebe:
     *      categoria do job acabado
     *      PUi respectiva
     *      tempo de execução do job
     *      tempo de transmissão dos resultados do job //TODO
     */
    MPI_Unpack(recvb, dataSize, &pos, &(fjn->category),      1, MPI_INT, MPI_COMM_WORLD);
    MPI_Unpack(recvb, dataSize, &pos, &(fjn->ranAtPU),       1, MPI_INT, MPI_COMM_WORLD);
    MPI_Unpack(recvb, dataSize, &pos, &(fjn->succeeded),     1, MPI_BYTE, MPI_COMM_WORLD);
    MPI_Unpack(recvb, dataSize, &pos, &(fjn->overheads),     1, MPI_DOUBLE, MPI_COMM_WORLD);
    MPI_Unpack(recvb, dataSize, &pos, &(fjn->executionTime), 1, MPI_DOUBLE, MPI_COMM_WORLD);

    treatFinishNotification(fjn, status.MPI_SOURCE);

  }

  free(fjn);

  return NULL;
}


