/*
 * retcolUtils.c
 *
 *  Created on: 16 Apr 2010
 *      Author: luis
 */

#include "rc.h"


/*****
 * Receiving a result from a PUM
 *****/

void *resultReceiver() {
  int pos,
      gotmsg,
      dataSize;

  MPI_Status status;
  JobResults *job;

  while (true) {
    job = malloc(sizeof(JobResults));
    pos = 0;
    gotmsg = false;
    dataSize = 0;

    cheetah_debug_print("Result Receiver waiting for results...\n");

    /*** Receive a result ***/
    receiveMsg(&dataSize, 1, MPI_INT, MPI_ANY_SOURCE, COMM_TAG_RESULTSEND, &status);

    cheetah_debug_print("Result Receiver received result SIZES..., source: %i, size: %i\n", status.MPI_SOURCE, dataSize);

    sendMsg(NULL, 0, MPI_BYTE, status.MPI_SOURCE, COMM_TAG_RESULTSEND, MPI_STATUS_IGNORE);

    char *receivedData = malloc(dataSize);
    receiveMsg(receivedData, dataSize, MPI_PACKED, status.MPI_SOURCE, COMM_TAG_RESULTSEND, MPI_STATUS_IGNORE);

    cheetah_debug_print("Result Receiver received RESULTS - going to unpack them\n");


    MPI_Unpack(receivedData, dataSize, &pos,        &(job->probID),                   1, MPI_INT, MPI_COMM_WORLD);
    MPI_Unpack(receivedData, dataSize, &pos,         &(job->jobID),                   1, MPI_INT, MPI_COMM_WORLD);
    MPI_Unpack(receivedData, dataSize, &pos, &(job->nTotalResults),                   1, MPI_INT, MPI_COMM_WORLD);

    job->resultSizes = calloc(job->nTotalResults, sizeof(int));
    MPI_Unpack(receivedData, dataSize, &pos,      job->resultSizes,  job->nTotalResults, MPI_INT, MPI_COMM_WORLD);

    job->results = calloc(job->nTotalResults, sizeof(void *));
    for (cl_uint i = 0; i < job->nTotalResults; i++ ) {

      job->results[i] = malloc(job->resultSizes[i]);
      MPI_Unpack(receivedData, dataSize, &pos,     job->results[i], job->resultSizes[i], MPI_BYTE, MPI_COMM_WORLD);
    }

    MPI_Unpack(receivedData, dataSize, &pos,   &(job->returnStatus),                  1, MPI_INT,  MPI_COMM_WORLD);

    free(receivedData);
    cheetah_debug_print("RC: Result received by RC!\n");

    if (debug_RC) {
      printJobResults(job);
    }


    if (!storeJobRes(job)) {
      cheetah_print_error("THIS SHOULDN'T HAVE HAPPENED.\n");
      return NULL;
    }

    //Notify job->probID that job->jobID has arrived.
    //The notification will only be sent if job->probID has already registered
    //with this RC and is now waiting for this notification message.
    notifyRank(job->probID, job->jobID);
  }
}



/******
 * Saving result locally
 ******/

void *rootJob = NULL;
pthread_mutex_t resultsMutex = PTHREAD_MUTEX_INITIALIZER;

int compare(const void *pa, const void *pb) {
  JobResults *jobA = (JobResults *) pa;
  JobResults *jobB = (JobResults *) pb;

  int probA = jobA->probID,
      probB = jobB->probID,
      jobIDA = jobA->jobID,
      jobIDB = jobB->jobID;

  if (debug_RC) {
    cheetah_debug_print("comparing probA: %i, jobA: %i, probB: %i, jobB: %i\n", probA, jobIDA, probB, jobIDB);
  }

  if (probA < probB) {
    return -1;
  }
  if (probA > probB) {
    return 1;
  }
  if (probA == probB) {
    if (jobIDA < jobIDB) {
      return -1;
    }
    if (jobIDA > jobIDB) {
      return 1;
    }
  }
  return 0;
}




