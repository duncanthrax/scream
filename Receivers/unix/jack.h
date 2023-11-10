#ifndef JACK_H
#define JACK_H

#include "scream.h"

int jack_output_init(int latency, char *stream_name, int connect);
int jack_output_send(receiver_data_t *data);

#endif
