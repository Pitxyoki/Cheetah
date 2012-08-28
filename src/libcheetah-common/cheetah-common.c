/*
 * cheetah-common.c
 *
 * Common data to all components of the Cheetah Framework.
 * - includes, constants, variables, data types & function prototypes
 *
 *  First created on: 30 Mar 2010
 *      Author: Luís Miguel Picciochi de Oliveira (Pitxyoki@Gmail.com)
 *
 *
 */


#include "cheetah/cheetah-common.h"
#include <errno.h>
#include <stdarg.h>


/*------------------------------------
   COMMON VARIABLES TO ALL COMPONENTS
 -------------------------------------*/

/* The shutdown procedure works as follows:
 * A component can be shut down while it is doing anything except when it is
 * exchanging messages with other components. This is done only to avoid
 * data to be left hanging around in the mpi daemons after programs exit.
 */
/**/
pthread_mutex_t shutdown_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  shutdown_condition = PTHREAD_COND_INITIALIZER;
bool shutdown = false;
int shutdown_threads = 0;


/* This is currently needed due to a bug on MPICH.
 * Should be removed on later releases.
 */
MPI_Request aux;
MPI_Request *NULL_REQUEST = &aux;


/* Array for command-line arguments to the components.
 * TODO: make this more flexible so that, aside from these global options,
 * each component can also have specific ones
 */
struct option long_options[] = {
    //If the order of these arguments is changed,
    //each component must be manually updated
    {"job-manager",      required_argument, NULL, 0}, //0
    {"job-scheduler",    required_argument, NULL, 0}, //1
    {"pu-manager",       required_argument, NULL, 0}, //2
    {"result-collector", required_argument, NULL, 0}, //3

    {"global-items",     required_argument, NULL, 0}, //4
    {"items-per-group",  required_argument, NULL, 0}, //5
    {0, 0, 0, 0}
};



/*-----------------------------
   COMMON, AUXILIARY FUNCTIONS
 ------------------------------*/



void initializeComponent (char *name, char *shortname, int argc, char *argv[]) {
  componentName = shortname;
  int prov;
  int namelen;
  char processorname[MPI_MAX_PROCESSOR_NAME];

  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &prov);
  MPI_Comm_rank(MPI_COMM_WORLD, &myid);
  MPI_Get_processor_name(processorname, &namelen);

  printf("%s started at %s (rank %i). Thread support level: %i (asked %i). Setting up internal state...\n", name, processorname, myid, prov, MPI_THREAD_MULTIPLE);
  MPI_Irecv(NULL, 0, MPI_BYTE, MPI_ANY_SOURCE, COMM_TAG_SHUTDOWN, MPI_COMM_WORLD, &shutdown_request);
}

void finalizeComponent () {
/*  char *space = calloc((4-strlen(name))+1, sizeof(char));
  for (int i = 0; i< 4-strlen(name); i++)
    space[i] = ' ';
  space[4-strlen(name)] = '\0';

  if (fprintf(stderr, "%s%s(%i): Shutting down...\n", name, space, myid) < 0)
      perror("fprintf");
  shutdown = true;
  free(space);*/
  int gotmsg = false;
  while (!gotmsg) {

    MPI_Test(&shutdown_request, &gotmsg, MPI_STATUS_IGNORE);
    guaranteedSleep(SLEEP_TIME);
  }

  cheetah_print("Shutting down...");

  shutdown = true;
  if (pthread_mutex_lock(&shutdown_mutex)) {
      perror("pthread_mutex_lock");
  }

  while(shutdown_threads > 0) {
    if (pthread_cond_wait(&shutdown_condition, &shutdown_mutex))
        perror("pthread_cond_wait");
  }

  if (pthread_mutex_unlock(&shutdown_mutex)) {
      perror("pthread_mutex_unlock");
  }


  MPI_Finalize();
}

void finalizeThread() {

  if (shutdown) {
    if (pthread_cond_signal(&shutdown_condition)) {
      perror("pthread_cond_signal");
    }

    //TODO: cleanup thread's structures?
    pthread_exit(EXIT_SUCCESS);
  }

}



/* Sleep "exactly" msec milliseconds.
 * Threads can be finalized at this moment.
 * Source: http://cc.byexamples.com/2007/05/25/nanosleep-is-better-than-sleep-and-usleep/
 */
