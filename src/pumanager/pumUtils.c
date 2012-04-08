/*
 * pumUtils.c
 *
 *  Created on: 9 Apr 2010
 *      Author: luis
 */


#include "pum.h"
#include <assert.h>
#include <math.h>
#include <alloca.h>


//Array of mutexes and conditions. Each entry is the responsibility of the respective PU
// - also corresponds to the index at PUInfoStruct.availablePUs
pthread_mutex_t *JQueMutex;
pthread_cond_t  *condition_cond;

struct JQueueElem **JQueueHead;
struct JQueueElem **JQueueTail;

pthread_mutex_t sendResultMutex = PTHREAD_MUTEX_INITIALIZER;


bool JMSetup () {
  //TODO: Clean/free these up on program termination
  JQueMutex      = calloc(PUInfoStruct.nPUs, sizeof(pthread_mutex_t));
  condition_cond = calloc(PUInfoStruct.nPUs, sizeof(pthread_cond_t));
  JQueueHead     = calloc(PUInfoStruct.nPUs, sizeof(struct JQueueElem *));
  JQueueTail     = calloc(PUInfoStruct.nPUs, sizeof(struct JQueueElem *));

  for (int i=0; i < PUInfoStruct.nPUs; i++) {
    if (pthread_mutex_init(&(JQueMutex[i]), NULL) != 0)
      fprintf(stderr, "PUM (%i): PROGRAMMING ERROR: INITIALIZING MUTEX %i.\n", myid, i);
    if(pthread_cond_init(&(condition_cond[i]), NULL) != 0)
      fprintf(stderr, "PUM (%i): PROGRAMMING ERROR: INITIALIZING COND %i.\n", myid, i);
    JQueueHead[i] = NULL;
    JQueueTail[i] = NULL;
  }

  int pos = 0;
  int sizeofData = sizeof(int)*2 + PUInfoStruct.nPUs*sizeof(PUProperties);


  char *buff = malloc(sizeofData);
  MPI_Pack(&(PUInfoStruct.id),                        1, MPI_INT,    buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(&(PUInfoStruct.nPUs),                      1, MPI_INT,    buff, sizeofData, &pos, MPI_COMM_WORLD);

  for (int i = 0; i < PUInfoStruct.nPUs; i++) {
    MPI_Pack(&(PUInfoStruct.availablePUs[i].device_type),              1, MPI_UNSIGNED_LONG, buff, sizeofData, &pos, MPI_COMM_WORLD);
    MPI_Pack(&(PUInfoStruct.availablePUs[i].clock_frequency),          1, MPI_UNSIGNED,      buff, sizeofData, &pos, MPI_COMM_WORLD);
    MPI_Pack(&(PUInfoStruct.availablePUs[i].max_memory_alloc),         1, MPI_UNSIGNED_LONG, buff, sizeofData, &pos, MPI_COMM_WORLD);
    MPI_Pack(&(PUInfoStruct.availablePUs[i].cache_type),               1, MPI_UNSIGNED,      buff, sizeofData, &pos, MPI_COMM_WORLD);
    MPI_Pack(&(PUInfoStruct.availablePUs[i].global_mem_size),          1, MPI_UNSIGNED_LONG, buff, sizeofData, &pos, MPI_COMM_WORLD);
    MPI_Pack(&(PUInfoStruct.availablePUs[i].local_mem_type),           1, MPI_UNSIGNED,      buff, sizeofData, &pos, MPI_COMM_WORLD);
    MPI_Pack(&(PUInfoStruct.availablePUs[i].local_mem_size),           1, MPI_UNSIGNED_LONG, buff, sizeofData, &pos, MPI_COMM_WORLD);
    MPI_Pack(&(PUInfoStruct.availablePUs[i].max_compute_units),        1, MPI_UNSIGNED,      buff, sizeofData, &pos, MPI_COMM_WORLD);
    MPI_Pack(&(PUInfoStruct.availablePUs[i].max_work_item_dimensions), 1, MPI_UNSIGNED,      buff, sizeofData, &pos, MPI_COMM_WORLD);
    MPI_Pack(&(PUInfoStruct.availablePUs[i].max_work_item_sizes),      3, MPI_LONG,          buff, sizeofData, &pos, MPI_COMM_WORLD);
    MPI_Pack(&(PUInfoStruct.availablePUs[i].max_work_group_size),      1, MPI_LONG,          buff, sizeofData, &pos, MPI_COMM_WORLD);
    MPI_Pack(&(PUInfoStruct.availablePUs[i].max_parameter_size),       1, MPI_LONG,          buff, sizeofData, &pos, MPI_COMM_WORLD);
    MPI_Pack(&(PUInfoStruct.availablePUs[i].latency),                  1, MPI_DOUBLE,        buff, sizeofData, &pos, MPI_COMM_WORLD);
    MPI_Pack(&(PUInfoStruct.availablePUs[i].throughput),               1, MPI_DOUBLE,        buff, sizeofData, &pos, MPI_COMM_WORLD);
    MPI_Pack(&(PUInfoStruct.availablePUs[i].bandwidth),               1, MPI_DOUBLE,        buff, sizeofData, &pos, MPI_COMM_WORLD);
  }


  /*** Telling the JobScheduler that I am online ***/
  MPI_Isend(&pos, 1, MPI_INT, defaultSchedID, COMM_TAG_PUONLINE, MPI_COMM_WORLD, NULL_REQUEST);

  if (debug_PUM)
    cheetah_debug_print("Sending my packed InfoStruct\n");

  int gotmsg = false;
  MPI_Test(&shutdown_request, &gotmsg, MPI_STATUS_IGNORE);
  if (gotmsg) {
    return false;
  }

  sendMsg(buff, pos, MPI_PACKED, defaultSchedID, COMM_TAG_PUONLINE, MPI_STATUS_IGNORE);

  /*** Latency test answer ***/
  for (int i = 0; i < NUM_JS_LATENCY_TESTS; i++) {
    receiveMsg(NULL, 0, MPI_BYTE, defaultSchedID, COMM_TAG_PUONLINE, MPI_STATUS_IGNORE);
    MPI_Isend (NULL, 0, MPI_BYTE, defaultSchedID, COMM_TAG_PUONLINE, MPI_COMM_WORLD, NULL_REQUEST);
  }

  free(buff);

  cheetah_info_print("Answered latency test.\n");

  MPI_Test(&shutdown_request, &gotmsg, MPI_STATUS_IGNORE);
  if (gotmsg) {
    return false;
  }

  /*** Throughput test answer ***/
  char buffLat[PUM_THROUGHPUT_TEST_SIZE / sizeof(char)];

  receiveMsg(buffLat, PUM_THROUGHPUT_TEST_SIZE, MPI_BYTE, defaultSchedID, COMM_TAG_PUONLINE, MPI_STATUS_IGNORE);
  sendMsg(NULL, 0, MPI_BYTE, defaultSchedID, COMM_TAG_PUONLINE, MPI_STATUS_IGNORE);

  cheetah_info_print("Answered throughput test. registered with Job Scheduler.\n");

  //Now we should be able to receive test jobs, run them and return results
  return true;

}

bool enqueue (JobToPUM *job) {
  int devID = job->runOn;
  if (devID >= PUInfoStruct.nPUs)
    return false;

  struct JQueueElem *newElem = malloc(sizeof(struct JQueueElem));

  pthread_mutex_lock(&(JQueMutex[devID]));

  newElem->next = NULL;
  newElem->job = job;

  if (JQueueHead[devID] == NULL) { //the queue is empty
    assert(JQueueTail[devID] == NULL);
    JQueueHead[devID] = newElem;
  }
  else {
    assert(JQueueTail[devID] != NULL && JQueueTail[devID]->next == NULL);
    JQueueTail[devID]->next = newElem;
  }

  JQueueTail[devID] = newElem;

  pthread_cond_signal(&(condition_cond[devID]));
  pthread_mutex_unlock(&(JQueMutex[devID]));

  return true;
}

JobToPUM *dequeue (int devID) {
  JobToPUM *result;
  struct JQueueElem *oldHead;

  pthread_mutex_lock(&(JQueMutex[devID]));
  while (JQueueHead[devID] == NULL) {//if the queue is empty, wait until a job is added
    assert(JQueueTail[devID] == NULL);
    pthread_cond_wait(&(condition_cond[devID]), &(JQueMutex[devID]));
  }

  result = JQueueHead[devID]->job;

  oldHead = JQueueHead[devID];
  JQueueHead[devID] = JQueueHead[devID]->next;

  if (JQueueHead[devID] == NULL) {//if this was the only element on the list, delete the reference to the tail
    assert(JQueueTail[devID] == oldHead);
    JQueueTail[devID] = NULL;
  }
  else
    assert(JQueueTail[devID] != NULL);

  free(oldHead);

  pthread_mutex_unlock(&(JQueMutex[devID]));
  return result;
}


/*
 * A producer thread.
 * This will receive jobs and add them to their respective queues
 */
void *receiveJobs() {
  int pos,
      dataSize;


  JobToPUM *job;

  printf("PUM (%i): Job receiver started.\n", myid);

  while (true) {
    job = malloc(sizeof(JobToPUM));
    pos = 0;

    dataSize = 0;


    /*** Receive a computation request ***/
    receiveMsg(&dataSize, 1, MPI_INT, defaultSchedID, COMM_TAG_JOBSEND, MPI_STATUS_IGNORE);

    char *receivedData = malloc(dataSize);
    assert (receivedData != NULL);

    //TODO: this could be treated by a new thread
    receiveMsg(receivedData, dataSize, MPI_PACKED, defaultSchedID, COMM_TAG_JOBSEND, MPI_STATUS_IGNORE);


    if (debug_PUM)
      fprintf(stderr, "PUM (%i):  THIS IS THE DATASIZE: %i\n", myid, dataSize);
    MPI_Unpack(receivedData, dataSize, &pos, &(job->probID),                          1, MPI_INT, MPI_COMM_WORLD);
    if (debug_PUM)
      fprintf(stderr, "PUM (%i):  unpacked probID\n", myid);

    MPI_Unpack(receivedData, dataSize, &pos, &(job->jobID),                           1, MPI_INT, MPI_COMM_WORLD);
    MPI_Unpack(receivedData, dataSize, &pos, &(job->runOn),                           1, MPI_INT, MPI_COMM_WORLD);
    MPI_Unpack(receivedData, dataSize, &pos, &(job->category),                        1, MPI_INT, MPI_COMM_WORLD);
    MPI_Unpack(receivedData, dataSize, &pos, &(job->nDimensions),                     1, MPI_INT, MPI_COMM_WORLD);
    MPI_Unpack(receivedData, dataSize, &pos, job->nItems,                             3, MPI_INT, MPI_COMM_WORLD);
    MPI_Unpack(receivedData, dataSize, &pos, job->nGroupItems,                        3, MPI_INT, MPI_COMM_WORLD);
    MPI_Unpack(receivedData, dataSize, &pos, &(job->startingNameSize),                1, MPI_INT, MPI_COMM_WORLD);
    if (debug_PUM)
      fprintf(stderr, "PUM (%i):  unpacked ints\n", myid);

    job->startingKernel = calloc(job->startingNameSize, sizeof(char));
    MPI_Unpack(receivedData, dataSize, &pos, job->startingKernel, job->startingNameSize, MPI_CHAR, MPI_COMM_WORLD);
    MPI_Unpack(receivedData, dataSize, &pos, &(job->taskSourceSize),                  1, MPI_INT,  MPI_COMM_WORLD);
    if (debug_PUM)
      fprintf(stderr, "PUM (%i):  unpacked name %s\n", myid, job->startingKernel);

    job->taskSource = calloc(job->taskSourceSize, sizeof(char));
    MPI_Unpack(receivedData, dataSize, &pos, job->taskSource,       job->taskSourceSize, MPI_CHAR, MPI_COMM_WORLD);
    MPI_Unpack(receivedData, dataSize, &pos, &(job->nTotalArgs),                      1, MPI_INT,  MPI_COMM_WORLD);
    if (debug_PUM)
      fprintf(stderr, "PUM (%i):  unpacked source, size (%i) & ntotalArgs %s \n \n%i\n", myid, job->taskSourceSize, job->taskSource, job->nTotalArgs);

    job->argTypes = calloc(job->nTotalArgs, sizeof(argument_type));
    job->argSizes = calloc(job->nTotalArgs, sizeof(int));
    MPI_Unpack(receivedData, dataSize, &pos, job->argTypes,             job->nTotalArgs, MPI_INT,  MPI_COMM_WORLD);
    MPI_Unpack(receivedData, dataSize, &pos, job->argSizes,             job->nTotalArgs, MPI_INT,  MPI_COMM_WORLD);
    if (debug_PUM)
      fprintf(stderr,"PUM (%i):  unpacked argtypes & sizes\n", myid);

    job->arguments = calloc(job->nTotalArgs, sizeof (void *));
    for (cl_uint i = 0; i < job->nTotalArgs; i++) {

      if (job->argTypes[i] != EMPTY_BUFFER)
        job->arguments[i] = malloc(job->argSizes[i]);

      if ((job->argTypes[i] == INPUT) ||
          (job->argTypes[i] == INPUT_OUTPUT)) {
        MPI_Unpack(receivedData, dataSize, &pos, job->arguments[i],      job->argSizes[i], MPI_BYTE,          MPI_COMM_WORLD);
      }
    }
    if (debug_PUM)
      fprintf(stderr,"PUM (%i):  unpacked arguments\n", myid);

    MPI_Unpack(receivedData, dataSize, &pos, &(job->returnTo),                        1, MPI_INT,           MPI_COMM_WORLD);

    if (debug_PUM) {
      fprintf(stderr,"PUM (%i):  Received job %i (prob: %i). All seems ok. (later will return it to %i)\n", myid, job->jobID, job->probID, job->returnTo);
      printJobToPUM(job);
      fprintf(stderr,"PUM (%i):  ENDED PRINTING\n", myid);
    }


    free(receivedData);

    /*** Enqueue the job on its respective queue ***
     If the queue is full, return failure result ***/

    if (enqueue(job) == false) {
      fprintf(stderr, "PUM (%i):  PROGRAMMING ERROR (on the scheduler's side): Received a job for an unsupported PU (%ui). I only have %i PUs. The JS should have prevented this.\n", myid, job->runOn, PUInfoStruct.nPUs);

      jobNResult jnr;
      jnr.job = job;
      jnr.res = JOB_RETURN_STATUS_FAILURE;
      resultPrepareAndSend(&jnr);
    }
  }
}


/*
 * A consumer thread.
 * This will wait until the queue has a job and run it
 */
void *JobConsumer (void *device) {
  JobToPUM *job;
  int devID = *((int *) device);

  jobNResult jnr;

  struct timeval start, end, result,
                 overheadstart, overheadend, overheadresult; //for capturing job execution times
  finishedJobNotification fjn;

  //  cl_device_type dev = *((cl_device_type *)device);

  //TODO: Does doing this only once have any consequences? (Can we reuse a command queue *safely*? What if the device is left on an "inconsistent state"?)
  //prepare OpenCL
/*  if (!initializeCLExec(dev)) {
    fprintf(stderr,"PUM (%i):  FAILED INITIALIZING %s DEVICE\n", myid, dev == CL_DEVICE_TYPE_CPU ? "CPU" : "GPU");
    return NULL;
  }*/

  printf("PUM (%i): Job consumer started\n", myid);

  while (true) {
    //wait until the queue has a job
    job = dequeue(devID);

    jnr.job = job;
    fjn.category = job->category;
    fjn.ranAtPU = job->runOn;
    fjn.executionTime = 0.0;
    fjn.succeeded = false;
    if (gettimeofday(&overheadstart, NULL)) {
      perror("gettimeofday");
      exit (EXIT_FAILURE);
    }

    if (!initializeCLBuffers(job)) {
      fprintf(stderr,"PUM (%i):  FAILED INITIALIZING BUFFERS\n", myid);

      fjn.executionTime = DBL_MAX ;
      sendFinishedJobNotification(&fjn);


      jnr.res = JOB_RETURN_STATUS_FAILURE;
      resultPrepareAndSend(&jnr);
      /*
      pthread_t t;
      if (pthread_create (&t, NULL, sendResults, &jnr)) {
        perror("pthread_create");
        MPI_Finalize();
        exit(EXIT_FAILURE);
      }*/
      continue;
    }

    if (debug_PUM) {
      printf("PUM (%i):  Arguments BEFORE execution:\n", myid);
      for (cl_uint i = 0; i < job->nTotalArgs; i++) {
        printf("arg %i: ", i);
        if (job->argTypes[i] == EMPTY_BUFFER)
          printf("EMPTY BUFFER.\n");
        else
          print1DArray(job->arguments[i], job->argSizes[i] / sizeof(unsigned int));
      }
    }
    if (gettimeofday(&overheadend, NULL)) {
      perror("gettimeofday");
      return false;
    }

    start = overheadend;
    if (!execJob(job)) {
      fprintf(stderr,"PUM (%i):  FAILED RUNNING JOB\n", myid);

      fjn.executionTime = DBL_MAX ;
      sendFinishedJobNotification(&fjn);

      jnr.res = JOB_RETURN_STATUS_FAILURE;
      fprintf(stderr,"JOB FAIL NOTIFICATION SENT TO JS. Sending to RC as well.\n");
      resultPrepareAndSend(&jnr);
      continue;
    }
    if (gettimeofday(&end, NULL)) {
      perror("gettimeofday");
      return false;
    }

    if (debug_PUM) {
      printf("PUM (%i):  Arguments AFTER execution:\n", myid);
      for (cl_uint i = 0; i < job->nTotalArgs; i++) {
        printf("arg %i: ", i);
        if (job->argTypes[i] == EMPTY_BUFFER)
          printf("EMPTY BUFFER.\n");
        else
          print1DArray(job->arguments[i], job->argSizes[i] / sizeof(unsigned int));
      }
    }

    if (!SILENT)
      printf("PUM (%i):  Finished running job (%s).\n", myid, job->startingKernel);
//    if (debug_PUM)
//      printJobToPUM(job);


    jnr.res = JOB_RETURN_STATUS_SUCCESS;

//    sendResults(&jnr);//TODO: include this on a file-transfer metric for the scheduler?

    timeval_subtract(&result, &end, &start);
    fjn.executionTime = result.tv_sec + (result.tv_usec / 1000000.0);
    timeval_subtract(&overheadresult, &overheadend, &overheadstart);
    fjn.overheads = overheadresult.tv_sec + (overheadresult.tv_usec / 1000000.0);
    fjn.succeeded = jnr.res;
    sendFinishedJobNotification(&fjn);


    resultPrepareAndSend(&jnr);
  }
}

void sendFinishedJobNotification (finishedJobNotification *fjn) {

  char *buff = malloc(sizeof(finishedJobNotification));
  int pos = 0;

  MPI_Pack(&(fjn->category),      1, MPI_INT,    buff, sizeof(finishedJobNotification), &pos, MPI_COMM_WORLD);
  MPI_Pack(&(fjn->ranAtPU),       1, MPI_INT,    buff, sizeof(finishedJobNotification), &pos, MPI_COMM_WORLD);
  MPI_Pack(&(fjn->succeeded),     1, MPI_BYTE,   buff, sizeof(finishedJobNotification), &pos, MPI_COMM_WORLD);
  MPI_Pack(&(fjn->overheads),     1, MPI_DOUBLE, buff, sizeof(finishedJobNotification), &pos, MPI_COMM_WORLD);
  MPI_Pack(&(fjn->executionTime), 1, MPI_DOUBLE, buff, sizeof(finishedJobNotification), &pos, MPI_COMM_WORLD);

  sendMsg(buff, pos, MPI_PACKED, defaultSchedID, COMM_TAG_PUSTATUS, MPI_STATUS_IGNORE);

  free(buff);
}

/////////////////////////////////////////////////////////////////
// Create OpenCL memory buffers
/////////////////////////////////////////////////////////////////
bool initializeCLBuffers (JobToPUM *job) {
  cl_int status = CL_SUCCESS;

  int devID = job->runOn;
  cl_context *context = PUInfoStruct.PUsContexts[devID];

  cl_device_id device;// = calloc(1, sizeof(cl_device_id));

  size_t deviceListSize;
  status = clGetContextInfo(*context, CL_CONTEXT_DEVICES, 0, NULL, &deviceListSize);
  if (status != CL_SUCCESS) {
    fprintf(stderr, "clGetContextInfo failed (%i).\n", status);
    return false;
  }

  //Get the corresponding device (TODO: currently only one)
  status = clGetContextInfo(*context, CL_CONTEXT_DEVICES, sizeof(cl_device_id), &device, NULL);
  if (status != CL_SUCCESS) {
    fprintf(stderr, "clGetContextInfo failed (%i).\n", status);
    return false;
  }

  PUInfoStruct.argBuffers[devID] = calloc(job->nTotalArgs, sizeof(cl_mem));


  //TODO: support other memory locations
  for (cl_uint i = 0; i < job->nTotalArgs; i++) {
    /* Create buffers */
    PUInfoStruct.argBuffers[devID][i] = clCreateBuffer(*context, CL_MEM_READ_WRITE, job->argSizes[i], 0, &status);
    if (status != CL_SUCCESS) {
      fprintf(stderr,"clCreateBuffer failed. (inputBuffers[%i]))\n",i);
      return false;
    }

    if ((job->argTypes[i] == INPUT) ||
        (job->argTypes[i] == INPUT_OUTPUT)) {
      if (job->probID == myid && job->jobID == 2){
        for (int j = 0; j < 30; j++) {
          status = clEnqueueWriteBuffer(*(PUInfoStruct.PUsCmdQs[devID]), PUInfoStruct.argBuffers[devID][i], CL_TRUE, 0, job->argSizes[i], job->arguments[i], 0, 0, 0);
          if (status != CL_SUCCESS) {
            fprintf(stderr,"clEnqueueWriteBuffer failed. (inputBuffers[%i])\n",i);
            return false;
          }
        }
      }
      else {
        status = clEnqueueWriteBuffer(*(PUInfoStruct.PUsCmdQs[devID]), PUInfoStruct.argBuffers[devID][i], CL_TRUE, 0, job->argSizes[i], job->arguments[i], 0, 0, 0);
        if (status != CL_SUCCESS) {
          fprintf(stderr,"clEnqueueWriteBuffer failed. (inputBuffers[%i])\n",i);
          return false;
        }
      }
    }
  }

  /////////////////////////////////////////////////////////////////
  // Load CL source, build CL program object, create CL kernel object
  /////////////////////////////////////////////////////////////////
  /* create a CL program using the kernel source */
  cl_program program;// = malloc(sizeof(cl_program));
//  PUInfoStruct.currKernels[devID] = malloc(sizeof(cl_kernel));

  const char *sourceStr = job->taskSource;
  if (debug_PUM)
    fprintf(stderr, "TRYING (%i)\n", job->taskSourceSize);
  if (sourceStr == NULL) {
    fprintf(stderr, "initializeCLBuffers srcStr is NULL\n");
    return false;
  }
  size_t sourceSize[] = { strlen(sourceStr) };
  assert(sourceSize[0] == job->taskSourceSize-1);

  program = clCreateProgramWithSource(*context, 1, &sourceStr, sourceSize, &status);
  if (status != CL_SUCCESS) {
    fprintf(stderr, "clCreateProgramWithSource failed.\n");
    return false;
  }

  /* create a cl program executable for all the devices specified */
  //currently only one device supported
  status = clBuildProgram(program, 1, &device, NULL, NULL, NULL);

  size_t ret_val_size;
  clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &ret_val_size);


  char *build_log = calloc(ret_val_size+1, sizeof(char));
  clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, ret_val_size, build_log, NULL);

  build_log[ret_val_size] = '\0';

  if (!SILENT){
    if (strlen(build_log) > 0) {
      fprintf(stderr, "PUM (%i): buildlog:\n>>>\n%s\n<<<\n__END OF BUILDLOG__\n",myid, build_log);
    } else {
      fprintf(stderr, "PUM (%i): NO BUILD LOG\n", myid);
    }
  }
  free(build_log);

  if (status != CL_SUCCESS) {
    fprintf(stderr, "clBuildProgram failed (%i).\n", status);
    return false;
  }

  /* get a kernel object handle for a kernel with the given name */
  PUInfoStruct.currKernels[devID] = clCreateKernel(program, job->startingKernel, &status);
  if (status != CL_SUCCESS) {
    fprintf(stderr, "clCreateKernel failed.\n");
    return false;
  }

