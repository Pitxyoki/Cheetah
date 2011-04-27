#include "fdist.h"

#define NOC 200
#define VPNOC 300
//#define NJOBS 10 /*512000 iters for each job*/
//#define NJOBS 20 /*with 512000 total iters -> 256000 iters for each job*/
int NJOBS = 1;/*with  512000 total iters -> 20480 iters for each job
                   with 5120000 total iters-> 204800 iters for each job*/

//#define NGLOBITEMS 2000 //3.5GB
//#define NGLOBITEMS 1000 //1.9GB (GTX480 can handle up 1296)
int NTOTALGLOBITEMS = 512; //1.0GB

/*
 * 1 dimension: itno should be a multiple of NTOTALGLOBITEMS
 * NTOTALGLOBITEMS should be <= 1024
 *
 * 2 dimensions: NTOTALGLOBITEMS must be a square number < itno (at fdist_params2)
 * itno should be a multiple of NTOTALGLOBITEMS
 *
 * square numbers:
 *  1, 4, 9, 16, 25, 36, 49, 64, 81, 100, 121, 144, 169, 196, 225, 256
 * 289, 324, 361, 400, 441, 484, 529, 576, 625, 676, 729, 784, 841, 900
 * 961, 1024, 1089, 1156, 1225, 1296, 1369, 1444, 1521, 1600, 1681,
 * 1764, 1849, 1936, 2025, 2116, 2209, 2304, 2401
 *
 * Watch out: the greatest arguments are 0.286102MB * NTOTALGLOBITEMS large.
 *  1 glob item,
 *     iters:     1 =  83.658014s
 *  1296 glob items (max on GTX480)
 *     iters:  5184 =   93.038350s (4 iters / item)
 *     iters: 12960 =  106.700616s (10 iters / item)
 *     iters: 25920 =  132.505792s (20 iters / item)
 *  512 glob items,
 *     iters: 20480 =   77.594058s C-9 (40 iters / item) parallel
 *                     122.723485s GPU
 *  100 glob items
 *     iters: 20000 =   16.221594s (200 iters / item) SunFire
 *                      90.724585  C-9
 *                     207.834997  GPU
 * 1024 glob items
 *     iters: 20480 =   12.213640 (20 iters / item) SunFire
 *                      76.335816 C-9
 *                     123.413840 GPU
 *     iters: 51200 =  <20.634793 SunFire     //profile with this
 *                     <83.871620 pitxFCT-CPU
 *                     178.315872 C-9
 *                     193.125176 pitxFCT-GPU
 *
 *     iters:100000 =
 *                     173.764397s pitxFCT-CPU
 *                     355.843341s C-9
 *                     411.989062s pitxFCT-GPU
 *
 *           200000 =
 *                     313.916063 pitxFCT-CPU
 *                     647.093076 C-9
 *                     794.136032 pitxFCT-GPU
 *
 *     iters:204800 =   74.234825 SunFire     //run one of these on each PU
 *                     335.692265, 330.405271 pitxFCT-CPU
 *                     612.114851, 542.661103, 615.904403 GPU
 *                     700.251965, 649.648401, 771.630434 C-9
 *
 *                     2dims: 32x32
 *                     335.489401, pitxFCT-CPU
 *                     609.310731, pitxFCT-GPU
 *                     714.836812, C-9
 *
 *     iters:512000 =  163.580317 SunFire
 *                    1367.933553 pitxFCT-GPU
 *                    1699.054713 C-9
 *                 total(@home): 264.457727s (profiled with 51200, 20 jobs)
 *     iters:819200
 *                      265.218921s
 *                     1303.279946s, pitxFCT-CPU, 32x32 (1024 tot)
 *                     2136.586522s, pitxFCT-GPU, 32x32 (1024 tot)
 *                     2916.934271s, C-9
 *                     669.514466s total @ home, C-9, pitx-{C,G}PU
 *    iters:2048000
 *    iters:5120000 = 1616.448602 SunFire (~27 minutes) //to get this
 *                 total(@home): 1427.297634 (profiled with 51200, 25 jobs)
 *
 *   iters: 51200000= (max @ GTX480)
 * 1º enviar 1 job por PU com items 100, 2000 iterações (ver tempo que demora na GTX)
 */

