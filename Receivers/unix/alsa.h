#ifndef ALSA_H
#define ALSA_H

#include <alsa/asoundlib.h>

#include "scream.h"

#define MAX_CHANNELS 8

int alsa_output_init(int latency, char *alsa_device);
int alsa_output_send(receiver_data_t *data);

#endif
