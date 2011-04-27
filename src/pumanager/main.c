/*
 * Processing Unit Manager
 * Program main. Thread starter.
 *  Created on: 26 Mar 2010
 *      Author: luis
 */

#include "pum.h"

void parseOpts (int argc, char *argv[]) {
  int option_index;

  while ( getopt_long_only(argc, argv, "", long_options, &option_index) != -1) {
    //TODO: Sanitizing
    //TODO: Multiple Job Schedulers?
    if (option_index == 1) //job scheduler
      defaultSchedID = atoi(optarg);
  }
}


int main (int argc, char *argv[]) {
  initializeComponent("PU Manager", argc, argv);
  parseOpts(argc, argv);


  if (debug_PUM && !printLocalInfo()) {
    MPI_Finalize();
    exit(EXIT_FAILURE);
  }

  //Detect machine properties & build corresponding struct
  if (!getLocalInfo()) {
    MPI_Finalize();
    exit(EXIT_FAILURE);
  }
  printf("PUM (%i): Starting internal tests.\n", myid);
  if (!runPUTests()) {
    MPI_Finalize();
    exit(EXIT_FAILURE);
  }
  printf("PUM (%i): Internal tests finished.\n", myid);

  printf("PUM (%i): Setting up with Job Scheduler...\n", myid);
  if (!JMSetup()) {
    MPI_Finalize();
    exit(EXIT_FAILURE);
  }
  printf("PUM (%i): Setup with Job Scheduler done.\n",myid);


  pthread_t tjobReceiver, tjobConsumer;
  //JS-sent jobs receiving thread
  if (pthread_create (&tjobReceiver, NULL, receiveJobs, NULL)) {
    perror("pthread_create");
    MPI_Finalize();
    exit(EXIT_FAILURE);
  }

  //Job consumers for each PU (== OpenCL device), also job senders to RC
  for (int i = 0; i < PUInfoStruct.nPUs; i++) {
    int *devID = malloc(sizeof(int));
    *devID = i;
    if (pthread_create (&tjobConsumer, NULL, JobConsumer, devID)) {
      perror("pthread_create");
      MPI_Finalize();
      exit(EXIT_FAILURE);
    }
  }

  //TODO: Status responder

  receiveMsg(NULL, 0, MPI_BYTE, MPI_ANY_SOURCE, COMM_TAG_SHUTDOWN, MPI_STATUS_IGNORE);
  finalizeComponent("PUM");
  return EXIT_SUCCESS;
}
