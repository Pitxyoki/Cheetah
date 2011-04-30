/*
 * pum.h
 * General headers
 *  Created on: 1 Apr 2010
 *      Author: luis
 */

#ifndef PUM_H_
#define PUM_H_

/* INCLUDES */
#include "cheetah/cheetah-common.h"
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>

/* General functionality */



/*** OpenCL-related variables ***/
PUMStruct PUInfoStruct;  //Information struct of this PUM

int defaultSchedID;     //for one-to-one, scheduler-PUM mode

struct JQueueElem {
  struct JQueueElem *next;
  JobToPUM *job;
};

typedef struct {
  JobToPUM *job;
  int res;
} jobNResult;





/*** Initial setup functions ***/
bool getLocalInfo();
bool printLocalInfo();

bool runPUTests();

bool JMSetup ();


/*** Misc. functions ***/
void print1DArray(const void* arrayData, const unsigned int length);

/*** Job execution functions ***/
//Producer thread
void *receiveJobs ();



//Consumer Threads
void *JobConsumer();

bool initializeCLExec (cl_device_type dType);
bool initializeCLBuffers (JobToPUM *job);
bool execJob (JobToPUM *job);

//void *jobNotificationSender(void *fjnStru);
void sendFinishedJobNotification(finishedJobNotification *fjn);

//void *sendFailure(void *job);
void resultPrepareAndSend(jobNResult *jnr);
void *sendResults (void *job_and_result);

bool cleanup (JobToPUM *job);

//Auxiliary thread, TODO
int getStatusOf (int devID);

/* Test functions */


bool createLowProcJob (JobToPUM *job);
bool createHighProcJob (JobToPUM *job);
bool createTestMandelbrot (JobToPUM *job);
bool createBandwidthTestJob (JobToPUM *job);
bool createTestFFT (JobToPUM *job);

void checkJobResults(JobToPUM *job);


#endif /* PUM_H_ */
