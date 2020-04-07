#ifndef NETWORK_H
#define NETWORK_H

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

#include "scream.h"

#define DEFAULT_MULTICAST_GROUP "239.255.77.77"
#define DEFAULT_PORT 4010

#define HEADER_SIZE 5
#define MAX_SO_PACKETSIZE 1152+HEADER_SIZE

typedef struct rctx_network {
  int sockfd;
  struct sockaddr_in servaddr;
  struct ip_mreq imreq;
  unsigned char buf[MAX_SO_PACKETSIZE];
} rctx_network_t;

int init_network(enum receiver_type receiver_mode, in_addr_t interface, int port, char* multicast_group);
void rcv_network(receiver_data_t* receiver_data);

#endif
