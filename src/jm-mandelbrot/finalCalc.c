/*
* Cálculo da superfície de Mandelbrot
* Cria N jobs, dividindo a superfície em partes iguais
*/


#include "distribCL.h"

int NJOBS = 1;

int WAIT_TIME = 10;

Job **job;//[NJOBS];

JobResults **jobRes;//[NJOBS];


int nPUs = 0;
pthread_cond_t profile_over = PTHREAD_COND_INITIALIZER;
int profilesReceived = 0;
bool GPUs;

bool program_over = false;
pthread_mutex_t end_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t end_condition = PTHREAD_COND_INITIALIZER;
int nArrivals = 0;





struct timeval start, end, result;

//tempos de execução sequencial (um único processador a ser usado) no meu portátil
//0.8s (sequencial)
//#define MAXITER     256
//18s
//#define MAXITER     49192
//24s
//#define MAXITER     65536
//48s
//#define MAXITER    131072
//~3m10
//#define MAXITER    524288
//~3m35
#define MAXITER    1048576

#define WIDTH 10240
#define HEIGHT 10240
int jobHeight;// = HEIGHT / NJOBS;
int jobWidth;// = WIDTH;
int rgb[MAXITER][3];
int groupSize;

void savePic () {

  if (gettimeofday(&end, NULL)) { //TODO a ver: clock_getres
    perror("gettimeofday");
    return;
  }
  timeval_subtract(&result, &end, &start);

  double timeWaited = result.tv_sec + (result.tv_usec / 1000000.0);
  printf("JM  (%i): Done! Time waited: %f.\n", myid, timeWaited);

//  for (int i = 0; i < NJOBS; i++)
//    fprintf(stderr,"JM  (%i): Number of iterations: %i\n", myid, *((int *) jobRes[i]->results[0]));



  printf("JM  (%i): Saving image.\n", myid);
  FILE *fd = fopen ("file.ppm", "w");
  if (fd == NULL) {
    fprintf (stderr,"Couldn't open 'file.ppm' for writing! Aborting...\n");
    exit (EXIT_FAILURE);
  }


  int *p;

  fprintf (fd, "P6\n%d %d\n255\n", WIDTH, HEIGHT);
  for (int j = 0; j < NJOBS; j++) {

    p = (int *) jobRes[j]->results[0];

    for (int i = 0; i < WIDTH*jobHeight; i++) {
      fputc(rgb[p[i]][0], fd);
      fputc(rgb[p[i]][1], fd);
      fputc(rgb[p[i]][2], fd);
    }
  }

  fclose (fd);


  printf("JM  (%i): All jobs done.\n", myid);

  for (int i = 0; i < NJOBS; i++) {
    deleteJob(job[i]);
    deleteResults(jobRes[i]);
  }



  pthread_mutex_lock(&end_mutex);
  program_over = true;
  pthread_cond_signal(&end_condition);
  pthread_mutex_unlock(&end_mutex);
}

void resultAvailable (int jobID) {
  pthread_mutex_lock(&end_mutex);
  if (jobID < 0) {
    Job tmpJob;
    tmpJob.jobID = jobID;
    tmpJob.returnTo = defaultRCID;
    JobResults *jr = getResults(&tmpJob);
    printf("JM  (%i): got profiling result number %i\n", myid, jobID);
    deleteResults(jr);
    printf("JM  (%i): deleted profiling result number %i.\n", myid, jobID);
    profilesReceived++;
    pthread_cond_signal(&profile_over);
    pthread_mutex_unlock(&end_mutex);
    return;
  }
//  printf("JM  (%i): jobID %i's results arrived. GONNA FETCH'EM.\n", myid, jobID);

  jobRes[jobID] = getResults(job[jobID]);


//  printf("JM  (%i): jobID %i's results fetched. Job %s (%i).\n", myid, jobID,
//      jobRes[jobID]->returnStatus == JOB_RETURN_STATUS_FAILURE ? "FAILED to execute" : "executed successfully",
//      jobRes[jobID]->returnStatus);

  if (jobRes[jobID]->returnStatus == JOB_RETURN_STATUS_FAILURE)
    exit (EXIT_FAILURE);


  nArrivals++;
  pthread_mutex_unlock(&end_mutex);

  if (nArrivals == NJOBS)
    savePic();
}






void init_rgb () {
  int i,j;
  srand (123);
  for (i = 1;  i < MAXITER-1;  i++ )
    for (j = 0;  j < 3;  j++ )
      rgb[i][j] = rand () % MAXITER;

  rgb[0][0] = rgb[0][1] = rgb[0][2] = 0;
  rgb[MAXITER-1][0] = rgb[MAXITER-1][1] = rgb[MAXITER-1][2] = MAXITER;
}




