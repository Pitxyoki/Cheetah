/*
 * testFFT.c
 *
 *  Created on: 15 Apr 2010
 *      Author: luis
 */


#include "pum.h"

cl_float *inBuff1;
cl_float *inBuff2;

bool createLowProcJob (JobToPUM *job) {
  job->probID = myid;

  //job->jobID //TO BE FILLED BY THE PUM
  //job->runOn = CL_DEVICE_TYPE_CPU;//TO BE FILLED BY THE PUM
  job->nDimensions = 0;

  job->startingKernel = malloc(sizeof("setupLowProcTest"));
  strcpy(job->startingKernel,"setupLowProcTest");
  job->startingNameSize = strlen(job->startingKernel)+1;

  job->taskSource = fileToString("../PUManager/setupLowProcTest.cl");
  if (job->taskSource == NULL) {
    fprintf(stderr,"FILE NOT FOUND! (while creating LowProcJob)\n");
    return false;
  }
  job->taskSourceSize = strlen(job->taskSource)+1;

  ///Setting up arguments
  job->nTotalArgs = 2;
  job->argTypes = calloc(job->nTotalArgs, sizeof(argument_type));
  job->argTypes[0] = INPUT;
  job->argTypes[1] = OUTPUT;

  job->argSizes = calloc(job->nTotalArgs, sizeof(argument_type));
  job->argSizes[0] = job->argSizes[1] = sizeof(cl_uint);

  cl_uint *inBuff = malloc(sizeof(cl_uint));
  cl_uint *outBuff = malloc(sizeof(cl_uint));
  inBuff[0] = 100;

  job->arguments = calloc(job->nTotalArgs, sizeof(void *));
  job->arguments[0] = inBuff;
  job->arguments[1] = outBuff;

//  job->returnTo = defaultRCID; //not used

  return true;
}




bool createHighProcJob (JobToPUM *job) {
  job->probID = myid;

//  if (job->runOn == CL_DEVICE_TYPE_CPU) {
    job->nDimensions = 1;
    job->nItems[0] = 1024;
    job->nGroupItems[0] = 1;
/*  }
  else {
    job->nDimensions = 2;
    job->nItems[0] = job->nItems[1] = 512;
    job->nGroupItems[0] = job->nGroupItems[1] = 16;
  }
*/
  job->startingKernel = malloc(sizeof("setupHighProcTest") * sizeof(char));
  strcpy(job->startingKernel, "setupHighProcTest");



  job->taskSource = fileToString("../PUManager/setupHighProcTest.cl");
  if (job->taskSource == NULL) {
    fprintf(stderr,"FILE NOT FOUND! (building setupHighProcTest)\n");
    return false;
  }

  job->taskSourceSize = strlen(job->taskSource)+1;


  job->nTotalArgs = 1;
  job->argTypes = calloc(job->nTotalArgs, sizeof(argument_type));
  job->argTypes[0] = OUTPUT;

  job->argSizes = calloc(job->nTotalArgs, sizeof(int));
  job->argSizes[0] = sizeof(int);


  job->arguments = calloc(job->nTotalArgs, sizeof(void *));
  for (int i = 0; i < job->nTotalArgs; i++) {
    job->arguments[i] = malloc(job->argSizes[i]);
  }

//  job->returnTo = defaultRCID; //not used

  return true;
}

bool createBandwidthTestJob (JobToPUM *job) {
  job->probID = myid;
  job->nDimensions = 1;
  job->nItems[0] = 1;
  job->nGroupItems[0] = 1;
  job->startingKernel = malloc(sizeof("setupBandwidthTest") * sizeof(char));
  strcpy(job->startingKernel, "setupBandwidthTest");

  job->taskSource = fileToString("../PUManager/setupBandwidthTest.cl");
  if (job->taskSource == NULL) {
    fprintf(stderr,"FILE NOT FOUND! (building setupBandwidthTest)\n");
    return false;
  }

  job->taskSourceSize = strlen(job->taskSource)+1;


  job->nTotalArgs = 1;
  job->argTypes = calloc(job->nTotalArgs, sizeof(argument_type));
  job->argTypes[0] = INPUT;
  job->argSizes = calloc(job->nTotalArgs, sizeof(void *));
  job->argSizes[0] = sizeof(cl_char) * PUInfoStruct.availablePUs[job->runOn].max_memory_alloc - (100.0*1024*1024); //512MB
  fprintf(stderr,"running with %i Byte\n",job->argSizes[0]);
  job->arguments = calloc(job->nTotalArgs, sizeof(void *));
  job->arguments[0] = malloc(job->argSizes[0]);
  return true;
}