void guaranteedSleep(int msec) {
  if (shutdown) {
    if (pthread_mutex_lock(&shutdown_mutex))
      perror("pthread_mutex_lock");
    shutdown_threads--;
    if (pthread_mutex_unlock(&shutdown_mutex))
      perror("pthread_mutex_unlock");

    finalizeThread();
  }

  struct timespec timeout0;
  struct timespec timeout1;
  struct timespec* tmp;
  struct timespec* t0 = &timeout0;
  struct timespec* t1 = &timeout1;

  t0->tv_sec = msec / 1000;
  t0->tv_nsec = (msec % 1000) * (1000 * 1000);

  while ((nanosleep(t0, t1) == (-1)) && (errno == EINTR)) {
    tmp = t0;
    t0 = t1;
    t1 = tmp;
  }
/*
 * Old Implementation:
  struct timespec time;
  time.tv_sec = msec / 1000;
  time.tv_nsec = (msec % 1000) * (1000 * 1000);
  nanosleep(&time,NULL);*/
}


/* Blocking send that releases the CPU.
 * TODO: currently only uses MPI_COMM_WORLD.
 */
void sendMsg (void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Status *status) {
  if (pthread_mutex_lock(&shutdown_mutex))
    perror("pthread_mutex_lock");
  shutdown_threads++;
  if (pthread_mutex_unlock(&shutdown_mutex))
    perror("pthread_mutex_unlock");


  MPI_Request req;
  int gotmsg = false;

  MPI_Isend(buf, count, datatype, dest, tag, MPI_COMM_WORLD, &req);

  while (!gotmsg) {

    MPI_Test(&req, &gotmsg, status);
    guaranteedSleep(SLEEP_TIME);
  }

  if (pthread_mutex_lock(&shutdown_mutex))
    perror("pthread_mutex_lock");
  shutdown_threads--;
  if (pthread_mutex_unlock(&shutdown_mutex))
    perror("pthread_mutex_unlock");

}


/* Blocking receive that releases the CPU.
 * TODO: currently only uses MPI_COMM_WORLD
 */
void receiveMsg (void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Status *status) {
  if (pthread_mutex_lock(&shutdown_mutex))
    perror("pthread_mutex_lock");
  shutdown_threads++;
  if (pthread_mutex_unlock(&shutdown_mutex))
    perror("pthread_mutex_unlock");


  MPI_Request req;
  int gotmsg = false;

  MPI_Irecv(buf, count, datatype, source, tag, MPI_COMM_WORLD, &req);

  while (!gotmsg) {

    MPI_Test(&req, &gotmsg, status);
    guaranteedSleep(SLEEP_TIME);
  }

  if (pthread_mutex_lock(&shutdown_mutex))
    perror("pthread_mutex_lock");
  shutdown_threads--;
  if (pthread_mutex_unlock(&shutdown_mutex))
    perror("pthread_mutex_unlock");

}


/* Functions to print messages to the console */
void cheetah_print_stream (FILE *stream, char *message, va_list *ap) {
  char *formatString; //A format string of type 'COMPONENT (ID): MESSAGE\n'
  char *space;        //The white space between 'COMPONENT' and '(ID)'
  int stringSize;


  // 1. Determine the amount of white space to use
  if (strlen(componentName) < 5) {
    space = calloc((5-strlen(componentName))+1, sizeof(char));
    if (space == NULL) {
      perror("calloc");
      return;
    }

    for (int i = 0; i < 5-strlen(componentName); i++) {
      space[i] = ' ';
    }
    space[5-strlen(componentName)] = '\0';
  }
  else { // componentName == 'XPTOABCD'
    space = calloc(2, sizeof(char));
    if (space == NULL) {
      perror("calloc");
      return;
    }
    space[0] = ' ';
    space[1] = '\0';
  }


  //2. determine the size of the format string
  stringSize = strlen(componentName); //JM, JS, PUM, RC
  if (stringSize < 5) {
    stringSize = 4;
  }

  stringSize += strlen(space); // ' '
  stringSize += 8;             // '(0000): ' support up to 9999 MPI IDs (...heh)
  stringSize += strlen(message);
  stringSize += 2;             // '\n\0'


  //2. allocate space for the format string to be used
  formatString = calloc(stringSize, sizeof(char));
  if (formatString == NULL) {
    perror("calloc");
    return;
  }

  //3. print to the string, using vprintf and variadic arguments received for
  //formatting
  if (sprintf(formatString, "%s%s(%i): %s\n", componentName, space, myid, message) < 0) {
    perror("sprintf");
    return;
  }

  if (vfprintf(stream, formatString, *ap) < 0) {
    perror("vfprintf");
    return;
  }

  free(space);
  free(formatString);
}

