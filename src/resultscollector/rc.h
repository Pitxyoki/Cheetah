/*
 * rc.h
 *
 *  Created on: 16 Apr 2010
 *      Author: luis
 */

#ifndef RC_H_
#define RC_H_

#include "cheetah/cheetah-common.h"
#include <search.h>


/*****
 * Receiving a result from a PUM
 *****/

void *resultReceiver ();

/******
 * Saving result locally
 ******/
cl_bool storeJobRes (JobResults *job);


/*****
 * Sending results
 *****/

//Return the indicated job's results. NULL if the job's results are not here
JobResults *getJobRes (int probID, int jobID);

void *resultSender();


/*****
 * Registering functions
 *****/
void *resultNotifRegister ();

//Notifies rank that the jobID's results have arrived.
//This will only do so if the rank asked to be notified.
void notifyRank (int rank, int jobID);

// Eliminate register for notification to mantain memory footprint low
void deleteNotification(int rank, int jobID);

#endif /* RC_H_ */