int NDIMS = 1;


Job **job;
JobResults **jobRes;

pthread_mutex_t end_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t end_condition = PTHREAD_COND_INITIALIZER;
pthread_cond_t profile_over = PTHREAD_COND_INITIALIZER;
bool program_over = false;

int nArrivals = 0;
int profilesReceived = 0;

struct timeval start, end, result;




int itno;

void simulator(int itno, int Smp, int seed, int Ms,
    int initot, int* init, int Subs, int Spno, float efst,
//    int **val_arr, int **freq_arr,
    //out args
    float* fsum, float* wsum, float* fsts, float* h1s);


void mergeResults () {
  if (gettimeofday(&end, NULL)) {
    perror("gettimeofday");
    return;
  }
  timeval_subtract(&result, &end, &start);

  double timeWaited = result.tv_sec + (result.tv_usec / 1000000.0);
  printf("JM  (%i): Done! Time waited: %f.\n", myid, timeWaited);


  float fsum = 0;
  float wsum = 0;

  FILE *alls;
  alls = fopen("out.dat", "w");
  if (alls == NULL) {
    fprintf(stderr,"Could not open alleles file (\"out.dat\")!\n");
    exit (EXIT_FAILURE);
  }
  printf("\nold out.dat has now been lost\n");


  for (int i = 0; i < NJOBS; i++) {
    assert (jobRes[i]->resultSizes[0] == jobRes[i]->resultSizes[1]);
    assert ((jobRes[i]->resultSizes[0]  / sizeof(float)) == NTOTALGLOBITEMS);
    for (int j = 0; j < NTOTALGLOBITEMS; j++) {
      fsum += ((float *) jobRes[i]->results[0])[j];
      wsum += ((float *) jobRes[i]->results[1])[j];
    }

    assert (jobRes[i]->resultSizes[2] == jobRes[i]->resultSizes[3]);
    assert ((jobRes[i]->resultSizes[2] / sizeof(float)) == (itno/NJOBS));
    for (int j = 0; j < itno/NJOBS ; j++)
      fprintf(alls, "%f %f\n", ((float *) jobRes[i]->results[3])[j], ((float *) jobRes[i]->results[2])[j]);//h1s,fsts

    deleteResults(jobRes[i]);
    deleteJob(job[i]);
  }
  printf("average Fst is %f\n", fsum / wsum);


  fclose(alls);


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


  jobRes[jobID] = getResults(job[jobID]);

  if (!SILENT)
  printf("JM  (%i): jobID %i's results fetched. Job %s (%i).\n", myid, jobID,
      jobRes[jobID]->returnStatus == JOB_RETURN_STATUS_FAILURE ? "FAILED to execute" : "executed successfully",
      jobRes[jobID]->returnStatus);

  if (jobRes[jobID]->returnStatus == JOB_RETURN_STATUS_FAILURE)
    exit (EXIT_FAILURE);

  nArrivals++;
  pthread_mutex_unlock(&end_mutex);



  if (!SILENT){
  printf("Number of obtained results: %i\n", jobRes[jobID]->nTotalResults);

  printf(" Result sizes:");
  for (int i = 0; i < jobRes[jobID]->nTotalResults; i++)
    printf(" %i",jobRes[jobID]->resultSizes[i]);

/*  printf("\nObtained fsums (by %i items): ", nGlobItems[0]);
  for (int i = 0; i < nGlobItems[0]; i++) {
    printf(" %f", ((float *) jobRes[jobID]->results[0])[i]);

  }

  printf("\nObtained wsums: ");
  for (int i = 0; i < nGlobItems[0]; i++) {
    printf(" %f", ((float *) jobRes[jobID]->results[1])[i]);
  }
*/  printf("\n");
  }

  if (nArrivals == NJOBS)
    mergeResults();

}