void cheetah_print (char *message, ...) {
  va_list ap;
  va_start (ap, message);
  cheetah_print_stream(stdout, message, &ap);
  va_end(ap);
}

void cheetah_print_error(char *message, ...) {
  va_list ap;
  va_start (ap, message);
  cheetah_print_stream(stderr, message, &ap);
  va_end(ap);
}

void cheetah_info_print(char *message, ...) {
  if (!ALLOW_INFO_MESSAGES)
    return;
  va_list ap;
  va_start (ap, message);
  cheetah_print_stream(stdout, message, &ap);
  va_end(ap);
}

void cheetah_info_print_error(char *message, ...) {
  if (!ALLOW_INFO_MESSAGES)
    return;
  va_list ap;
  va_start (ap, message);
  cheetah_print_stream(stderr, message, &ap);
  va_end(ap);
}

void cheetah_debug_print(char *message, ...) {
  if (!ALLOW_DEBUG_MESSAGES)
    return;
  va_list ap;
  va_start (ap, message);
  cheetah_print_stream(stdout, message, &ap);
  va_end(ap);
}

void cheetah_debug_print_error(char *message, ...) {
  if (!ALLOW_DEBUG_MESSAGES)
    return;
  va_list ap;
  va_start (ap, message);
  cheetah_print_stream(stderr, message, &ap);
  va_end(ap);
}


/* Functions to print Job contents. */

void printJob (Job *printThis) {
  printf("Problem ID: %i\n", printThis->probID);
  printf("JobID: %i\n", printThis->jobID);
  printf("Require a specific PU? %s", !(printThis->required_is_set) ? "No.\n" : printThis->requirePU == CL_DEVICE_TYPE_CPU? "Yes, CPU.\n" : printThis->requirePU == CL_DEVICE_TYPE_GPU ? "Yes, GPU.\n" : "Yes, unsupported PU.\n");

  if (printThis->nPreferPUs > 0) {
    printf("Preferred PUs: ");
    for (cl_uint i = 0; i<printThis->nPreferPUs ; i++) {
      printf("%s", printThis->preferPUs[i] == CL_DEVICE_TYPE_CPU ? "CPU " : printThis->preferPUs[i] == CL_DEVICE_TYPE_GPU ? "GPU " : "Unsupported PU.");
      if (i+1<printThis->nPreferPUs)
        printf(", ");
    }
    printf(".\n");
  }


  if (printThis->nAllowPUs > 0) {
    printf("Allowed PUs: ");
    for (cl_uint i = 0; i<printThis->nAllowPUs ; i++) {
      printf("%s", printThis->allowPUs[i] == CL_DEVICE_TYPE_CPU ? "CPU " : printThis->allowPUs[i] == CL_DEVICE_TYPE_GPU ? "GPU " : "Unsupported PU.");
      if (i+1<printThis->nAllowPUs)
        printf(", ");
    }
    printf(".\n");
  }

  if (printThis->nAvoidPUs > 0) {
    printf("PUs to avoid: ");
    for (cl_uint i = 0; i<printThis->nAvoidPUs ; i++) {
      printf("%s", printThis->avoidPUs[i] == CL_DEVICE_TYPE_CPU ? "CPU " : printThis->avoidPUs[i] == CL_DEVICE_TYPE_GPU ? "GPU " : "Unsupported PU.");
      if (i+1<printThis->nAvoidPUs)
        printf(", ");
    }
    printf(".\n");
  }

  if (printThis->nForbidPUs > 0) {
    printf("Forbidden PUs: ");
    for (cl_uint i = 0; i<printThis->nForbidPUs ; i++) {
      printf("%s", printThis->forbidPUs[i] == CL_DEVICE_TYPE_CPU ? "CPU " : printThis->forbidPUs[i] == CL_DEVICE_TYPE_GPU ? "GPU " : "Unsupported PU.");
      if (i+1<printThis->nForbidPUs)
        printf(", ");
    }
    printf(".\n");
  }

  printf("Job's category: %i\n", printThis->category);



  printf("Starting kernel name size: %i\n", printThis->startingNameSize);
  printf("Starting kernel name: %s\n", printThis->startingKernel);
  printf("Task source size: %i\n", printThis->taskSourceSize);

  printf("Task source ommited (actual size: %ul+1).\n", strlen(printThis->taskSource));

  printf("Number of arguments: %i\n", printThis->nTotalArgs);

  for (cl_uint i = 0; i< printThis->nTotalArgs; i++) {
    printf("Argument %i: %s, size: %i\n",
        i,
        printThis->argTypes[i] == INPUT ? "INPUT" : printThis->argTypes[i] == OUTPUT ? "OUTPUT" : printThis->argTypes[i] == INPUT_OUTPUT ? "INPUT-OUTPUT" : printThis->argTypes[i] == EMPTY_BUFFER ? "EMPTY_BUFFER" : "",
        printThis->argSizes[i]);
  }

  for (cl_uint i = 0; i < printThis->nTotalArgs; i++)
    printf("buffers[%i] = %f\n", i, *((float *) printThis->arguments[i]));

  printf("Return the results to RC with ID: %i\n", printThis->returnTo);

}


