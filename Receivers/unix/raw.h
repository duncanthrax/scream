#ifndef RAW_H
#define RAW_H

#include <stdio.h>
#include <string.h>

#include "scream.h"

int raw_output_init();
int raw_output_send(receiver_data_t *data);

#endif
