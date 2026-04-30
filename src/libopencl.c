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
#include "libopencl.h"
#if defined(__APPLE__) || defined(__MACOSX) || defined(__ANDROID__) || defined(__linux__) || defined(_POSIX_C_SOURCE)
#include <dlfcn.h>
#include <pthread.h>
#elif defined(_WIN32) || defined(WINVER)
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

static struct {
    long lasterror;
    const char *err_rutin;
} var = {
    0,
    NULL
};

void *dlopen (const char *filename, int flags){
    HINSTANCE hInst;

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
        snprintf (errstr, sizeof errstr, "%s error #%ld", var.err_rutin, var.lasterror);
        return errstr;
    } else {
        return NULL;
    }
}
#endif

#if defined(__APPLE__) || defined(__MACOSX)
static const char *default_so_paths[] = {
  "libOpenCL.dylib",
  "/System/Library/Frameworks/OpenCL.framework/OpenCL"
};
#elif defined(__ANDROID__)
static const char *default_so_paths[] = {
  "libOpenCL.so",
  "/system/vendor/lib64/libOpenCL.so",
  "/system/lib64/libOpenCL.so",
  "/system/vendor/lib/libOpenCL.so",
  "/system/lib/libOpenCL.so"
};
#elif defined(_WIN32) || defined(WINVER)
static const char *default_so_paths[] = {
  "OpenCL.dll"
};
#elif defined(__linux__)
static const char *default_so_paths[] = {
  "libOpenCL.so",
  "libOpenCL.so.1",
  "/usr/lib/libOpenCL.so",
  "/usr/lib64/libOpenCL.so",
  "/usr/lib32/libOpenCL.so",
  "/usr/local/lib/libOpenCL.so"
};
#endif

static void *so_handle = NULL;

#if defined(_WIN32) || defined(WINVER)
static CRITICAL_SECTION g_stub_lock;
static INIT_ONCE g_stub_lock_once = INIT_ONCE_STATIC_INIT;
static BOOL CALLBACK stub_lock_init(PINIT_ONCE o, PVOID p, PVOID *c) {
  (void)o; (void)p; (void)c;
  InitializeCriticalSection(&g_stub_lock);
  return TRUE;
}
static void stub_lock(void)   { InitOnceExecuteOnce(&g_stub_lock_once, stub_lock_init, NULL, NULL); EnterCriticalSection(&g_stub_lock); }
static void stub_unlock(void) { LeaveCriticalSection(&g_stub_lock); }
#else
static pthread_mutex_t g_stub_lock = PTHREAD_MUTEX_INITIALIZER;
static void stub_lock(void)   { pthread_mutex_lock(&g_stub_lock); }
static void stub_unlock(void) { pthread_mutex_unlock(&g_stub_lock); }
#endif

/* Try to dlopen `path`. On success, sets so_handle and returns 1. On failure
 * (NULL path, file missing, wrong arch, unreadable, etc.) drains dlerror and
 * returns 0. Caller must hold g_stub_lock. */
static int try_dlopen(const char *path, int dl_flags)
{
  if(!path)
    return 0;
  so_handle = dlopen(path, dl_flags);
  if(so_handle)
    return 1;
#if !(defined(_WIN32) || defined(WINVER))
  (void)dlerror();
#endif
  return 0;
}

/* Caller must hold g_stub_lock. Returns 0 on success, -1 on failure. */
static int open_libopencl_so_locked(void)
{
  size_t i;
  int dl_flags;

  if(so_handle)
    return 0;

#if defined(_WIN32) || defined(WINVER)
  dl_flags = 0;
#else
  dl_flags = RTLD_LAZY | RTLD_LOCAL;
#endif

  if(try_dlopen(getenv("LIBOPENCL_SO_PATH"),   dl_flags)) return 0;
  if(try_dlopen(getenv("LIBOPENCL_SO_PATH_2"), dl_flags)) return 0;
  if(try_dlopen(getenv("LIBOPENCL_SO_PATH_3"), dl_flags)) return 0;
  if(try_dlopen(getenv("LIBOPENCL_SO_PATH_4"), dl_flags)) return 0;

  for(i = 0; i < (sizeof(default_so_paths) / sizeof(char*)); i++) {
    if(try_dlopen(default_so_paths[i], dl_flags))
      return 0;
  }

  return -1;
}

/* Resolve symbol; returns NULL if loader cannot be opened. Thread-safe. */
static void *stub_resolve(const char *name)
{
  void *sym;
  stub_lock();
  if(!so_handle)
    (void)open_libopencl_so_locked();
  sym = so_handle ? dlsym(so_handle, name) : NULL;
  stub_unlock();
  return sym;
}

void stubOpenclReset(void)
{
  stub_lock();
  if(so_handle)
    dlclose(so_handle);
  so_handle = NULL;
  stub_unlock();
}

cl_int
clGetPlatformIDs(cl_uint          num_entries,
                 cl_platform_id * platforms,
                 cl_uint *        num_platforms)
{
  static f_clGetPlatformIDs func = NULL;
  if (!func) func = (f_clGetPlatformIDs) stub_resolve("clGetPlatformIDs");
  if(func) {
    return func(num_entries, platforms, num_platforms);
  } else {
    return CL_INVALID_OPERATION;
  }
}


cl_int
clGetPlatformInfo(cl_platform_id   platform,
                  cl_platform_info param_name,
                  size_t           param_value_size,
                  void *           param_value,
                  size_t *         param_value_size_ret)
{
  static f_clGetPlatformInfo func = NULL;
  if (!func) func = (f_clGetPlatformInfo) stub_resolve("clGetPlatformInfo");
  if(func) {
    return func(platform, param_name, param_value_size, param_value, param_value_size_ret);
  } else {
    return CL_INVALID_OPERATION;
  }
}