int main(int argc, char **argv) {
//  NTOTALGLOBITEMS = atoi(argv[1]);
  bool useGPU = false;
//  if (*(argv[1]) == 'G') {
//    useGPU = true;
//  }
  itno = atoi(argv[1]);
  nItemsGroup[0] = atoi(argv[2]);
  job = calloc(NJOBS, sizeof(Job *));
  jobRes = calloc(NJOBS, sizeof(JobResults *));
  initDistribCL(argc-2, &(argv[2]));
  int nPUs;
  MPI_Comm_size(MPI_COMM_WORLD, &nPUs);
  nPUs -= 3;//JM,JS,RC
  if (useGPU)
    nPUs += 1;//additional GPUs


  cl_float efst;
  cl_int Ms, j, initot, Smp, Subs, Spno,
      init[NOC];

//  char dic[100];
  FILE *inp;

  inp = fopen("fdist_params2.dat", "r");
  if (inp == NULL) {
    fprintf(stderr, "Could not open fdist_params2.dat file.\n");
    quitDistribCL();
  }
  fscanf(inp, "%d", &Spno);
  if (Spno > NOC) {
    fprintf(stderr,"error in parameter file - number of subpopulation greater than %d\n", NOC);
    quitDistribCL();
  }
  fscanf(inp, "%d", &Subs);
  fscanf(inp, "%f", &efst);
  fscanf(inp, "%d", &Smp);
  fscanf(inp, "%d", &Ms);
//  fscanf(inp, "%d", &itno);
  fclose(inp);
  printf("number of demes is %d\n", Spno);
  printf("number of samples  is %d\n", Subs);
  printf("expected (infinite allele, infinite island) Fst is %f\n", efst);
  printf("median sample size is %d - suggest give 50 if >50\n", Smp);
  if (Ms) {
    printf("stepwise mutation model assumed\n");
  } else
    printf("infinite alleles mutation model assumed\n");
  printf("%d realizations (loci) requested\n", itno);
/*  while (true) {
    printf("are these parameters correct? (y/n)  ");
//    scanf("%s", dic);
    if (dic[0] == 'y')
      break;
    else if (dic[0] == 'n')
      quitDistribCL();
    else
      printf("que ???\n\n\n");
  }*/
//sleep(5);

  for (j = 0; j < Subs; ++j)
    init[j] = Smp;
  for (j = Subs; j < Spno; ++j)
    init[j] = 0;

  initot = Smp * Subs;


  cl_int nitersperjob=itno/NJOBS;
  if (NDIMS == 1)
    nGlobItems[0] = nGlobItems[1] = NTOTALGLOBITEMS;
  else//NDIMS = 2
    nGlobItems[0] = nGlobItems[1] = sqrt(NTOTALGLOBITEMS);
//  nItemsGroup[0] = nItemsGroup[1] = 1;


  cl_uint seed = 0; //seed = seed + lastNGlobItems;

  //profiling:
  //Jobs of cat 1: shorter, first phase is heavier
  //Jobs of cat 2: longer, first phase heavy but has a lot of processing afterwards
  //jobs for initial profiling
//  cl_int nItersProf1   = 51200;//819200;
//  cl_int nItersProf2   = 204800;
  cl_int nItersProfile = nitersperjob;//12 minutes to profile (see comment at top of this file)

  int GLOTITEMSPROFILE = NTOTALGLOBITEMS;
  int globItemsProfile[2] = {nGlobItems[0], nGlobItems[1]};
  //float y1Prof = 0.20, y2Prof = 0.21;
  //int hProf = 1024, nProf = 12298;

/*
  for (int i = 0; i < nPUs ; i++) {
//    if (i >= nPUs) //to send another category for profiling
//      nItersProfile = nItersProf2;

    Job *job = createJob();
    setJobID(job, -1-i);
    setAllowPU(job, CL_DEVICE_TYPE_CPU);
    if (useGPU)
      setAllowPU(job, CL_DEVICE_TYPE_GPU);
    setJobCategory(job, 1);
    loadSourceFile(job, "simulation.cl");
    setStartingKernel(job, "simulation");


    setArgument(job, INPUT,  sizeof(cl_int),     &nItersProfile);//itno
    setArgument(job, INPUT,  sizeof(cl_int),     &Smp);//Smp
    setArgument(job, INPUT,  sizeof(cl_uint),    &seed);//seed
    setArgument(job, INPUT,  sizeof(cl_int),     &Ms);//Ms
    setArgument(job, INPUT,  sizeof(cl_int),     &initot);//initot
    setArgument(job, INPUT,  sizeof(cl_int)*NOC, init);//init
    setArgument(job, INPUT,  sizeof(cl_int),     &Subs);//Subs
    setArgument(job, INPUT,  sizeof(cl_int),     &Spno);//Spno
    setArgument(job, INPUT,  sizeof(cl_float),   &efst);//efst

    setArgument(job, EMPTY_BUFFER,  sizeof(struct node)      * 8000      * GLOTITEMSPROFILE, NULL);//NODES (4000->148MB)
    setArgument(job, EMPTY_BUFFER,  sizeof(struct nodeptr *) * NOC*VPNOC * GLOTITEMSPROFILE, NULL);//List  (234.375MB)
    setArgument(job, EMPTY_BUFFER,  sizeof(struct nodeptr *) * 8000      * GLOTITEMSPROFILE, NULL);//Nlist (4000->15MB, 8000->31.25MB)

    setArgument(job, EMPTY_BUFFER,  sizeof(cl_float) * NOC   * GLOTITEMSPROFILE, NULL);      //Sd           400K
    setArgument(job, EMPTY_BUFFER,  sizeof(cl_float) * NOC   * GLOTITEMSPROFILE, NULL);      //Migrate      400K
    setArgument(job, EMPTY_BUFFER,  sizeof(cl_float) * NOC*2 * GLOTITEMSPROFILE, NULL);      //Den          800K
    setArgument(job, EMPTY_BUFFER,  sizeof(cl_float) * NOC   * GLOTITEMSPROFILE, NULL);      //Ni           400K
    setArgument(job, EMPTY_BUFFER,  sizeof(cl_float) * NOC   * GLOTITEMSPROFILE, NULL);      //Occlist      400K
    setArgument(job, EMPTY_BUFFER,  sizeof(cl_float) * NOC   * GLOTITEMSPROFILE, NULL);      //Lmax         400K
    setArgument(job, EMPTY_BUFFER,  sizeof(cl_float) * Subs*Smp*100 * GLOTITEMSPROFILE, NULL);//val_arr     146.48MB min, 30->292.96675MB
    setArgument(job, EMPTY_BUFFER,  sizeof(cl_float) * Subs*Smp*100 * GLOTITEMSPROFILE, NULL);//freq_arr    146.48MB min, 30->292.96675

    setArgument(job, OUTPUT, sizeof(cl_float) * GLOTITEMSPROFILE, NULL);//fsum                              2K
    setArgument(job, OUTPUT, sizeof(cl_float) * GLOTITEMSPROFILE, NULL);//wsum                              2K
    setArgument(job, OUTPUT, sizeof(cl_float) * nItersProfile,  NULL);//fsts                                19.53125MB max
    setArgument(job, OUTPUT, sizeof(cl_float) * nItersProfile,  NULL);//h1s                                 19.53125MB max
                                                                                                          //----
                                                                                                          //Fixed: 234.375+2.34375+39.0625+0.00390625
                                                                                                          //       275.78515625
                                                                                                          //
                                                                                                          //Subs=15->292.96875 MB
                                                                                                          //Subs=30->585.9375 MB
                                                                                                          //Subs=80->1562.5 MB
                                                                                                          //4000->163MB
                                                                                                          //5000->205.078125 MB
                                                                                                          //6000->246.09375
                                                                                                          //7000->291.015625
                                                                                                          //8000->218.75+31.25=250MB
                                                                                                          //

    setDimensions(job, NDIMS, globItemsProfile, nItemsGroup);

    setResultCollector(job, defaultRCID);
    requestResultNotification(job);

    if (i == 0) {
      sleep(20);
//      printf("Type something to send the first PROFILING job.\n");
//      char asd[100];
//      scanf("%s",asd);
      sleep(2);
    }
    sendJobToExec(job, defaultSchedID);
  }


*/

  for (int i = 0; i < NJOBS; i++) {
    job[i] = createJob();
    setJobID(job[i],i);
    if (!useGPU)
      setRequiredPU(job[i], CL_DEVICE_TYPE_CPU);
    else if (useGPU)
      setRequiredPU(job[i], CL_DEVICE_TYPE_GPU);
    setJobCategory(job[i], 1 );
    loadSourceFile(job[i],"simulation.cl");
    setStartingKernel(job[i], "simulation");



    setArgument(job[i], INPUT,  sizeof(cl_int),     &nitersperjob);//itno
    setArgument(job[i], INPUT,  sizeof(cl_int),     &Smp);//Smp
    setArgument(job[i], INPUT,  sizeof(cl_uint),    &seed);//seed
    setArgument(job[i], INPUT,  sizeof(cl_int),     &Ms);//Ms
    setArgument(job[i], INPUT,  sizeof(cl_int),     &initot);//initot
    setArgument(job[i], INPUT,  sizeof(cl_int)*NOC, init);//init
    setArgument(job[i], INPUT,  sizeof(cl_int),     &Subs);//Subs
    setArgument(job[i], INPUT,  sizeof(cl_int),     &Spno);//Spno
    setArgument(job[i], INPUT,  sizeof(cl_float),   &efst);//efst

    setArgument(job[i], EMPTY_BUFFER,  sizeof(struct node)      * 8000      * NTOTALGLOBITEMS, NULL);//NODES
    setArgument(job[i], EMPTY_BUFFER,  sizeof(struct nodeptr *) * NOC*VPNOC * NTOTALGLOBITEMS, NULL);//List
    setArgument(job[i], EMPTY_BUFFER,  sizeof(struct nodeptr *) * 8000      * NTOTALGLOBITEMS, NULL);//Nlist

    setArgument(job[i], EMPTY_BUFFER,  sizeof(cl_float) * NOC   * NTOTALGLOBITEMS, NULL);      //Sd
    setArgument(job[i], EMPTY_BUFFER,  sizeof(cl_float) * NOC   * NTOTALGLOBITEMS, NULL);      //Migrate
    setArgument(job[i], EMPTY_BUFFER,  sizeof(cl_float) * NOC*2 * NTOTALGLOBITEMS, NULL);      //Den
    setArgument(job[i], EMPTY_BUFFER,  sizeof(cl_float) * NOC   * NTOTALGLOBITEMS, NULL);      //Ni
    setArgument(job[i], EMPTY_BUFFER,  sizeof(cl_float) * NOC   * NTOTALGLOBITEMS, NULL);      //Occlist
    setArgument(job[i], EMPTY_BUFFER,  sizeof(cl_float) * NOC   * NTOTALGLOBITEMS, NULL);      //Lmax
    setArgument(job[i], EMPTY_BUFFER,  sizeof(cl_float) * Subs*Smp*100 * NTOTALGLOBITEMS, NULL);//val_arr
    setArgument(job[i], EMPTY_BUFFER,  sizeof(cl_float) * Subs*Smp*100 * NTOTALGLOBITEMS, NULL);//freq_arr

    setArgument(job[i], OUTPUT, sizeof(cl_float) * NTOTALGLOBITEMS, NULL);//fsum
    setArgument(job[i], OUTPUT, sizeof(cl_float) * NTOTALGLOBITEMS, NULL);//wsum
    setArgument(job[i], OUTPUT, sizeof(cl_float) * nitersperjob,    NULL);//fsts
    setArgument(job[i], OUTPUT, sizeof(cl_float) * nitersperjob,    NULL);//h1s


    setDimensions(job[i], NDIMS, nGlobItems, nItemsGroup);

    setResultCollector(job[i], defaultRCID);
    requestResultNotification(job[i]);

    pthread_mutex_lock(&end_mutex);
    while(profilesReceived != nPUs)
      pthread_cond_wait(&profile_over, &end_mutex);
    pthread_mutex_unlock(&end_mutex);
    if (i == 0) {
//      char asd[100];
      sleep(27);
      printf("\n\n\n#####PROFILING OVER#####\n");// Type something to send the first (real) job.\n");
//      scanf("%s",asd);
      sleep(1);
      if (gettimeofday(&start, NULL)) { //TODO: criar os jobs todos e só depois iniciar o timer
        perror("gettimeofday");
        exit (EXIT_FAILURE);
      }
    }
    sendJobToExec(job[i], defaultSchedID);

    seed += NTOTALGLOBITEMS;
  }



  pthread_mutex_lock(&end_mutex);
  while(!program_over)
    pthread_cond_wait(&end_condition, &end_mutex);
  pthread_mutex_unlock(&end_mutex);


  fprintf(stderr, "Over and out.\n");
  quitDistribCL();
  return EXIT_SUCCESS;
}