bool createTestMandelbrot (JobToPUM *job) {
  job->probID = myid;

/*  if (job->runOn == CL_DEVICE_TYPE_CPU) {
    job->nDimensions = 1;
    job->nItems[0] = 1024;
    job->nGroupItems[0] = 1;
  }
  else {
*/    job->nDimensions = 2;
    job->nItems[0] = job->nItems[1] = 1024;
    job->nGroupItems[0] = job->nGroupItems[1] = 16;
//  }

  job->startingKernel = malloc(sizeof("calc_tile") * sizeof(char));
  strcpy(job->startingKernel, "calc_tile");



  job->taskSource = fileToString("../PUManager/mandel.cl");
  if (job->taskSource == NULL) {
    fprintf(stderr,"FILE NOT FOUND! (building testMandelbrot)\n");
    return false;
  }

  job->taskSourceSize = strlen(job->taskSource)+1;

  ///Setting up arguments
  float *x1, *y1, *x2, *y2;
  int *w, *h, *niters;
  x1 = malloc(sizeof(float));

  y1 = malloc(sizeof(float));
  x2 = malloc(sizeof(float));
  y2 = malloc(sizeof(float));
  w = malloc(sizeof(int));
  h = malloc(sizeof(int));
  niters = malloc(sizeof(int));
  *x1 = 0.40;
  *y1 = 0.20;
  *x2 = 0.41;
  *y2 = 0.21;
  *w = 1024;
  *h = 1024;
  *niters = 65536;

  job->nTotalArgs = 8;
  job->argTypes = calloc(job->nTotalArgs, sizeof(argument_type));
  job->argTypes[0] = job->argTypes[1] = job->argTypes[2] = job->argTypes[3] =
      job->argTypes[4] = job->argTypes[5] = INPUT;
  job->argTypes[6] = INPUT_OUTPUT;
  job->argTypes[7] = OUTPUT;

  job->argSizes = calloc(job->nTotalArgs, sizeof(int));
  job->argSizes[0] = job->argSizes[1] = job->argSizes[2] = job->argSizes[3] = sizeof(float);
  job->argSizes[4] = job->argSizes[5] = job->argSizes[6] = sizeof(int);
  job->argSizes[7] = (*w)*(*h)*sizeof(int);


  job->arguments = calloc(job->nTotalArgs, sizeof(void *));
  for (int i = 0; i < job->nTotalArgs; i++) {
    job->arguments[i] = malloc(job->argSizes[i]);
  }

  job->arguments[0] = x1;
  job->arguments[1] = y1;
  job->arguments[2] = x2;
  job->arguments[3] = y2;
  job->arguments[4] = w;
  job->arguments[5] = h;
  job->arguments[6] = niters;

//  job->returnTo = defaultRCID; //not used

  return true;
}


//int fillRandom (cl_float *arrayPtr, const int width, const cl_float rangeMin, const cl_float rangeMax);


int fillRandom (cl_float *arrayPtr, const int width, const cl_float rangeMin, const cl_float rangeMax) {

  if(!arrayPtr) {
    fprintf(stderr,"Cannot fill array. NULL pointer.\n");
    return CL_FALSE;
  }

  unsigned int seed = (unsigned int)10;

  srand(seed);
  double range = rangeMax - rangeMin + 1.0;

  /* random initialisation of input */
  for(int i = 0; i < width; i++)
    arrayPtr[i] = rangeMin + range*rand()/(RAND_MAX + 1.0);

  return CL_TRUE;
}