cl_bool storeJobRes (JobResults *job) {

  void *existing;

  pthread_mutex_lock(&resultsMutex);

  existing = tsearch((void *) job, &rootJob, compare);

  pthread_mutex_unlock(&resultsMutex);

  if ((*(JobResults **) existing) != job) {//the job was already on the tree
    return CL_FALSE;
  }
  return CL_TRUE;
}


/*****
 * Sending results
 *****/
//Return the indicated job's results.
// NULL if the job's results are not here
JobResults *getJobRes (int probID, int jobID) {
  JobResults *testJob = malloc(sizeof(JobResults));
  JobResults **result = NULL;
  testJob->probID = probID;
  testJob->jobID = jobID;

  pthread_mutex_lock(&resultsMutex);
  if (debug_RC)
    cheetah_debug_print("Going to tfind prob: %i, job: %i \n", testJob->probID, testJob->jobID);

  result = tfind(testJob, &rootJob, compare);
  pthread_mutex_unlock(&resultsMutex);
  free(testJob);
  if (result == NULL) {
    if (debug_RC) {
      cheetah_debug_print("probID %i, jobID %i not found\n", probID, jobID);
    }
    return NULL;
  }
  if (debug_RC) {
    cheetah_debug_print("tdeleted prob: %i, job: %i \n", (*result)->probID, (*result)->jobID);
  }


  return *result;
}

void *resultSender() {
  int pos,
      gotmsg;

  MPI_Status status;
  JobResults *jobResults;
  int jobID;
  while (true) {
    pos = 0;
    gotmsg = false;

    cheetah_debug_print("Result Sender waiting for requests...\n");

    /*** Receive a result request ***/
    receiveMsg(&jobID, 1, MPI_INT, MPI_ANY_SOURCE, COMM_TAG_RESULTGET , &status);

    jobResults = getJobRes(status.MPI_SOURCE, jobID);

    //If the request corresponds to a job that isn't present, we don't have any results to send
    if (jobResults == NULL) {
      cheetah_debug_print_error("REQUESTED A NOT-AVAILABLE RESULT\n");
      int error = -1;
      sendMsg(&error, 1, MPI_INT, status.MPI_SOURCE, COMM_TAG_RESULTGET, MPI_STATUS_IGNORE);
      continue;
    }



    /*** Preparing the data transfer ***/
    cl_uint totalResultsSize = 0;
    for (cl_uint i = 0; i<jobResults->nTotalResults; i++ ) {
      totalResultsSize += jobResults->resultSizes[i];
    }

    int sizeofData = sizeof(int)*3
        + sizeof(int)*jobResults->nTotalResults
        + totalResultsSize
        + sizeof(int);

    char *buff = malloc(sizeofData);

    MPI_Pack(&(jobResults->probID),                             1, MPI_INT, buff, sizeofData, &pos, MPI_COMM_WORLD);
    MPI_Pack(&(jobResults->jobID),                              1, MPI_INT, buff, sizeofData, &pos, MPI_COMM_WORLD);
    MPI_Pack(&(jobResults->nTotalResults),                      1, MPI_INT, buff, sizeofData, &pos, MPI_COMM_WORLD);

    MPI_Pack(jobResults->resultSizes,   jobResults->nTotalResults, MPI_INT, buff, sizeofData, &pos, MPI_COMM_WORLD);

    for (cl_uint i = 0; i < jobResults->nTotalResults; i++ ) {
      MPI_Pack(jobResults->results[i], jobResults->resultSizes[i], MPI_BYTE,buff, sizeofData, &pos, MPI_COMM_WORLD);
    }

    MPI_Pack(&(jobResults->returnStatus),                       1, MPI_INT, buff, sizeofData, &pos, MPI_COMM_WORLD);

    /*** Sending ***/
    //MPI_Isend(&pos,   1, MPI_INT,    status.MPI_SOURCE, COMM_TAG_RESULTGET, MPI_COMM_WORLD, &NULL_REQ);
    sendMsg(&pos,   1,    MPI_INT, status.MPI_SOURCE, COMM_TAG_RESULTGET, MPI_STATUS_IGNORE);
    sendMsg(buff, pos, MPI_PACKED, status.MPI_SOURCE, COMM_TAG_RESULTGET, MPI_STATUS_IGNORE);

    free(buff);

    receiveMsg(NULL, 0, MPI_BYTE, status.MPI_SOURCE, COMM_TAG_RESULTGET, MPI_STATUS_IGNORE);

    cheetah_debug_print("just sent a result.\n");
    if(debug_RC) {
      printJobResults(jobResults);
      cheetah_debug_print("That was it.\n");
    }

    for (int i = 0; i < jobResults->nTotalResults; i++) {
      free(jobResults->results[i]);
    }

    free(jobResults->results);
    free(jobResults->resultSizes);

    pthread_mutex_lock(&resultsMutex);
    tdelete(jobResults, &rootJob, compare);
    pthread_mutex_unlock(&resultsMutex);

  }
}




