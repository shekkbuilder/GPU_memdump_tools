/////////////////////////////////////////////////////////////////////////////////
//   Copyright (c) 2014 NVidia Corporation
//
//   Permission is hereby granted, free of charge, to any person obtaining a copy
//   of this software and associated documentation files (the "Software"), to
//   deal in the Software without restriction, including without limitation the
//   rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
//   sell copies of the Software, and to permit persons to whom the Software is
//   furnished to do so, subject to the following conditions:
//
//       The above copyright notice and this permission notice shall be
//       included in all copies or substantial portions of the Software.
//
//   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
//   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//   DEALINGS IN THE SOFTWARE.
//
/////////////////////////////////////////////////////////////////////////////////

//
//
// v1.0, first distribution on 9/18/2014.
// Corrected minor forensic validity issues, formatting, combined source files.
// Added check to make sure application is running with root privs, which is
// required for correct operation. Clarified some error conditions.  Better
// UUID error checking.
//
// All 9/14/2014 by Golden G. Richard III / @nolaforensix.
//
//

#include "dump_fb.h"
#include "uvm.h"
#include "uvmtypes.h"
#include "nvgetopt.h"
#include "common-utils.h"
#include <nvml.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>

char * RmErrorNumToString(RM_STATUS rmStatus) {
    switch (rmStatus) {
        case RM_ERROR:
            return "general RM error from UVM--incorrect GPU UUID?";

        case RM_ERR_INSUFFICIENT_PERMISSIONS:
            return "insufficient permissions";

        case RM_ERR_INVALID_ARGUMENT:
            return "invalid argument";

        case RM_ERR_INSUFFICIENT_RESOURCES:
            return "insufficient resources";

        case RM_ERR_MODULE_LOAD_FAILED:
            return "failed to load the kernel driver";

        case RM_ERR_OVERLAPPING_UVM_COMMIT:
            return "overlapping UVM commit range";

        case RM_ERR_UVM_ADDRESS_IN_USE:
            return "UVM address in use";

        case RM_ERR_NOT_SUPPORTED:
            return "not supported";

        case RM_ERR_BUSY_RETRY:
            return "busy retry; try again later";

        case RM_ERR_GPU_DMA_NOT_INITIALIZED:
            return "GPU DMA not initialized";

        case RM_ERR_INVALID_INDEX:
            return "invalid index";

        case RM_ERR_ECC_ERROR:
            return "ECC error";

        case RM_ERR_RC_ERROR:
            return "RC error";

        case RM_ERR_SIGNAL_PENDING:
            return "signal pending";

        case RM_ERR_NO_MEMORY:
            return "no memory";

        case RM_ERR_INVALID_ADDRESS:
            return "invalid address";

        case RM_ERR_INVALID_PATH:
            return "invalid path";

        default:
            return "general UVM failure";
    }

    // For compilers with weak "switch/case" foo:
    return "general UVM failure";
}


int getNumGpus() {
    unsigned int gpuCount = 0;
    nvmlReturn_t nvmlStatus = nvmlDeviceGetCount(&gpuCount);
    if (nvmlStatus != NVML_SUCCESS) {
        nv_error_msg("Could not get GPU count\n");
        return -1;
    }

    return gpuCount;
}

void nvmlUuidToUvmUuid(const char* nvmlUuid, UvmGpuUuid* uvmUuid) {
    sscanf(nvmlUuid, "GPU-%2x%2x%2x%2x-%2x%2x-%2x%2x-%2x%2x-%2x%2x%2x%2x%2x%2x", 
            (unsigned int*) &uvmUuid->uuid[0],
            (unsigned int*) &uvmUuid->uuid[1],
            (unsigned int*) &uvmUuid->uuid[2],
            (unsigned int*) &uvmUuid->uuid[3],
            (unsigned int*) &uvmUuid->uuid[4],
            (unsigned int*) &uvmUuid->uuid[5],
            (unsigned int*) &uvmUuid->uuid[6],
            (unsigned int*) &uvmUuid->uuid[7],
            (unsigned int*) &uvmUuid->uuid[8],
            (unsigned int*) &uvmUuid->uuid[9],
            (unsigned int*) &uvmUuid->uuid[10],
            (unsigned int*) &uvmUuid->uuid[11],
            (unsigned int*) &uvmUuid->uuid[12],
            (unsigned int*) &uvmUuid->uuid[13],
            (unsigned int*) &uvmUuid->uuid[14],
            (unsigned int*) &uvmUuid->uuid[15]);
}

