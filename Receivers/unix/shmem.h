#ifndef SHMEM_H
#define SHMEM_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/stat.h>
#include <sys/mman.h>

#include "scream.h"

struct shmheader {
  uint32_t magic;
  uint16_t write_idx;
  uint8_t  offset;
  uint16_t max_chunks;
  uint32_t chunk_size;
  uint8_t  sample_rate;
  uint8_t  sample_size;
  uint8_t  channels;
  uint16_t channel_map;
};

typedef struct rctx_shmem {
  unsigned char* mmap;
  uint16_t read_idx;
} rctx_shmem_t;

int init_shmem(char* shmem_device_file);
void rcv_shmem(receiver_data_t* receiver_data);

#endif