void runFJob() {
  int  w, h[NJOBS], n;
  float x1, y1[NJOBS], x2, y2[NJOBS];

  float diff = ((1.0*jobHeight) / HEIGHT) * 0.01;
  x1 = 0.40; //left

  y1[0] = 0.20; //up
  for (int i = 1; i < NJOBS; i++)
    y1[i] = y1[i-1]+diff;//0.20828125;


  x2 = 0.41; //right


  y2[0] = 0.20 + diff;//down
  for (int i = 1; i < NJOBS; i++)
    y2[i] = y2[i-1]+diff;



  w  = WIDTH;  //  -
  for (int i = 0; i < NJOBS; i++)
    h[i] = jobHeight;//-   +




  n  = MAXITER;//  +


  init_rgb();

  printf ("x1=%f y1=%f x2=%f y2=%f w=%d h=%d n=%d file=%s\n",
      x1, y1[0], x2, y2[1], w, h[0], n, "file.ppm");



  nGlobItems[0] = 512;
  nGlobItems[1] = jobHeight < 512 ? jobHeight : 512;
  int nGlobItemsProfile[2] = {512,512};
  nItemsGroup[0] = 8;
  nItemsGroup[1] = (jobHeight % 8) == 0 ? 8 : (jobHeight % 4 == 0) ? 4 : (jobHeight % 2 == 0) ? 2 : 1;
  int nItemsGroupGPU[2];
  nItemsGroupGPU[0] = nItemsGroupGPU[1] = groupSize;

  //jobs for initial profiling
/*  float y1Prof=0.20,  y2Prof=0.21;
  int   wProf =1024,  hProf =1024,  nProf = 1048576;

  for (int i = 0; i < nPUs ; i++) {
    Job *job = createJob();
    setJobID(job, -1-i);
    setAllowPU(job, CL_DEVICE_TYPE_CPU);
    if (GPUs)
      setAllowPU(job, CL_DEVICE_TYPE_GPU);
    setJobCategory(job, 1);
    loadSourceFile(job, "mandel.cl");
    setStartingKernel(job, "calc_tile");

    setArgument(job, INPUT,  1 * sizeof(float), &x1);
    setArgument(job, INPUT,  1 * sizeof(float), &y2Prof);
    setArgument(job, INPUT,  1 * sizeof(float), &x2);
    setArgument(job, INPUT,  1 * sizeof(float), &y1Prof);
    setArgument(job, INPUT,  1 * sizeof(int), &wProf);
    setArgument(job, INPUT,  1 * sizeof(int), &hProf);
    setArgument(job, INPUT,  1 * sizeof(int), &nProf);
    setArgument(job, OUTPUT, wProf*hProf * sizeof(int), NULL);

    setDimensions(job, 2, nGlobItemsProfile, nItemsGroup);
    setResultCollector(job, defaultRCID);
    requestResultNotification(job);

    if (i == 0) {
      sleep(50);
//      printf("Type something to send the first PROFILING job.\n");
//      char asd[100];
//      scanf("%s",asd);
    }
    sendJobToExec(job, defaultSchedID);
  }

  printf("JM  (%i): Sent all PROFILING jobs.\n", myid);
  //jobs for actual work
*/sleep(50);
  for (int i = 0; i < NJOBS; i++) {

    job[i] = createJob();


    setJobID(job[i], i);

    setAllowPU(job[i], CL_DEVICE_TYPE_CPU);
    if (GPUs)
      setAllowPU(job[i], CL_DEVICE_TYPE_GPU);

    setJobCategory(job[i], 1);
    loadSourceFile(job[i], "mandel.cl");
    setStartingKernel(job[i], "calc_tile");


    setArgument(job[i], INPUT,  1 * sizeof(float), &x1);
    setArgument(job[i], INPUT,  1 * sizeof(float), &(y2[(NJOBS-1)-i]));
    setArgument(job[i], INPUT,  1 * sizeof(float), &x2);
    setArgument(job[i], INPUT,  1 * sizeof(float), &(y1[(NJOBS-1)-i]));
    setArgument(job[i], INPUT,  1 * sizeof(int), &w);
    setArgument(job[i], INPUT,  1 * sizeof(int), &(h[i]));
    setArgument(job[i], INPUT,  1 * sizeof(int), &n);
    setArgument(job[i], OUTPUT, w*(h[i]) * sizeof(int), NULL);

    setDimensions(job[i], 2, nGlobItems, nItemsGroup);
    setResultCollector(job[i], defaultRCID);
    requestResultNotification(job[i]);
  }

  pthread_mutex_lock(&end_mutex);
//  while(profilesReceived != nPUs)
//    pthread_cond_wait(&profile_over, &end_mutex);
  pthread_mutex_unlock(&end_mutex);
  //char asd[100];
  sleep(7);
  printf("\n\n\n#####PROFILING OVER#####\n");// Type something to send the first (real) job.\n");
  //      scanf("%s",asd);
  sleep(2);
  if (gettimeofday(&start, NULL)) {
    perror("gettimeofday");
    exit (EXIT_FAILURE);
  }

//  printf("JM: Sending now!\n");
  for (int i = 0; i < NJOBS; i++)
    sendJobToExec(job[i], defaultSchedID);


}


int main(int argc, char **argv) {
  NJOBS = atoi(argv[1]);
  if (strcmp(argv[2],"G") == 0)
    GPUs = true;
  else GPUs = false;

  jobHeight = HEIGHT / NJOBS;
  jobWidth = WIDTH;
  initDistribCL(argc-2, &(argv[2]));

  MPI_Comm_size(MPI_COMM_WORLD, &nPUs);
  fprintf(stderr,"nPUs acquired: %i.\n",nPUs);
  nPUs -= 3;//JM,JS,RC
  if (GPUs)
    nPUs += 5; //additional GPUs
  job = calloc(NJOBS , sizeof(Job *));
  jobRes = calloc(NJOBS , sizeof(JobResults *));


  runFJob();




  pthread_mutex_lock(&end_mutex);
  while(!program_over)
    pthread_cond_wait(&end_condition, &end_mutex);
  pthread_mutex_unlock(&end_mutex);
  fprintf(stderr, "signaled!\n");
  quitDistribCL();
  return EXIT_SUCCESS;
}
