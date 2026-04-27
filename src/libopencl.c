/*
 *   Stub libopencl that dlsyms into actual library based on environment variable
 *
 *   LIBOPENCL_SO_PATH      -- Path to opencl so that will be searched first
 *   LIBOPENCL_SO_PATH_2    -- Searched second
 *   LIBOPENCL_SO_PATH_3    -- Searched third
 *   LIBOPENCL_SO_PATH_4    -- Searched fourth
 *
 *   If none of these are set, default system paths will be considered
**/
#include <stdlib.h>
#include <sys/stat.h>
#include "libopencl.h"
#if defined(__APPLE__) || defined(__MACOSX) || defined(__ANDROID__) || defined(__linux__) || defined(_POSIX_C_SOURCE)
#include <dlfcn.h>
#include <pthread.h>
static pthread_mutex_t g_init_mutex = PTHREAD_MUTEX_INITIALIZER;
static void stub_lock(void)   { pthread_mutex_lock(&g_init_mutex); }
static void stub_unlock(void) { pthread_mutex_unlock(&g_init_mutex); }
#elif defined(_WIN32) || defined(WINVER)
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#ifndef RTLD_LAZY
#define RTLD_LAZY 0
#endif

static volatile LONG g_init_lock = 0;
static void stub_lock(void)   { while (InterlockedCompareExchange(&g_init_lock, 1, 0) != 0) Sleep(0); }
static void stub_unlock(void) { InterlockedExchange(&g_init_lock, 0); }

static struct {
    long lasterror;
    const char *err_rutin;
} var = {
    0,
    NULL
};

void *dlopen (const char *filename, int flags){
    HINSTANCE hInst;
    (void)flags;

    hInst= LoadLibrary (filename);
    if (hInst==NULL) {
        var.lasterror = GetLastError ();
        var.err_rutin = "dlopen";
    }
    return hInst;
}

int dlclose (void *handle){
    BOOL ok;
    int rc= 0;

    ok= FreeLibrary ((HINSTANCE)handle);
    if (! ok) {
        var.lasterror = GetLastError ();
        var.err_rutin = "dlclose";
        rc= -1;
    }
    return rc;
}

void *dlsym (void *handle, const char *name){
    FARPROC fp;

    if (!handle) return NULL;
    fp= GetProcAddress ((HINSTANCE)handle, name);
    if (!fp) {
        var.lasterror = GetLastError ();
        var.err_rutin = "dlsym";
    }
    return (void *)(intptr_t)fp;
}
const char *dlerror (void){
static char errstr [88];

    if (var.lasterror) {
        snprintf (errstr, sizeof(errstr), "%s error #%ld", var.err_rutin, var.lasterror);
        return errstr;
    } else {
        return NULL;
    }
}
#endif

#if defined(__APPLE__) || defined(__MACOSX)
static const char *default_so_paths[] = {
  "libOpenCL.so",
  "/System/Library/Frameworks/OpenCL.framework/OpenCL"
};
#elif defined(__ANDROID__)
static const char *default_so_paths[] = {
  "/system/lib64/libOpenCL.so",
  "/system/vendor/lib64/libOpenCL.so",
  "/system/vendor/lib64/egl/libGLES_mali.so",
  "/system/vendor/lib64/libPVROCL.so",
  "/data/data/org.pocl.libs/files/lib64/libpocl.so",
  "/system/lib/libOpenCL.so",
  "/system/vendor/lib/libOpenCL.so",
  "/system/vendor/lib/egl/libGLES_mali.so",
  "/system/vendor/lib/libPVROCL.so",
  "/data/data/org.pocl.libs/files/lib/libpocl.so",
  "libOpenCL.so"
};
#elif defined(_WIN32) || defined(WINVER)
static const char *default_so_paths[] = {
  "OpenCL.dll"
};
#elif defined(__linux__)
static const char *default_so_paths[] = {
  "/usr/lib/libOpenCL.so",
  "/usr/local/lib/libOpenCL.so",
  "/usr/local/lib/libpocl.so",
  "/usr/lib64/libOpenCL.so",
  "/usr/lib32/libOpenCL.so",
  "libOpenCL.so"
};
#endif

static void *so_handle = NULL;


static int access_file(const char *filename)
{
  struct stat buffer;
  return (stat(filename, &buffer) == 0);
}

static int open_libopencl_so()
{
  const char *env_vars[] = {
    "LIBOPENCL_SO_PATH", "LIBOPENCL_SO_PATH_2",
    "LIBOPENCL_SO_PATH_3", "LIBOPENCL_SO_PATH_4"
  };
  char *str = NULL;
  size_t i;

  for (i = 0; i < sizeof(env_vars) / sizeof(char*); i++) {
    if ((str = getenv(env_vars[i])) && access_file(str)) {
      so_handle = dlopen(str, RTLD_LAZY);
      if (so_handle) return 0;
    }
  }

  for (i = 0; i < sizeof(default_so_paths) / sizeof(char*); i++) {
    so_handle = dlopen(default_so_paths[i], RTLD_LAZY);
    if (so_handle) return 0;
  }

  return -1;
}

#define SET_ERR_RET_NULL() do { if (errcode_ret) *errcode_ret = CL_INVALID_PLATFORM; return NULL; } while(0)

static void *stub_dlsym(const char *name)
{
  if (!so_handle) {
    stub_lock();
    if (!so_handle) open_libopencl_so();
    stub_unlock();
  }
  if (!so_handle) return NULL;
  return stub_dlsym(name);
}

void stubOpenclReset()
{
  stub_lock();
  if(so_handle)
    dlclose(so_handle);
  so_handle = NULL;
  stub_unlock();
}

/* ==========================================================================
 * OpenCL 1.0 / 1.1 / 1.2 API wrappers
 * ======================================================================== */

cl_int
clGetPlatformIDs(cl_uint          num_entries,
                 cl_platform_id * platforms,
                 cl_uint *        num_platforms)
{
  f_clGetPlatformIDs func;

  func = (f_clGetPlatformIDs) stub_dlsym("clGetPlatformIDs");
  if(func) {
    return func(num_entries, platforms, num_platforms);
  } else {
    return CL_INVALID_PLATFORM;
  }
}


cl_int
clGetPlatformInfo(cl_platform_id   platform,
                  cl_platform_info param_name,
                  size_t           param_value_size,
                  void *           param_value,
                  size_t *         param_value_size_ret)
{
  f_clGetPlatformInfo func;

  func = (f_clGetPlatformInfo) stub_dlsym("clGetPlatformInfo");
  if(func) {
    return func(platform, param_name, param_value_size, param_value, param_value_size_ret);
  } else {
    return CL_INVALID_PLATFORM;
  }
}