/*****
 * Registering functions
 *****/
void *rootRegist = NULL;
pthread_mutex_t registerMutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
  int probID;
  int jobID;
} registeredElem;

int compareRegist (const void *pa, const void *pb) {
  registeredElem *regA = (registeredElem *) pa;
  registeredElem *regB = (registeredElem *) pb;

  int probA = regA->probID,
      probB = regB->probID,
      jobIDA = regA->jobID,
      jobIDB = regB->jobID;

  if (debug_RC) {
    cheetah_debug_print("comparing (registrations) probA: %i, jobA: %i, probB: %i, jobB: %i\n", probA, jobIDA, probB, jobIDB);
  }

  if (probA < probB) {
    return -1;
  }
  if (probA > probB) {
    return 1;
  }
  if (probA == probB) {
    if (jobIDA < jobIDB) {
      return -1;
    }
    if (jobIDA > jobIDB) {
      return 1;
    }
  }
  return 0;
}

void *resultNotifRegister () {
  MPI_Status status;
  int jobID;
  registeredElem *re;

  while (true) {
    receiveMsg(&jobID, 1, MPI_INT, MPI_ANY_SOURCE, COMM_TAG_RESULTREGISTER, &status);


    cheetah_debug_print("Received request to notify rank %i for jobID %i\n", status.MPI_SOURCE, jobID);
    re = malloc(sizeof(registeredElem));
    re->probID = status.MPI_SOURCE;
    re->jobID = jobID;

    pthread_mutex_lock(&registerMutex);
    tsearch((void *) re, &rootRegist, compareRegist);
    pthread_mutex_unlock(&registerMutex);

    //If someone had already asked to be notified, we will notify him again now
    if (getJobRes(re->probID, re->jobID) != NULL) {//only if we already have the results
      notifyRank(status.MPI_SOURCE, jobID);
    }
  }
}

//Notifies rank that the jobID's results have arrived.
//This will only do so if the rank asked to be notified
//and the results are already available
void notifyRank (int rank, int jobID) {
  registeredElem testRegist;
  testRegist.probID = rank;
  testRegist.jobID = jobID;


  cheetah_debug_print("Asked to notify rank %i for jobID %i\n", rank, jobID);

  registeredElem *result = NULL;

  pthread_mutex_lock(&registerMutex);
  result = tfind(&testRegist, &rootRegist, compareRegist);

  MPI_Status status;
  if (result != NULL) {
    cheetah_debug_print("going to notify rank %i for jobID %i\n", rank, jobID);

    sendMsg(&jobID, 1, MPI_INT, rank, COMM_TAG_RESULTREGISTER, &status);
    tdelete(result, &rootRegist, compareRegist);

    cheetah_debug_print("notified rank %i for jobID %i\n", rank, jobID);
  }
  else {
    cheetah_debug_print_error("DID NOT notify rank %i for jobID %i\n", rank, jobID);
  }

  pthread_mutex_unlock(&registerMutex);
}