bool createTestFFT (JobToPUM *job) {
  job->probID = myid;

//  job->runOn = CL_DEVICE_TYPE_CPU; //TO BE FILLED BY THE JS

  job->nDimensions = 1;
  job->nItems[0] = 64;
  job->nGroupItems[0] = 64;

  job->startingKernel = malloc(sizeof("kfft"));
  strcpy(job->startingKernel,"kfft");
  job->startingNameSize = strlen(job->startingKernel)+1;

  job->taskSource = fileToString("../PUManager/FFT_Kernels.cl");
  if (job->taskSource == NULL) {
    fprintf(stderr,"FILE NOT FOUND! (building testFFT)\n");
    return false;
  }
  job->taskSourceSize = strlen(job->taskSource)+1;

  ///Setting up arguments
  job->nTotalArgs = 2;
  job->argTypes = calloc(job->nTotalArgs, sizeof(argument_type));
  job->argTypes[0] = INPUT_OUTPUT;
  job->argTypes[1] = INPUT_OUTPUT;

  job->argSizes = calloc(job->nTotalArgs, sizeof(int));
  job->argSizes[0] = job->argSizes[1] = 1024*sizeof(cl_uint);


  inBuff1 = calloc(1024, sizeof(cl_float));
  inBuff2 = calloc(1024, sizeof(cl_float));


  if (!fillRandom(inBuff1, 1024, 0, 255)) {
    fprintf(stderr, "Error filling input buffer 1 for FFT kernel\n");
    return false;
  }
  if (!fillRandom(inBuff2, 1024, 0, 0)) {
    fprintf(stderr, "Error filling input buffer 2 for FFT kernel\n");
    return false;
  }

  job->arguments = calloc(job->nTotalArgs, sizeof(void *));
  job->arguments[0] = calloc(1024, sizeof(cl_float));
  job->arguments[1] = calloc(1024, sizeof(cl_float));
  memcpy(job->arguments[0], inBuff1, 1024*sizeof(cl_float));
  memcpy(job->arguments[1], inBuff2, 1024*sizeof(cl_float));


  if(debug_PUM) {
    printf("Original Input Real: ");
    print1DArray(inBuff1, NUM_PRINT1DARRAY_ELEMS);
    printf("Original Input Imaginary: ");
    print1DArray(inBuff2, NUM_PRINT1DARRAY_ELEMS);
  }

//  job->returnTo = defaultRCID; //not used


  return true;
}



/*
   This computes an in-place complex-to-complex FFT
   x and y are the real and imaginary arrays of 2^m points.
   dir =  1 gives forward transform
   dir = -1 gives reverse transform
*/
void fftCPU(short int dir,long m,cl_float *x,cl_float *y)
{
   long n,i,i1,j,k,i2,l,l1,l2;
   double c1,c2,tx,ty,t1,t2,u1,u2,z;

   /* Calculate the number of points */
   n = 1;
   for (i=0;i<m;i++)
      n *= 2;

   /* Do the bit reversal */
   i2 = n >> 1;
   j = 0;
   for (i=0;i<n-1;i++)
   {
      if (i < j)
      {
         tx = x[i];
         ty = y[i];
         x[i] = x[j];
         y[i] = y[j];
         x[j] = (cl_float)tx;
         y[j] = (cl_float)ty;
      }
      k = i2;
      while (k <= j)
      {
         j -= k;
         k >>= 1;
      }
      j += k;
   }

   /* Compute the FFT */
   c1 = -1.0;
   c2 = 0.0;
   l2 = 1;
   for (l=0;l<m;l++)
   {
      l1 = l2;
      l2 <<= 1;
      u1 = 1.0;
      u2 = 0.0;
      for (j=0;j<l1;j++)
      {
         for (i=j;i<n;i+=l2)
         {
            i1 = i + l1;
            t1 = u1 * x[i1] - u2 * y[i1];
            t2 = u1 * y[i1] + u2 * x[i1];
            x[i1] = (cl_float)(x[i] - t1);
            y[i1] = (cl_float)(y[i] - t2);
            x[i] += (cl_float)t1;
            y[i] += (cl_float)t2;
         }
         z =  u1 * c1 - u2 * c2;
         u2 = u1 * c2 + u2 * c1;
         u1 = z;
      }
      c2 = sqrt((1.0 - c1) / 2.0);
      if (dir == 1)
         c2 = -c2;
      c1 = sqrt((1.0 + c1) / 2.0);
   }

   /* Scaling for forward transform */
   /*if (dir == 1) {
      for (i=0;i<n;i++) {
         x[i] /= n;
         y[i] /= n;
      }
   }*/

}

