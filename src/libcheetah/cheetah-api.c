/*
 * cheetah-api.c
 *
 *  Created on: 12 May 2010
 *      Author: luis
 */

#include "cheetah/cheetah.h"

/***
 *
 * Initialization & general functions
 *
 ***/


void parseOpts (int argc, char *argv[]) {
  int option_index;
  int currarg = 0;

  while ( getopt_long_only(argc, argv, "", long_options, &option_index) != -1) {
    //TODO: Sanitizing?
    //TODO: Support for multiple Job Schedulers
    switch (option_index) {
      case 1://job scheduler
        defaultSchedID = atoi(optarg);
        break;
      case 3://result collector
        defaultRCID = atoi(optarg);
        break;
      case 4: //global items
        nGlobItems[0] = nGlobItems[1] = atoi(optarg);
        break;
      case 5: //items per group
        nItemsGroup[0] = nItemsGroup[1] = atoi(optarg);
        break;
      default:
        cheetah_print_error("%s: unrecognised option '--%s'\n", argv[0], long_options[option_index].name);
        exit (139);
    }
    currarg++;
  }
}

pthread_t notificationWaiterThread;
void *notificationWaiter ();


void initDistribCL(int argc, char *argv[]) {
  int namelen;
  char processorname[MPI_MAX_PROCESSOR_NAME];
  componentName = "JM";

  int prov;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &prov);
  MPI_Comm_rank(MPI_COMM_WORLD, &myid);
  MPI_Get_processor_name(processorname, &namelen);


  cheetah_print("Job Manager Started at %s (rank %i). Thread support level: %i (asked %i). Setting up...\n", processorname, myid, prov, MPI_THREAD_MULTIPLE);

  parseOpts(argc, argv);


  pthread_create(&notificationWaiterThread, NULL, notificationWaiter, NULL);

}


/***
 *
 * Job creation, setup and deletion
 *
 ***/

Job *createJob () {
  Job *job = malloc(sizeof(Job));
  job->probID = myid;
  job->jobID = -1;

  job->required_is_set = false;
  job->requirePU = CL_DEVICE_TYPE_DEFAULT;

  job->nPreferPUs = 0;
  job->nAllowPUs = 0;
  job->nAvoidPUs = 0;
  job->nForbidPUs = 0;

  job->preferPUs = NULL;
  job->allowPUs = NULL;
  job->avoidPUs = NULL;
  job->forbidPUs = NULL;

  job->category = -1;//PDVE_INVALID_LOW;

  job->nDimensions = 0;

  job->startingNameSize = 0;
  job->startingKernel = NULL;

  job->taskSourceSize = 0;
  job->taskSource = NULL;

  job->nTotalArgs = 0;
  job->argTypes = NULL;

  job->argSizes = NULL;
  job->arguments = NULL;

  job->returnTo = -1;
  return job;
}



void setJobID (Job *job, int jobID) {
  job->jobID = jobID;
}

//mutually exclusive with set*PU
void setRequiredPU (Job *job, cl_device_type requirePU) {
  assert(job->nPreferPUs == 0 && job->nAllowPUs == 0 && job->nAvoidPUs == 0 && job->nForbidPUs == 0);

  job->required_is_set = true;
  job->requirePU = requirePU;
}


void setPreferPU (Job *job, cl_device_type preferPU) {
  assert(!job->required_is_set);

  job->preferPUs = realloc(job->preferPUs, (job->nPreferPUs + 1) * sizeof(cl_device_type));

  job->preferPUs[job->nPreferPUs] = preferPU;
  job->nPreferPUs++;
}


void setAllowPU (Job *job, cl_device_type allowPU) {
  assert(!job->required_is_set);

  job->allowPUs = realloc(job->allowPUs, (job->nAllowPUs + 1) * sizeof(cl_device_type));

  job->allowPUs[job->nAllowPUs] = allowPU;
  job->nAllowPUs++;
}


void setAvoidPUs (Job *job, cl_device_type avoidPU) {
  assert(!job->required_is_set);

  job->avoidPUs = realloc(job->avoidPUs, (job->nAvoidPUs + 1 ) * sizeof(cl_device_type));

  job->avoidPUs[job->nAvoidPUs] = avoidPU;
  job->nAvoidPUs++;
}


void setForbidPU (Job *job, cl_device_type forbidPU) {
  assert(!job->required_is_set);

  job->forbidPUs = realloc(job->forbidPUs, (job->nForbidPUs + 1) * sizeof(cl_device_type));

  job->forbidPUs[job->nForbidPUs] = forbidPU;
  job->nForbidPUs++;
}


void setJobCategory (Job *job, int category) {
  job->category = category;
}


