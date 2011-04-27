/*
 * info.c
 *  Functions to retrieve information from the system
 *  Created on: 1 Apr 2010
 *      Author: Luís Miguel Picciochi de Oliveira
 *                           <Pitxyoki@Gmail.com>
 */
/*
 * Ao iniciar: criar um contexto para cada device
   - associar contexto a cada device no PUInfoStruct.PUsContexts[IDrespectivo]

Ex.:
1 CPU
  - PUMStruct
     - available[0] - stuff
     - PUsContexts[0] - CPU's context

1 CPU
1 GPU
  - PUMStruct
     - available[0] - CPU's stuff
                [1] - GPU's stuff
     - PUsContexts[0] - CPU's context
                  [1] - GPU's context
 *
 */

#include "pum.h"


#define BUFSIZE 1024
int BUFBYTES = BUFSIZE*sizeof(char);

bool checkErr(cl_int err, const char * name) {
  if (err != CL_SUCCESS) {
    fprintf(stderr, "ERROR: %s (%i)\n", name, err);
    return false;
  }
  else {
    fprintf(stderr, "No error to print (%s / %i).\n", name, err);
    return true;
  }
}


bool getLocalInfo () {
  PUInfoStruct.nPUs = 0;
  PUInfoStruct.id = myid;
  cl_int status = CL_SUCCESS;
  cl_uint numDevices;

  cl_uint numPlatforms;
  cl_platform_id *platforms;

  status = clGetPlatformIDs(0, NULL, &numPlatforms);
  if (status != CL_SUCCESS)
    return checkErr(status, "clGetPlatformIDs failed.\n");


  if (numPlatforms <= 0) {
    fprintf(stderr, "numPlatforms is %i. Please check if an OpenCL environment is correctly installed.\n", numPlatforms);
    return false;
  }

  platforms = calloc(numPlatforms, sizeof(cl_platform_id));
  status = clGetPlatformIDs(numPlatforms, platforms, NULL);
  if (status != CL_SUCCESS)
    return checkErr(status, "clGetPlatformIDs failed.\n");

  for (int i = 0; i<numPlatforms; i++) {
    numDevices = 0;


    status = clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, 0, NULL, &numDevices);
    if (status != CL_SUCCESS)
      return checkErr(status, "clGetDeviceIDs failed (CL_DEVICE_TYPE_ALL).\n");


    PUInfoStruct.nPUs += numDevices;
    PUInfoStruct.availablePUs = realloc (PUInfoStruct.availablePUs, (PUInfoStruct.nPUs) * sizeof(PUProperties));
    PUInfoStruct.PUsContexts = realloc (PUInfoStruct.PUsContexts, (PUInfoStruct.nPUs) * sizeof(cl_context *));
    PUInfoStruct.PUsCmdQs = realloc(PUInfoStruct.PUsCmdQs, (PUInfoStruct.nPUs) * sizeof(cl_command_queue *));
    PUInfoStruct.argBuffers = realloc(PUInfoStruct.argBuffers, (PUInfoStruct.nPUs) * sizeof(cl_mem *));
    PUInfoStruct.currKernels = realloc(PUInfoStruct.currKernels, (PUInfoStruct.nPUs) * sizeof(cl_kernel));

    //Create a context properties array for this platform. This will be used to create contextes for each of its devices
    cl_context_properties cps[3] = { CL_CONTEXT_PLATFORM, (cl_context_properties) platforms[i], 0 };



    //Now let's get the properties of the devices managed by this platform...
    cl_device_id devices[numDevices];

    status = clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, numDevices, devices, NULL);
    if (status != CL_SUCCESS)
      return checkErr(status, "clGetDeviceIDs failed (CL_DEVICE_TYPE_ALL).\n");

    for (cl_uint j = 0; j < numDevices; j++) {
      PUProperties currPU;
      status = clGetDeviceInfo(devices[j], CL_DEVICE_TYPE, sizeof(cl_device_type), &(currPU.device_type), NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_TYPE).\n");

      status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(cl_uint), &(currPU.clock_frequency), NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_CLOCK_FREQUENCY).\n");

      status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(cl_ulong), &(currPU.max_memory_alloc), NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_MEM_ALLOC_SIZE).\n");

      status = clGetDeviceInfo(devices[j], CL_DEVICE_GLOBAL_MEM_CACHE_TYPE, sizeof(cl_device_mem_cache_type), &(currPU.cache_type), NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_GLOBAL_MEM_CACHE_TYPE).\n");

      status = clGetDeviceInfo(devices[j], CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(cl_ulong), &(currPU.global_mem_size), NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_GLOBAL_MEM_SIZE).\n");

      status = clGetDeviceInfo(devices[j], CL_DEVICE_LOCAL_MEM_TYPE, sizeof(cl_device_local_mem_type), &(currPU.local_mem_type), NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_LOCAL_MEM_TYPE).\n");
      switch(currPU.local_mem_type) {
        case CL_LOCAL:
        case CL_GLOBAL:
        break;
        default:
          fprintf(stderr,"Error: Unkown local memory type!\n");
          return false;
      }

      status = clGetDeviceInfo(devices[j], CL_DEVICE_LOCAL_MEM_SIZE, sizeof(cl_ulong), &(currPU.local_mem_size), NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_LOCAL_MEM_SIZE).\n");

      status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), &(currPU.max_compute_units), NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_COMPUTE_UNITS).\n");

      status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof(cl_uint), &(currPU.max_work_item_dimensions), NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS).\n");

      status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_WORK_ITEM_SIZES, currPU.max_work_item_dimensions*sizeof(size_t), currPU.max_work_item_sizes, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_WORK_ITEM_SIZES).\n");

      status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), &(currPU.max_work_group_size), NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_WORK_GROUP_SIZE).\n");

      status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_PARAMETER_SIZE, sizeof(size_t), &(currPU.max_parameter_size), NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_PARAMETER_SIZE).\n");

      PUInfoStruct.availablePUs[PUInfoStruct.nPUs-numDevices+j] = currPU;




      /***
       * Now create this device's context and command queue
       ***/
      PUInfoStruct.PUsContexts[PUInfoStruct.nPUs-numDevices + j] = malloc(sizeof(cl_context));
      *(PUInfoStruct.PUsContexts[PUInfoStruct.nPUs-numDevices + j]) = clCreateContext(cps, 1, &(devices[j]), NULL, NULL, &status);
      if (status != CL_SUCCESS) {
        fprintf(stderr, "clCreateContextFromType failed (%i).\n", status);
        return false;
      }

      cl_command_queue_properties prop = 0;
      cl_command_queue *commandQueue = malloc(sizeof(cl_command_queue));
      //Notice: The following call creates 3 threads on this process, each waiting on a condition variable
      *commandQueue = clCreateCommandQueue(*(PUInfoStruct.PUsContexts[PUInfoStruct.nPUs-numDevices + j]), devices[j], prop, &status);
      if (status != CL_SUCCESS) {
        fprintf(stderr,"clCreateCommandQueue failed (%i).\n", status);
        return false;
      }