//  free (device);
//  free (program); //TODO: check if this should be persistent

  return true;

}



//TODO: make this code more readable
bool execJob (JobToPUM *job) {

  int devID = job->runOn;
  cl_int status;
  int nOutputBuffers = 0;

  for (int i = 0; i<job->nTotalArgs; i++) {
    if ((job->argTypes[i] == OUTPUT) ||
        (job->argTypes[i] == INPUT_OUTPUT))
      nOutputBuffers++;
  }
  cl_event events[1+nOutputBuffers];

  //TODO: this should be dynamic and depend on the device capabilities & job requirements
  size_t *globalItems = NULL;
  size_t *localItems = NULL;
  int nDim;

  if (job->nDimensions > 0) {
    nDim = job->nDimensions;
    globalItems = calloc(nDim, sizeof(size_t));
    localItems = calloc(nDim, sizeof(size_t));

    for (cl_uint i = 0; i < nDim; i++) {
      globalItems[i] = job->nItems[i];
      localItems[i] = job->nGroupItems[i];
    }
  } else { // job->nDimensions <= 0
    nDim = 1;
    //TODO: isto serão os melhores defaults? ...ou deixar a cargo da implementação?
    fprintf(stderr, "Going to select max global threads: %lu\n", PUInfoStruct.availablePUs[devID].max_work_item_sizes[0]);
    globalItems = calloc(nDim, sizeof(size_t));
    globalItems[0] = PUInfoStruct.availablePUs[devID].max_work_item_sizes[0];
  }

  /* kernel input arguments */
  for (int i = 0; i < job->nTotalArgs; i++ ) {
    status = clSetKernelArg(PUInfoStruct.currKernels[devID], i, sizeof(cl_mem), &(PUInfoStruct.argBuffers[devID][i]));
    if (status != CL_SUCCESS) {
      fprintf(stderr, "clSetKernelArg failed. (inputBuffers[%i])\n", i);
      return false;
    }
  }

  if (!SILENT){
    fprintf(stderr,"ENQUEUEING KERNEL. DIMS: %i.\n", nDim);
    for (cl_uint i = 0; i < nDim; i++) {
      fprintf(stderr, "global items[%u] = %li\n", i, globalItems[i]);
      if (localItems != NULL)
        fprintf(stderr, "group items[%u] = %li\n", i, localItems[i]);
    }
  }
  if (debug_PUM)
    fprintf(stderr, "Enqueueing Kernel!\n");

  status = clEnqueueNDRangeKernel(*(PUInfoStruct.PUsCmdQs[devID]), PUInfoStruct.currKernels[devID], nDim, NULL, globalItems, localItems, 0, NULL, &events[0]);
  free(globalItems);
  free(localItems);

  if (status != CL_SUCCESS) {
    fprintf(stderr, "clEnqueueNDRangeKernel failed (%i).\n", status);
    return false;
  }

  /* wait for the kernel call to finish execution */
  status = clWaitForEvents(1, &events[0]);
  if (status != CL_SUCCESS) {
    fprintf(stderr, "PUM (%i): clWaitForEvents failed for a running kernel! Error value: %s.\n", myid,
                    status == CL_INVALID_VALUE ? "CL_INVALID_VALUE" :
                    status == CL_INVALID_CONTEXT ? "CL_INVALID_CONTEXT" : 
                    status == CL_INVALID_EVENT ? "CL_INVALID_EVENT" : 
                    status == CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST ? "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST" :
                    status == CL_OUT_OF_RESOURCES ? "CL_OUT_OF_RESOURCES" :
                    status == CL_OUT_OF_HOST_MEMORY ? "CL_OUT_OF_HOST_MEMORY" :
                    "Something unexpected. Weird");
    return false;
  }
  cl_int retval;//TODO: send this retval back to the JM...
  status = clGetEventInfo(events[0], CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int),&retval, NULL);
  if (status != CL_SUCCESS) {
    fprintf(stderr,"clGetEventInfo failed.\n");
    return false;
  }
  if (retval != CL_COMPLETE) {
    fprintf(stderr,"PUM (%i): Kernel did not complete! (PUM programming error?) Error value: %s.\n", myid,
                   retval == CL_QUEUED ? "CL_QUEUED" :
                   retval == CL_SUBMITTED ? "CL_SUBMITTED" :
                   retval == CL_RUNNING ? "CL_RUNNING":
                   "Something else");
    if (retval < 0  || retval != CL_QUEUED || retval != CL_SUBMITTED || retval != CL_RUNNING)
      fprintf(stderr, "PUM (%i) : Error value: %i\n", myid, retval);
    return false;
  }


  if (debug_PUM)
    fprintf(stderr, "Kernel completed!\n");


  /* Read output data */
  int nDetectedOutputs = 0;
  for (int i = 0; i<job->nTotalArgs; i++ ) {
    //read only the arguments that are marked as OUTPUT (or INPUT_OUTPUT)
    if ((job->argTypes[i] == OUTPUT) ||
        (job->argTypes[i] == INPUT_OUTPUT)) {
      status = clEnqueueReadBuffer(*(PUInfoStruct.PUsCmdQs[devID]), PUInfoStruct.argBuffers[devID][i], CL_TRUE, 0, job->argSizes[i], job->arguments[i], 0, NULL, &events[1+nDetectedOutputs]);

      nDetectedOutputs++;
      if(status != CL_SUCCESS) {
        fprintf(stderr,"Error: clEnqueueReadBuffer failed at argument %i. (clEnqueueReadBuffer)\n", i);
        return false;
      }
    }
  }
  if (debug_PUM)
    fprintf(stderr, "ReadBuffers enqueued, nOutputBuffs: %i\n", nOutputBuffers);

  /* Wait for the read buffer to finish execution */
  for (int i = 0; i < nOutputBuffers; i++) {
    if (debug_PUM)
      fprintf(stderr, "Waiting for ReadBuffer event to complete %i\n", i+1);
    status |= clWaitForEvents(1, &events[i+1]);
  }

  if (status != CL_SUCCESS) {
    fprintf(stderr, "clWaitForEvents failed.\n");
    return false;
  }
  if (debug_PUM)
    fprintf(stderr, "Finished waited for events.\n");

  for (int i = 0; i < nOutputBuffers+1; i++ ) {
    status = clReleaseEvent(events[i]);
    if (status != CL_SUCCESS) {
      fprintf(stderr, "clReleaseEvent failed. (events[%i])\n", i);
      return false;
    }
  }

  status = clReleaseKernel(PUInfoStruct.currKernels[devID]);
  if (status != CL_SUCCESS) {
    fprintf(stderr,"clReleaseKernel failed.\n");
    return false;
  }
  //free(PUInfoStruct.currKernels[devID]);//this is freed automatically by the OpenCL runtime

  for (int i = 0; i < job->nTotalArgs; i++) {
    status = clReleaseMemObject(PUInfoStruct.argBuffers[devID][i]);
    if (status != CL_SUCCESS) {
      fprintf(stderr,"clReleaseMemObject failed.\n");
      return false;
    }
    //free(PUInfoStruct.argBuffers[devID][i]);//this is freed automatically by the OpenCL runtime
  }
  free(PUInfoStruct.argBuffers[devID]);
  if (debug_PUM)
    fprintf(stderr, "ReadBuffer events released\n");

  return true;
}