/**
 * Reference CPU implementation of FFT Convolution
 * for performance comparison
 */
void  fftCPUReference(cl_float *referenceReal,
                     cl_float *referenceImaginary,
                     cl_float *input_r,
                     cl_float *input_i,
                     cl_uint  w)
{
    /* Copy data from input to reference buffers */
    memcpy(referenceReal, input_r, w * sizeof(cl_float));
    memcpy(referenceImaginary, input_i, w * sizeof(cl_float));
    /* Compute reference FFT */
    fftCPU(1, 10, referenceReal, referenceImaginary);
}





cl_bool compare(const float *refData, const float *data,
                        const int length)
{
    float error = 0.0f;
    float ref = 0.0f;
    float epsilon =  1e-6f;
    for(int i = 1; i < length; ++i)
    {
        float diff = refData[i] - data[i];
        error += diff * diff;
        ref += refData[i] * refData[i];
    }

    float normRef =  sqrtf((float) ref);
    if (fabs((float) ref) < 1e-7f) {
        return CL_FALSE;
    }
    float normError = sqrtf((float) error);
    error = normError / normRef;

    return error < epsilon;
}

bool verifyFFTResults(JobToPUM *job) {
  cl_float *verificationOutput_i = calloc(1024, sizeof(cl_float));
  cl_float *verificationOutput_r = calloc(1024, sizeof(cl_float));

  if (!verificationOutput_i || !verificationOutput_r) {
    fprintf(stderr,"Failed to allocate host memory" "(verificationOutput)\n");
    return false;
  }

  /* Compute reference FFT on input */
  fftCPUReference(verificationOutput_r, verificationOutput_i, inBuff1, inBuff2, 1024);

  if (debug_PUM) {
    printf("Verification output imaginary buffer: \n");
    print1DArray(verificationOutput_i, NUM_PRINT1DARRAY_ELEMS);
  }

  /* Compare results */
  if (compare(verificationOutput_i, (float *) (job->arguments[1]), 1024)) {
    if (debug_PUM)
      fprintf(stderr,"Passed!\n");
    return true;
  } else {
    if (debug_PUM)
      fprintf(stderr,"Failed\n");
    return false;
  }
}





void checkJobResults (JobToPUM *job) {

  switch (job->jobID) {
    case 0: //lowProcJob
      assert(job->probID == myid);
      assert(job->argSizes[0] == sizeof(cl_uint));
      assert(((cl_uint *)job->arguments[1])[0] == 200);
      break;
    case 1: {//highProcJob
//      assert(*((int *)job->arguments[0]) == 1048576);
      //mandelbrot
      fprintf(stderr, "verifying mandelbrot\n");
      int MAXITER = 524288;
      int rgb[MAXITER][3];
      int i,j;
      srand (123);
      for (i = 1;  i < MAXITER-1;  i++ )
        for (j = 0;  j < 3;  j++ )
          rgb[i][j] = rand () % MAXITER;

      rgb[0][0] = rgb[0][1] = rgb[0][2] = 0;
      rgb[MAXITER-1][0] = rgb[MAXITER-1][1] = rgb[MAXITER-1][2] = MAXITER;



      FILE *fd = fopen ("../PUManager/file.ppm", "w");
      if (fd == NULL) {
        fprintf (stderr,"Couldn't open 'file.ppm' for writing! Aborting...\n");
        exit (EXIT_FAILURE);
      }


      int *p;

      fprintf (fd, "P6\n%d %d\n255\n", 1024, 1024);
      p = (int *) job->arguments[7];

      for (int i = 0; i < 1024*1024; i++) {
        fputc(rgb[p[i]][0], fd);
        fputc(rgb[p[i]][1], fd);
        fputc(rgb[p[i]][2], fd);
      }

      fclose (fd);

      break;
    }
    case 2: //bandwidth
      assert(true);
      break;
    default:
      break;
  }

  fprintf(stderr,"PUM (%i): Test job executed successfully\n", myid);
  /* 
   else {//FFT
    assert(job->probID == myid);
    assert(job->argSizes[0] = job->argSizes[1] = 1024 * sizeof(cl_float));
    assert(verifyFFTResults(job));
    free(inBuff1);
    free(inBuff2);
  }*/



}



