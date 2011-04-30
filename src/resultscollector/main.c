/*
 * Result Collector
 *
 *  Created on: 16 Apr 2010
 *      Author: luis
 */

#include "rc.h"

void parseOpts (int argc, char *argv[]) {
  int option_index;

  while ( getopt_long_only(argc, argv, "", long_options, &option_index) != -1) {
    //what do we need?
  }
}


int main (int argc, char *argv[]) {
  initializeComponent("Result Collector", "RC", argc, argv);
  parseOpts(argc, argv);


  pthread_t tresultReceiver, tresultNotifier, tresultSendThread;

  //Result receiving thread
  if (pthread_create(&tresultReceiver, NULL, resultReceiver, NULL)) {
    perror("pthread_create");
    MPI_Finalize();
    exit(EXIT_FAILURE);
  }

  //Result reception notification thread
  if (pthread_create(&tresultNotifier, NULL, resultNotifRegister, NULL)) {
    perror("pthread_create");
    MPI_Finalize();
    exit(EXIT_FAILURE);
  }

  //Result sending thread
  if (pthread_create(&tresultSendThread, NULL, resultSender, NULL)) {
    perror("pthread_create");
    MPI_Finalize();
    exit(EXIT_FAILURE);
  }


  receiveMsg(NULL, 0, MPI_BYTE, MPI_ANY_SOURCE, COMM_TAG_SHUTDOWN, MPI_STATUS_IGNORE);
  finalizeComponent("RC");
  return EXIT_SUCCESS;
}