//      PUInfoStruct.PUsContexts[PUInfoStruct.nPUs-numDevices + j] = context;
      PUInfoStruct.PUsCmdQs[PUInfoStruct.nPUs-numDevices + j] = commandQueue;


    }

  }

  return true;
}

bool printLocalInfo() {
  cl_int status;
  char charbuf[BUFSIZE];

  // Plaform info
  cl_uint numPlatforms;
  cl_platform_id *platforms;

  status = clGetPlatformIDs(0, NULL, &numPlatforms);
  if (status != CL_SUCCESS)
    return checkErr(status, "clGetPlatformIDs failed.\n");

  if (numPlatforms <= 0) {
    fprintf(stderr, "numPlatforms = %i. Check if an OpenCL environment is correctly installed.\n", numPlatforms);
    return false;
  }

  platforms = calloc(numPlatforms, sizeof(cl_platform_id));
  status = clGetPlatformIDs(numPlatforms, platforms, NULL);
  if (status != CL_SUCCESS)
    return checkErr(status, "clGetPlatformIDs failed.\n");

  // Iteratate over platforms
  printf("Number of platforms:\t\t\t\t %i\n", numPlatforms);
  for (unsigned int i = 0; i < numPlatforms; i++) {

    status = clGetPlatformInfo(platforms[i], CL_PLATFORM_PROFILE, BUFBYTES, charbuf, NULL);
    if (status != CL_SUCCESS)
      return checkErr(status, "clGetPlatformInfo failed (CL_PLATFORM_PROFILE).\n");
    printf("  Plaform Profile:\t\t\t\t %s\n", charbuf);

    status = clGetPlatformInfo(platforms[i], CL_PLATFORM_VERSION, BUFBYTES, charbuf, NULL);
    if (status != CL_SUCCESS)
      return checkErr(status, "clGetPlatformInfo failed (CL_PLATFORM_VERSION).\n");
    printf("  Plaform Version:\t\t\t\t %s\n", charbuf);

    status = clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, BUFBYTES, charbuf, NULL);
    if (status != CL_SUCCESS)
      return checkErr(status, "clGetPlatformInfo failed (CL_PLATFORM_NAME).\n");
    printf("  Plaform Name:\t\t\t\t\t %s\n", charbuf);

    status = clGetPlatformInfo(platforms[i], CL_PLATFORM_VENDOR, BUFBYTES, charbuf, NULL);
    if (status != CL_SUCCESS)
      return checkErr(status, "clGetPlatformInfo failed (CL_PLATFORM_VENDOR).\n");
    printf("  Plaform Vendor:\t\t\t\t %s\n", charbuf);

    status = clGetPlatformInfo(platforms[i], CL_PLATFORM_EXTENSIONS, BUFBYTES, charbuf, NULL);
    if (status != CL_SUCCESS)
      return checkErr(status, "clGetPlatformInfo failed (CL_PLATFORM_EXTENSIONS).\n");
    if (charbuf[0] != '\0')
      printf("  Plaform Extensions:\t\t\t %s\n", charbuf);
  }

  printf("\n\n");
  // Now Iteratate over each platform and its devices
  for (unsigned int i = 0; i< numPlatforms; i++) {
    status = clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, BUFBYTES, charbuf, NULL);
    if (status != CL_SUCCESS)
      return checkErr(status, "clGetPlatformInfo failed (CL_PLATFORM_NAME).\n");
    printf("  Plaform Name:\t\t\t\t\t %s\n", charbuf);


    cl_uint numDevices;
    status = clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, 0, NULL, &numDevices);
    if (status != CL_SUCCESS)
      return checkErr(status, "clGetDeviceIDs failed (CL_DEVICE_TYPE_ALL).\n");
    printf("Number of devices:\t\t\t\t %i\n", numDevices);

    cl_device_id devices[numDevices];

    status = clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, numDevices, devices, NULL);
    if (status != CL_SUCCESS)
      return checkErr(status, "clGetDeviceIDs failed (CL_DEVICE_TYPE_ALL).\n");

    for (unsigned int j = 0; j < numDevices; j++) {

      printf("  Device Type:\t\t\t\t\t ");
      cl_device_type dtype;
      status = clGetDeviceInfo(devices[j], CL_DEVICE_TYPE, sizeof(cl_device_type), &dtype, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_TYPE).\n");
      switch (dtype) {
        case CL_DEVICE_TYPE_ACCELERATOR:
          printf("CL_DEVICE_TYPE_ACCELERATOR\n");
          break;
        case CL_DEVICE_TYPE_CPU:
          printf("CL_DEVICE_TYPE_CPU\n");
          break;
        case CL_DEVICE_TYPE_DEFAULT:
          printf("CL_DEVICE_TYPE_DEFAULT\n");
          break;
        case CL_DEVICE_TYPE_GPU:
          printf("CL_DEVICE_TYPE_GPU\n");
          break;
        default:
          fprintf(stderr,"ERROR: Unknown device type!\n");
          return false;
          break;
      }

      cl_uint uintinfo;
      cl_ulong ulonginfo;
      cl_bool boolinfo;
      size_t sizeinfo;

      status = clGetDeviceInfo(devices[j], CL_DEVICE_VENDOR_ID, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_VENDOR_ID).\n");
      printf("  Device ID:\t\t\t\t\t %i\n", uintinfo);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_COMPUTE_UNITS).\n");
      printf("  Max compute units:\t\t\t\t %i\n", uintinfo);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS).\n");
      printf("  Max work items dimensions:\t\t\t %i\n", uintinfo);

      size_t witems[uintinfo];
      status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_WORK_ITEM_SIZES, uintinfo*sizeof(size_t), witems, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_WORK_ITEM_SIZES).\n");
      for (cl_uint x = 0; x < uintinfo; x++)
        printf("    Max work items[%i]:\t\t\t\t %zi\n", x, witems[x]);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), &sizeinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_WORK_GROUP_SIZE).\n");
      printf("  Max work group size:\t\t\t\t %zi\n", sizeinfo);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR).\n");
      printf("  Preferred vector width char:\t\t\t %i\n", uintinfo);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT).\n");
      printf("  Preferred vector width short:\t\t\t %i\n", uintinfo);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT).\n");
      printf("  Preferred vector width int:\t\t\t %i\n", uintinfo);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG).\n");
      printf("  Preferred vector width long:\t\t\t %i\n", uintinfo);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT).\n");
      printf("  Preferred vector width float:\t\t\t %i\n", uintinfo);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE).\n");
      printf("  Preferred vector width double:\t\t %i\n", uintinfo);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_CLOCK_FREQUENCY).\n");
      printf("  Max clock frequency:\t\t\t\t %iMhz\n", uintinfo);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_ADDRESS_BITS, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_ADDRESS_BITS).\n");
      printf("  Address bits:\t\t\t\t\t %i\n", uintinfo);


      status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(cl_ulong), &ulonginfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_MEM_ALLOC_SIZE).\n");
      printf("  Max memeory allocation:\t\t\t %li\n", ulonginfo);


      status = clGetDeviceInfo(devices[j], CL_DEVICE_IMAGE_SUPPORT, sizeof(cl_bool), &boolinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_IMAGE_SUPPORT).\n");
      printf("  Image support:\t\t\t\t %s\n", boolinfo ? "Yes" : "No");

      if (boolinfo) {
        status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_READ_IMAGE_ARGS, sizeof(cl_uint), &uintinfo, NULL);
        if (status != CL_SUCCESS)
          return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_READ_IMAGE_ARGS).\n");
        printf("  Max number of images read arguments:\t %i\n", uintinfo);

        status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_WRITE_IMAGE_ARGS, sizeof(cl_uint), &uintinfo, NULL);
        if (status != CL_SUCCESS)
          return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_WRITE_IMAGE_ARGS).\n");
        printf("  Max number of images write arguments:\t %i\n", uintinfo);

        status = clGetDeviceInfo(devices[j], CL_DEVICE_IMAGE2D_MAX_WIDTH, sizeof(size_t), &sizeinfo, NULL);
        if (status != CL_SUCCESS)
          return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_IMAGE2D_MAX_WIDTH).\n");
        printf("  Max image 2D width:\t\t\t %zi\n", sizeinfo);

        status = clGetDeviceInfo(devices[j], CL_DEVICE_IMAGE2D_MAX_HEIGHT, sizeof(size_t), &sizeinfo, NULL);
        if (status != CL_SUCCESS)
          return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_IMAGE2D_MAX_HEIGHT).\n");
        printf("  Max image 2D height:\t\t\t %zi\n", sizeinfo);

        status = clGetDeviceInfo(devices[j], CL_DEVICE_IMAGE3D_MAX_WIDTH, sizeof(size_t), &sizeinfo, NULL);
        if (status != CL_SUCCESS)
          return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_IMAGE2D_MAX_WIDTH).\n");
        printf("  Max image 3D width:\t\t\t %zi\n", sizeinfo);

        status = clGetDeviceInfo(devices[j], CL_DEVICE_IMAGE3D_MAX_HEIGHT, sizeof(size_t), &sizeinfo, NULL);
        if (status != CL_SUCCESS)
          return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_IMAGE3D_MAX_HEIGHT).\n");
        printf("  Max image 3D height:\t %zi\n", sizeinfo);

        status = clGetDeviceInfo(devices[j], CL_DEVICE_IMAGE3D_MAX_DEPTH, sizeof(size_t), &sizeinfo, NULL);
        if (status != CL_SUCCESS)
          return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_IMAGE3D_MAX_DEPTH).\n");
        printf("  Max image 3D depth:\t\t\t %zi\n", sizeinfo);

        status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_SAMPLERS, sizeof(cl_uint), &uintinfo, NULL);
        if (status != CL_SUCCESS)
          return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_SAMPLERS).\n");
        printf("  Max samplers within kernel:\t\t %i\n", uintinfo);
      }

      status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_PARAMETER_SIZE, sizeof(size_t), &sizeinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_PARAMETER_SIZE).\n");
      printf("  Max size of kernel argument:\t\t\t %zi\n", sizeinfo);


      status = clGetDeviceInfo(devices[j], CL_DEVICE_MEM_BASE_ADDR_ALIGN, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MEM_BASE_ADDR_ALIGN).\n");
      printf("  Alignment (bits) of base address:\t\t %i\n", uintinfo);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE).\n");
      printf("  Minimum alignment (bytes) for any datatype:\t %i\n", uintinfo);


      cl_device_fp_config fpinfo;
      status = clGetDeviceInfo(devices[j], CL_DEVICE_SINGLE_FP_CONFIG, sizeof(cl_device_fp_config), &fpinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_SINGLE_FP_CONFIG).\n");
      printf("  Single precision floating point capability\n");
      printf("    Denorms:\t\t\t\t\t %s\n", fpinfo & CL_FP_DENORM ? "Yes" : "No");
      printf("    Quiet NaNs:\t\t\t\t\t %s\n", fpinfo & CL_FP_INF_NAN ? "Yes" : "No");
      printf("    Round to nearest even:\t\t\t %s\n", fpinfo & CL_FP_ROUND_TO_NEAREST ? "Yes" : "No");
      printf("    Round to zero:\t\t\t\t %s\n", fpinfo & CL_FP_ROUND_TO_ZERO ? "Yes" : "No");
      printf("    Round to +ve and infinity:\t\t\t %s\n", fpinfo & CL_FP_ROUND_TO_INF ? "Yes" : "No");
      printf("    IEEE754-2008 fused multiply-add:\t\t %s\n", fpinfo & CL_FP_FMA ? "Yes" : "No");


      cl_device_mem_cache_type mctype;
      status = clGetDeviceInfo(devices[j], CL_DEVICE_GLOBAL_MEM_CACHE_TYPE, sizeof(cl_device_mem_cache_type), &mctype, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_GLOBAL_MEM_CACHE_TYPE).\n");

      printf("  Cache type:\t\t\t\t\t ");
      switch (mctype) {
        case CL_NONE:
          printf("None\n");
        break;
        case CL_READ_ONLY_CACHE:
          printf("Read only\n");
        break;
        case CL_READ_WRITE_CACHE:
          printf("Read/Write\n");
        break;
      }

      status = clGetDeviceInfo(devices[j], CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE).\n");
      printf("  Cache line size:\t\t\t\t %i\n", uintinfo);


      status = clGetDeviceInfo(devices[j], CL_DEVICE_GLOBAL_MEM_CACHE_SIZE, sizeof(cl_ulong), &ulonginfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_GLOBAL_MEM_CACHE_SIZE).\n");
      printf("  Cache size:\t\t\t\t\t %li\n", ulonginfo);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(cl_ulong), &ulonginfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_GLOBAL_MEM_SIZE).\n");
      printf("  Global memory size:\t\t\t\t %li\n", ulonginfo);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE, sizeof(cl_ulong), &ulonginfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE).\n");
      printf("  Constant buffer size:\t\t\t\t %li\n", ulonginfo);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_CONSTANT_ARGS, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_CONSTANT_ARGS).\n");
      printf("  Max number of constant args:\t\t\t %i\n", uintinfo);

      cl_device_local_mem_type lmtype;
      status = clGetDeviceInfo(devices[j], CL_DEVICE_LOCAL_MEM_TYPE, sizeof(cl_device_local_mem_type), &lmtype, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_LOCAL_MEM_TYPE).\n");
      printf("  Local memory type:\t\t\t\t ");
      switch (lmtype) {
        case CL_LOCAL:
          printf("Scratchpad\n");
          break;
        case CL_GLOBAL:
          printf("Global\n");
          break;
        default:
          printf("Error: Unkown local memory type!\n");
          return false;
          break;
      }

      status = clGetDeviceInfo(devices[j], CL_DEVICE_LOCAL_MEM_SIZE, sizeof(cl_ulong), &ulonginfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_LOCAL_MEM_SIZE).\n");
      printf("  Local memory size:\t\t\t\t %li\n", ulonginfo);


      status = clGetDeviceInfo(devices[j], CL_DEVICE_PROFILING_TIMER_RESOLUTION, sizeof(size_t), &sizeinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_PROFILING_TIMER_RESOLUTION).\n");
      printf("  Profiling timer resolution:\t\t\t %zi\n", sizeinfo);



      status = clGetDeviceInfo(devices[j], CL_DEVICE_ENDIAN_LITTLE, sizeof(cl_bool), &boolinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_ENDIAN_LITTLE).\n");
      printf("  Device endianess:\t\t\t\t %s\n", boolinfo ? "Little" : "Big");

      status = clGetDeviceInfo(devices[j], CL_DEVICE_AVAILABLE, sizeof(cl_bool), &boolinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_AVAILABLE).\n");
      printf("  Available:\t\t\t\t\t %s\n", boolinfo ? "Yes" : "No");

      status = clGetDeviceInfo(devices[j], CL_DEVICE_COMPILER_AVAILABLE, sizeof(cl_bool), &boolinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_COMPILER_AVAILABLE).\n");
      printf("  Compiler available:\t\t\t\t %s\n", boolinfo ? "Yes" : "No");

      cl_device_exec_capabilities excapa;
      status = clGetDeviceInfo(devices[j], CL_DEVICE_EXECUTION_CAPABILITIES, sizeof(cl_device_exec_capabilities), &excapa, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_EXECUTION_CAPABILITIES).\n");
      printf("  Execution capabilities:\t\t\t\t \n");
      printf("    Execute OpenCL kernels:\t\t\t %s\n", excapa & CL_EXEC_KERNEL ? "Yes" : "No");
      printf("    Execute native function:\t\t\t %s\n", excapa & CL_EXEC_NATIVE_KERNEL ? "Yes" : "No");

      cl_command_queue_properties cmdqprops;
      status = clGetDeviceInfo(devices[j], CL_DEVICE_QUEUE_PROPERTIES, sizeof(cl_command_queue_properties), &cmdqprops, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_QUEUE_PROPERTIES).\n");

      printf("  Queue properties:\t\t\t\t \n");
      printf("    Out-of-Order:\t\t\t\t %s\n", cmdqprops & CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE ? "Yes" : "No");
      printf("    Profiling :\t\t\t\t\t %s\n", cmdqprops & CL_QUEUE_PROFILING_ENABLE ? "Yes" : "No");

      cl_platform_id platid;
      status = clGetDeviceInfo(devices[j], CL_DEVICE_PLATFORM, sizeof(cl_platform_id), &platid, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_PLATFORM).\n");
      printf("  Platform ID:\t\t\t\t\t %p\n", platid);


      status = clGetDeviceInfo(devices[j], CL_DEVICE_NAME, BUFBYTES, charbuf, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_NAME).\n");
      printf("  Name:\t\t\t\t\t\t %s\n", charbuf);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_VENDOR, BUFBYTES, charbuf, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_VENDOR).\n");
      printf("  Vendor:\t\t\t\t\t %s\n", charbuf);

      status = clGetDeviceInfo(devices[j], CL_DRIVER_VERSION, BUFBYTES, charbuf, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DRIVER_VERSION).\n");
      printf("  Driver version:\t\t\t\t %s\n", charbuf);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_PROFILE, BUFBYTES, charbuf, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_PROFILE).\n");
      printf("  Profile:\t\t\t\t\t %s\n", charbuf);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_VERSION, BUFBYTES, charbuf, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_VERSION).\n");
      printf("  Version:\t\t\t\t\t %s\n", charbuf);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_EXTENSIONS, BUFBYTES, charbuf, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_EXTENSIONS).\n");
      printf("  Extensions:\t\t\t\t\t %s\n", charbuf);

    }

    printf("\n\n");
  }


  free(platforms);

  return true;
}





