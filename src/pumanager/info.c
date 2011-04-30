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
    cheetah_print_error("ERROR: %s (%i)", name, err);
    return false;
  }
  else {
    cheetah_print_error("No error to print (%s / %i).", name, err);
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
    cheetah_print_error("numPlatforms is %i. Please check if an OpenCL environment is correctly installed.", numPlatforms);
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
          cheetah_print_error("ERROR: Unkown local memory type!\n");
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
        cheetah_print_error("clCreateContextFromType failed (%i).", status);
        return false;
      }

      cl_command_queue_properties prop = 0;
      cl_command_queue *commandQueue = malloc(sizeof(cl_command_queue));
      //Notice: The following call creates 3 threads on this process, each waiting on a condition variable
      *commandQueue = clCreateCommandQueue(*(PUInfoStruct.PUsContexts[PUInfoStruct.nPUs-numDevices + j]), devices[j], prop, &status);
      if (status != CL_SUCCESS) {
        cheetah_print_error("clCreateCommandQueue failed (%i).", status);
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
    cheetah_print_error("numPlatforms = %i. Check if an OpenCL environment is correctly installed.", numPlatforms);
    return false;
  }

  platforms = calloc(numPlatforms, sizeof(cl_platform_id));
  status = clGetPlatformIDs(numPlatforms, platforms, NULL);
  if (status != CL_SUCCESS)
    return checkErr(status, "clGetPlatformIDs failed.\n");

  // Iteratate over platforms
  cheetah_print("Number of platforms:\t\t\t\t %i", numPlatforms);
  for (unsigned int i = 0; i < numPlatforms; i++) {

    status = clGetPlatformInfo(platforms[i], CL_PLATFORM_PROFILE, BUFBYTES, charbuf, NULL);
    if (status != CL_SUCCESS)
      return checkErr(status, "clGetPlatformInfo failed (CL_PLATFORM_PROFILE).\n");
    cheetah_print("  Plaform Profile:\t\t\t\t %s", charbuf);

    status = clGetPlatformInfo(platforms[i], CL_PLATFORM_VERSION, BUFBYTES, charbuf, NULL);
    if (status != CL_SUCCESS)
      return checkErr(status, "clGetPlatformInfo failed (CL_PLATFORM_VERSION).\n");
    cheetah_print("  Plaform Version:\t\t\t\t %s", charbuf);

    status = clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, BUFBYTES, charbuf, NULL);
    if (status != CL_SUCCESS)
      return checkErr(status, "clGetPlatformInfo failed (CL_PLATFORM_NAME).\n");
    cheetah_print("  Plaform Name:\t\t\t\t\t %s", charbuf);

    status = clGetPlatformInfo(platforms[i], CL_PLATFORM_VENDOR, BUFBYTES, charbuf, NULL);
    if (status != CL_SUCCESS)
      return checkErr(status, "clGetPlatformInfo failed (CL_PLATFORM_VENDOR).\n");
    cheetah_print("  Plaform Vendor:\t\t\t\t %s", charbuf);

    status = clGetPlatformInfo(platforms[i], CL_PLATFORM_EXTENSIONS, BUFBYTES, charbuf, NULL);
    if (status != CL_SUCCESS)
      return checkErr(status, "clGetPlatformInfo failed (CL_PLATFORM_EXTENSIONS).\n");
    if (charbuf[0] != '\0')
      cheetah_print("  Plaform Extensions:\t\t\t %s", charbuf);
  }

  cheetah_print("\n");
  // Now Iteratate over each platform and its devices
  for (unsigned int i = 0; i< numPlatforms; i++) {
    status = clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, BUFBYTES, charbuf, NULL);
    if (status != CL_SUCCESS)
      return checkErr(status, "clGetPlatformInfo failed (CL_PLATFORM_NAME).\n");
    cheetah_print("  Plaform Name:\t\t\t\t\t %s", charbuf);


    cl_uint numDevices;
    status = clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, 0, NULL, &numDevices);
    if (status != CL_SUCCESS)
      return checkErr(status, "clGetDeviceIDs failed (CL_DEVICE_TYPE_ALL).\n");
    cheetah_print("Number of devices:\t\t\t\t %i", numDevices);

    cl_device_id devices[numDevices];

    status = clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, numDevices, devices, NULL);
    if (status != CL_SUCCESS)
      return checkErr(status, "clGetDeviceIDs failed (CL_DEVICE_TYPE_ALL).\n");

    for (unsigned int j = 0; j < numDevices; j++) {

      cheetah_print("  Device Type:\t\t\t\t\t ");
      cl_device_type dtype;
      status = clGetDeviceInfo(devices[j], CL_DEVICE_TYPE, sizeof(cl_device_type), &dtype, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_TYPE).\n");
      switch (dtype) {
        case CL_DEVICE_TYPE_ACCELERATOR:
          cheetah_print("CL_DEVICE_TYPE_ACCELERATOR");
          break;
        case CL_DEVICE_TYPE_CPU:
          cheetah_print("CL_DEVICE_TYPE_CPU");
          break;
        case CL_DEVICE_TYPE_DEFAULT:
          cheetah_print("CL_DEVICE_TYPE_DEFAULT");
          break;
        case CL_DEVICE_TYPE_GPU:
          cheetah_print("CL_DEVICE_TYPE_GPU");
          break;
        default:
          cheetah_print_error("ERROR: Unknown device type!");
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
      cheetah_print("  Device ID:\t\t\t\t\t %i", uintinfo);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_COMPUTE_UNITS).\n");
      cheetah_print("  Max compute units:\t\t\t\t %i", uintinfo);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS).\n");
      cheetah_print("  Max work items dimensions:\t\t\t %i", uintinfo);

      size_t witems[uintinfo];
      status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_WORK_ITEM_SIZES, uintinfo*sizeof(size_t), witems, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_WORK_ITEM_SIZES).\n");
      for (cl_uint x = 0; x < uintinfo; x++)
        cheetah_print("    Max work items[%i]:\t\t\t\t %zi", x, witems[x]);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), &sizeinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_WORK_GROUP_SIZE).\n");
      cheetah_print("  Max work group size:\t\t\t\t %zi", sizeinfo);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR).\n");
      cheetah_print("  Preferred vector width char:\t\t\t %i", uintinfo);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT).\n");
      cheetah_print("  Preferred vector width short:\t\t\t %i", uintinfo);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT).\n");
      cheetah_print("  Preferred vector width int:\t\t\t %i", uintinfo);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG).\n");
      cheetah_print("  Preferred vector width long:\t\t\t %i", uintinfo);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT).\n");
      cheetah_print("  Preferred vector width float:\t\t\t %i", uintinfo);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE).\n");
      cheetah_print("  Preferred vector width double:\t\t %i", uintinfo);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_CLOCK_FREQUENCY).\n");
      cheetah_print("  Max clock frequency:\t\t\t\t %iMhz", uintinfo);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_ADDRESS_BITS, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_ADDRESS_BITS).\n");
      cheetah_print("  Address bits:\t\t\t\t\t %i", uintinfo);


      status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(cl_ulong), &ulonginfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_MEM_ALLOC_SIZE).\n");
      cheetah_print("  Max memeory allocation:\t\t\t %li", ulonginfo);


      status = clGetDeviceInfo(devices[j], CL_DEVICE_IMAGE_SUPPORT, sizeof(cl_bool), &boolinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_IMAGE_SUPPORT).\n");
      cheetah_print("  Image support:\t\t\t\t %s", boolinfo ? "Yes" : "No");

      if (boolinfo) {
        status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_READ_IMAGE_ARGS, sizeof(cl_uint), &uintinfo, NULL);
        if (status != CL_SUCCESS)
          return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_READ_IMAGE_ARGS).\n");
        cheetah_print("  Max number of images read arguments:\t %i", uintinfo);

        status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_WRITE_IMAGE_ARGS, sizeof(cl_uint), &uintinfo, NULL);
        if (status != CL_SUCCESS)
          return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_WRITE_IMAGE_ARGS).\n");
        cheetah_print("  Max number of images write arguments:\t %i", uintinfo);

        status = clGetDeviceInfo(devices[j], CL_DEVICE_IMAGE2D_MAX_WIDTH, sizeof(size_t), &sizeinfo, NULL);
        if (status != CL_SUCCESS)
          return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_IMAGE2D_MAX_WIDTH).\n");
        cheetah_print("  Max image 2D width:\t\t\t %zi", sizeinfo);

        status = clGetDeviceInfo(devices[j], CL_DEVICE_IMAGE2D_MAX_HEIGHT, sizeof(size_t), &sizeinfo, NULL);
        if (status != CL_SUCCESS)
          return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_IMAGE2D_MAX_HEIGHT).\n");
        cheetah_print("  Max image 2D height:\t\t\t %zi", sizeinfo);

        status = clGetDeviceInfo(devices[j], CL_DEVICE_IMAGE3D_MAX_WIDTH, sizeof(size_t), &sizeinfo, NULL);
        if (status != CL_SUCCESS)
          return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_IMAGE2D_MAX_WIDTH).\n");
        cheetah_print("  Max image 3D width:\t\t\t %zi", sizeinfo);

        status = clGetDeviceInfo(devices[j], CL_DEVICE_IMAGE3D_MAX_HEIGHT, sizeof(size_t), &sizeinfo, NULL);
        if (status != CL_SUCCESS)
          return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_IMAGE3D_MAX_HEIGHT).\n");
        cheetah_print("  Max image 3D height:\t %zi", sizeinfo);

        status = clGetDeviceInfo(devices[j], CL_DEVICE_IMAGE3D_MAX_DEPTH, sizeof(size_t), &sizeinfo, NULL);
        if (status != CL_SUCCESS)
          return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_IMAGE3D_MAX_DEPTH).\n");
        cheetah_print("  Max image 3D depth:\t\t\t %zi", sizeinfo);

        status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_SAMPLERS, sizeof(cl_uint), &uintinfo, NULL);
        if (status != CL_SUCCESS)
          return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_SAMPLERS).\n");
        cheetah_print("  Max samplers within kernel:\t\t %i", uintinfo);
      }

      status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_PARAMETER_SIZE, sizeof(size_t), &sizeinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_PARAMETER_SIZE).\n");
      cheetah_print("  Max size of kernel argument:\t\t\t %zi", sizeinfo);


      status = clGetDeviceInfo(devices[j], CL_DEVICE_MEM_BASE_ADDR_ALIGN, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MEM_BASE_ADDR_ALIGN).\n");
      cheetah_print("  Alignment (bits) of base address:\t\t %i", uintinfo);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE).\n");
      cheetah_print("  Minimum alignment (bytes) for any datatype:\t %i", uintinfo);


      cl_device_fp_config fpinfo;
      status = clGetDeviceInfo(devices[j], CL_DEVICE_SINGLE_FP_CONFIG, sizeof(cl_device_fp_config), &fpinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_SINGLE_FP_CONFIG).\n");
      cheetah_print("  Single precision floating point capability");
      cheetah_print("    Denorms:\t\t\t\t\t %s", fpinfo & CL_FP_DENORM ? "Yes" : "No");
      cheetah_print("    Quiet NaNs:\t\t\t\t\t %s", fpinfo & CL_FP_INF_NAN ? "Yes" : "No");
      cheetah_print("    Round to nearest even:\t\t\t %s", fpinfo & CL_FP_ROUND_TO_NEAREST ? "Yes" : "No");
      cheetah_print("    Round to zero:\t\t\t\t %s", fpinfo & CL_FP_ROUND_TO_ZERO ? "Yes" : "No");
      cheetah_print("    Round to +ve and infinity:\t\t\t %s", fpinfo & CL_FP_ROUND_TO_INF ? "Yes" : "No");
      cheetah_print("    IEEE754-2008 fused multiply-add:\t\t %s", fpinfo & CL_FP_FMA ? "Yes" : "No");


      cl_device_mem_cache_type mctype;
      status = clGetDeviceInfo(devices[j], CL_DEVICE_GLOBAL_MEM_CACHE_TYPE, sizeof(cl_device_mem_cache_type), &mctype, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_GLOBAL_MEM_CACHE_TYPE).\n");

      cheetah_print("  Cache type:\t\t\t\t\t %s",
                    mctype == CL_NONE? "None" :
                    mctype == CL_READ_ONLY_CACHE ? "Read only" :
                    mctype == CL_READ_WRITE_CACHE ? "Read/Write" : "");

      status = clGetDeviceInfo(devices[j], CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE).\n");
      cheetah_print("  Cache line size:\t\t\t\t %i", uintinfo);


      status = clGetDeviceInfo(devices[j], CL_DEVICE_GLOBAL_MEM_CACHE_SIZE, sizeof(cl_ulong), &ulonginfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_GLOBAL_MEM_CACHE_SIZE).\n");
      cheetah_print("  Cache size:\t\t\t\t\t %li", ulonginfo);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(cl_ulong), &ulonginfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_GLOBAL_MEM_SIZE).\n");
      cheetah_print("  Global memory size:\t\t\t\t %li", ulonginfo);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE, sizeof(cl_ulong), &ulonginfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE).\n");
      cheetah_print("  Constant buffer size:\t\t\t\t %li", ulonginfo);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_MAX_CONSTANT_ARGS, sizeof(cl_uint), &uintinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_MAX_CONSTANT_ARGS).\n");
      cheetah_print("  Max number of constant args:\t\t\t %i", uintinfo);

      cl_device_local_mem_type lmtype;
      status = clGetDeviceInfo(devices[j], CL_DEVICE_LOCAL_MEM_TYPE, sizeof(cl_device_local_mem_type), &lmtype, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_LOCAL_MEM_TYPE).\n");
      cheetah_print("  Local memory type:\t\t\t\t %s",
                    lmtype == CL_LOCAL ? "Scratchpad" :
                    lmtype == CL_GLOBAL ? "Global" : "");
      if (lmtype != CL_LOCAL && lmtype != CL_GLOBAL) {
        cheetah_print_error("ERROR: Unkown local memory type!");
        return false;
      }

      status = clGetDeviceInfo(devices[j], CL_DEVICE_LOCAL_MEM_SIZE, sizeof(cl_ulong), &ulonginfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_LOCAL_MEM_SIZE).\n");
      cheetah_print("  Local memory size:\t\t\t\t %li", ulonginfo);


      status = clGetDeviceInfo(devices[j], CL_DEVICE_PROFILING_TIMER_RESOLUTION, sizeof(size_t), &sizeinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_PROFILING_TIMER_RESOLUTION).\n");
      cheetah_print("  Profiling timer resolution:\t\t\t %zi", sizeinfo);



      status = clGetDeviceInfo(devices[j], CL_DEVICE_ENDIAN_LITTLE, sizeof(cl_bool), &boolinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_ENDIAN_LITTLE).\n");
      cheetah_print("  Device endianess:\t\t\t\t %s", boolinfo ? "Little" : "Big");

      status = clGetDeviceInfo(devices[j], CL_DEVICE_AVAILABLE, sizeof(cl_bool), &boolinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_AVAILABLE).\n");
      cheetah_print("  Available:\t\t\t\t\t %s", boolinfo ? "Yes" : "No");

      status = clGetDeviceInfo(devices[j], CL_DEVICE_COMPILER_AVAILABLE, sizeof(cl_bool), &boolinfo, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_COMPILER_AVAILABLE).\n");
      cheetah_print("  Compiler available:\t\t\t\t %s", boolinfo ? "Yes" : "No");

      cl_device_exec_capabilities excapa;
      status = clGetDeviceInfo(devices[j], CL_DEVICE_EXECUTION_CAPABILITIES, sizeof(cl_device_exec_capabilities), &excapa, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_EXECUTION_CAPABILITIES).\n");
      cheetah_print("  Execution capabilities:\t\t\t\t ");
      cheetah_print("    Execute OpenCL kernels:\t\t\t %s", excapa & CL_EXEC_KERNEL ? "Yes" : "No");
      cheetah_print("    Execute native function:\t\t\t %s", excapa & CL_EXEC_NATIVE_KERNEL ? "Yes" : "No");

      cl_command_queue_properties cmdqprops;
      status = clGetDeviceInfo(devices[j], CL_DEVICE_QUEUE_PROPERTIES, sizeof(cl_command_queue_properties), &cmdqprops, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_QUEUE_PROPERTIES).\n");

      cheetah_print("  Queue properties:\t\t\t\t ");
      cheetah_print("    Out-of-Order:\t\t\t\t %s", cmdqprops & CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE ? "Yes" : "No");
      cheetah_print("    Profiling :\t\t\t\t\t %s", cmdqprops & CL_QUEUE_PROFILING_ENABLE ? "Yes" : "No");

      cl_platform_id platid;
      status = clGetDeviceInfo(devices[j], CL_DEVICE_PLATFORM, sizeof(cl_platform_id), &platid, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_PLATFORM).\n");
      cheetah_print("  Platform ID:\t\t\t\t\t %p", platid);


      status = clGetDeviceInfo(devices[j], CL_DEVICE_NAME, BUFBYTES, charbuf, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_NAME).\n");
      cheetah_print("  Name:\t\t\t\t\t\t %s", charbuf);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_VENDOR, BUFBYTES, charbuf, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_VENDOR).\n");
      cheetah_print("  Vendor:\t\t\t\t\t %s", charbuf);

      status = clGetDeviceInfo(devices[j], CL_DRIVER_VERSION, BUFBYTES, charbuf, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DRIVER_VERSION).\n");
      cheetah_print("  Driver version:\t\t\t\t %s", charbuf);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_PROFILE, BUFBYTES, charbuf, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_PROFILE).\n");
      cheetah_print("  Profile:\t\t\t\t\t %s", charbuf);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_VERSION, BUFBYTES, charbuf, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_VERSION).\n");
      cheetah_print("  Version:\t\t\t\t\t %s", charbuf);

      status = clGetDeviceInfo(devices[j], CL_DEVICE_EXTENSIONS, BUFBYTES, charbuf, NULL);
      if (status != CL_SUCCESS)
        return checkErr(status, "clGetDeviceInfo failed (CL_DEVICE_EXTENSIONS).\n");
      cheetah_print("  Extensions:\t\t\t\t\t %s", charbuf);

    }

    cheetah_print("\n");
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
      cheetah_info_print("Running test job %i.", jobID);
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
          cheetah_print_error("PROGRAMMING ERROR: trying to create too many test jobs.");
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

    cheetah_info_print("Test job %i DONE.", jobID);

    }

  }
  return true;
}


