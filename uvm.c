/*******************************************************************************
    Copyright (c) 2013 NVidia Corporation

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to
    deal in the Software without restriction, including without limitation the
    rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
    sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

        The above copyright notice and this permission notice shall be
        included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*******************************************************************************/

#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "uvm.h"
#include "uvm_ioctl.h"
#include "uvm_linux_ioctl.h"
//#include "user_counters.h"
//#include "user_events.h"
#include "nvidia-modprobe-utils.h"
//#include "nvidia-modprobe-client-utils.h"

#define UVM_DEVICE_FILE "/dev/nvidia-uvm"

// Global control interface (file descriptor):
static int g_devUvmFd = -1;

static pthread_mutex_t g_uvmInitMutex = PTHREAD_MUTEX_INITIALIZER;

static inline size_t getCountersLength(void)
{
    // per process per gpu counters + per process all gpu counter
    return (UVM_MAX_GPUS + 1) * getpagesize();
}
RM_STATUS UvmErrnoToRmStatus(int errnoCode);



//
// UvmInitialize
//
RM_STATUS UvmInitialize(void)
{
    RM_STATUS status = RM_OK;

    pthread_mutex_lock(&g_uvmInitMutex);

    if (nvidia_uvm_modprobe(NV_TRUE) == 0)
    {
        status = RM_ERR_MODULE_LOAD_FAILED;
        goto done;
    }

    if (nvidia_uvm_mknod(0) == 0)
    {
        status = RM_ERR_MODULE_LOAD_FAILED;
        goto done;
    }

    if (-1 != g_devUvmFd)
        // Already initialized
        goto done;

    g_devUvmFd = open(UVM_DEVICE_FILE, O_RDWR);
    if (-1 == g_devUvmFd)
    {
        status = RM_ERR_MODULE_LOAD_FAILED;
        goto done;
    }

    if (-1 == ioctl(g_devUvmFd, UVM_INITIALIZE, 0))
    {
        close(g_devUvmFd);
        g_devUvmFd = -1;
        status = RM_ERR_MODULE_LOAD_FAILED;
    }

done:
    pthread_mutex_unlock(&g_uvmInitMutex);

    return status;
}

//
// UvmDeinitialize
//
RM_STATUS UvmDeinitialize(void)
{
    RM_STATUS status = RM_OK;

    pthread_mutex_lock(&g_uvmInitMutex);

    if (-1 == g_devUvmFd)
        // Already deinitialized
        goto done;

    if (-1 == ioctl(g_devUvmFd, UVM_DEINITIALIZE, 0))
    {
        status = RM_ERROR;
    }

    if (-1 == close(g_devUvmFd))
    {
        status = RM_ERROR;
    }

    g_devUvmFd = -1;

done:
    pthread_mutex_unlock(&g_uvmInitMutex);

    return status;
}

RM_STATUS UvmDumpGpuMemory(UvmGpuUuid *pGpuUuidStruct, 
                           void* pOutput, 
                           unsigned long long baseAddress, 
                           NvLength sizeBytes)
{
    UVM_DUMP_GPU_MEMORY_PARAMS params;
    memset(&params, 0, sizeof(params));
    memcpy(&(params.gpuUuid), (pGpuUuidStruct), sizeof(params.gpuUuid));
    params.baseAddress = baseAddress;
    params.sizeBytes   = sizeBytes;
    params.pOutput     = NV_PTR_TO_NvP64(pOutput);

    if (-1 == ioctl(g_devUvmFd, UVM_DUMP_GPU_MEMORY, &params))
    {
        UvmErrnoToRmStatus(errno);
    }

    return params.rmStatus;
}

RM_STATUS UvmErrnoToRmStatus(int errnoCode)
{
    if (errnoCode < 0)
        errnoCode = -errnoCode;

    switch (errnoCode)
    {
        case 0:
            return RM_OK;

        case EFAULT:
        case EINVAL:
            return RM_ERR_INVALID_ARGUMENT;

        case EEXIST:
        case EISDIR:
        case ENAMETOOLONG:
        case ENOENT:
        case ENOTDIR:
            return RM_ERR_INVALID_PATH;

        case ENODEV:
        case ENXIO:
            return RM_ERR_MODULE_LOAD_FAILED;

        case EPERM:
            return RM_ERR_INSUFFICIENT_PERMISSIONS;

        case ENOTTY:
        case EBADF:
        case EFBIG:
        case EMFILE:
        case ENFILE:
        case ENOSPC:
        case EOVERFLOW:
        case EROFS:
        case ETXTBSY:
        case EWOULDBLOCK:
        default:
            return RM_ERROR;
    };

    // For compilers with weak "switch/case" foo:
    return RM_ERROR;
}
