#include "network.h"

void* init_network(enum receiver_type receiver_mode, in_addr_t interface, int port, char* multicast_group)
{
  rctx_network_t* ctx = malloc(sizeof(rctx_network_t));
  ctx->sockfd = socket(AF_INET,SOCK_DGRAM,0);

  memset((void *)&(ctx->servaddr), 0, sizeof(ctx->servaddr));
  ctx->servaddr.sin_family = AF_INET;
  ctx->servaddr.sin_addr.s_addr = (receiver_mode == Unicast) ? interface : htonl(INADDR_ANY);
  ctx->servaddr.sin_port = htons(port);
  bind(ctx->sockfd, (struct sockaddr *)&ctx->servaddr, sizeof(ctx->servaddr));

  if (receiver_mode == Multicast) {
    ctx->imreq.imr_multiaddr.s_addr = inet_addr(multicast_group ? multicast_group : DEFAULT_MULTICAST_GROUP);
    ctx->imreq.imr_interface.s_addr = interface;

    setsockopt(ctx->sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
              (const void *)&ctx->imreq, sizeof(struct ip_mreq));
  }

  return ctx;
}

void rcv_network(void* network_ctx, receiver_data_t* receiver_data)
{
  rctx_network_t* ctx = (rctx_network_t*)network_ctx;
  ssize_t n = 0;

  while (n < HEADER_SIZE) {
    n = recvfrom(ctx->sockfd, ctx->buf, MAX_SO_PACKETSIZE, 0, NULL, 0);
  }
  receiver_data->format.sample_rate = ctx->buf[0];
  receiver_data->format.sample_size = ctx->buf[1];
  receiver_data->format.channels = ctx->buf[2];
  receiver_data->format.channel_map = (ctx->buf[3] << 8) | ctx->buf[4];
  receiver_data->audio_size = n - HEADER_SIZE;
  receiver_data->audio = &ctx->buf[5];
}