void printJobToPUM (JobToPUM *printThis) {
  printf("Problem ID: %i\n", printThis->probID);
  printf("JobID: %i\n", printThis->jobID);

  printf("Run on this PU: %i\n", printThis->runOn);
  printf("Starting kernel name size: %i\n", printThis->startingNameSize);
  printf("Starting kernel name: %s\n", printThis->startingKernel);
  printf("Task source size: %i\n", printThis->taskSourceSize);

//  printf("Task source: %s\n", printThis.taskSource);
  printf("Task source ommited (actual size: %ul+1).\n", strlen(printThis->taskSource));
  printf("Number of arguments: %i\n", printThis->nTotalArgs);

  for (cl_uint i = 0; i< printThis->nTotalArgs; i++) {
    printf("Argument %i: %s, size: %i\n",
        i,
        printThis->argTypes[i] == INPUT ? "INPUT" : printThis->argTypes[i] == OUTPUT ? "OUTPUT" : printThis->argTypes[i] == INPUT_OUTPUT ? "INPUT-OUTPUT" : printThis->argTypes[i] == EMPTY_BUFFER ? "EMPTY_BUFFER" : "",
        printThis->argSizes[i]);
  }


  for (cl_uint i = 0; i < printThis->nTotalArgs; i++) {
    if (printThis->argTypes[i] == OUTPUT)
      printf("buffers[%i] = OUTPUT BUFFER\n", i);
    else if (printThis->argTypes[i] == EMPTY_BUFFER)
      printf("buffers[%i] = EMPTY_BUFFER\n", i);
    else
      printf("buffers[%i] = %f\n", i, *((float *) printThis->arguments[i]));
  }


  printf("Return the results to RC with ID: %i\n", printThis->returnTo);

}


void printPUMStruct (PUMStruct *printThis) {
  printf("Setup Struct Data:\n"
          " ID:   %i\n"
          " nPUs:   %i\n",
          printThis->id, printThis->nPUs);

  for (cl_uint i = 0; i <printThis->nPUs; i++){
    printf("PUM %i has a", printThis->id);
    switch (printThis->availablePUs[i].device_type) {
      case CL_DEVICE_TYPE_CPU:
        printf(" CPU @ %uMhz\n", printThis->availablePUs[i].clock_frequency);
        break;
      case CL_DEVICE_TYPE_GPU:
        printf(" GPU @ %uMhz\n", printThis->availablePUs[i].clock_frequency);
        break;
      default:
        printf("n unsupported PU (%lu).\n", printThis->availablePUs[i].device_type);
        break;
    }

    printf("Max memory per argument: %lu\n", printThis->availablePUs[i].max_memory_alloc);
    printf("cache type: ");
    switch (printThis->availablePUs[i].cache_type) {
      case CL_NONE:
        printf("no cache.\n");
        break;
      case CL_READ_ONLY_CACHE:
        printf("read-only.\n");
        break;
      case CL_READ_WRITE_CACHE:
        printf("read-write.\n");
        break;
      default:
        printf("unexpected value.\n");
        return;
    }

    printf("Global memory size: %lu\n", printThis->availablePUs[i].global_mem_size);
    printf("Local Memory type: %i (%s).\n", printThis->availablePUs[i].local_mem_type, printThis->availablePUs[i].local_mem_type == CL_LOCAL? "Local" : printThis->availablePUs[i].local_mem_type == CL_GLOBAL ? "Global" : "Unexpected value");

    printf("Local Memory size: %lu\n", printThis->availablePUs[i].local_mem_size);
    printf("Max compute units: %u\n", printThis->availablePUs[i].max_compute_units);
    printf("Max work-item dimensions: %u\n", printThis->availablePUs[i].max_work_item_dimensions);
    printf("  Max work item sizes: ");
    for (cl_uint j = 0; j < printThis->availablePUs[i].max_work_item_dimensions; j++)
      printf("%zi ", printThis->availablePUs[i].max_work_item_sizes[j]);

    printf("\nMax Work-group size: %zi\n", printThis->availablePUs[i].max_work_group_size);
    printf("Max Parameter size: %zi\n", printThis->availablePUs[i].max_parameter_size);

    printf("Detected latency: %f\n", printThis->availablePUs[i].latency);
    printf("Detected throughput: %f\n", printThis->availablePUs[i].throughput);
  }
}


