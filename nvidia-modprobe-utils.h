/*
 * Copyright (c) 2013, NVIDIA CORPORATION.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * This file provides utility functions on Linux for loading the
 * NVIDIA kernel module and creating NVIDIA device files.
 */

#ifndef __NVIDIA_MODPROBE_UTILS_H__
#define __NVIDIA_MODPROBE_UTILS_H__

#include <stdio.h>

#define NV_MAX_CHARACTER_DEVICE_FILE_STRLEN  128
#define NV_MODULE_INSTANCE_NONE              -1
#define NV_MODULE_INSTANCE_ZERO              0
#define NV_MAX_MODULE_INSTANCES              8
#define NV_CTL_DEVICE_NUM                    255

#define NV_FRONTEND_CONTROL_DEVICE_MINOR_MAX NV_CTL_DEVICE_NUM

#define NV_DEVICE_FILE_PATH "/dev/nvidia%d"
#define NV_CTRL_DEVICE_FILE_PATH "/dev/nvidiactl"

#define NV_NMODULE_CTRL_DEVICE_FILE_PATH "/dev/nvidiactl%d"

#define NV_FRONTEND_CONTROL_DEVICE_MINOR_MIN \
    (NV_FRONTEND_CONTROL_DEVICE_MINOR_MAX - \
     NV_MAX_MODULE_INSTANCES)

#define NV_FRONTEND_IS_CONTROL_DEVICE(x) \
    ((x <= NV_FRONTEND_CONTROL_DEVICE_MINOR_MAX) && \
     (x > NV_FRONTEND_CONTROL_DEVICE_MINOR_MIN))

#if defined(NV_LINUX)

int nvidia_modprobe(const int print_errors, int module_instance);
int nvidia_mknod(int minor, int module_instance);
int nvidia_uvm_modprobe(const int print_errors);
int nvidia_uvm_mknod(int minor);

#endif /* NV_LINUX */


/*
 * Detect use of multiple kernel module instances. Use a single 
 * module instance unless instance != NV_MODULE_INSTANCE_NONE
 */
static __inline__ int is_multi_module(int module_instance)
{
    return (module_instance != NV_MODULE_INSTANCE_NONE);
}


/*
 * Construct the device file name, based on 'minor'.  If an error
 * occurs, the nul terminator will be written to name[0].
 */
static __inline__ void assign_device_file_name
(
    char name[NV_MAX_CHARACTER_DEVICE_FILE_STRLEN],
    int minor,
    int module_instance
)
{
    int ret;

    if ((minor < 0) || (minor > NV_CTL_DEVICE_NUM))
    {
        goto fail;
    }

    if (!is_multi_module(module_instance) && minor == NV_CTL_DEVICE_NUM)
    {
        ret = snprintf(name,
                       NV_MAX_CHARACTER_DEVICE_FILE_STRLEN,
                       NV_CTRL_DEVICE_FILE_PATH);
    }
    else if (is_multi_module(module_instance) && 
             NV_FRONTEND_IS_CONTROL_DEVICE(minor))
    {
        ret = snprintf(name,
                       NV_MAX_CHARACTER_DEVICE_FILE_STRLEN,
                       NV_NMODULE_CTRL_DEVICE_FILE_PATH,
                       module_instance);
    }
    else
    {
        ret = snprintf(name,
                       NV_MAX_CHARACTER_DEVICE_FILE_STRLEN,
                       NV_DEVICE_FILE_PATH, minor);
    }

    if (ret <= 0)
    {
        goto fail;
    }

    name[NV_MAX_CHARACTER_DEVICE_FILE_STRLEN - 1] = '\0';

    return;

fail:

    name[0] = '\0';
}

#endif /* __NVIDIA_MODPROBE_UTILS_H__ */