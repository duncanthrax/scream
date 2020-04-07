#ifndef SCREAM_H
#define SCREAM_H

#include <stdint.h>

enum receiver_type {
  Unicast, Multicast, SharedMem
};

enum output_type {
  Raw, Alsa, Pulseaudio
};

typedef struct receiver_format {
  unsigned char sample_rate;
  unsigned char sample_size;
  unsigned char channels;
  uint16_t channel_map;
} receiver_format_t;

typedef struct receiver_data {
  receiver_format_t format;
  unsigned int audio_size;
  unsigned char* audio;
} receiver_data_t;

static int verbosity = 0;

#endif