void resultPrepareAndSend (jobNResult *jnrMayDisappear) {
  //1. copies jnr to dynamic memory
  //2. launches a new thread to send the result
  //3. returns. Job will be sent whenever that thread gets CPU time
  jobNResult *jnr = malloc(sizeof (jobNResult));
  jnr->job = jnrMayDisappear->job;
  jnr->res = jnrMayDisappear->res;

  pthread_t t;
  if (pthread_create (&t, NULL, sendResults, jnr)) {
    perror("pthread_create");
    MPI_Finalize();
    exit(EXIT_FAILURE);
  }

}


/*
 * Returns the result of the job to a RC.
 * Only one result is send at a time, but we can have multiple results queueing to be sent
 * If retStatus == JOB_RETURN_STATUS_FAILURE, the result buffers' contents are unspecified
 */
void *sendResults (void *job_and_result) {
  jobNResult *jnr = (jobNResult *) job_and_result;

  JobToPUM *job = jnr->job;
  int retStatus = jnr->res;

  JobResults jobResults;
  jobResults.probID = job->probID;
  jobResults.jobID = job->jobID;
  jobResults.nTotalResults = 0;
  jobResults.returnStatus = retStatus;

  pthread_mutex_lock(&sendResultMutex); //because we could be sending error results and exec results simultaneously

  if (!SILENT)
    printf("PUM (%i):  Sending results to RC at %i\n", myid, job->returnTo);
  if (retStatus == JOB_RETURN_STATUS_SUCCESS) {
    for (cl_uint i = 0; i < job->nTotalArgs; i++) {
      if ((job->argTypes[i] == OUTPUT) ||
          (job->argTypes[i] == INPUT_OUTPUT))
        jobResults.nTotalResults++;
    }

    jobResults.resultSizes = calloc(jobResults.nTotalResults, sizeof(int));
    jobResults.results = calloc(jobResults.nTotalResults, sizeof(void *));
    int currResultIndex = 0;
    for (cl_uint i = 0; i < job->nTotalArgs; i++) {

      if ((job->argTypes[i] == OUTPUT) ||
          (job->argTypes[i] == INPUT_OUTPUT)) {
        jobResults.resultSizes[currResultIndex] = job->argSizes[i];
        jobResults.results[currResultIndex] = malloc(jobResults.resultSizes[currResultIndex]);
        jobResults.results[currResultIndex] = job->arguments[i];

        currResultIndex++;
      }
    }
  }


  /*** Preparing the data transfer ***/
  cl_uint totalResultsSize = 0;
  for (cl_uint i = 0; i<jobResults.nTotalResults; i++ )
    totalResultsSize += jobResults.resultSizes[i];

  int pos = 0;
  int sizeofData = sizeof(int)*3
      + sizeof(int)*jobResults.nTotalResults
      + totalResultsSize
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
  if (debug_PUM)
    fprintf(stderr, "PUM (%i):  Results' packed size: %i, returning to: %i\n", myid, pos, job->returnTo);


  //TODO: Test with Isend
//  MPI_Isend(&pos,   1, MPI_INT,    job->returnTo, COMM_TAG_RESULTSEND, MPI_COMM_WORLD, &NULL_REQ);
  sendMsg(&pos, 1, MPI_INT, job->returnTo, COMM_TAG_RESULTSEND, MPI_STATUS_IGNORE);

//  MPI_Recv(NULL, 0, MPI_BYTE, job->returnTo, COMM_TAG_RESULTSEND, MPI_COMM_WORLD, &NULL_STATUS);
  receiveMsg(NULL, 0, MPI_BYTE, job->returnTo, COMM_TAG_RESULTSEND, MPI_STATUS_IGNORE);

  if (debug_PUM)
    fprintf(stderr,"PUM (%i):  Sent return size. Sending packed result NOW.\n", myid);

  sendMsg(buff, pos, MPI_PACKED, job->returnTo, COMM_TAG_RESULTSEND, MPI_STATUS_IGNORE);

  free(buff);

  if (!SILENT)
    printf("PUM (%i):  Sent packed result.\n", myid);

  pthread_mutex_unlock(&sendResultMutex);

  cleanup(job);
  return NULL;
}

bool cleanup (JobToPUM *job) {

  free(job->startingKernel);
  free(job->taskSource);
  free(job->argTypes);
  free(job->argSizes);

  for (cl_uint i = 0; i<job->nTotalArgs; i++)
    free(job->arguments[i]);

  free(job->arguments);

  free(job);

  return true;
}



int getStatusOf (int devID); //TODO
