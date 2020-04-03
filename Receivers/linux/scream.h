#ifndef SCREAM_H
#define SCREAM_H

enum receiver_type {
    Unicast, Multicast, SharedMem
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


#endif