const char* getRequestedUuid(const char* uuid) {
    unsigned int gpuCount = 0;
    unsigned int i;
    nvmlReturn_t nvmlStatus;
    char devuuid[NVML_DEVICE_UUID_BUFFER_SIZE];
    char *retuuid = NULL;

    gpuCount = getNumGpus();
    if (strlen(uuid) >= NVML_DEVICE_UUID_BUFFER_SIZE) {
        printf("Requested UUID is too long (Max UUID length %d)\n", 
                NVML_DEVICE_UUID_BUFFER_SIZE);
        return NULL;
    }

    for (i = 0; i < gpuCount; i++) {
        nvmlDevice_t device;
        nvmlStatus = nvmlDeviceGetHandleByIndex(i, &device);

        if (nvmlStatus != NVML_SUCCESS)  {
            printf("Could not retrieve device index %d\n", i);
            continue;
        }

        nvmlStatus = nvmlDeviceGetUUID(device, devuuid, sizeof(devuuid));
        if (nvmlStatus != NVML_SUCCESS) {
            printf("Could not retrieve UUID for device index %d\n", i);
            continue;
        }

        if (strncmp(uuid, &devuuid[4], strlen(uuid)) == 0) {
            if (retuuid) {
                printf("Ambiguous UUID fragment\n");
                free(retuuid);
                return NULL;
            }
            retuuid = strdup(devuuid);
        }
    }

    return retuuid;
}


NvLength getFbSize(const char*uuid) {
    nvmlDevice_t device;
    nvmlMemory_t memory;
    if (NVML_SUCCESS != nvmlDeviceGetHandleByUUID(uuid, &device)) {
        nv_error_msg("Couldn't get device by UUID %s\n", uuid);
        return 0;
    }

    if (NVML_SUCCESS != nvmlDeviceGetMemoryInfo(device, &memory)) {
        nv_error_msg("Could not query memory info for GPU\n");
        return 0;
    }

    return memory.total;
}

static const NVGetoptOption __options[] = {

    { "help",
      'h',
      NVGETOPT_HELP_ALWAYS,
      NULL,
      "Print usage information for the command line options and exit.\n" },

    { "gpu",
      'g',
      NVGETOPT_STRING_ARGUMENT | NVGETOPT_HELP_ALWAYS,
      "GPU-UUID",
      "The GPU UUID to extract data from.  To get the available UUIDs,\n"
      "use nvidia-smi -L (DO NOT include the \"GPU-\" prefix!).\n"
    },

    // Both offset and size are marked as string arguments because nvgetopt
    // only supports integers and we want to support values >32b.
    { "offset",
      'o',
      NVGETOPT_STRING_ARGUMENT | NVGETOPT_HELP_ALWAYS,
      "GPU-FB-OFFSET",
      "The physical offset inside the GPU's memory to extract.\n"
      "You can use nvidia-smi -q to get the size of memory for a given GPU.\n"
      "Note on some GPUs the size of memory does not correspond to the\n"
      "largest physical offset.  These GPUs are not supported by this\n"
      "utility.  The offset must be 0 or a multiple of 4096." 
    },

    { "size",
      's',
      NVGETOPT_STRING_ARGUMENT | NVGETOPT_HELP_ALWAYS,
      "SIZE-BYTES",
      "The number of bytes to extract. This must be a multiple of 4096.\n"
    },

    { "file",
      'f',
      NVGETOPT_STRING_ARGUMENT |NVGETOPT_HELP_ALWAYS,
      "OUTPUT-FILE",
      "The file to write to.  To maintain forensic integrity, the file\n"
      "must not currently exist.\n"
    },

    { NULL, 0, 0, NULL, NULL },
};

static void print_help_helper(const char *name, const char *description) {
    nv_info_msg(TAB, "    %s", name);
    nv_info_msg(BIGTAB, "%s", description);
    nv_info_msg(NULL, "");
}

static void print_help(void) {

    nv_info_msg(NULL, "");
    nv_info_msg(NULL, "%s [options]", PROGRAM_NAME);
    nv_info_msg(NULL, "");

    nvgetopt_print_help(__options, 0, print_help_helper);
}