cl_int
clGetDeviceIDs(cl_platform_id   platform,
               cl_device_type   device_type,
               cl_uint          num_entries,
               cl_device_id *   devices,
               cl_uint *        num_devices)
{
  f_clGetDeviceIDs func;

  func = (f_clGetDeviceIDs) stub_dlsym("clGetDeviceIDs");
  if(func) {
    return func(platform, device_type, num_entries, devices, num_devices);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clGetDeviceInfo(cl_device_id    device,
                cl_device_info  param_name,
                size_t          param_value_size,
                void *          param_value,
                size_t *        param_value_size_ret)
{
  f_clGetDeviceInfo func;

  func = (f_clGetDeviceInfo) stub_dlsym("clGetDeviceInfo");
  if(func) {
    return func(device, param_name, param_value_size, param_value, param_value_size_ret);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clCreateSubDevices(cl_device_id                         in_device,
                   const cl_device_partition_property * properties,
                   cl_uint                              num_devices,
                   cl_device_id *                       out_devices,
                   cl_uint *                            num_devices_ret)
{
  f_clCreateSubDevices func;

  func = (f_clCreateSubDevices) stub_dlsym("clCreateSubDevices");
  if(func) {
    return func(in_device, properties, num_devices, out_devices, num_devices_ret);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clRetainDevice(cl_device_id device)
{
  f_clRetainDevice func;

  func = (f_clRetainDevice) stub_dlsym("clRetainDevice");
  if(func) {
    return func(device);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clReleaseDevice(cl_device_id device)
{
  f_clReleaseDevice func;

  func = (f_clReleaseDevice) stub_dlsym("clReleaseDevice");
  if(func) {
    return func(device);
  } else {
    return CL_INVALID_PLATFORM;
  }
}


cl_context
clCreateContext(const cl_context_properties * properties,
                cl_uint                 num_devices,
                const cl_device_id *    devices,
                void (*pfn_notify)(const char *, const void *, size_t, void *),
                void *                  user_data,
                cl_int *                errcode_ret)
{
  f_clCreateContext func;

  func = (f_clCreateContext) stub_dlsym("clCreateContext");
  if(func) {
    return func(properties, num_devices, devices, pfn_notify, user_data, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}

cl_context
clCreateContextFromType(const cl_context_properties * properties,
                        cl_device_type          device_type,
                        void (*pfn_notify )(const char *, const void *, size_t, void *),
                        void *                  user_data,
                        cl_int *                errcode_ret)
{
  f_clCreateContextFromType func;

  func = (f_clCreateContextFromType) stub_dlsym("clCreateContextFromType");
  if(func) {
    return func(properties, device_type, pfn_notify, user_data, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}

cl_int
clRetainContext(cl_context context)
{
  f_clRetainContext func;

  func = (f_clRetainContext) stub_dlsym("clRetainContext");
  if(func) {
    return func(context);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clReleaseContext(cl_context context)
{
  f_clReleaseContext func;

  func = (f_clReleaseContext) stub_dlsym("clReleaseContext");
  if(func) {
    return func(context);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clGetContextInfo(cl_context         context,
                 cl_context_info    param_name,
                 size_t             param_value_size,
                 void *             param_value,
                 size_t *           param_value_size_ret)
{
  f_clGetContextInfo func;

  func = (f_clGetContextInfo) stub_dlsym("clGetContextInfo");
  if(func) {
    return func(context, param_name, param_value_size,
                param_value, param_value_size_ret);
  } else {
    return CL_INVALID_PLATFORM;
  }
}


cl_command_queue
clCreateCommandQueue(cl_context                     context,
                     cl_device_id                   device,
                     cl_command_queue_properties    properties,
                     cl_int *                       errcode_ret)
{
  f_clCreateCommandQueue func;

  func = (f_clCreateCommandQueue) stub_dlsym("clCreateCommandQueue");
  if(func) {
    return func(context, device, properties, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}

#ifdef CL_VERSION_2_0
cl_command_queue
clCreateCommandQueueWithProperties(cl_context                     context,
                                   cl_device_id                   device,
                     	             const cl_queue_properties *    properties,
                                   cl_int *                       errcode_ret)
{
  f_clCreateCommandQueueWithProperties func;

  func = (f_clCreateCommandQueueWithProperties) stub_dlsym("clCreateCommandQueueWithProperties");
  if(func) {
    return func(context, device, properties, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}
#endif

cl_int
clRetainCommandQueue(cl_command_queue command_queue)
{
  f_clRetainCommandQueue func;

  func = (f_clRetainCommandQueue) stub_dlsym("clRetainCommandQueue");
  if(func) {
    return func(command_queue);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clReleaseCommandQueue(cl_command_queue command_queue)
{
  f_clReleaseCommandQueue func;

  func = (f_clReleaseCommandQueue) stub_dlsym("clReleaseCommandQueue");
  if(func) {
    return func(command_queue);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clGetCommandQueueInfo(cl_command_queue      command_queue,
                      cl_command_queue_info param_name,
                      size_t                param_value_size,
                      void *                param_value,
                      size_t *              param_value_size_ret)
{
  f_clGetCommandQueueInfo func;

  func = (f_clGetCommandQueueInfo) stub_dlsym("clGetCommandQueueInfo");
  if(func) {
    return func(command_queue, param_name, param_value_size,
                param_value, param_value_size_ret);
  } else {
    return CL_INVALID_PLATFORM;
  }
}


cl_mem
clCreateBuffer(cl_context   context,
               cl_mem_flags flags,
               size_t       size,
               void *       host_ptr,
               cl_int *     errcode_ret)
{
  f_clCreateBuffer func;

  func = (f_clCreateBuffer) stub_dlsym("clCreateBuffer");
  if(func) {
    return func(context, flags, size, host_ptr, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}

cl_mem
clCreateSubBuffer(cl_mem                   buffer,
                  cl_mem_flags             flags,
                  cl_buffer_create_type    buffer_create_type,
                  const void *             buffer_create_info,
                  cl_int *                 errcode_ret)
{
  f_clCreateSubBuffer func;

  func = (f_clCreateSubBuffer) stub_dlsym("clCreateSubBuffer");
  if(func) {
    return func(buffer, flags, buffer_create_type,
                buffer_create_info, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}

cl_mem
clCreateImage(cl_context              context,
              cl_mem_flags            flags,
              const cl_image_format * image_format,
              const cl_image_desc *   image_desc,
              void *                  host_ptr,
              cl_int *                errcode_ret)
{
  f_clCreateImage func;

  func = (f_clCreateImage) stub_dlsym("clCreateImage");
  if(func) {
    return func(context, flags, image_format, image_desc,
                host_ptr, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}

cl_int
clRetainMemObject(cl_mem memobj)
{
  f_clRetainMemObject func;

  func = (f_clRetainMemObject) stub_dlsym("clRetainMemObject");
  if(func) {
    return func(memobj);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clReleaseMemObject(cl_mem memobj)
{
  f_clReleaseMemObject func;

  func = (f_clReleaseMemObject) stub_dlsym("clReleaseMemObject");
  if(func) {
    return func(memobj);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clGetSupportedImageFormats(cl_context           context,
                           cl_mem_flags         flags,
                           cl_mem_object_type   image_type,
                           cl_uint              num_entries,
                           cl_image_format *    image_formats,
                           cl_uint *            num_image_formats)
{
  f_clGetSupportedImageFormats func;

  func = (f_clGetSupportedImageFormats) stub_dlsym("clGetSupportedImageFormats");
  if(func) {
    return func(context, flags, image_type, num_entries,
                image_formats, num_image_formats);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clGetMemObjectInfo(cl_mem           memobj,
                   cl_mem_info      param_name,
                   size_t           param_value_size,
                   void *           param_value,
                   size_t *         param_value_size_ret)
{
  f_clGetMemObjectInfo func;

  func = (f_clGetMemObjectInfo) stub_dlsym("clGetMemObjectInfo");
  if(func) {
    return func(memobj, param_name, param_value_size,
                param_value, param_value_size_ret);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clGetImageInfo(cl_mem           image,
               cl_image_info    param_name,
               size_t           param_value_size,
               void *           param_value,
               size_t *         param_value_size_ret)
{
  f_clGetImageInfo func;

  func = (f_clGetImageInfo) stub_dlsym("clGetImageInfo");
  if(func) {
    return func(image, param_name, param_value_size,
                param_value, param_value_size_ret);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clSetMemObjectDestructorCallback(  cl_mem memobj,
                                   void (*pfn_notify)( cl_mem memobj, void* user_data),
                                   void * user_data )
{
  f_clSetMemObjectDestructorCallback func;

  func = (f_clSetMemObjectDestructorCallback) stub_dlsym("clSetMemObjectDestructorCallback");
  if(func) {
    return func(memobj, pfn_notify, user_data);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_sampler
clCreateSampler(cl_context          context,
                cl_bool             normalized_coords,
                cl_addressing_mode  addressing_mode,
                cl_filter_mode      filter_mode,
                cl_int *            errcode_ret)
{
  f_clCreateSampler func;

  func = (f_clCreateSampler) stub_dlsym("clCreateSampler");
  if(func) {
    return func(context, normalized_coords, addressing_mode, filter_mode, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}

cl_int
clRetainSampler(cl_sampler sampler)
{
  f_clRetainSampler func;

  func = (f_clRetainSampler) stub_dlsym("clRetainSampler");
  if(func) {
    return func(sampler);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clReleaseSampler(cl_sampler sampler)
{
  f_clReleaseSampler func;

  func = (f_clReleaseSampler) stub_dlsym("clReleaseSampler");
  if(func) {
    return func(sampler);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clGetSamplerInfo(cl_sampler         sampler,
                 cl_sampler_info    param_name,
                 size_t             param_value_size,
                 void *             param_value,
                 size_t *           param_value_size_ret)
{
  f_clGetSamplerInfo func;

  func = (f_clGetSamplerInfo) stub_dlsym("clGetSamplerInfo");
  if(func) {
    return func(sampler, param_name, param_value_size, param_value, param_value_size_ret);
  } else {
    return CL_INVALID_PLATFORM;
  }
}


cl_program
clCreateProgramWithSource(cl_context        context,
                          cl_uint           count,
                          const char **     strings,
                          const size_t *    lengths,
                          cl_int *          errcode_ret)
{
  f_clCreateProgramWithSource func;

  func = (f_clCreateProgramWithSource) stub_dlsym("clCreateProgramWithSource");
  if(func) {
    return func(context, count, strings, lengths, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}



cl_program
clCreateProgramWithBinary(cl_context                     context,
                          cl_uint                        num_devices,
                          const cl_device_id *           device_list,
                          const size_t *                 lengths,
                          const unsigned char **         binaries,
                          cl_int *                       binary_status,
                          cl_int *                       errcode_ret)
{
  f_clCreateProgramWithBinary func;

  func = (f_clCreateProgramWithBinary) stub_dlsym("clCreateProgramWithBinary");
  if(func) {
    return func(context, num_devices, device_list, lengths, binaries, binary_status, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}

cl_program
clCreateProgramWithBuiltInKernels(cl_context            context,
                                  cl_uint               num_devices,
                                  const cl_device_id *  device_list,
                                  const char *          kernel_names,
                                  cl_int *              errcode_ret)
{
  f_clCreateProgramWithBuiltInKernels func;

  func = (f_clCreateProgramWithBuiltInKernels) stub_dlsym("clCreateProgramWithBuiltInKernels");
  if(func) {
    return func(context, num_devices, device_list, kernel_names, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}

cl_int
clRetainProgram(cl_program program)
{
  f_clRetainProgram func;

  func = (f_clRetainProgram) stub_dlsym("clRetainProgram");
  if(func) {
    return func(program);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clReleaseProgram(cl_program program)
{
  f_clReleaseProgram func;

  func = (f_clReleaseProgram) stub_dlsym("clReleaseProgram");
  if(func) {
    return func(program);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clBuildProgram(cl_program           program,
               cl_uint              num_devices,
               const cl_device_id * device_list,
               const char *         options,
               void (*pfn_notify)(cl_program program, void * user_data),
               void *               user_data)
{
  f_clBuildProgram func;

  func = (f_clBuildProgram) stub_dlsym("clBuildProgram");
  if(func) {
    return func(program, num_devices, device_list, options, pfn_notify, user_data);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clCompileProgram(cl_program           program,
                 cl_uint              num_devices,
                 const cl_device_id * device_list,
                 const char *         options,
                 cl_uint              num_input_headers,
                 const cl_program *   input_headers,
                 const char **        header_include_names,
                 void (*pfn_notify)(cl_program program, void * user_data),
                 void *               user_data)
{
  f_clCompileProgram func;

  func = (f_clCompileProgram) stub_dlsym("clCompileProgram");
  if(func) {
    return func(program, num_devices, device_list, options, num_input_headers, input_headers,
                header_include_names, pfn_notify, user_data);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_program
clLinkProgram(cl_context           context,
              cl_uint              num_devices,
              const cl_device_id * device_list,
              const char *         options,
              cl_uint              num_input_programs,
              const cl_program *   input_programs,
              void (*pfn_notify)(cl_program program, void * user_data),
              void *               user_data,
              cl_int *             errcode_ret)
{
  f_clLinkProgram func;

  func = (f_clLinkProgram) stub_dlsym("clLinkProgram");
  if(func) {
    return func(context, num_devices, device_list, options, num_input_programs,
                input_programs, pfn_notify, user_data, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}


cl_int
clUnloadPlatformCompiler(cl_platform_id platform)
{
  f_clUnloadPlatformCompiler func;

  func = (f_clUnloadPlatformCompiler) stub_dlsym("clUnloadPlatformCompiler");
  if(func) {
    return func(platform);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clGetProgramInfo(cl_program         program,
                 cl_program_info    param_name,
                 size_t             param_value_size,
                 void *             param_value,
                 size_t *           param_value_size_ret)
{
  f_clGetProgramInfo func;

  func = (f_clGetProgramInfo) stub_dlsym("clGetProgramInfo");
  if(func) {
    return func(program, param_name, param_value_size,
                param_value, param_value_size_ret);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clGetProgramBuildInfo(cl_program            program,
                      cl_device_id          device,
                      cl_program_build_info param_name,
                      size_t                param_value_size,
                      void *                param_value,
                      size_t *              param_value_size_ret)
{
  f_clGetProgramBuildInfo func;

  func = (f_clGetProgramBuildInfo) stub_dlsym("clGetProgramBuildInfo");
  if(func) {
    return func(program, device, param_name, param_value_size,
                param_value, param_value_size_ret);
  } else {
    return CL_INVALID_PLATFORM;
  }
}


cl_kernel
clCreateKernel(cl_program      program,
               const char *    kernel_name,
               cl_int *        errcode_ret)
{
  f_clCreateKernel func;

  func = (f_clCreateKernel) stub_dlsym("clCreateKernel");
  if(func) {
    return func(program, kernel_name, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}

cl_int
clCreateKernelsInProgram(cl_program     program,
                         cl_uint        num_kernels,
                         cl_kernel *    kernels,
                         cl_uint *      num_kernels_ret)
{
  f_clCreateKernelsInProgram func;

  func = (f_clCreateKernelsInProgram) stub_dlsym("clCreateKernelsInProgram");
  if(func) {
    return func(program, num_kernels, kernels, num_kernels_ret);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clRetainKernel(cl_kernel    kernel)
{
  f_clRetainKernel func;

  func = (f_clRetainKernel) stub_dlsym("clRetainKernel");
  if(func) {
    return func(kernel);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clReleaseKernel(cl_kernel   kernel)
{
  f_clReleaseKernel func;

  func = (f_clReleaseKernel) stub_dlsym("clReleaseKernel");
  if(func) {
    return func(kernel);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clSetKernelArg(cl_kernel    kernel,
               cl_uint      arg_index,
               size_t       arg_size,
               const void * arg_value)
{
  f_clSetKernelArg func;

  func = (f_clSetKernelArg) stub_dlsym("clSetKernelArg");
  if(func) {
    return func(kernel, arg_index, arg_size, arg_value);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clGetKernelInfo(cl_kernel       kernel,
                cl_kernel_info  param_name,
                size_t          param_value_size,
                void *          param_value,
                size_t *        param_value_size_ret)
{
  f_clGetKernelInfo func;

  func = (f_clGetKernelInfo) stub_dlsym("clGetKernelInfo");
  if(func) {
    return func(kernel, param_name, param_value_size, param_value, param_value_size_ret);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clGetKernelArgInfo(cl_kernel       kernel,
                   cl_uint         arg_indx,
                   cl_kernel_arg_info  param_name,
                   size_t          param_value_size,
                   void *          param_value,
                   size_t *        param_value_size_ret)
{
  f_clGetKernelArgInfo func;

  func = (f_clGetKernelArgInfo) stub_dlsym("clGetKernelArgInfo");
  if(func) {
    return func(kernel, arg_indx, param_name, param_value_size,
                param_value, param_value_size_ret);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clGetKernelWorkGroupInfo(cl_kernel                  kernel,
                         cl_device_id               device,
                         cl_kernel_work_group_info  param_name,
                         size_t                     param_value_size,
                         void *                     param_value,
                         size_t *                   param_value_size_ret)
{
  f_clGetKernelWorkGroupInfo func;

  func = (f_clGetKernelWorkGroupInfo) stub_dlsym("clGetKernelWorkGroupInfo");
  if(func) {
    return func(kernel, device, param_name, param_value_size, param_value, param_value_size_ret);
  } else {
    return CL_INVALID_PLATFORM;
  }
}


cl_int
clWaitForEvents(cl_uint             num_events,
                const cl_event *    event_list)
{
  f_clWaitForEvents func;

  func = (f_clWaitForEvents) stub_dlsym("clWaitForEvents");
  if(func) {
    return func(num_events, event_list);
  } else {
    return CL_INVALID_PLATFORM;
  }
}


cl_int
clGetEventInfo(cl_event         event,
               cl_event_info    param_name,
               size_t           param_value_size,
               void *           param_value,
               size_t *         param_value_size_ret)
{
  f_clGetEventInfo func;

  func = (f_clGetEventInfo) stub_dlsym("clGetEventInfo");
  if(func) {
    return func(event, param_name, param_value_size, param_value, param_value_size_ret);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_event
clCreateUserEvent(cl_context    context,
                  cl_int *      errcode_ret)
{
  f_clCreateUserEvent func;

  func = (f_clCreateUserEvent) stub_dlsym("clCreateUserEvent");
  if(func) {
    return func(context, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}

cl_int
clRetainEvent(cl_event event)
{
  f_clRetainEvent func;

  func = (f_clRetainEvent) stub_dlsym("clRetainEvent");
  if(func) {
    return func(event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clReleaseEvent(cl_event event)
{
  f_clReleaseEvent func;

  func = (f_clReleaseEvent) stub_dlsym("clReleaseEvent");
  if(func) {
    return func(event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clSetUserEventStatus(cl_event   event,
                     cl_int     execution_status)
{
  f_clSetUserEventStatus func;

  func = (f_clSetUserEventStatus) stub_dlsym("clSetUserEventStatus");
  if(func) {
    return func(event, execution_status);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clSetEventCallback( cl_event    event,
                    cl_int      command_exec_callback_type,
                    void (*pfn_notify)(cl_event, cl_int, void *),
                    void *      user_data)
{
  f_clSetEventCallback func;

  func = (f_clSetEventCallback) stub_dlsym("clSetEventCallback");
  if(func) {
    return func(event, command_exec_callback_type, pfn_notify, user_data);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clGetEventProfilingInfo(cl_event            event,
                        cl_profiling_info   param_name,
                        size_t              param_value_size,
                        void *              param_value,
                        size_t *            param_value_size_ret)
{
  f_clGetEventProfilingInfo func;

  func = (f_clGetEventProfilingInfo) stub_dlsym("clGetEventProfilingInfo");
  if(func) {
    return func(event, param_name, param_value_size, param_value, param_value_size_ret);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clFlush(cl_command_queue command_queue)
{
  f_clFlush func;

  func = (f_clFlush) stub_dlsym("clFlush");
  if(func) {
    return func(command_queue);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clFinish(cl_command_queue command_queue)
{
  f_clFinish func;

  func = (f_clFinish) stub_dlsym("clFinish");
  if(func) {
    return func(command_queue);
  } else {
    return CL_INVALID_PLATFORM;
  }
}


cl_int
clEnqueueReadBuffer(cl_command_queue    command_queue,
                    cl_mem              buffer,
                    cl_bool             blocking_read,
                    size_t              offset,
                    size_t              size,
                    void *              ptr,
                    cl_uint             num_events_in_wait_list,
                    const cl_event *    event_wait_list,
                    cl_event *          event)
{
  f_clEnqueueReadBuffer func;

  func = (f_clEnqueueReadBuffer) stub_dlsym("clEnqueueReadBuffer");
  if(func) {
    return func(command_queue, buffer, blocking_read, offset, size, ptr,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clEnqueueReadBufferRect(cl_command_queue    command_queue,
                        cl_mem              buffer,
                        cl_bool             blocking_read,
                        const size_t *      buffer_offset,
                        const size_t *      host_offset,
                        const size_t *      region,
                        size_t              buffer_row_pitch,
                        size_t              buffer_slice_pitch,
                        size_t              host_row_pitch,
                        size_t              host_slice_pitch,
                        void *              ptr,
                        cl_uint             num_events_in_wait_list,
                        const cl_event *    event_wait_list,
                        cl_event *          event)
{
  f_clEnqueueReadBufferRect func;

  func = (f_clEnqueueReadBufferRect) stub_dlsym("clEnqueueReadBufferRect");
  if(func) {
    return func(command_queue, buffer, blocking_read, buffer_offset, host_offset, region,
                buffer_row_pitch, buffer_slice_pitch, host_row_pitch, host_slice_pitch, ptr,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clEnqueueWriteBuffer(cl_command_queue   command_queue,
                     cl_mem             buffer,
                     cl_bool            blocking_write,
                     size_t             offset,
                     size_t             size,
                     const void *       ptr,
                     cl_uint            num_events_in_wait_list,
                     const cl_event *   event_wait_list,
                     cl_event *         event)
{
  f_clEnqueueWriteBuffer func;

  func = (f_clEnqueueWriteBuffer) stub_dlsym("clEnqueueWriteBuffer");
  if(func) {
    return func(command_queue, buffer, blocking_write, offset, size, ptr,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}


cl_int
clEnqueueWriteBufferRect(cl_command_queue    command_queue,
                         cl_mem              buffer,
                         cl_bool             blocking_write,
                         const size_t *      buffer_offset,
                         const size_t *      host_offset,
                         const size_t *      region,
                         size_t              buffer_row_pitch,
                         size_t              buffer_slice_pitch,
                         size_t              host_row_pitch,
                         size_t              host_slice_pitch,
                         const void *        ptr,
                         cl_uint             num_events_in_wait_list,
                         const cl_event *    event_wait_list,
                         cl_event *          event)
{
  f_clEnqueueWriteBufferRect func;

  func = (f_clEnqueueWriteBufferRect) stub_dlsym("clEnqueueWriteBufferRect");
  if(func) {
    return func(command_queue, buffer, blocking_write, buffer_offset, host_offset, region,
                buffer_row_pitch, buffer_slice_pitch, host_row_pitch, host_slice_pitch,
                ptr, num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}


cl_int
clEnqueueFillBuffer(cl_command_queue   command_queue,
                    cl_mem             buffer,
                    const void *       pattern,
                    size_t             pattern_size,
                    size_t             offset,
                    size_t             size,
                    cl_uint            num_events_in_wait_list,
                    const cl_event *   event_wait_list,
                    cl_event *         event)
{
  f_clEnqueueFillBuffer func;

  func = (f_clEnqueueFillBuffer) stub_dlsym("clEnqueueFillBuffer");
  if(func) {
    return func(command_queue, buffer, pattern, pattern_size, offset, size,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clEnqueueCopyBuffer(cl_command_queue    command_queue,
                    cl_mem              src_buffer,
                    cl_mem              dst_buffer,
                    size_t              src_offset,
                    size_t              dst_offset,
                    size_t              size,
                    cl_uint             num_events_in_wait_list,
                    const cl_event *    event_wait_list,
                    cl_event *          event)
{
  f_clEnqueueCopyBuffer func;

  func = (f_clEnqueueCopyBuffer) stub_dlsym("clEnqueueCopyBuffer");
  if(func) {
    return func(command_queue, src_buffer, dst_buffer, src_offset, dst_offset, size,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}



cl_int
clEnqueueCopyBufferRect(cl_command_queue    command_queue,
                        cl_mem              src_buffer,
                        cl_mem              dst_buffer,
                        const size_t *      src_origin,
                        const size_t *      dst_origin,
                        const size_t *      region,
                        size_t              src_row_pitch,
                        size_t              src_slice_pitch,
                        size_t              dst_row_pitch,
                        size_t              dst_slice_pitch,
                        cl_uint             num_events_in_wait_list,
                        const cl_event *    event_wait_list,
                        cl_event *          event)
{
  f_clEnqueueCopyBufferRect func;

  func = (f_clEnqueueCopyBufferRect) stub_dlsym("clEnqueueCopyBufferRect");
  if(func) {
    return func(command_queue, src_buffer, dst_buffer, src_origin, dst_origin, region, src_row_pitch,
                src_slice_pitch, dst_row_pitch, dst_slice_pitch, num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clEnqueueReadImage(cl_command_queue     command_queue,
                   cl_mem               image,
                   cl_bool              blocking_read,
                   const size_t *       origin,
                   const size_t *       region,
                   size_t               row_pitch,
                   size_t               slice_pitch,
                   void *               ptr,
                   cl_uint              num_events_in_wait_list,
                   const cl_event *     event_wait_list,
                   cl_event *           event)
{
  f_clEnqueueReadImage func;

  func = (f_clEnqueueReadImage) stub_dlsym("clEnqueueReadImage");
  if(func) {
    return func(command_queue, image, blocking_read, origin, region, row_pitch, slice_pitch,
                ptr, num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clEnqueueWriteImage(cl_command_queue    command_queue,
                    cl_mem              image,
                    cl_bool             blocking_write,
                    const size_t *      origin,
                    const size_t *      region,
                    size_t              input_row_pitch,
                    size_t              input_slice_pitch,
                    const void *        ptr,
                    cl_uint             num_events_in_wait_list,
                    const cl_event *    event_wait_list,
                    cl_event *          event)
{
  f_clEnqueueWriteImage func;

  func = (f_clEnqueueWriteImage) stub_dlsym("clEnqueueWriteImage");
  if(func) {
    return func(command_queue, image, blocking_write, origin, region, input_row_pitch, input_slice_pitch, ptr,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}


cl_int
clEnqueueFillImage(cl_command_queue   command_queue,
                   cl_mem             image,
                   const void *       fill_color,
                   const size_t *     origin,
                   const size_t *     region,
                   cl_uint            num_events_in_wait_list,
                   const cl_event *   event_wait_list,
                   cl_event *         event)
{
  f_clEnqueueFillImage func;

  func = (f_clEnqueueFillImage) stub_dlsym("clEnqueueFillImage");
  if(func) {
    return func(command_queue, image, fill_color, origin, region, num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clEnqueueCopyImage(cl_command_queue     command_queue,
                   cl_mem               src_image,
                   cl_mem               dst_image,
                   const size_t *       src_origin,
                   const size_t *       dst_origin,
                   const size_t *       region,
                   cl_uint              num_events_in_wait_list,
                   const cl_event *     event_wait_list,
                   cl_event *           event)
{
  f_clEnqueueCopyImage func;

  func = (f_clEnqueueCopyImage) stub_dlsym("clEnqueueCopyImage");
  if(func) {
    return func(command_queue, src_image, dst_image, src_origin, dst_origin, region,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clEnqueueCopyImageToBuffer(cl_command_queue command_queue,
                           cl_mem           src_image,
                           cl_mem           dst_buffer,
                           const size_t *   src_origin,
                           const size_t *   region,
                           size_t           dst_offset,
                           cl_uint          num_events_in_wait_list,
                           const cl_event * event_wait_list,
                           cl_event *       event)
{
  f_clEnqueueCopyImageToBuffer func;

  func = (f_clEnqueueCopyImageToBuffer) stub_dlsym("clEnqueueCopyImageToBuffer");
  if(func) {
    return func(command_queue, src_image, dst_buffer, src_origin, region, dst_offset,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}


cl_int
clEnqueueCopyBufferToImage(cl_command_queue command_queue,
                           cl_mem           src_buffer,
                           cl_mem           dst_image,
                           size_t           src_offset,
                           const size_t *   dst_origin,
                           const size_t *   region,
                           cl_uint          num_events_in_wait_list,
                           const cl_event * event_wait_list,
                           cl_event *       event)
{
  f_clEnqueueCopyBufferToImage func;

  func = (f_clEnqueueCopyBufferToImage) stub_dlsym("clEnqueueCopyBufferToImage");
  if(func) {
    return func(command_queue, src_buffer, dst_image, src_offset, dst_origin, region,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

void *
clEnqueueMapBuffer(cl_command_queue command_queue,
                   cl_mem           buffer,
                   cl_bool          blocking_map,
                   cl_map_flags     map_flags,
                   size_t           offset,
                   size_t           size,
                   cl_uint          num_events_in_wait_list,
                   const cl_event * event_wait_list,
                   cl_event *       event,
                   cl_int *         errcode_ret)
{
  f_clEnqueueMapBuffer func;

  func = (f_clEnqueueMapBuffer) stub_dlsym("clEnqueueMapBuffer");
  if(func) {
    return func(command_queue, buffer, blocking_map, map_flags, offset, size,
                num_events_in_wait_list, event_wait_list, event, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}

void *
clEnqueueMapImage(cl_command_queue  command_queue,
                  cl_mem            image,
                  cl_bool           blocking_map,
                  cl_map_flags      map_flags,
                  const size_t *    origin,
                  const size_t *    region,
                  size_t *          image_row_pitch,
                  size_t *          image_slice_pitch,
                  cl_uint           num_events_in_wait_list,
                  const cl_event *  event_wait_list,
                  cl_event *        event,
                  cl_int *          errcode_ret)
{
  f_clEnqueueMapImage func;

  func = (f_clEnqueueMapImage) stub_dlsym("clEnqueueMapImage");
  if(func) {
    return func(command_queue, image, blocking_map, map_flags, origin, region, image_row_pitch,
                image_slice_pitch, num_events_in_wait_list, event_wait_list, event, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}

cl_int
clEnqueueUnmapMemObject(cl_command_queue command_queue,
                        cl_mem           memobj,
                        void *           mapped_ptr,
                        cl_uint          num_events_in_wait_list,
                        const cl_event *  event_wait_list,
                        cl_event *        event)
{
  f_clEnqueueUnmapMemObject func;

  func = (f_clEnqueueUnmapMemObject) stub_dlsym("clEnqueueUnmapMemObject");
  if(func) {
    return func(command_queue, memobj, mapped_ptr, num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clEnqueueMigrateMemObjects(cl_command_queue       command_queue,
                           cl_uint                num_mem_objects,
                           const cl_mem *         mem_objects,
                           cl_mem_migration_flags flags,
                           cl_uint                num_events_in_wait_list,
                           const cl_event *       event_wait_list,
                           cl_event *             event)
{
  f_clEnqueueMigrateMemObjects func;

  func = (f_clEnqueueMigrateMemObjects) stub_dlsym("clEnqueueMigrateMemObjects");
  if(func) {
    return func(command_queue, num_mem_objects, mem_objects, flags, num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clEnqueueNDRangeKernel(cl_command_queue command_queue,
                       cl_kernel        kernel,
                       cl_uint          work_dim,
                       const size_t *   global_work_offset,
                       const size_t *   global_work_size,
                       const size_t *   local_work_size,
                       cl_uint          num_events_in_wait_list,
                       const cl_event * event_wait_list,
                       cl_event *       event)
{
  f_clEnqueueNDRangeKernel func;

  func = (f_clEnqueueNDRangeKernel) stub_dlsym("clEnqueueNDRangeKernel");
  if(func) {
    return func(command_queue, kernel, work_dim, global_work_offset, global_work_size, local_work_size,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clEnqueueTask(cl_command_queue  command_queue,
              cl_kernel         kernel,
              cl_uint           num_events_in_wait_list,
              const cl_event *  event_wait_list,
              cl_event *        event)
{
  f_clEnqueueTask func;

  func = (f_clEnqueueTask) stub_dlsym("clEnqueueTask");
  if(func) {
    return func(command_queue, kernel, num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clEnqueueNativeKernel(cl_command_queue  command_queue,
                      void (*user_func)(void *),
                      void *            args,
                      size_t            cb_args,
                      cl_uint           num_mem_objects,
                      const cl_mem *    mem_list,
                      const void **     args_mem_loc,
                      cl_uint           num_events_in_wait_list,
                      const cl_event *  event_wait_list,
                      cl_event *        event)
{
  f_clEnqueueNativeKernel func;

  func = (f_clEnqueueNativeKernel) stub_dlsym("clEnqueueNativeKernel");
  if(func) {
    return func(command_queue, user_func, args, cb_args, num_mem_objects, mem_list,
                args_mem_loc, num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clEnqueueMarkerWithWaitList(cl_command_queue command_queue,
                            cl_uint           num_events_in_wait_list,
                            const cl_event *  event_wait_list,
                            cl_event *        event)
{
  f_clEnqueueMarkerWithWaitList func;

  func = (f_clEnqueueMarkerWithWaitList) stub_dlsym("clEnqueueMarkerWithWaitList");
  if(func) {
    return func(command_queue, num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clEnqueueBarrierWithWaitList(cl_command_queue command_queue,
                             cl_uint           num_events_in_wait_list,
                             const cl_event *  event_wait_list,
                             cl_event *        event)
{
  f_clEnqueueBarrierWithWaitList func;

  func = (f_clEnqueueBarrierWithWaitList) stub_dlsym("clEnqueueBarrierWithWaitList");
  if(func) {
    return func(command_queue, num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

void *
clGetExtensionFunctionAddressForPlatform(cl_platform_id platform,
                                         const char *   func_name)
{
  f_clGetExtensionFunctionAddressForPlatform func;

  func = (f_clGetExtensionFunctionAddressForPlatform) stub_dlsym("clGetExtensionFunctionAddressForPlatform");
  if(func) {
    return func(platform, func_name);
  } else {
    return NULL;
  }
}


cl_mem
clCreateImage2D(cl_context              context,
                cl_mem_flags            flags,
                const cl_image_format * image_format,
                size_t                  image_width,
                size_t                  image_height,
                size_t                  image_row_pitch,
                void *                  host_ptr,
                cl_int *                errcode_ret)
{
  f_clCreateImage2D func;

  func = (f_clCreateImage2D) stub_dlsym("clCreateImage2D");
  if(func) {
    return func(context, flags, image_format, image_width, image_height,
                image_row_pitch, host_ptr, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}

cl_mem
clCreateImage3D(cl_context              context,
                cl_mem_flags            flags,
                const cl_image_format * image_format,
                size_t                  image_width,
                size_t                  image_height,
                size_t                  image_depth,
                size_t                  image_row_pitch,
                size_t                  image_slice_pitch,
                void *                  host_ptr,
                cl_int *                errcode_ret)
{
  f_clCreateImage3D func;

  func = (f_clCreateImage3D) stub_dlsym("clCreateImage3D");
  if(func) {
    return func(context, flags, image_format, image_width, image_height, image_depth,
                image_row_pitch, image_slice_pitch, host_ptr, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}

cl_int
clEnqueueMarker(cl_command_queue    command_queue,
                cl_event *          event)
{
  f_clEnqueueMarker func;

  func = (f_clEnqueueMarker) stub_dlsym("clEnqueueMarker");
  if(func) {
    return func(command_queue, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clEnqueueWaitForEvents(cl_command_queue command_queue,
                       cl_uint          num_events,
                       const cl_event * event_list)
{
  f_clEnqueueWaitForEvents func;

  func = (f_clEnqueueWaitForEvents) stub_dlsym("clEnqueueWaitForEvents");
  if(func) {
    return func(command_queue, num_events, event_list);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clEnqueueBarrier(cl_command_queue command_queue)
{
  f_clEnqueueBarrier func;

  func = (f_clEnqueueBarrier) stub_dlsym("clEnqueueBarrier");
  if(func) {
    return func(command_queue);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clUnloadCompiler(void)
{
  f_clUnloadCompiler func;

  func = (f_clUnloadCompiler) stub_dlsym("clUnloadCompiler");
  if(func) {
    return func();
  } else {
    return CL_INVALID_PLATFORM;
  }
}

void *
clGetExtensionFunctionAddress(const char * func_name)
{
  f_clGetExtensionFunctionAddress func;

  func = (f_clGetExtensionFunctionAddress) stub_dlsym("clGetExtensionFunctionAddress");
  if(func) {
    return func(func_name);
  } else {
    return NULL;
  }
}


cl_mem
clCreateFromGLBuffer(cl_context     context,
                     cl_mem_flags   flags,
                     cl_GLuint      bufobj,
                     int *          errcode_ret)
{
  f_clCreateFromGLBuffer func;

  func = (f_clCreateFromGLBuffer) stub_dlsym("clCreateFromGLBuffer");
  if(func) {
    return func(context, flags, bufobj, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}

cl_mem
clCreateFromGLTexture(cl_context      context,
                      cl_mem_flags    flags,
                      cl_GLenum       target,
                      cl_GLint        miplevel,
                      cl_GLuint       texture,
                      cl_int *        errcode_ret)
{
  f_clCreateFromGLTexture func;

  func = (f_clCreateFromGLTexture) stub_dlsym("clCreateFromGLTexture");
  if(func) {
    return func(context, flags, target, miplevel, texture, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}

cl_mem
clCreateFromGLRenderbuffer(cl_context   context,
                           cl_mem_flags flags,
                           cl_GLuint    renderbuffer,
                           cl_int *     errcode_ret)
{
  f_clCreateFromGLRenderbuffer func;

  func = (f_clCreateFromGLRenderbuffer) stub_dlsym("clCreateFromGLRenderbuffer");
  if(func) {
    return func(context, flags, renderbuffer, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}

cl_int
clGetGLObjectInfo(cl_mem                memobj,
                  cl_gl_object_type *   gl_object_type,
                  cl_GLuint *           gl_object_name)
{
  f_clGetGLObjectInfo func;

  func = (f_clGetGLObjectInfo) stub_dlsym("clGetGLObjectInfo");
  if(func) {
    return func(memobj, gl_object_type, gl_object_name);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clGetGLTextureInfo(cl_mem               memobj,
                   cl_gl_texture_info   param_name,
                   size_t               param_value_size,
                   void *               param_value,
                   size_t *             param_value_size_ret)
{
  f_clGetGLTextureInfo func;

  func = (f_clGetGLTextureInfo) stub_dlsym("clGetGLTextureInfo");
  if(func) {
    return func(memobj, param_name, param_value_size, param_value, param_value_size_ret);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clEnqueueAcquireGLObjects(cl_command_queue      command_queue,
                          cl_uint               num_objects,
                          const cl_mem *        mem_objects,
                          cl_uint               num_events_in_wait_list,
                          const cl_event *      event_wait_list,
                          cl_event *            event)
{
  f_clEnqueueAcquireGLObjects func;

  func = (f_clEnqueueAcquireGLObjects) stub_dlsym("clEnqueueAcquireGLObjects");
  if(func) {
    return func(command_queue, num_objects, mem_objects, num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clEnqueueReleaseGLObjects(cl_command_queue      command_queue,
                          cl_uint               num_objects,
                          const cl_mem *        mem_objects,
                          cl_uint               num_events_in_wait_list,
                          const cl_event *      event_wait_list,
                          cl_event *            event)
{
  f_clEnqueueReleaseGLObjects func;

  func = (f_clEnqueueReleaseGLObjects) stub_dlsym("clEnqueueReleaseGLObjects");
  if(func) {
    return func(command_queue, num_objects, mem_objects, num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}


cl_mem
clCreateFromGLTexture2D(cl_context      context,
                        cl_mem_flags    flags,
                        cl_GLenum       target,
                        cl_GLint        miplevel,
                        cl_GLuint       texture,
                        cl_int *        errcode_ret)
{
  f_clCreateFromGLTexture2D func;

  func = (f_clCreateFromGLTexture2D) stub_dlsym("clCreateFromGLTexture2D");
  if(func) {
    return func(context, flags, target, miplevel, texture, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}

cl_mem
clCreateFromGLTexture3D(cl_context      context,
                        cl_mem_flags    flags,
                        cl_GLenum       target,
                        cl_GLint        miplevel,
                        cl_GLuint       texture,
                        cl_int *        errcode_ret)
{
  f_clCreateFromGLTexture3D func;

  func = (f_clCreateFromGLTexture3D) stub_dlsym("clCreateFromGLTexture3D");
  if(func) {
    return func(context, flags, target, miplevel, texture, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}

cl_int
clGetGLContextInfoKHR(const cl_context_properties * properties,
                      cl_gl_context_info            param_name,
                      size_t                        param_value_size,
                      void *                        param_value,
                      size_t *                      param_value_size_ret)
{
  f_clGetGLContextInfoKHR func;

  func = (f_clGetGLContextInfoKHR) stub_dlsym("clGetGLContextInfoKHR");
  if(func) {
    return func(properties, param_name, param_value_size, param_value, param_value_size_ret);
  } else {
    return CL_INVALID_PLATFORM;
  }
}


/* ==========================================================================
 * OpenCL 2.0 API wrappers
 * ======================================================================== */
#ifdef CL_VERSION_2_0

cl_mem
clCreatePipe(cl_context                 context,
             cl_mem_flags               flags,
             cl_uint                    pipe_packet_size,
             cl_uint                    pipe_max_packets,
             const cl_pipe_properties * properties,
             cl_int *                   errcode_ret)
{
  f_clCreatePipe func;

  func = (f_clCreatePipe) stub_dlsym("clCreatePipe");
  if(func) {
    return func(context, flags, pipe_packet_size, pipe_max_packets, properties, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}

cl_int
clGetPipeInfo(cl_mem           pipe,
              cl_pipe_info     param_name,
              size_t           param_value_size,
              void *           param_value,
              size_t *         param_value_size_ret)
{
  f_clGetPipeInfo func;

  func = (f_clGetPipeInfo) stub_dlsym("clGetPipeInfo");
  if(func) {
    return func(pipe, param_name, param_value_size, param_value, param_value_size_ret);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

void *
clSVMAlloc(cl_context       context,
           cl_svm_mem_flags flags,
           size_t           size,
           cl_uint          alignment)
{
  f_clSVMAlloc func;

  func = (f_clSVMAlloc) stub_dlsym("clSVMAlloc");
  if(func) {
    return func(context, flags, size, alignment);
  } else {
    return NULL;
  }
}

void
clSVMFree(cl_context   context,
          void *       svm_pointer)
{
  f_clSVMFree func;

  func = (f_clSVMFree) stub_dlsym("clSVMFree");
  if(func) {
    func(context, svm_pointer);
  }
}

cl_int
clEnqueueSVMFree(cl_command_queue  command_queue,
                 cl_uint           num_svm_pointers,
                 void *            svm_pointers[],
                 void (CL_CALLBACK *pfn_free_func)(cl_command_queue, cl_uint, void *[], void *),
                 void *            user_data,
                 cl_uint           num_events_in_wait_list,
                 const cl_event *  event_wait_list,
                 cl_event *        event)
{
  f_clEnqueueSVMFree func;

  func = (f_clEnqueueSVMFree) stub_dlsym("clEnqueueSVMFree");
  if(func) {
    return func(command_queue, num_svm_pointers, svm_pointers, pfn_free_func, user_data,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clEnqueueSVMMemcpy(cl_command_queue  command_queue,
                   cl_bool           blocking_copy,
                   void *            dst_ptr,
                   const void *      src_ptr,
                   size_t            size,
                   cl_uint           num_events_in_wait_list,
                   const cl_event *  event_wait_list,
                   cl_event *        event)
{
  f_clEnqueueSVMMemcpy func;

  func = (f_clEnqueueSVMMemcpy) stub_dlsym("clEnqueueSVMMemcpy");
  if(func) {
    return func(command_queue, blocking_copy, dst_ptr, src_ptr, size,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clEnqueueSVMMemFill(cl_command_queue  command_queue,
                    void *            svm_ptr,
                    const void *      pattern,
                    size_t            pattern_size,
                    size_t            size,
                    cl_uint           num_events_in_wait_list,
                    const cl_event *  event_wait_list,
                    cl_event *        event)
{
  f_clEnqueueSVMMemFill func;

  func = (f_clEnqueueSVMMemFill) stub_dlsym("clEnqueueSVMMemFill");
  if(func) {
    return func(command_queue, svm_ptr, pattern, pattern_size, size,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clEnqueueSVMMap(cl_command_queue  command_queue,
                cl_bool           blocking_map,
                cl_map_flags      flags,
                void *            svm_ptr,
                size_t            size,
                cl_uint           num_events_in_wait_list,
                const cl_event *  event_wait_list,
                cl_event *        event)
{
  f_clEnqueueSVMMap func;

  func = (f_clEnqueueSVMMap) stub_dlsym("clEnqueueSVMMap");
  if(func) {
    return func(command_queue, blocking_map, flags, svm_ptr, size,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clEnqueueSVMUnmap(cl_command_queue  command_queue,
                  void *            svm_ptr,
                  cl_uint           num_events_in_wait_list,
                  const cl_event *  event_wait_list,
                  cl_event *        event)
{
  f_clEnqueueSVMUnmap func;

  func = (f_clEnqueueSVMUnmap) stub_dlsym("clEnqueueSVMUnmap");
  if(func) {
    return func(command_queue, svm_ptr, num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_sampler
clCreateSamplerWithProperties(cl_context                     context,
                              const cl_sampler_properties *  sampler_properties,
                              cl_int *                       errcode_ret)
{
  f_clCreateSamplerWithProperties func;

  func = (f_clCreateSamplerWithProperties) stub_dlsym("clCreateSamplerWithProperties");
  if(func) {
    return func(context, sampler_properties, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}

cl_int
clSetKernelArgSVMPointer(cl_kernel    kernel,
                         cl_uint      arg_index,
                         const void * arg_value)
{
  f_clSetKernelArgSVMPointer func;

  func = (f_clSetKernelArgSVMPointer) stub_dlsym("clSetKernelArgSVMPointer");
  if(func) {
    return func(kernel, arg_index, arg_value);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clSetKernelExecInfo(cl_kernel            kernel,
                    cl_kernel_exec_info  param_name,
                    size_t               param_value_size,
                    const void *         param_value)
{
  f_clSetKernelExecInfo func;

  func = (f_clSetKernelExecInfo) stub_dlsym("clSetKernelExecInfo");
  if(func) {
    return func(kernel, param_name, param_value_size, param_value);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

#endif /* CL_VERSION_2_0 */


/* ==========================================================================
 * OpenCL 2.1 API wrappers
 * ======================================================================== */
#ifdef CL_VERSION_2_1

cl_int
clSetDefaultDeviceCommandQueue(cl_context        context,
                               cl_device_id      device,
                               cl_command_queue  command_queue)
{
  f_clSetDefaultDeviceCommandQueue func;

  func = (f_clSetDefaultDeviceCommandQueue) stub_dlsym("clSetDefaultDeviceCommandQueue");
  if(func) {
    return func(context, device, command_queue);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clGetDeviceAndHostTimer(cl_device_id    device,
                        cl_ulong *      device_timestamp,
                        cl_ulong *      host_timestamp)
{
  f_clGetDeviceAndHostTimer func;

  func = (f_clGetDeviceAndHostTimer) stub_dlsym("clGetDeviceAndHostTimer");
  if(func) {
    return func(device, device_timestamp, host_timestamp);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clGetHostTimer(cl_device_id device,
               cl_ulong *   host_timestamp)
{
  f_clGetHostTimer func;

  func = (f_clGetHostTimer) stub_dlsym("clGetHostTimer");
  if(func) {
    return func(device, host_timestamp);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_program
clCreateProgramWithIL(cl_context    context,
                      const void *  il,
                      size_t        length,
                      cl_int *      errcode_ret)
{
  f_clCreateProgramWithIL func;

  func = (f_clCreateProgramWithIL) stub_dlsym("clCreateProgramWithIL");
  if(func) {
    return func(context, il, length, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}

cl_kernel
clCloneKernel(cl_kernel  source_kernel,
              cl_int *   errcode_ret)
{
  f_clCloneKernel func;

  func = (f_clCloneKernel) stub_dlsym("clCloneKernel");
  if(func) {
    return func(source_kernel, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}

cl_int
clGetKernelSubGroupInfo(cl_kernel                   kernel,
                        cl_device_id                device,
                        cl_kernel_sub_group_info    param_name,
                        size_t                      input_value_size,
                        const void *                input_value,
                        size_t                      param_value_size,
                        void *                      param_value,
                        size_t *                    param_value_size_ret)
{
  f_clGetKernelSubGroupInfo func;

  func = (f_clGetKernelSubGroupInfo) stub_dlsym("clGetKernelSubGroupInfo");
  if(func) {
    return func(kernel, device, param_name, input_value_size, input_value,
                param_value_size, param_value, param_value_size_ret);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clEnqueueSVMMigrateMem(cl_command_queue         command_queue,
                       cl_uint                  num_svm_pointers,
                       const void **            svm_pointers,
                       const size_t *           sizes,
                       cl_mem_migration_flags   flags,
                       cl_uint                  num_events_in_wait_list,
                       const cl_event *         event_wait_list,
                       cl_event *               event)
{
  f_clEnqueueSVMMigrateMem func;

  func = (f_clEnqueueSVMMigrateMem) stub_dlsym("clEnqueueSVMMigrateMem");
  if(func) {
    return func(command_queue, num_svm_pointers, svm_pointers, sizes, flags,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

#endif /* CL_VERSION_2_1 */


/* ==========================================================================
 * OpenCL 2.2 API wrappers
 * ======================================================================== */
#ifdef CL_VERSION_2_2

cl_int
clSetProgramReleaseCallback(cl_program          program,
                            void (CL_CALLBACK * pfn_notify)(cl_program, void *),
                            void *              user_data)
{
  f_clSetProgramReleaseCallback func;

  func = (f_clSetProgramReleaseCallback) stub_dlsym("clSetProgramReleaseCallback");
  if(func) {
    return func(program, pfn_notify, user_data);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

cl_int
clSetProgramSpecializationConstant(cl_program   program,
                                   cl_uint      spec_id,
                                   size_t       spec_size,
                                   const void * spec_value)
{
  f_clSetProgramSpecializationConstant func;

  func = (f_clSetProgramSpecializationConstant) stub_dlsym("clSetProgramSpecializationConstant");
  if(func) {
    return func(program, spec_id, spec_size, spec_value);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

#endif /* CL_VERSION_2_2 */


/* ==========================================================================
 * OpenCL 3.0 API wrappers
 * ======================================================================== */
#ifdef CL_VERSION_3_0

cl_mem
clCreateBufferWithProperties(cl_context                context,
                             const cl_mem_properties * properties,
                             cl_mem_flags              flags,
                             size_t                    size,
                             void *                    host_ptr,
                             cl_int *                  errcode_ret)
{
  f_clCreateBufferWithProperties func;

  func = (f_clCreateBufferWithProperties) stub_dlsym("clCreateBufferWithProperties");
  if(func) {
    return func(context, properties, flags, size, host_ptr, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}

cl_mem
clCreateImageWithProperties(cl_context                context,
                            const cl_mem_properties * properties,
                            cl_mem_flags              flags,
                            const cl_image_format *   image_format,
                            const cl_image_desc *     image_desc,
                            void *                    host_ptr,
                            cl_int *                  errcode_ret)
{
  f_clCreateImageWithProperties func;

  func = (f_clCreateImageWithProperties) stub_dlsym("clCreateImageWithProperties");
  if(func) {
    return func(context, properties, flags, image_format, image_desc, host_ptr, errcode_ret);
  } else {
    SET_ERR_RET_NULL();
  }
}

cl_int
clSetContextDestructorCallback(cl_context         context,
                               void (CL_CALLBACK *pfn_notify)(cl_context, void *),
                               void *             user_data)
{
  f_clSetContextDestructorCallback func;

  func = (f_clSetContextDestructorCallback) stub_dlsym("clSetContextDestructorCallback");
  if(func) {
    return func(context, pfn_notify, user_data);
  } else {
    return CL_INVALID_PLATFORM;
  }
}

#endif /* CL_VERSION_3_0 */
