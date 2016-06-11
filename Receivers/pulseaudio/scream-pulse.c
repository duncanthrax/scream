#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pulse/simple.h>
#include <pulse/error.h>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MULTICAST_TARGET "239.255.77.77"
#define MULTICAST_PORT 4010
#define MAX_SO_PACKETSIZE 1764
#define MIN_PA_PACKETSIZE MAX_SO_PACKETSIZE
#define BUFFER_SIZE MIN_PA_PACKETSIZE * 2

static void show_usage(const char *arg0)
{
  fprintf(stderr, "Usage: %s [-i interface_name_or_address]\n",
	  arg0);
  exit(1);
}

static in_addr_t get_interface(const char *name)
{
  int sockfd = socket(AF_INET,SOCK_DGRAM,0);
  struct ifreq ifr;
  in_addr_t addr = inet_addr(name);
  struct if_nameindex *ni;
  int i;

  if (addr != INADDR_NONE) {
    return addr;
  }

  if (strlen(name) >= sizeof(ifr.ifr_name)) {
    fprintf(stderr, "Too long interface name: %s\n\n", name);
    goto error_exit;
  }
  strcpy(ifr.ifr_name, name);

  sockfd = socket(AF_INET,SOCK_DGRAM,0);
  if (ioctl(sockfd, SIOCGIFADDR, &ifr) != 0) {
    fprintf(stderr, "Invalid interface: %s\n\n", name);
    goto error_exit;
  }
  close(sockfd);
  return ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;

error_exit:
  ni = if_nameindex();
  fprintf(stderr, "Available interfaces:\n");
  for (i = 0; ni[i].if_name != NULL; i++) {
    strcpy(ifr.ifr_name, ni[i].if_name);
    if (ioctl(sockfd, SIOCGIFADDR, &ifr) == 0) {
      fprintf(stderr, "  %-10s (%s)\n", ni[i].if_name, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
    }
  }
  exit(1);
}

int main(int argc, char*argv[]) {
  int sockfd, error;
  ssize_t n;
  int offset;
  struct sockaddr_in servaddr;
  struct ip_mreq imreq;
  pa_simple *s;
  pa_sample_spec ss;
  unsigned char buf[BUFFER_SIZE];
  in_addr_t interface = INADDR_ANY;
  int opt;

  while ((opt = getopt(argc, argv, "i:")) != -1) {
    switch (opt) {
    case 'i':
      interface = get_interface(optarg);
      break;
    default:
      show_usage(argv[0]);
    }
  }
  if (optind < argc) {
    fprintf(stderr, "Expected argument after options\n");
    show_usage(argv[0]);
  }

  ss.format = PA_SAMPLE_S16NE;
  ss.channels = 2;
  ss.rate = 44100;
  s = pa_simple_new(NULL,
    "Scream",
    PA_STREAM_PLAYBACK,
    NULL,
    "Audio",
    &ss,
    NULL,
    NULL,
    NULL
  );
  if (!s) goto BAIL;

  sockfd = socket(AF_INET,SOCK_DGRAM,0);

  memset((void *)&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(MULTICAST_PORT);
  bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

  imreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_TARGET);
  imreq.imr_interface.s_addr = interface;

  setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
            (const void *)&imreq, sizeof(struct ip_mreq));

  offset = 0;
  for (;;) {
    n = recvfrom(sockfd, &buf[offset], MAX_SO_PACKETSIZE, 0, NULL, 0);
    if (n > 0) {
      offset += n;
      if (offset >= MIN_PA_PACKETSIZE) {
        if (pa_simple_write(s, buf, offset, &error) < 0) {
          printf("pa_simple_write() failed: %s\n", pa_strerror(error));
          goto BAIL;
        }
        offset = 0;
      }
    }
  }

  BAIL:
  if (s) pa_simple_free(s);
  return 0;
};