bool runPUTests () {
  struct timeval start, end, result;
  JobToPUM *job;

  //Inferring PUs' latency and throughput
  for (int PUID = 0; PUID < PUInfoStruct.nPUs; PUID++) { //each PU

    for (int jobID = 0; jobID < 3; jobID++) { //three jobs
fprintf(stderr,"PUM (%i): Running test job %i.\n",myid, jobID);
      job = malloc(sizeof(JobToPUM));
      job->runOn = PUID;
      job->jobID = jobID;
      switch (jobID) {
        case 0:
          if(!createLowProcJob(job))
            return false;
          break;
        case 1:
          //if (!createHighProcJob(job))
          if (!createTestMandelbrot (job))
            return false;
          break;
        case 2:
          if (!createBandwidthTestJob(job))
            return false;
          break;
        default:
          fprintf(stderr,"PROGRAMMING ERROR: trying to create too many test jobs.\n");
          return false;
        break;
      }

      if (jobID == 2) {
        //for bandwidth testing, we only care about buffer transfers
	//start timer
        if (gettimeofday(&start, NULL)) {
          perror("gettimeofday");
          return false;
        }

        //initialize buffers
        if (!initializeCLBuffers(job))
          return false;
      }
     
      else {
	if (!initializeCLBuffers(job))
	  return false;

        //start timer
        if (gettimeofday(&start, NULL)) {
          perror("gettimeofday");
          return false;
        }

        //run job
        if (!execJob(job))
          return false;
      }

      //end timer
      if (gettimeofday(&end, NULL)) {
        perror("gettimeofday");
        return false;
      }


      timeval_subtract(&result, &end, &start);

      switch (jobID) {
        case 0: //lowProcJob
          PUInfoStruct.availablePUs[PUID].latency    = result.tv_sec + (result.tv_usec / 1000000.0);
          break;
        case 1: //highProcJob
          PUInfoStruct.availablePUs[PUID].throughput = result.tv_sec + (result.tv_usec / 1000000.0);
          break;
        case 2: //BandwidthTestJob
          PUInfoStruct.availablePUs[PUID].bandwidth =  ((PUInfoStruct.availablePUs[PUID].max_memory_alloc*0.9)/(1024*1024)*30) / result.tv_sec + (result.tv_usec / 1000000.0);
          break;
      }

      //verify results
      checkJobResults(job);

      cleanup(job);

fprintf(stderr,"PUM (%i): Test job %i DONE.\n",myid, jobID);

    }

  }
  return true;
}


