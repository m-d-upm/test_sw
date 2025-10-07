/*
 * CMAlib - Contiguous Memory Allocator user space library
 *
 * Copyright (C) 2025 Milos Dordevic, CEI-UPM.
 * 
 */

#include "cma.h"

#include "accel_dyn_cma.h"

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h> // mmap()
#include <glob.h>
#include <stdbool.h>
#include <pthread.h>

#include <stdio.h>

#define GLOB_PATTERN                    ("sys/devices/platform/soc/*.strela/misc/strela*")
#define DELIMITER                       ("/")
#define MAX_NUM_OF_SUPP_BUFF_MAPPINGS   (256)
#define DEV_MAPPING_MATCH_NOT_FOUND     (-1)
#define DEV_MAPPING_MATCH_FOUND         (-2)

struct dev_mapping
{
    char dev_name[16];
    char sys_name[16];
};

struct dev_mappings
{
    bool initialized;
    struct dev_mapping map[MAX_NUM_OF_SUPP_BUFF_MAPPINGS];
};

struct buf_ptr_id_mapping
{
    void *ptr;
    uint32_t size;
};

static struct dev_mappings dev_name_map = { 0 }; 
static struct buf_ptr_id_mapping buf_ptr_id_map[MAX_NUM_OF_SUPP_BUFF_MAPPINGS]; // ID is equal to index
static uint32_t buf_id_cnt = 0;
static pthread_mutex_t lib_mutex;

static int init_dev_mappings(void)
{
    glob_t glob_res = { 0 };
    int index = 0;

    if(glob(GLOB_PATTERN, GLOB_ONLYDIR, NULL, &glob_res) == 0)
    {
        for (char **dir = glob_res.gl_pathv; *dir != NULL; dir++)
        {
            struct stat path_stat;
            stat(*dir, &path_stat);
            
            if(S_ISDIR(path_stat.st_mode)) 
            {      
                char* token = strtok(*dir, DELIMITER); // sys
                token = strtok(NULL, DELIMITER);       // devices
                token = strtok(NULL, DELIMITER);       // platform
                token = strtok(NULL, DELIMITER);       // soc

                token = strtok(NULL, DELIMITER);       // *.strela
                strncpy(dev_name_map.map[index].sys_name, token, sizeof(dev_name_map.map[index].sys_name));

                token = strtok(NULL, DELIMITER);       // misc

                token = strtok(NULL, DELIMITER);       // strela*
                strncpy(dev_name_map.map[index].dev_name, token, sizeof(dev_name_map.map[index].dev_name));

                ++index;

                if(index >= MAX_NUM_OF_SUPP_BUFF_MAPPINGS)
                    break;
            }
        }
        
        globfree(&glob_res);
        dev_name_map.initialized = true;

        return true;
    }
    else 
    {
        return false;
    }
}

// ptr to the buffer, size of the buffer in bytes, device to which to bind the buffer (name as in /dev e.g. /dev/strela0)
void *cma_alloc(uint32_t size, const char* dev_name)
{
    char buf_dev_name[32]; // udmabuf
    void *buff_ptr = NULL;

    struct accel_dyn_cma_alloc_req_ioctl_arg buf_args_input = {
        .size = size,
        .id = DEV_MAPPING_MATCH_NOT_FOUND,
    };

    pthread_mutex_lock(&lib_mutex);

    if(!dev_name_map.initialized)
    {
        if(!init_dev_mappings())
            return NULL; 
    }
    
    int file_desc_alloc = open(ACCEL_DYN_CMA_DEV_NAME, O_RDWR);

    if (file_desc_alloc < 0) 
        return NULL;
    
    memset(buf_args_input.dev_name, 0, sizeof(buf_args_input.dev_name));

    int num_chars = strlen(dev_name) > sizeof(buf_args_input.dev_name) ? sizeof(buf_args_input.dev_name) : strlen(dev_name);

    for (int i = 0; i < MAX_NUM_OF_SUPP_BUFF_MAPPINGS; i++)
    {
        if(!strncmp(dev_name_map.map[i].dev_name, dev_name, num_chars))
        {
            strncpy(buf_args_input.dev_name, dev_name_map.map[i].sys_name, sizeof(buf_args_input.dev_name));
            buf_args_input.id = DEV_MAPPING_MATCH_FOUND;
            break;
        } 
    }

    if(buf_args_input.id == DEV_MAPPING_MATCH_NOT_FOUND)
    {
        goto error_dev_name_match_not_found;
    }

    if(ioctl(file_desc_alloc, ACCEL_DYN_CMA_IOCTL_ALLOC, &buf_args_input) != 0)
    {
        goto error_cma_iotcl;
    }

    snprintf(buf_dev_name, sizeof(buf_dev_name), "/dev/udmabuf%d", buf_args_input.id);

    int buf_input_fd = open(buf_dev_name, O_RDWR);

    if (buf_input_fd < 0) 
    {
        goto error_udmabuf_fd;
    }

    // void *mmap(void addr[.length], size_t length, int prot, int flags, int fd, off_t offset);
    // NULL: Kernel chooses address
    buff_ptr = mmap(NULL, buf_args_input.size, PROT_READ | PROT_WRITE, MAP_SHARED, buf_input_fd, 0);

    if (buff_ptr == MAP_FAILED)
    {
        goto error_udmabuf_mmap;
    }

    buf_ptr_id_map[buf_id_cnt].ptr = buff_ptr;
    buf_ptr_id_map[buf_id_cnt].size = buf_args_input.size;

    ++buf_id_cnt;

    close(buf_input_fd);
    close(file_desc_alloc);

    pthread_mutex_unlock(&lib_mutex);

    return buff_ptr;

error_udmabuf_mmap:
    close(buf_input_fd);
error_udmabuf_fd:
error_cma_iotcl:
error_dev_name_match_not_found:
    close(file_desc_alloc);
    
    pthread_mutex_unlock(&lib_mutex);

    return NULL;
}

void cma_free(void *ptr)
{   
    pthread_mutex_lock(&lib_mutex);

    if((buf_id_cnt > 0) && (ptr))
    {
        int id = -1;

        for (int i = 0; i < MAX_NUM_OF_SUPP_BUFF_MAPPINGS; i++)
        {
            if(buf_ptr_id_map[i].ptr == ptr)
            {
                id = i;
                break;
            } 
        }

        if(id < 0)
            return;

        munmap(buf_ptr_id_map[id].ptr, buf_ptr_id_map[id].size);

        int file_desc_alloc = open(ACCEL_DYN_CMA_DEV_NAME, O_RDWR);

        if (file_desc_alloc >= 0) 
        {
            if (ioctl(file_desc_alloc, ACCEL_DYN_CMA_IOCTL_FREE, &id) == 0)
            {
                buf_ptr_id_map[id].ptr = NULL;
                buf_ptr_id_map[id].size = 0;
                
                --buf_id_cnt;
            }

            close(file_desc_alloc);
        }
    }

    pthread_mutex_unlock(&lib_mutex);
}

int cma_get_buff_id(void* ptr)
{
    int id = -1;

    pthread_mutex_lock(&lib_mutex);

    for (int i = 0; i < MAX_NUM_OF_SUPP_BUFF_MAPPINGS; i++)
    {
        if(buf_ptr_id_map[i].ptr == ptr)
        {
            id = i;
            break;
        } 
    }

    pthread_mutex_unlock(&lib_mutex);

    return id;
}