//If this is not used, we will try to use the device's maximum number of threads
void setDimensions (Job *job, int nDim, int *nItemsPerDim, int *nItemsPerGroup) {
  assert(nDim > 0 && nDim < 4);

  job->nDimensions = nDim;
  for (int i = 0; i < nDim; i++) {
    assert(nItemsPerDim[i] > 0 && nItemsPerGroup[i] > 0);
    assert((nItemsPerDim[i] % nItemsPerGroup[i]) == 0);

    job->nItems[i] = nItemsPerDim[i];
    job->nGroupItems[i] = nItemsPerGroup[i];
  }
}



void loadSourceFile (Job *job, const char *taskSourceFile) {
  job->taskSource = fileToString(taskSourceFile);
  if (job->taskSource == NULL) {
    cheetah_print_error("FILE NOT FOUND! Please verify kernel file path: %s.", taskSourceFile);
    exit (EXIT_FAILURE);
  }
  job->taskSourceSize = strlen(job->taskSource)+1;
}

//startingKernel _MUST_ be null-terminated ('\0')
void setStartingKernel (Job *job, const char *startingKernel) {
  job->startingNameSize = strlen(startingKernel)+1;
  job->startingKernel = calloc(job->startingNameSize, sizeof(char));

  strncpy(job->startingKernel, startingKernel, job->startingNameSize);

  assert(startingKernel[job->startingNameSize-1] == '\0');
}


/** The argument must be previously malloc'ed. It will NOT be copied
 * Argument Types: INPUT, OUTPUT or INPUT_OUTPUT, EMPTY_BUFFER
 * OUTPUT and EMPTY_BUFFER arguments do not need to be malloc'ed.
 * Size is in bytes.
 */
void setArgument (Job *job, argument_type argType, size_t argSize, void *argument) {
  assert(argSize > 0);
  assert(argType == INPUT || argType == INPUT_OUTPUT || argType == OUTPUT || argType == EMPTY_BUFFER);

  job->argTypes  = realloc(job->argTypes,  (job->nTotalArgs + 1) * sizeof(int));
  job->argSizes  = realloc(job->argSizes,  (job->nTotalArgs + 1) * sizeof(int));
  if ((argType == INPUT) || (argType == INPUT_OUTPUT))
    job->arguments = realloc(job->arguments, (job->nTotalArgs + 1) * sizeof(void *));

  job->argTypes[job->nTotalArgs]  = argType;
  job->argSizes[job->nTotalArgs]  = argSize;
  if ((argType == INPUT) || (argType == INPUT_OUTPUT))
    job->arguments[job->nTotalArgs] = argument;
  job->nTotalArgs++;
}


void setResultCollector (Job *job, int rcID) {
  assert(rcID >= 0);
  assert(rcID != myid);

  job->returnTo = rcID;
}



//NOTICE: job arguments aren't freed. Those were allocated by the JM.
void deleteJob (Job *job) {
  if (job->nPreferPUs > 0)
    free(job->preferPUs);
  if(job->nAllowPUs > 0)
    free(job->allowPUs);
  if (job->nAvoidPUs > 0)
    free(job->avoidPUs);
  if (job->nForbidPUs > 0)
    free(job->forbidPUs);
  if (job->startingNameSize > 0)
    free(job->startingKernel);
  if (job->taskSourceSize > 0)
    free(job->taskSource);

  //we won't delete each of the job's arguments - these were allocated by the app!
  if (job->nTotalArgs > 0) {
    free(job->argTypes);
    free(job->argSizes);
    free(job->arguments);
  }

  free(job);
}


/***
 *
 * Sending to execution
 *
 ***/

//These checks are to be sure that no invalid data is sent on the system
//OpenCL-validity is not checked here. It will be checked when launching the kernel on the device
void checkJob(Job *job) {
  assert(job->probID == myid);
  assert((job->required_is_set && (job->requirePU == CL_DEVICE_TYPE_CPU || job->requirePU == CL_DEVICE_TYPE_GPU)) || !(job->required_is_set));

  assert(job->nPreferPUs > -1 && job->nAllowPUs > -1 && job-> nAvoidPUs > -1 && job->nForbidPUs > -1);
  if (job->nPreferPUs > 0)
    assert(!job->required_is_set && job->preferPUs != NULL);
  if (job->nAllowPUs > 0)
    assert(!job->required_is_set && job->allowPUs != NULL);
  if (job->nAvoidPUs > 0)
    assert(!job->required_is_set && job->avoidPUs != NULL);
  if (job->nForbidPUs > 0)
    assert(!job->required_is_set && job->forbidPUs != NULL);

//  assert(job->PDVE > PDVE_INVALID_LOW && job->PDVE < PDVE_INVALID_HIGH);
  assert(job->nDimensions >= 0 && job->nDimensions < 4);
  for (int i = 0; i < job->nDimensions; i++) {
    assert(job->nItems[i] > 0 && job->nGroupItems[i] > 0);
    assert((job->nItems[i] % job->nGroupItems[i]) == 0);
  }
  assert(job->startingNameSize > 0);
  assert(job->startingKernel != NULL && (strlen(job->startingKernel)+1) == job->startingNameSize);
  assert(job->taskSourceSize > 0);
  assert(job->taskSource != NULL && (strlen(job->taskSource)+1) == job->taskSourceSize);
  assert(job->nTotalArgs > 0);
  assert(job->argTypes != NULL);
  assert(job->argSizes != NULL);
  assert(job->arguments != NULL);
  assert(job->returnTo >= 0 && job->returnTo != myid); //TODO: check that returnTo is in fact a resultCollector
}


