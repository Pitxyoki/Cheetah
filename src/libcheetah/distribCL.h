/*
 * distribCL.h
 *
 * API functions visible to the application programmer
 *
 *  Created on: 12 May 2010
 *      Author: Luís Miguel Picciochi de Oliveira
 */

#ifndef DISTRIBCL_H_
#define DISTRIBCL_H_

#include <constants.h>


int nGlobItems[2];
int nItemsGroup[2];


int defaultSchedID;
int defaultRCID;

void initDistribCL(int argc, char *argv[]);
void quitDistribCL();

Job *createJob();

//jobIDs must be unique. Non-compliance may give origin to loss of data
void setJobID (Job *job, int jobID);

//mutually exclusive with set*PU
void setRequiredPU (Job *job, cl_device_type requirePU);
void setPreferPU   (Job *job, cl_device_type preferPU);
void setAllowPU    (Job *job, cl_device_type allowPU);
void setAvoidPU    (Job *job, cl_device_type avoidPU);
void setForbidPU   (Job *job, cl_device_type forbidPU);


//one of:
//HIGH_PROCESSING_FEW_DATA, HIGH_PROCESSING_MANY_DATA
//LOW_PROCESSING_FEW_DATA,  LOW_PROCESSING_MANY_DATA
void setJobCategory (Job *job, int category);

//If this is not used, we will try to use the device's maximum (or default?) number of threads
void setDimensions (Job *job, int nDim, int *nItemsPerDim, int *nItemsPerGroup);

void loadSourceFile (Job *job, char *taskSourceFile);

//startingKernel _MUST_ be null-terminated ('\0')
void setStartingKernel (Job *job, char *startingKernel);


/**
 * Argument Types: INPUT, OUTPUT, INPUT_OUTPUT or EMPTY_BUFFER
 * INPUT and INPUT_OUTPUT must be previously malloc'ed and available until
 * sendJobToExec is called. The data will NOT be duplicated.
 *
 * OUTPUT and EMPTY_BUFFER arguments do not need to be malloc'ed.
 */
void setArgument (Job *job, argument_type argType, size_t argSize, void *argument);


//TODO: verify that rank is indeed a RC
void setResultCollector (Job *job, int rcID);

//NOTICE: job arguments aren't freed. Those were allocated by the JM.
void deleteJob (Job *job);




//THIS IS *NOT* THREAD-SAFE. Only one sendJobToExec at a time, please
void sendJobToExec (Job *job, int schedID);





/* Receiving results */
//typedef void (*callback_f)(int jobID);
void requestResultNotification (Job *job/*TODO, callback_f callback*/);

/*
 * To be implemented by the Application Programmer.
 *
 * NOTICE: this will run on a single thread for each received result. Take thread-safety into account.
 */
void resultAvailable (int jobID);


//Will only return when the results are available
JobResults *getResults (Job *job);

void deleteResults (JobResults *JR);


#endif /* DISTRIBCL_H_ */
