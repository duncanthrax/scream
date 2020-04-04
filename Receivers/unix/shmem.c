#include "shmem.h"

static rctx_shmem_t rctx_shmem;

int init_shmem(char* shmem_device_file)
{
  struct stat st;
  if (stat(shmem_device_file, &st) < 0)  {
    fprintf(stderr, "Failed to stat the shared memory file: %s\n", shmem_device_file);
    exit(2);
  }

  int shmFD = open(shmem_device_file, O_RDONLY);
  if (shmFD < 0) {
    fprintf(stderr, "Failed to open the shared memory file: %s\n", shmem_device_file);
    exit(3);
  }

  rctx_shmem.mmap = mmap(0, st.st_size, PROT_READ, MAP_SHARED, shmFD, 0);
  if (rctx_shmem.mmap == MAP_FAILED) {
    fprintf(stderr, "Failed to map the shared memory file: %s\n", shmem_device_file);
    close(shmFD);
    exit(4);
  }

  struct shmheader *header = (struct shmheader*)rctx_shmem.mmap;
  rctx_shmem.read_idx = header->write_idx;

  return 0;
}

void rcv_shmem(receiver_data_t* receiver_data)
{
  struct shmheader *header = (struct shmheader*)rctx_shmem.mmap;

  int valid = 0;
  do {
    if (header->magic != 0x11112014) {
      while (header->magic != 0x11112014) {
        usleep(10000);//10ms
      }
      rctx_shmem.read_idx = header->write_idx;
      continue;
    }
    if (rctx_shmem.read_idx == header->write_idx) {
      usleep(10000);//10ms
      continue;
    }
    if (header->channels == 0 || header->channel_map == 0)
      continue;

    valid = 1;
  } while (!valid);

  if (++rctx_shmem.read_idx == header->max_chunks) {
    rctx_shmem.read_idx = 0;
  }

  receiver_data->format.sample_rate = header->sample_rate;
  receiver_data->format.sample_size = header->sample_size;
  receiver_data->format.channels = header->channels;
  receiver_data->format.channel_map = header->channel_map;

  receiver_data->audio_size = header->chunk_size;
  receiver_data->audio = &rctx_shmem.mmap[header->offset+header->chunk_size*rctx_shmem.read_idx];
}

