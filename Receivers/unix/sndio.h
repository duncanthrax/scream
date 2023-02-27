#ifndef SCREAM_SNDIO_H
#define SCREAM_SNDIO_H

#include <stdio.h>
#include <string.h>
#include <sndio.h>

#include "scream.h"

int sndio_output_init(unsigned int max_latency_ms, char *dev);
int sndio_output_send(receiver_data_t *data);

#endif