cl_int
clGetDeviceIDs(cl_platform_id   platform,
               cl_device_type   device_type,
               cl_uint          num_entries,
               cl_device_id *   devices,
               cl_uint *        num_devices)
{
  static f_clGetDeviceIDs func = NULL;
  if (!func) func = (f_clGetDeviceIDs) stub_resolve("clGetDeviceIDs");
  if(func) {
    return func(platform, device_type, num_entries, devices, num_devices);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clGetDeviceInfo(cl_device_id    device,
                cl_device_info  param_name,
                size_t          param_value_size,
                void *          param_value,
                size_t *        param_value_size_ret)
{
  static f_clGetDeviceInfo func = NULL;
  if (!func) func = (f_clGetDeviceInfo) stub_resolve("clGetDeviceInfo");
  if(func) {
    return func(device, param_name, param_value_size, param_value, param_value_size_ret);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clCreateSubDevices(cl_device_id                         in_device,
                   const cl_device_partition_property * properties,
                   cl_uint                              num_devices,
                   cl_device_id *                       out_devices,
                   cl_uint *                            num_devices_ret)
{
  static f_clCreateSubDevices func = NULL;
  if (!func) func = (f_clCreateSubDevices) stub_resolve("clCreateSubDevices");
  if(func) {
    return func(in_device, properties, num_devices, out_devices, num_devices_ret);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clRetainDevice(cl_device_id device)
{
  static f_clRetainDevice func = NULL;
  if (!func) func = (f_clRetainDevice) stub_resolve("clRetainDevice");
  if(func) {
    return func(device);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clReleaseDevice(cl_device_id device)
{
  static f_clReleaseDevice func = NULL;
  if (!func) func = (f_clReleaseDevice) stub_resolve("clReleaseDevice");
  if(func) {
    return func(device);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clCreateContext func = NULL;
  if (!func) func = (f_clCreateContext) stub_resolve("clCreateContext");
  if(func) {
    return func(properties, num_devices, devices, pfn_notify, user_data, errcode_ret);
  } else {
    if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
    return NULL;
  }
}

cl_context
clCreateContextFromType(const cl_context_properties * properties,
                        cl_device_type          device_type,
                        void (*pfn_notify )(const char *, const void *, size_t, void *),
                        void *                  user_data,
                        cl_int *                errcode_ret)
{
  static f_clCreateContextFromType func = NULL;
  if (!func) func = (f_clCreateContextFromType) stub_resolve("clCreateContextFromType");
  if(func) {
    return func(properties, device_type, pfn_notify, user_data, errcode_ret);
  } else {
    if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
    return NULL;
  }
}

cl_int
clRetainContext(cl_context context)
{
  static f_clRetainContext func = NULL;
  if (!func) func = (f_clRetainContext) stub_resolve("clRetainContext");
  if(func) {
    return func(context);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clReleaseContext(cl_context context)
{
  static f_clReleaseContext func = NULL;
  if (!func) func = (f_clReleaseContext) stub_resolve("clReleaseContext");
  if(func) {
    return func(context);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clGetContextInfo(cl_context         context,
                 cl_context_info    param_name,
                 size_t             param_value_size,
                 void *             param_value,
                 size_t *           param_value_size_ret)
{
  static f_clGetContextInfo func = NULL;
  if (!func) func = (f_clGetContextInfo) stub_resolve("clGetContextInfo");
  if(func) {
    return func(context, param_name, param_value_size,
                param_value, param_value_size_ret);
  } else {
    return CL_INVALID_OPERATION;
  }
}


cl_command_queue
clCreateCommandQueue(cl_context                     context,
                     cl_device_id                   device,
                     cl_command_queue_properties    properties,
                     cl_int *                       errcode_ret)
{
  static f_clCreateCommandQueue func = NULL;
  if (!func) func = (f_clCreateCommandQueue) stub_resolve("clCreateCommandQueue");
  if(func) {
    return func(context, device, properties, errcode_ret);
  } else {
    if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
    return NULL;
  }
}

#ifdef CL_VERSION_2_0
cl_command_queue
clCreateCommandQueueWithProperties(cl_context                     context,
                                   cl_device_id                   device,
                     	             const cl_queue_properties *    properties,
                                   cl_int *                       errcode_ret)
{
  static f_clCreateCommandQueueWithProperties func = NULL;
  if (!func) func = (f_clCreateCommandQueueWithProperties) stub_resolve("clCreateCommandQueueWithProperties");
  if(func) {
    return func(context, device, properties, errcode_ret);
  } else {
    if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
    return NULL;
  }
}
#endif

cl_int
clRetainCommandQueue(cl_command_queue command_queue)
{
  static f_clRetainCommandQueue func = NULL;
  if (!func) func = (f_clRetainCommandQueue) stub_resolve("clRetainCommandQueue");
  if(func) {
    return func(command_queue);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clReleaseCommandQueue(cl_command_queue command_queue)
{
  static f_clReleaseCommandQueue func = NULL;
  if (!func) func = (f_clReleaseCommandQueue) stub_resolve("clReleaseCommandQueue");
  if(func) {
    return func(command_queue);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clGetCommandQueueInfo(cl_command_queue      command_queue,
                      cl_command_queue_info param_name,
                      size_t                param_value_size,
                      void *                param_value,
                      size_t *              param_value_size_ret)
{
  static f_clGetCommandQueueInfo func = NULL;
  if (!func) func = (f_clGetCommandQueueInfo) stub_resolve("clGetCommandQueueInfo");
  if(func) {
    return func(command_queue, param_name, param_value_size,
                param_value, param_value_size_ret);
  } else {
    return CL_INVALID_OPERATION;
  }
}


cl_mem
clCreateBuffer(cl_context   context,
               cl_mem_flags flags,
               size_t       size,
               void *       host_ptr,
               cl_int *     errcode_ret)
{
  static f_clCreateBuffer func = NULL;
  if (!func) func = (f_clCreateBuffer) stub_resolve("clCreateBuffer");
  if(func) {
    return func(context, flags, size, host_ptr, errcode_ret);
  } else {
    if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
    return NULL;
  }
}

cl_mem
clCreateSubBuffer(cl_mem                   buffer,
                  cl_mem_flags             flags,
                  cl_buffer_create_type    buffer_create_type,
                  const void *             buffer_create_info,
                  cl_int *                 errcode_ret)
{
  static f_clCreateSubBuffer func = NULL;
  if (!func) func = (f_clCreateSubBuffer) stub_resolve("clCreateSubBuffer");
  if(func) {
    return func(buffer, flags, buffer_create_type,
                buffer_create_info, errcode_ret);
  } else {
    if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
    return NULL;
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
  static f_clCreateImage func = NULL;
  if (!func) func = (f_clCreateImage) stub_resolve("clCreateImage");
  if(func) {
    return func(context, flags, image_format, image_desc,
                host_ptr, errcode_ret);
  } else {
    if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
    return NULL;
  }
}

cl_int
clRetainMemObject(cl_mem memobj)
{
  static f_clRetainMemObject func = NULL;
  if (!func) func = (f_clRetainMemObject) stub_resolve("clRetainMemObject");
  if(func) {
    return func(memobj);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clReleaseMemObject(cl_mem memobj)
{
  static f_clReleaseMemObject func = NULL;
  if (!func) func = (f_clReleaseMemObject) stub_resolve("clReleaseMemObject");
  if(func) {
    return func(memobj);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clGetSupportedImageFormats func = NULL;
  if (!func) func = (f_clGetSupportedImageFormats) stub_resolve("clGetSupportedImageFormats");
  if(func) {
    return func(context, flags, image_type, num_entries,
                image_formats, num_image_formats);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clGetMemObjectInfo(cl_mem           memobj,
                   cl_mem_info      param_name,
                   size_t           param_value_size,
                   void *           param_value,
                   size_t *         param_value_size_ret)
{
  static f_clGetMemObjectInfo func = NULL;
  if (!func) func = (f_clGetMemObjectInfo) stub_resolve("clGetMemObjectInfo");
  if(func) {
    return func(memobj, param_name, param_value_size,
                param_value, param_value_size_ret);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clGetImageInfo(cl_mem           image,
               cl_image_info    param_name,
               size_t           param_value_size,
               void *           param_value,
               size_t *         param_value_size_ret)
{
  static f_clGetImageInfo func = NULL;
  if (!func) func = (f_clGetImageInfo) stub_resolve("clGetImageInfo");
  if(func) {
    return func(image, param_name, param_value_size,
                param_value, param_value_size_ret);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clSetMemObjectDestructorCallback(  cl_mem memobj,
                                   void (*pfn_notify)( cl_mem memobj, void* user_data),
                                   void * user_data )
{
  static f_clSetMemObjectDestructorCallback func = NULL;
  if (!func) func = (f_clSetMemObjectDestructorCallback) stub_resolve("clSetMemObjectDestructorCallback");
  if(func) {
    return func(memobj, pfn_notify, user_data);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_sampler
clCreateSampler(cl_context          context,
                cl_bool             normalized_coords,
                cl_addressing_mode  addressing_mode,
                cl_filter_mode      filter_mode,
                cl_int *            errcode_ret)
{
  static f_clCreateSampler func = NULL;
  if (!func) func = (f_clCreateSampler) stub_resolve("clCreateSampler");
  if(func) {
    return func(context, normalized_coords, addressing_mode, filter_mode, errcode_ret);
  } else {
    if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
    return NULL;
  }
}

cl_int
clRetainSampler(cl_sampler sampler)
{
  static f_clRetainSampler func = NULL;
  if (!func) func = (f_clRetainSampler) stub_resolve("clRetainSampler");
  if(func) {
    return func(sampler);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clReleaseSampler(cl_sampler sampler)
{
  static f_clReleaseSampler func = NULL;
  if (!func) func = (f_clReleaseSampler) stub_resolve("clReleaseSampler");
  if(func) {
    return func(sampler);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clGetSamplerInfo(cl_sampler         sampler,
                 cl_sampler_info    param_name,
                 size_t             param_value_size,
                 void *             param_value,
                 size_t *           param_value_size_ret)
{
  static f_clGetSamplerInfo func = NULL;
  if (!func) func = (f_clGetSamplerInfo) stub_resolve("clGetSamplerInfo");
  if(func) {
    return func(sampler, param_name, param_value_size, param_value, param_value_size_ret);
  } else {
    return CL_INVALID_OPERATION;
  }
}


cl_program
clCreateProgramWithSource(cl_context        context,
                          cl_uint           count,
                          const char **     strings,
                          const size_t *    lengths,
                          cl_int *          errcode_ret)
{
  static f_clCreateProgramWithSource func = NULL;
  if (!func) func = (f_clCreateProgramWithSource) stub_resolve("clCreateProgramWithSource");
  if(func) {
    return func(context, count, strings, lengths, errcode_ret);
  } else {
    if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
    return NULL;
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
  static f_clCreateProgramWithBinary func = NULL;
  if (!func) func = (f_clCreateProgramWithBinary) stub_resolve("clCreateProgramWithBinary");
  if(func) {
    return func(context, num_devices, device_list, lengths, binaries, binary_status, errcode_ret);
  } else {
    if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
    return NULL;
  }
}

cl_program
clCreateProgramWithBuiltInKernels(cl_context            context,
                                  cl_uint               num_devices,
                                  const cl_device_id *  device_list,
                                  const char *          kernel_names,
                                  cl_int *              errcode_ret)
{
  static f_clCreateProgramWithBuiltInKernels func = NULL;
  if (!func) func = (f_clCreateProgramWithBuiltInKernels) stub_resolve("clCreateProgramWithBuiltInKernels");
  if(func) {
    return func(context, num_devices, device_list, kernel_names, errcode_ret);
  } else {
    if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
    return NULL;
  }
}

cl_int
clRetainProgram(cl_program program)
{
  static f_clRetainProgram func = NULL;
  if (!func) func = (f_clRetainProgram) stub_resolve("clRetainProgram");
  if(func) {
    return func(program);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clReleaseProgram(cl_program program)
{
  static f_clReleaseProgram func = NULL;
  if (!func) func = (f_clReleaseProgram) stub_resolve("clReleaseProgram");
  if(func) {
    return func(program);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clBuildProgram func = NULL;
  if (!func) func = (f_clBuildProgram) stub_resolve("clBuildProgram");
  if(func) {
    return func(program, num_devices, device_list, options, pfn_notify, user_data);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clCompileProgram func = NULL;
  if (!func) func = (f_clCompileProgram) stub_resolve("clCompileProgram");
  if(func) {
    return func(program, num_devices, device_list, options, num_input_headers, input_headers,
                header_include_names, pfn_notify, user_data);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clLinkProgram func = NULL;
  if (!func) func = (f_clLinkProgram) stub_resolve("clLinkProgram");
  if(func) {
    return func(context, num_devices, device_list, options, num_input_programs,
                input_programs, pfn_notify, user_data, errcode_ret);
  } else {
    if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
    return NULL;
  }
}


cl_int
clUnloadPlatformCompiler(cl_platform_id platform)
{
  static f_clUnloadPlatformCompiler func = NULL;
  if (!func) func = (f_clUnloadPlatformCompiler) stub_resolve("clUnloadPlatformCompiler");
  if(func) {
    return func(platform);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clGetProgramInfo(cl_program         program,
                 cl_program_info    param_name,
                 size_t             param_value_size,
                 void *             param_value,
                 size_t *           param_value_size_ret)
{
  static f_clGetProgramInfo func = NULL;
  if (!func) func = (f_clGetProgramInfo) stub_resolve("clGetProgramInfo");
  if(func) {
    return func(program, param_name, param_value_size,
                param_value, param_value_size_ret);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clGetProgramBuildInfo func = NULL;
  if (!func) func = (f_clGetProgramBuildInfo) stub_resolve("clGetProgramBuildInfo");
  if(func) {
    return func(program, device, param_name, param_value_size,
                param_value, param_value_size_ret);
  } else {
    return CL_INVALID_OPERATION;
  }
}


cl_kernel
clCreateKernel(cl_program      program,
               const char *    kernel_name,
               cl_int *        errcode_ret)
{
  static f_clCreateKernel func = NULL;
  if (!func) func = (f_clCreateKernel) stub_resolve("clCreateKernel");
  if(func) {
    return func(program, kernel_name, errcode_ret);
  } else {
    if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
    return NULL;
  }
}

cl_int
clCreateKernelsInProgram(cl_program     program,
                         cl_uint        num_kernels,
                         cl_kernel *    kernels,
                         cl_uint *      num_kernels_ret)
{
  static f_clCreateKernelsInProgram func = NULL;
  if (!func) func = (f_clCreateKernelsInProgram) stub_resolve("clCreateKernelsInProgram");
  if(func) {
    return func(program, num_kernels, kernels, num_kernels_ret);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clRetainKernel(cl_kernel    kernel)
{
  static f_clRetainKernel func = NULL;
  if (!func) func = (f_clRetainKernel) stub_resolve("clRetainKernel");
  if(func) {
    return func(kernel);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clReleaseKernel(cl_kernel   kernel)
{
  static f_clReleaseKernel func = NULL;
  if (!func) func = (f_clReleaseKernel) stub_resolve("clReleaseKernel");
  if(func) {
    return func(kernel);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clSetKernelArg(cl_kernel    kernel,
               cl_uint      arg_index,
               size_t       arg_size,
               const void * arg_value)
{
  static f_clSetKernelArg func = NULL;
  if (!func) func = (f_clSetKernelArg) stub_resolve("clSetKernelArg");
  if(func) {
    return func(kernel, arg_index, arg_size, arg_value);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clGetKernelInfo(cl_kernel       kernel,
                cl_kernel_info  param_name,
                size_t          param_value_size,
                void *          param_value,
                size_t *        param_value_size_ret)
{
  static f_clGetKernelInfo func = NULL;
  if (!func) func = (f_clGetKernelInfo) stub_resolve("clGetKernelInfo");
  if(func) {
    return func(kernel, param_name, param_value_size, param_value, param_value_size_ret);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clGetKernelArgInfo func = NULL;
  if (!func) func = (f_clGetKernelArgInfo) stub_resolve("clGetKernelArgInfo");
  if(func) {
    return func(kernel, arg_indx, param_name, param_value_size,
                param_value, param_value_size_ret);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clGetKernelWorkGroupInfo func = NULL;
  if (!func) func = (f_clGetKernelWorkGroupInfo) stub_resolve("clGetKernelWorkGroupInfo");
  if(func) {
    return func(kernel, device, param_name, param_value_size, param_value, param_value_size_ret);
  } else {
    return CL_INVALID_OPERATION;
  }
}


cl_int
clWaitForEvents(cl_uint             num_events,
                const cl_event *    event_list)
{
  static f_clWaitForEvents func = NULL;
  if (!func) func = (f_clWaitForEvents) stub_resolve("clWaitForEvents");
  if(func) {
    return func(num_events, event_list);
  } else {
    return CL_INVALID_OPERATION;
  }
}


cl_int
clGetEventInfo(cl_event         event,
               cl_event_info    param_name,
               size_t           param_value_size,
               void *           param_value,
               size_t *         param_value_size_ret)
{
  static f_clGetEventInfo func = NULL;
  if (!func) func = (f_clGetEventInfo) stub_resolve("clGetEventInfo");
  if(func) {
    return func(event, param_name, param_value_size, param_value, param_value_size_ret);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_event
clCreateUserEvent(cl_context    context,
                  cl_int *      errcode_ret)
{
  static f_clCreateUserEvent func = NULL;
  if (!func) func = (f_clCreateUserEvent) stub_resolve("clCreateUserEvent");
  if(func) {
    return func(context, errcode_ret);
  } else {
    if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
    return NULL;
  }
}

cl_int
clRetainEvent(cl_event event)
{
  static f_clRetainEvent func = NULL;
  if (!func) func = (f_clRetainEvent) stub_resolve("clRetainEvent");
  if(func) {
    return func(event);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clReleaseEvent(cl_event event)
{
  static f_clReleaseEvent func = NULL;
  if (!func) func = (f_clReleaseEvent) stub_resolve("clReleaseEvent");
  if(func) {
    return func(event);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clSetUserEventStatus(cl_event   event,
                     cl_int     execution_status)
{
  static f_clSetUserEventStatus func = NULL;
  if (!func) func = (f_clSetUserEventStatus) stub_resolve("clSetUserEventStatus");
  if(func) {
    return func(event, execution_status);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clSetEventCallback( cl_event    event,
                    cl_int      command_exec_callback_type,
                    void (*pfn_notify)(cl_event, cl_int, void *),
                    void *      user_data)
{
  static f_clSetEventCallback func = NULL;
  if (!func) func = (f_clSetEventCallback) stub_resolve("clSetEventCallback");
  if(func) {
    return func(event, command_exec_callback_type, pfn_notify, user_data);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clGetEventProfilingInfo(cl_event            event,
                        cl_profiling_info   param_name,
                        size_t              param_value_size,
                        void *              param_value,
                        size_t *            param_value_size_ret)
{
  static f_clGetEventProfilingInfo func = NULL;
  if (!func) func = (f_clGetEventProfilingInfo) stub_resolve("clGetEventProfilingInfo");
  if(func) {
    return func(event, param_name, param_value_size, param_value, param_value_size_ret);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clFlush(cl_command_queue command_queue)
{
  static f_clFlush func = NULL;
  if (!func) func = (f_clFlush) stub_resolve("clFlush");
  if(func) {
    return func(command_queue);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clFinish(cl_command_queue command_queue)
{
  static f_clFinish func = NULL;
  if (!func) func = (f_clFinish) stub_resolve("clFinish");
  if(func) {
    return func(command_queue);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clEnqueueReadBuffer func = NULL;
  if (!func) func = (f_clEnqueueReadBuffer) stub_resolve("clEnqueueReadBuffer");
  if(func) {
    return func(command_queue, buffer, blocking_read, offset, size, ptr,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clEnqueueReadBufferRect func = NULL;
  if (!func) func = (f_clEnqueueReadBufferRect) stub_resolve("clEnqueueReadBufferRect");
  if(func) {
    return func(command_queue, buffer, blocking_read, buffer_offset, host_offset, region,
                buffer_row_pitch, buffer_slice_pitch, host_row_pitch, host_slice_pitch, ptr,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clEnqueueWriteBuffer func = NULL;
  if (!func) func = (f_clEnqueueWriteBuffer) stub_resolve("clEnqueueWriteBuffer");
  if(func) {
    return func(command_queue, buffer, blocking_write, offset, size, ptr,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clEnqueueWriteBufferRect func = NULL;
  if (!func) func = (f_clEnqueueWriteBufferRect) stub_resolve("clEnqueueWriteBufferRect");
  if(func) {
    return func(command_queue, buffer, blocking_write, buffer_offset, host_offset, region,
                buffer_row_pitch, buffer_slice_pitch, host_row_pitch, host_slice_pitch,
                ptr, num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clEnqueueFillBuffer func = NULL;
  if (!func) func = (f_clEnqueueFillBuffer) stub_resolve("clEnqueueFillBuffer");
  if(func) {
    return func(command_queue, buffer, pattern, pattern_size, offset, size,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clEnqueueCopyBuffer func = NULL;
  if (!func) func = (f_clEnqueueCopyBuffer) stub_resolve("clEnqueueCopyBuffer");
  if(func) {
    return func(command_queue, src_buffer, dst_buffer, src_offset, dst_offset, size,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clEnqueueCopyBufferRect func = NULL;
  if (!func) func = (f_clEnqueueCopyBufferRect) stub_resolve("clEnqueueCopyBufferRect");
  if(func) {
    return func(command_queue, src_buffer, dst_buffer, src_origin, dst_origin, region, src_row_pitch,
                src_slice_pitch, dst_row_pitch, dst_slice_pitch, num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clEnqueueReadImage func = NULL;
  if (!func) func = (f_clEnqueueReadImage) stub_resolve("clEnqueueReadImage");
  if(func) {
    return func(command_queue, image, blocking_read, origin, region, row_pitch, slice_pitch,
                ptr, num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clEnqueueWriteImage func = NULL;
  if (!func) func = (f_clEnqueueWriteImage) stub_resolve("clEnqueueWriteImage");
  if(func) {
    return func(command_queue, image, blocking_write, origin, region, input_row_pitch, input_slice_pitch, ptr,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clEnqueueFillImage func = NULL;
  if (!func) func = (f_clEnqueueFillImage) stub_resolve("clEnqueueFillImage");
  if(func) {
    return func(command_queue, image, fill_color, origin, region, num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clEnqueueCopyImage func = NULL;
  if (!func) func = (f_clEnqueueCopyImage) stub_resolve("clEnqueueCopyImage");
  if(func) {
    return func(command_queue, src_image, dst_image, src_origin, dst_origin, region,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clEnqueueCopyImageToBuffer func = NULL;
  if (!func) func = (f_clEnqueueCopyImageToBuffer) stub_resolve("clEnqueueCopyImageToBuffer");
  if(func) {
    return func(command_queue, src_image, dst_buffer, src_origin, region, dst_offset,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clEnqueueCopyBufferToImage func = NULL;
  if (!func) func = (f_clEnqueueCopyBufferToImage) stub_resolve("clEnqueueCopyBufferToImage");
  if(func) {
    return func(command_queue, src_buffer, dst_image, src_offset, dst_origin, region,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clEnqueueMapBuffer func = NULL;
  if (!func) func = (f_clEnqueueMapBuffer) stub_resolve("clEnqueueMapBuffer");
  if(func) {
    return func(command_queue, buffer, blocking_map, map_flags, offset, size,
                num_events_in_wait_list, event_wait_list, event, errcode_ret);
  } else {
    if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
    return NULL;
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
  static f_clEnqueueMapImage func = NULL;
  if (!func) func = (f_clEnqueueMapImage) stub_resolve("clEnqueueMapImage");
  if(func) {
    return func(command_queue, image, blocking_map, map_flags, origin, region, image_row_pitch,
                image_slice_pitch, num_events_in_wait_list, event_wait_list, event, errcode_ret);
  } else {
    if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
    return NULL;
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
  static f_clEnqueueUnmapMemObject func = NULL;
  if (!func) func = (f_clEnqueueUnmapMemObject) stub_resolve("clEnqueueUnmapMemObject");
  if(func) {
    return func(command_queue, memobj, mapped_ptr, num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clEnqueueMigrateMemObjects func = NULL;
  if (!func) func = (f_clEnqueueMigrateMemObjects) stub_resolve("clEnqueueMigrateMemObjects");
  if(func) {
    return func(command_queue, num_mem_objects, mem_objects, flags, num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clEnqueueNDRangeKernel func = NULL;
  if (!func) func = (f_clEnqueueNDRangeKernel) stub_resolve("clEnqueueNDRangeKernel");
  if(func) {
    return func(command_queue, kernel, work_dim, global_work_offset, global_work_size, local_work_size,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clEnqueueTask(cl_command_queue  command_queue,
              cl_kernel         kernel,
              cl_uint           num_events_in_wait_list,
              const cl_event *  event_wait_list,
              cl_event *        event)
{
  static f_clEnqueueTask func = NULL;
  if (!func) func = (f_clEnqueueTask) stub_resolve("clEnqueueTask");
  if(func) {
    return func(command_queue, kernel, num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clEnqueueNativeKernel func = NULL;
  if (!func) func = (f_clEnqueueNativeKernel) stub_resolve("clEnqueueNativeKernel");
  if(func) {
    return func(command_queue, user_func, args, cb_args, num_mem_objects, mem_list,
                args_mem_loc, num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clEnqueueMarkerWithWaitList(cl_command_queue command_queue,
                            cl_uint           num_events_in_wait_list,
                            const cl_event *  event_wait_list,
                            cl_event *        event)
{
  static f_clEnqueueMarkerWithWaitList func = NULL;
  if (!func) func = (f_clEnqueueMarkerWithWaitList) stub_resolve("clEnqueueMarkerWithWaitList");
  if(func) {
    return func(command_queue, num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clEnqueueBarrierWithWaitList(cl_command_queue command_queue,
                             cl_uint           num_events_in_wait_list,
                             const cl_event *  event_wait_list,
                             cl_event *        event)
{
  static f_clEnqueueBarrierWithWaitList func = NULL;
  if (!func) func = (f_clEnqueueBarrierWithWaitList) stub_resolve("clEnqueueBarrierWithWaitList");
  if(func) {
    return func(command_queue, num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_OPERATION;
  }
}

void *
clGetExtensionFunctionAddressForPlatform(cl_platform_id platform,
                                         const char *   func_name)
{
  static f_clGetExtensionFunctionAddressForPlatform func = NULL;
  if (!func) func = (f_clGetExtensionFunctionAddressForPlatform) stub_resolve("clGetExtensionFunctionAddressForPlatform");
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
  static f_clCreateImage2D func = NULL;
  if (!func) func = (f_clCreateImage2D) stub_resolve("clCreateImage2D");
  if(func) {
    return func(context, flags, image_format, image_width, image_height,
                image_row_pitch, host_ptr, errcode_ret);
  } else {
    if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
    return NULL;
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
  static f_clCreateImage3D func = NULL;
  if (!func) func = (f_clCreateImage3D) stub_resolve("clCreateImage3D");
  if(func) {
    return func(context, flags, image_format, image_width, image_height, image_depth,
                image_row_pitch, image_slice_pitch, host_ptr, errcode_ret);
  } else {
    if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
    return NULL;
  }
}

cl_int
clEnqueueMarker(cl_command_queue    command_queue,
                cl_event *          event)
{
  static f_clEnqueueMarker func = NULL;
  if (!func) func = (f_clEnqueueMarker) stub_resolve("clEnqueueMarker");
  if(func) {
    return func(command_queue, event);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clEnqueueWaitForEvents(cl_command_queue command_queue,
                       cl_uint          num_events,
                       const cl_event * event_list)
{
  static f_clEnqueueWaitForEvents func = NULL;
  if (!func) func = (f_clEnqueueWaitForEvents) stub_resolve("clEnqueueWaitForEvents");
  if(func) {
    return func(command_queue, num_events, event_list);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clEnqueueBarrier(cl_command_queue command_queue)
{
  static f_clEnqueueBarrier func = NULL;
  if (!func) func = (f_clEnqueueBarrier) stub_resolve("clEnqueueBarrier");
  if(func) {
    return func(command_queue);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clUnloadCompiler(void)
{
  static f_clUnloadCompiler func = NULL;
  if (!func) func = (f_clUnloadCompiler) stub_resolve("clUnloadCompiler");
  if(func) {
    return func();
  } else {
    return CL_INVALID_OPERATION;
  }
}

void *
clGetExtensionFunctionAddress(const char * func_name)
{
  static f_clGetExtensionFunctionAddress func = NULL;
  if (!func) func = (f_clGetExtensionFunctionAddress) stub_resolve("clGetExtensionFunctionAddress");
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
  static f_clCreateFromGLBuffer func = NULL;
  if (!func) func = (f_clCreateFromGLBuffer) stub_resolve("clCreateFromGLBuffer");
  if(func) {
    return func(context, flags, bufobj, errcode_ret);
  } else {
    return NULL;
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
  static f_clCreateFromGLTexture func = NULL;
  if (!func) func = (f_clCreateFromGLTexture) stub_resolve("clCreateFromGLTexture");
  if(func) {
    return func(context, flags, target, miplevel, texture, errcode_ret);
  } else {
    if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
    return NULL;
  }
}

cl_mem
clCreateFromGLRenderbuffer(cl_context   context,
                           cl_mem_flags flags,
                           cl_GLuint    renderbuffer,
                           cl_int *     errcode_ret)
{
  static f_clCreateFromGLRenderbuffer func = NULL;
  if (!func) func = (f_clCreateFromGLRenderbuffer) stub_resolve("clCreateFromGLRenderbuffer");
  if(func) {
    return func(context, flags, renderbuffer, errcode_ret);
  } else {
    if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
    return NULL;
  }
}

cl_int
clGetGLObjectInfo(cl_mem                memobj,
                  cl_gl_object_type *   gl_object_type,
                  cl_GLuint *           gl_object_name)
{
  static f_clGetGLObjectInfo func = NULL;
  if (!func) func = (f_clGetGLObjectInfo) stub_resolve("clGetGLObjectInfo");
  if(func) {
    return func(memobj, gl_object_type, gl_object_name);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clGetGLTextureInfo(cl_mem               memobj,
                   cl_gl_texture_info   param_name,
                   size_t               param_value_size,
                   void *               param_value,
                   size_t *             param_value_size_ret)
{
  static f_clGetGLTextureInfo func = NULL;
  if (!func) func = (f_clGetGLTextureInfo) stub_resolve("clGetGLTextureInfo");
  if(func) {
    return func(memobj, param_name, param_value_size, param_value, param_value_size_ret);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clEnqueueAcquireGLObjects func = NULL;
  if (!func) func = (f_clEnqueueAcquireGLObjects) stub_resolve("clEnqueueAcquireGLObjects");
  if(func) {
    return func(command_queue, num_objects, mem_objects, num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clEnqueueReleaseGLObjects func = NULL;
  if (!func) func = (f_clEnqueueReleaseGLObjects) stub_resolve("clEnqueueReleaseGLObjects");
  if(func) {
    return func(command_queue, num_objects, mem_objects, num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clCreateFromGLTexture2D func = NULL;
  if (!func) func = (f_clCreateFromGLTexture2D) stub_resolve("clCreateFromGLTexture2D");
  if(func) {
    return func(context, flags, target, miplevel, texture, errcode_ret);
  } else {
    if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
    return NULL;
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
  static f_clCreateFromGLTexture3D func = NULL;
  if (!func) func = (f_clCreateFromGLTexture3D) stub_resolve("clCreateFromGLTexture3D");
  if(func) {
    return func(context, flags, target, miplevel, texture, errcode_ret);
  } else {
    if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
    return NULL;
  }
}

cl_int
clGetGLContextInfoKHR(const cl_context_properties * properties,
                      cl_gl_context_info            param_name,
                      size_t                        param_value_size,
                      void *                        param_value,
                      size_t *                      param_value_size_ret)
{
  static f_clGetGLContextInfoKHR func = NULL;
  if (!func) func = (f_clGetGLContextInfoKHR) stub_resolve("clGetGLContextInfoKHR");
  if(func) {
    return func(properties, param_name, param_value_size, param_value, param_value_size_ret);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clCreatePipe func = NULL;
  if (!func) func = (f_clCreatePipe) stub_resolve("clCreatePipe");
  if(func) {
    return func(context, flags, pipe_packet_size, pipe_max_packets, properties, errcode_ret);
  } else {
    if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
    return NULL;
  }
}

cl_int
clGetPipeInfo(cl_mem           pipe,
              cl_pipe_info     param_name,
              size_t           param_value_size,
              void *           param_value,
              size_t *         param_value_size_ret)
{
  static f_clGetPipeInfo func = NULL;
  if (!func) func = (f_clGetPipeInfo) stub_resolve("clGetPipeInfo");
  if(func) {
    return func(pipe, param_name, param_value_size, param_value, param_value_size_ret);
  } else {
    return CL_INVALID_OPERATION;
  }
}

void *
clSVMAlloc(cl_context       context,
           cl_svm_mem_flags flags,
           size_t           size,
           cl_uint          alignment)
{
  static f_clSVMAlloc func = NULL;
  if (!func) func = (f_clSVMAlloc) stub_resolve("clSVMAlloc");
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
  static f_clSVMFree func = NULL;
  if (!func) func = (f_clSVMFree) stub_resolve("clSVMFree");
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
  static f_clEnqueueSVMFree func = NULL;
  if (!func) func = (f_clEnqueueSVMFree) stub_resolve("clEnqueueSVMFree");
  if(func) {
    return func(command_queue, num_svm_pointers, svm_pointers, pfn_free_func, user_data,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clEnqueueSVMMemcpy func = NULL;
  if (!func) func = (f_clEnqueueSVMMemcpy) stub_resolve("clEnqueueSVMMemcpy");
  if(func) {
    return func(command_queue, blocking_copy, dst_ptr, src_ptr, size,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clEnqueueSVMMemFill func = NULL;
  if (!func) func = (f_clEnqueueSVMMemFill) stub_resolve("clEnqueueSVMMemFill");
  if(func) {
    return func(command_queue, svm_ptr, pattern, pattern_size, size,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clEnqueueSVMMap func = NULL;
  if (!func) func = (f_clEnqueueSVMMap) stub_resolve("clEnqueueSVMMap");
  if(func) {
    return func(command_queue, blocking_map, flags, svm_ptr, size,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clEnqueueSVMUnmap(cl_command_queue  command_queue,
                  void *            svm_ptr,
                  cl_uint           num_events_in_wait_list,
                  const cl_event *  event_wait_list,
                  cl_event *        event)
{
  static f_clEnqueueSVMUnmap func = NULL;
  if (!func) func = (f_clEnqueueSVMUnmap) stub_resolve("clEnqueueSVMUnmap");
  if(func) {
    return func(command_queue, svm_ptr, num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_sampler
clCreateSamplerWithProperties(cl_context                     context,
                              const cl_sampler_properties *  sampler_properties,
                              cl_int *                       errcode_ret)
{
  static f_clCreateSamplerWithProperties func = NULL;
  if (!func) func = (f_clCreateSamplerWithProperties) stub_resolve("clCreateSamplerWithProperties");
  if(func) {
    return func(context, sampler_properties, errcode_ret);
  } else {
    if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
    return NULL;
  }
}

cl_int
clSetKernelArgSVMPointer(cl_kernel    kernel,
                         cl_uint      arg_index,
                         const void * arg_value)
{
  static f_clSetKernelArgSVMPointer func = NULL;
  if (!func) func = (f_clSetKernelArgSVMPointer) stub_resolve("clSetKernelArgSVMPointer");
  if(func) {
    return func(kernel, arg_index, arg_value);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clSetKernelExecInfo(cl_kernel            kernel,
                    cl_kernel_exec_info  param_name,
                    size_t               param_value_size,
                    const void *         param_value)
{
  static f_clSetKernelExecInfo func = NULL;
  if (!func) func = (f_clSetKernelExecInfo) stub_resolve("clSetKernelExecInfo");
  if(func) {
    return func(kernel, param_name, param_value_size, param_value);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clSetDefaultDeviceCommandQueue func = NULL;
  if (!func) func = (f_clSetDefaultDeviceCommandQueue) stub_resolve("clSetDefaultDeviceCommandQueue");
  if(func) {
    return func(context, device, command_queue);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clGetDeviceAndHostTimer(cl_device_id    device,
                        cl_ulong *      device_timestamp,
                        cl_ulong *      host_timestamp)
{
  static f_clGetDeviceAndHostTimer func = NULL;
  if (!func) func = (f_clGetDeviceAndHostTimer) stub_resolve("clGetDeviceAndHostTimer");
  if(func) {
    return func(device, device_timestamp, host_timestamp);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clGetHostTimer(cl_device_id device,
               cl_ulong *   host_timestamp)
{
  static f_clGetHostTimer func = NULL;
  if (!func) func = (f_clGetHostTimer) stub_resolve("clGetHostTimer");
  if(func) {
    return func(device, host_timestamp);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_program
clCreateProgramWithIL(cl_context    context,
                      const void *  il,
                      size_t        length,
                      cl_int *      errcode_ret)
{
  static f_clCreateProgramWithIL func = NULL;
  if (!func) func = (f_clCreateProgramWithIL) stub_resolve("clCreateProgramWithIL");
  if(func) {
    return func(context, il, length, errcode_ret);
  } else {
    if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
    return NULL;
  }
}

cl_kernel
clCloneKernel(cl_kernel  source_kernel,
              cl_int *   errcode_ret)
{
  static f_clCloneKernel func = NULL;
  if (!func) func = (f_clCloneKernel) stub_resolve("clCloneKernel");
  if(func) {
    return func(source_kernel, errcode_ret);
  } else {
    if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
    return NULL;
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
  static f_clGetKernelSubGroupInfo func = NULL;
  if (!func) func = (f_clGetKernelSubGroupInfo) stub_resolve("clGetKernelSubGroupInfo");
  if(func) {
    return func(kernel, device, param_name, input_value_size, input_value,
                param_value_size, param_value, param_value_size_ret);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clEnqueueSVMMigrateMem func = NULL;
  if (!func) func = (f_clEnqueueSVMMigrateMem) stub_resolve("clEnqueueSVMMigrateMem");
  if(func) {
    return func(command_queue, num_svm_pointers, svm_pointers, sizes, flags,
                num_events_in_wait_list, event_wait_list, event);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clSetProgramReleaseCallback func = NULL;
  if (!func) func = (f_clSetProgramReleaseCallback) stub_resolve("clSetProgramReleaseCallback");
  if(func) {
    return func(program, pfn_notify, user_data);
  } else {
    return CL_INVALID_OPERATION;
  }
}

cl_int
clSetProgramSpecializationConstant(cl_program   program,
                                   cl_uint      spec_id,
                                   size_t       spec_size,
                                   const void * spec_value)
{
  static f_clSetProgramSpecializationConstant func = NULL;
  if (!func) func = (f_clSetProgramSpecializationConstant) stub_resolve("clSetProgramSpecializationConstant");
  if(func) {
    return func(program, spec_id, spec_size, spec_value);
  } else {
    return CL_INVALID_OPERATION;
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
  static f_clCreateBufferWithProperties func = NULL;
  if (!func) func = (f_clCreateBufferWithProperties) stub_resolve("clCreateBufferWithProperties");
  if(func) {
    return func(context, properties, flags, size, host_ptr, errcode_ret);
  } else {
    if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
    return NULL;
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
  static f_clCreateImageWithProperties func = NULL;
  if (!func) func = (f_clCreateImageWithProperties) stub_resolve("clCreateImageWithProperties");
  if(func) {
    return func(context, properties, flags, image_format, image_desc, host_ptr, errcode_ret);
  } else {
    if (errcode_ret) *errcode_ret = CL_INVALID_OPERATION;
    return NULL;
  }
}

cl_int
clSetContextDestructorCallback(cl_context         context,
                               void (CL_CALLBACK *pfn_notify)(cl_context, void *),
                               void *             user_data)
{
  static f_clSetContextDestructorCallback func = NULL;
  if (!func) func = (f_clSetContextDestructorCallback) stub_resolve("clSetContextDestructorCallback");
  if(func) {
    return func(context, pfn_notify, user_data);
  } else {
    return CL_INVALID_OPERATION;
  }
}

#endif /* CL_VERSION_3_0 */
