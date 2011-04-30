/*
 * Job Scheduler
 *
 */

#include "scheduler.h"

void parseOpts (int argc, char *argv[]) {
  int option_index;

  while ( getopt_long_only(argc, argv, "", long_options, &option_index) != -1) {
    //what do we need?
  }
}


int main (int argc, char *argv[]) {
  initializeComponent("Job Scheduler", "JS", argc, argv);
  parseOpts(argc, argv);


  pthread_t tpumReceiverv,  tjobReceiver, tjobDispatcher, tPUStatusReceiver;

  if (initSched() != 0) {
    cheetah_print("Error initializing scheduler.", myid);
    exit(EXIT_FAILURE);
  }


  //PUM setup receiving thread
  if (pthread_create (&tpumReceiverv, NULL, PUMReceiver, NULL)) {
    perror("pthread_create");
    MPI_Finalize();
    exit(EXIT_FAILURE);
  }

  //JM-sent jobs receiving thread
  if (pthread_create (&tjobReceiver, NULL, jobReceiver, NULL)) {
    perror("pthread_create");
    MPI_Finalize();
    exit(EXIT_FAILURE);
  }

  //Job dispatching thread
  if (pthread_create (&tjobDispatcher, NULL, jobDispatcher, NULL)) {
    perror("pthread_create");
    MPI_Finalize();
    exit(EXIT_FAILURE);
  }


  //job execution status receiver TODO: make this a generic PUM-status receiver (not only for job finish notifications)
  if (pthread_create (&tPUStatusReceiver, NULL, PUStatusReceiver, NULL)) {
    perror("pthread_create");
    MPI_Finalize();
    exit(EXIT_FAILURE);
  }


  receiveMsg(NULL, 0, MPI_BYTE, MPI_ANY_SOURCE, COMM_TAG_SHUTDOWN, MPI_STATUS_IGNORE);
  //TODO: cleanup PU structs on shutdown
  finalizeComponent("JS");
  return EXIT_SUCCESS;
}