/*
 * \brief Print no more than 256 elements of the given array.
 *
 *        Print Array name followed by elements.
 *
 * Adapted by Pitxyoki from AMD's Stream SDK v2.01
 */
void print1DArray(const void *arrayData, const unsigned int length) {
  cl_uint i;
  cl_uint numElementsToPrint = (NUM_PRINT1DARRAY_ELEMS < length) ? NUM_PRINT1DARRAY_ELEMS : length;

  int *pthis = (int *) arrayData;

  for(i = 0; i < numElementsToPrint; ++i) {
    printf("%i ", pthis[i]);
  }
  printf("\n");

}


void printJobResults (JobResults *printThis) {
  printf("Problem ID: %i\n", printThis->probID);
  printf("JobID: %i\n", printThis->jobID);

  printf("\nNumber of Results: %i\n", printThis->nTotalResults);

  for (cl_uint i = 0; i < printThis->nTotalResults; i++) {
    printf("results[%i] = ", i);
    print1DArray(printThis->results[i], NUM_PRINT1DARRAY_ELEMS);
  }
  printf("Return status: %s\n", printThis->returnStatus == JOB_RETURN_STATUS_SUCCESS ? "Success" : "Failure");
  printf("End of job results.\n");

}



void closeFile (FILE *f, const char *filename) {
  if (fclose(f) == EOF) {
    perror("fclose");
    fprintf(stderr, "...while trying to close %s.\n", filename);
  }
}

/* Converts the contents of a file into a string.
 * Returns NULL in case of failure.
 */
char *fileToString (const char *filename) {
  size_t size;
  char *str;
  FILE *f = fopen(filename,"r");
  if (f == NULL) {
      perror("fopen");
      fprintf(stderr,"...while trying to open %s.\n", filename);
      return NULL;
  }
  else {
    if (fseek(f, 0L, SEEK_END) == -1) {
      perror("ftell");
      fprintf(stderr,"...while trying to read %s,\n", filename);
    }
    size = ftell(f) + 1;
    if (size == -1) {
      perror("ftell");
      fprintf(stderr,"...while trying to read %s,\n", filename);
    }
    if (fseek(f, 0L, SEEK_SET) == -1) {
      perror("ftell");
      fprintf(stderr,"...while trying to read %s,\n", filename);
    }

    str = calloc(size, sizeof(char));
    if (str == NULL) {
      perror("calloc");
      closeFile(f, filename);
      return NULL;
    }

    fread(str, size-1, sizeof(char), f);
    closeFile(f, filename);

    str[size-1] = '\0';

    return str;
  }
}


/*
 * Returns the number of seconds elapsed between y and x
 * Source: http://www.gnu.org/software/libtool/manual/libc/Elapsed-Time.html
 */
int timeval_subtract (struct timeval *result, struct timeval *x, struct timeval * y) {
  /* Perform the carry for the later subtraction by updating y. */
  if (x->tv_usec < y->tv_usec) {
    int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
    y->tv_usec -= 1000000 * nsec;
    y->tv_sec += nsec;
  }
  if (x->tv_usec - y->tv_usec > 1000000) {
    int nsec = (x->tv_usec - y->tv_usec) / 1000000;
    y->tv_usec += 1000000 * nsec;
    y->tv_sec -= nsec;
  }

  /* Compute the time remaining to wait.
     tv_usec is certainly positive. */
  result->tv_sec = x->tv_sec - y->tv_sec;
  result->tv_usec = x->tv_usec - y->tv_usec;

  /* Return 1 if result is negative. */
  return x->tv_sec < y->tv_sec;
}