int main(int argc, char *argv[]) {
    char              *file   = NULL;
    unsigned long long offset = 0;
    unsigned long long size   = 0;
    nvmlReturn_t nvmlStatus;
    const char * uuid = NULL;
    const long PAGE_SIZE = sysconf(_SC_PAGE_SIZE);

    UvmGpuUuid uvmUuid;
    RM_STATUS rmStatus = RM_OK;

    while (1) {
        int c, intval;
        char *strval  = NULL;

        c = nvgetopt(argc,
                     argv,
                     __options,
                     &strval, /* strval */
                     NULL, /* boolval */
                     &intval,
                     NULL, /* doubleval */
                     NULL); /* disable */

        if (c == -1) break; 

        switch (c)  {
            case 'h':
                print_help();
                goto cleanup;
                break;
            case 'g':
                uuid = strval;
                break;
            case 'o':
                offset = strtoull(strval, NULL, 0);
                if (offset % PAGE_SIZE)  {
                    nv_error_msg("Offset must be a multiple of the system page size (%ld bytes).\n",
                            PAGE_SIZE);
                    goto cleanup;
                }
                break;
            case 's':
                size = strtoull(strval, NULL, 0);
                if (size % PAGE_SIZE)  {
                    nv_error_msg("Size must be a multiple of the system page size (%ld bytes).\n",
                            PAGE_SIZE);
                    goto cleanup;
                }
                if (size == 0) {
                    nv_error_msg("A size greater than 0 needs to be specified.\n");
                    goto cleanup;
                }
                break;
            case 'f':
                file = strval;
                break;
            default:
                nv_error_msg("Invalid commandline, please run `%s --help` "
                             "for usage information.\n", argv[0]);
                goto cleanup;
        }
    }

    if (!uuid) {
        nv_error_msg("Must provide a UUID using -g.  Use nvidia-smi -L to see\n"
		     "a list of UUIDs.  Omit the \"GPU-\" portion for -g.\n");
	goto cleanup;
    }

    if (!file) {
        nv_error_msg("No output file specified.\n");
        goto cleanup;
    }

    if (getuid() != 0 && geteuid() != 0 ) {
      nv_error_msg("Must be run with root privileges.  Try sudo ./dump_fb <args> instead.\n");
      goto cleanup;
    }

    if ((nvmlStatus = nvmlInit()) != NVML_SUCCESS) {
        nv_error_msg("Cannot initialize NVML.\n");
        goto cleanup;
    }

    if ((rmStatus = UvmInitialize()) != RM_OK)  {
        nv_error_msg("Cannot initialize UVM.\n");
        nv_error_msg("UVM error: %s\n", RmErrorNumToString(rmStatus));
        goto cleanup;
    }

    uuid = getRequestedUuid(uuid);

    if (! uuid) {
        nv_error_msg("Bad GPU UUID. Use nvidia-smi -L to see\n"
		     "a list of UUIDs. Omit the \"GPU-\" portion for -g.\n");
        goto cleanup;
    }
      
    nvmlUuidToUvmUuid(uuid, &uvmUuid);

    NvLength fbLength = getFbSize(uuid);
    if (offset > fbLength || offset+size > fbLength)  {
        nv_error_msg("0x%llx-0x%llx exceeds the size of GPU memory (0x%llx).\n",
                offset, (offset+size), fbLength);
        goto cleanup;
    }

    if (! access(file, F_OK)) {
        nv_error_msg("Refusing to overwrite file that already exists.\n");
        goto cleanup;
    }

    int fd = open(file, O_CREAT | O_RDWR);

    if (fd < 0) {
        nv_error_msg("Failed to open output file.\n");
        perror(file);
        goto cleanup;
    }

    if (ftruncate(fd, size)) {
        nv_error_msg("Failed to size file\n");
        perror(file);
        goto cleanup;
    }

    void *ptr = mmap(NULL, size, 
            PROT_READ|PROT_WRITE, 
            MAP_SHARED, fd, 0);

    if (ptr == MAP_FAILED)  {
        nv_error_msg("Failed to mmap file: %d.\n", errno);
        perror(file);
        goto cleanup;
    }

    if ((rmStatus = UvmDumpGpuMemory(&uvmUuid, ptr, offset, size)) != RM_OK)  {
        nv_error_msg("UVM error: %s\n", RmErrorNumToString(rmStatus));
    }

    munmap(ptr, size);

cleanup:
    if (fd >= 0) {
        close(fd);
    }

    UvmDeinitialize();

    return rmStatus;
}

