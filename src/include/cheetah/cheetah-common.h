/*
 * cheetah-common.h
 *
 * Common data to all components of the Cheetah Framework.
 * - includes, constants, variables, data types & function prototypes
 *
 */

#ifndef CHEETAH_COMMON_H_
#define CHEETAH_COMMON_H_

#define _GNU_SOURCE

/*--------------
    INCLUDES
 --------------*/
#include <CL/cl.h>
#include <mpi/mpi.h>

#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>
#include <pthread.h>
#include <assert.h>




/*--------------------
   GLOBAL DEFINITIONS
 ---------------------*/

/* Verbose, debug output TODO: make this configurable when components start */
#define ALLOW_DEBUG_MESSAGES true
#define ALLOW_INFO_MESSAGES true

#define SILENT true //SET TO TRUE WHEN RUNNING BENCHMARKS TODO: deprecate this
                    //When true, only job allocation decisions by the JS are
                    //printed *while* jobs are being processed.

//Additional debug messages for each component TODO
#define debug_JM  false
#define debug_JS  false
#define debug_PUM false
#define debug_RC  false


/* Communication tags used for MPI message passing */
#define COMM_TAG_PUONLINE       0 //PU-M -> JS  : Initial notification
#define COMM_TAG_JOBSUBMIT      1 //JM   -> JS  : Job submission
#define COMM_TAG_JOBSEND        2 //JS   -> PU-M: Job transmission
#define COMM_TAG_PUSTATUS       3 //JS   -> PU-M: Feedback request
#define COMM_TAG_RESULTSEND     4 //PU-M -> RC  : Result transmission
#define COMM_TAG_RESULTGET      5 //JM   -> RC  : Result request
#define COMM_TAG_RESULTREGISTER 6 //JM   -> RC  : Result availability
                                  //              notification request
#define COMM_TAG_SHUTDOWN       7 //JM   -> ALL : Request clean shutdown to
                                  //              all components


/* Return codes for Jobs */
#define JOB_RETURN_STATUS_SUCCESS 0
#define JOB_RETURN_STATUS_FAILURE 1



/*** JOB SCHEDULER <-> PU-MANAGER ***/

/* Amount of data transferred from the JS to a PU-M during the initial
 * registration phase to determine link throughput
 */
#define PUM_THROUGHPUT_TEST_SIZE 1048576 //bytes


/* Number of 'ping' requests sent from the JS to a PU-M
 * during the registration phase
 */
#define NUM_JS_LATENCY_TESTS 10




/*----------------------------------------
   AUXILIARY DEFINITIONS FOR THIS LIBRARY
 -----------------------------------------*/

/* Only used on libcheetah-common.c */
#define SLEEP_TIME 1//in useconds




/*------------------------------------
   COMMON VARIABLES TO ALL COMPONENTS
 -------------------------------------*/

/* Only used on libcheetah-common.c */
pthread_mutex_t shutdown_mutex;
pthread_cond_t shutdown_condition;
bool shutdown;


/*** WORKAROUNDS (TODO: ASSERT THEY ARE STILL NEEDED) ***/
/* This is currently needed due to a bug on MPICH.
 * Should be removed on later releases.
 */
extern MPI_Request aux;
extern MPI_Request *NULL_REQUEST;


/* Array for command-line arguments to the components.
 * TODO: make this more flexible so that, aside from these global options,
 * each component can also have specific ones
 */
extern struct option long_options[];


/* Variables defined by each component */
int myid;             //The MPI rank of the current process
char *componentName;  //The name of the component for the current process
                      //JM, JS, PU-M or RC
int shutdown_threads; //Number of total threads launched by the component.
                      //This is looked upon during the shutdown sequence to make
                      //sure all threads have finished before exiting the main
                      //program



/*------------------------
   COMMON DATA STRUCTURES
 -------------------------*/

/* Used by the JobScheduler and the PUManager */
typedef struct {
  cl_device_type device_type;              //8 byte//CL_DEVICE_TYPE_CPU, CL_DEVICE_TYPE_GPU
  cl_uint clock_frequency;                 //4     //Mhz
  cl_ulong max_memory_alloc;               //8     //Byte
  cl_device_mem_cache_type cache_type;     //4     //CL_NONE, CL_READ_ONLY_CACHE, CL_READ_WRITE_CACHE
  //TODO: global cache size
  cl_ulong global_mem_size;
  cl_device_local_mem_type local_mem_type; //4     //CL_LOCAL, CL_GLOBAL
  cl_ulong local_mem_size;                 //8     //Byte
  cl_uint max_compute_units;

  cl_uint max_work_item_dimensions;        //1, 2 or 3
  size_t max_work_item_sizes[3];           //(3*)8
  size_t max_work_group_size;
  size_t max_parameter_size;

  double latency;
  double throughput;//TODO: replace with benchmarked FLOPS
  double bandwidth;
  int nTimesUsed;//meh
  bool reserved; //meh
} PUProperties;