//THIS IS *NOT* THREAD-SAFE
void sendJobToExec (Job *job, int schedID) {
  checkJob(job);


  cl_uint totalArgumentsSize = 0;
  for (cl_uint i = 0; i < job->nTotalArgs; i++) {
    if ((job->argTypes[i] != OUTPUT) &&
        (job->argTypes[i] != EMPTY_BUFFER))
      totalArgumentsSize += job->argSizes[i];
  }

  int pos = 0;
  int sizeofData = sizeof(Job)
      + sizeof(cl_device_type)*(job->nPreferPUs + job->nAllowPUs + job->nAvoidPUs + job->nForbidPUs)
      + sizeof(char)*(job->startingNameSize)
      + sizeof(char)*(job->taskSourceSize)
      + sizeof(argument_type)*(job->nTotalArgs)
      + sizeof(int)*(job->nTotalArgs)
      + totalArgumentsSize;


  char *buff = malloc(sizeofData);
  MPI_Pack(&(job->probID),                     1,      MPI_INT,           buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(&(job->jobID),                      1,      MPI_INT,           buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(&(job->required_is_set),            1,      MPI_BYTE,          buff, sizeofData, &pos, MPI_COMM_WORLD); //TODO: boolean is 1 byte here
  MPI_Pack(&(job->requirePU),                  1,      MPI_UNSIGNED_LONG, buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(&(job->nPreferPUs),                 1,      MPI_INT,           buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(&(job->nAllowPUs),                  1,      MPI_INT,           buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(&(job->nAvoidPUs),                  1,      MPI_INT,           buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(&(job->nForbidPUs),                 1,      MPI_INT,           buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(job->preferPUs,       job->nPreferPUs,      MPI_UNSIGNED_LONG, buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(job->allowPUs,         job->nAllowPUs,      MPI_UNSIGNED_LONG, buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(job->avoidPUs,         job->nAvoidPUs,      MPI_UNSIGNED_LONG, buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(job->forbidPUs,       job->nForbidPUs,      MPI_UNSIGNED_LONG, buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(&(job->category),                   1,      MPI_INT,           buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(&(job->nDimensions),                1,      MPI_INT,           buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(job->nItems,                        3,      MPI_INT,           buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(job->nGroupItems,                   3,      MPI_INT,           buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(&(job->startingNameSize),           1,      MPI_INT,           buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(job->startingKernel, job->startingNameSize, MPI_CHAR,          buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(&(job->taskSourceSize),             1,      MPI_INT,           buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(job->taskSource,  job->taskSourceSize,      MPI_CHAR,          buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(&(job->nTotalArgs),                 1,      MPI_INT,           buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(job->argTypes,        job->nTotalArgs,      MPI_INT,           buff, sizeofData, &pos, MPI_COMM_WORLD);
  MPI_Pack(job->argSizes,        job->nTotalArgs,      MPI_INT,           buff, sizeofData, &pos, MPI_COMM_WORLD);

  for (cl_uint i = 0; i < job->nTotalArgs; i++) {
    if ((job->argTypes[i] != OUTPUT) &&
        (job->argTypes[i] != EMPTY_BUFFER))
      MPI_Pack(job->arguments[i], job->argSizes[i],      MPI_BYTE,          buff, sizeofData, &pos, MPI_COMM_WORLD);

  }

  MPI_Pack(&(job->returnTo),                   1,      MPI_INT,           buff, sizeofData, &pos, MPI_COMM_WORLD);

  //TODO: get locks around this
  sendMsg(&pos,   1,    MPI_INT, schedID, COMM_TAG_JOBSUBMIT, MPI_STATUS_IGNORE);
  sendMsg(buff, pos, MPI_PACKED, schedID, COMM_TAG_JOBSUBMIT, MPI_STATUS_IGNORE);

  free(buff);
}



/***
 *
 * Result obtaining
 *
 ***/
void *notifThread (void *arg) {
  int *recvNotif = (int *) arg;
  resultAvailable(*recvNotif);

  free(recvNotif);
  return NULL;
}

void *notificationWaiter (void *arg) {
  pthread_t notif_thread;
  pthread_attr_t attrs;
  if (pthread_attr_init(&attrs) != 0) {
    perror("pthread_attr_init");
    MPI_Finalize();
    exit(EXIT_FAILURE);
  }
  if (pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED) != 0) {
    perror("pthread_attr_setdetachstate");
    MPI_Finalize();
    exit(EXIT_FAILURE);
  }

  while (true) {
    int *recvNotif = malloc(sizeof (int));
    receiveMsg(recvNotif, 1, MPI_INT, MPI_ANY_SOURCE, COMM_TAG_RESULTREGISTER, MPI_STATUS_IGNORE);

    //A new thread is created to treat each new result
    if (pthread_create(&notif_thread, &attrs, notifThread, recvNotif) != 0) {
      perror("pthread_create");
      MPI_Finalize();
      exit(EXIT_FAILURE);
    }
  }
  if (pthread_attr_destroy(&attrs) != 0) {
    perror("pthread_attr_destroy");
    MPI_Finalize();
    exit(EXIT_FAILURE);
  }
}

void requestResultNotification (Job *job) {
  assert(job->returnTo >= 0);
  assert(job->returnTo != myid);
  sendMsg(&(job->jobID), 1, MPI_INT, job->returnTo, COMM_TAG_RESULTREGISTER, MPI_STATUS_IGNORE);
}



//Will only return when the results are available
JobResults *getResults (Job *job) {
  assert(job->returnTo > 0);
  assert(job->returnTo != myid);
  int pos = 0,
      dataSize = -2;
  MPI_Status status;
  JobResults *JR = malloc(sizeof (JobResults));


//  while (dataSize < 0) {
    sendMsg(&(job->jobID), 1, MPI_INT, job->returnTo, COMM_TAG_RESULTGET, MPI_STATUS_IGNORE);
    receiveMsg(&dataSize, 1, MPI_INT, job->returnTo, COMM_TAG_RESULTGET, &status);
//    guaranteedSleep(1000);
//  }
  if (dataSize < 0) {
    free(JR);
    return NULL;
  }


  char *receivedData = calloc(dataSize, sizeof(char));

  receiveMsg(receivedData, dataSize, MPI_PACKED, status.MPI_SOURCE, COMM_TAG_RESULTGET, MPI_STATUS_IGNORE);

  //TODO: Check if this makes any difference
  sendMsg(NULL, 0, MPI_BYTE, status.MPI_SOURCE, COMM_TAG_RESULTGET, MPI_STATUS_IGNORE);

  MPI_Unpack(receivedData, dataSize, &pos,        &(JR->probID),                   1, MPI_INT, MPI_COMM_WORLD);
  MPI_Unpack(receivedData, dataSize, &pos,         &(JR->jobID),                   1, MPI_INT, MPI_COMM_WORLD);
  MPI_Unpack(receivedData, dataSize, &pos, &(JR->nTotalResults),                   1, MPI_INT, MPI_COMM_WORLD);

  JR->resultSizes = calloc(JR->nTotalResults, sizeof(int));
  MPI_Unpack(receivedData, dataSize, &pos,      JR->resultSizes,  JR->nTotalResults, MPI_INT, MPI_COMM_WORLD);

  JR->results = calloc(JR->nTotalResults, sizeof(void *));
  for (cl_uint i = 0; i < JR->nTotalResults; i++ ) {
    JR->results[i] = malloc(JR->resultSizes[i]);
    MPI_Unpack(receivedData, dataSize, &pos,     JR->results[i], JR->resultSizes[i], MPI_BYTE, MPI_COMM_WORLD);
  }

  MPI_Unpack(receivedData, dataSize, &pos,   &(JR->returnStatus),                  1, MPI_INT,  MPI_COMM_WORLD);

  free(receivedData);

  return JR;
}

void deleteResults (JobResults *JR) {

  free(JR->resultSizes);

  for (int i = 0; i < JR->nTotalResults; i++)
    free(JR->results[i]);

  free(JR->results);
  free(JR);
}


void quitDistribCL () {
  int size;
  MPI_Comm_size(MPI_COMM_WORLD, &size);


  for (int i = 1; i < size; i++) {
    sendMsg(NULL, 0, MPI_BYTE, i, COMM_TAG_SHUTDOWN, MPI_STATUS_IGNORE);
  }

  pthread_cancel(notificationWaiterThread);
  cheetah_print_error("Goodbye.");
  MPI_Finalize();

}