/* Used by the PUManager */
typedef struct {
  int id;
  int nPUs;
  PUProperties *availablePUs;
  cl_context **PUsContexts;   //allocated at start of exec
  cl_command_queue **PUsCmdQs;//allocated at start of exec
  cl_mem **argBuffers;        //allocated when respective kernel should start
  cl_kernel *currKernels;     //allocated when respective kernel should start
} PUMStruct;


/* Types of arguments for Jobs */
typedef enum {
  INPUT,
  OUTPUT,
  INPUT_OUTPUT,
  EMPTY_BUFFER
} argument_type;


/* A Job as submitted by the JobManager to the Framework
 *
 *     NEVER FORGET TO INSTANTIATE _ALL_ MEMBERS
 */

typedef struct {
  int probID;
  int jobID;

  bool required_is_set;
  cl_device_type requirePU; //Do not exec job if this PU is not available

  int nPreferPUs;
  int nAllowPUs;
  int nAvoidPUs;
  int nForbidPUs;

  cl_device_type *preferPUs;
  cl_device_type *allowPUs;  // [list of PUs (CPU, GPU, ...)]
  cl_device_type *avoidPUs;
  cl_device_type *forbidPUs;

  int category;

  //OpenCL data-parallel programming support
  int nDimensions;
  int nItems[3];
  int nGroupItems[3];

  int startingNameSize;
  char *startingKernel;

  int taskSourceSize; //number of chars
  char *taskSource;

  int nTotalArgs;
  argument_type *argTypes;

  int *argSizes;    //size of the arguments in bytes. ex.: {10,4,20}
  void **arguments; // ex.: {10byte vals, 4 byte vals, 20 byte vals}

  int returnTo; //The rank of the ResultsCollector this Job's results
                //will be returned to
} Job;


/* A Job as sent from the JobScheduler to the PUManager
 */
typedef struct {
  int probID;
  int jobID;

  int runOn; //The ID of the PU to run on

  int category;

  int nDimensions;
  int nItems[3];
  int nGroupItems[3];

  int startingNameSize;
  char *startingKernel;

  int taskSourceSize;
  char *taskSource;


  int nTotalArgs;
  argument_type *argTypes; //INPUT arguments must be initialized with correct
                           //contents. OUTPUT-only arguments may be
                           //uninstantiated (see argument_type).
  int *argSizes;
  void **arguments;

  int returnTo;
} JobToPUM;


/* The Results of a Job. Tranferred from the PUManager to the ResultsCollector
 * and from the ResultsCollector to the JobManager.
 */
typedef struct {
  int probID;
  int jobID;

  int nTotalResults;

  int *resultSizes;
  void **results;

  int returnStatus;

} JobResults;


/* Notification message sent from the PUManager to the JobScheduler upon
 * completing the execution of a Job.
 */
typedef struct {
  int category;
  int ranAtPU;
  bool succeeded;
  double overheads;
  double executionTime;
//  double resultFlushTime;//TODO
} finishedJobNotification;



/*-----------------------------
   COMMON, AUXILIARY FUNCTIONS
 ------------------------------*/

void initializeComponent (char *name, char *shortname, int argc, char *argv[]);
void finalizeComponent ();
void finalizeThread ();


/* Sleep "exactly" msec milliseconds. */
void guaranteedSleep(int msec);


/* Blocking send that releases the CPU.
 * TODO: currently only uses MPI_COMM_WORLD.
 */
void sendMsg    (void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Status *status);


/* Blocking receive that releases the CPU.
 * TODO: currently only uses MPI_COMM_WORLD
 */
void receiveMsg (void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Status *status);


/* Functions to print messages to the console */
void cheetah_print            (char *message, ...);
void cheetah_print_error      (char *message, ...);
void cheetah_info_print       (char *message, ...);
void cheetah_info_print_error (char *message, ...);
void cheetah_debug_print      (char *message, ...);
void cheetah_debug_print_error(char *message, ...);


/* Functions to print Job contents. */
void printJob               (Job *printThis);
void printJobToPUM     (JobToPUM *printThis);
void printPUMStruct   (PUMStruct *printThis);

#define NUM_PRINT1DARRAY_ELEMS 10
void print1DArray    (const void* arrayData, const unsigned int length);
void printJobResults (JobResults *printThis); //Currently used by the PUManager
                                              //and the ResultsCollector.

/* Converts the contents of a file into a string.
 * Returns NULL in case of failure.
 */
char *fileToString     (const char *filename);


/* Returns the number of seconds elapsed between y and x */
int timeval_subtract (struct timeval *result, struct timeval *x, struct timeval * y);




#endif /* CHEETAH_COMMON_H_ */
