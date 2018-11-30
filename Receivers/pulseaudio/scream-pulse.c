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

#define DEFAULT_MULTICAST_GROUP "239.255.77.77"
#define DEFAULT_PORT 4010

#define MAX_SO_PACKETSIZE 1154

static void show_usage(const char *arg0)
{
  fprintf(stderr, "\n");
  fprintf(stderr, "Usage: %s [-u] [-p <port>] [-i <iface>] [-g <group>]\n", arg0);
  fprintf(stderr, "\n");
  fprintf(stderr, "         All command line options are optional. Default is to use\n");
  fprintf(stderr, "         multicast with group address 239.255.77.77, port 4010.\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "         -u          : Use unicast instead of multicast.\n");
  fprintf(stderr, "         -p <port>   : Use <port> instead of default port 4010.\n");
  fprintf(stderr, "                       Applies to both multicast and unicast.\n");
  fprintf(stderr, "         -i <iface>  : Use local interface <iface>. Either the IP\n");
  fprintf(stderr, "                       or the interface name can be specified. In\n");
  fprintf(stderr, "                       multicast mode, uses this interface for IGMP.\n");
  fprintf(stderr, "                       In unicast, binds to this interface only.\n");
  fprintf(stderr, "         -g <group>  : Multicast group address. Multicast mode only.\n");
  fprintf(stderr, "\n");
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
  struct sockaddr_in servaddr;
  struct ip_mreq imreq;
  pa_simple *s;
  pa_sample_spec ss;
  int opt;
  unsigned char cur_sample_rate = 0, cur_sample_size = 0;
  unsigned char buf[MAX_SO_PACKETSIZE];
  
  // Command line options
  int use_unicast       = 0;
  char *multicast_group = NULL;
  in_addr_t interface   = INADDR_ANY;
  uint16_t port         = DEFAULT_PORT;

  while ((opt = getopt(argc, argv, "i:g:p:uh")) != -1) {
    switch (opt) {
    case 'i':
      interface = get_interface(optarg);
      break;
    case 'p':
      port = atoi(optarg);
      if (!port) show_usage(argv[0]);
      break;
    case 'u':
      use_unicast = 1;
      break;
    case 'g':
      multicast_group = strdup(optarg);
      break;
    default:
      show_usage(argv[0]);
    }
  }
  if (optind < argc) {
    fprintf(stderr, "Expected argument after options\n");
    show_usage(argv[0]);
  }

  // Start with base default format, will switch to actual format later
  ss.format = PA_SAMPLE_S16LE;
  ss.rate = 44100;
  ss.channels = 2;
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
  if (!s) {
    printf("Unable to connect to PulseAudio\n");
    goto BAIL;
  }

  sockfd = socket(AF_INET,SOCK_DGRAM,0);

  memset((void *)&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = use_unicast ? interface : htonl(INADDR_ANY);
  servaddr.sin_port = htons(port);
  bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

  if (!use_unicast) {
    imreq.imr_multiaddr.s_addr = inet_addr(multicast_group ? multicast_group : DEFAULT_MULTICAST_GROUP);
    imreq.imr_interface.s_addr = interface;

    setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
              (const void *)&imreq, sizeof(struct ip_mreq));
  }

  for (;;) {
    n = recvfrom(sockfd, buf, MAX_SO_PACKETSIZE, 0, NULL, 0);
    if (n > 2) {

      if (cur_sample_rate != buf[0] || cur_sample_size != buf[1]) {
        cur_sample_rate = buf[0];
        cur_sample_size = buf[1];

        ss.rate = ((cur_sample_rate >= 128) ? 44100 : 48000) * (cur_sample_rate % 128);
        switch (cur_sample_size) {
          case 16: ss.format = PA_SAMPLE_S16LE; break;
          case 24: ss.format = PA_SAMPLE_S24LE; break;
          case 32: ss.format = PA_SAMPLE_S32LE; break;
          default:
            printf("Unsupported sample size %hhu, not playing until next format switch.\n", cur_sample_size);
            ss.rate = 0;
        }

        if (ss.rate > 0) {
          if (s) pa_simple_free(s);
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
          if (s) {
            printf("Switched format to sample rate %u and sample size %hhu.\n", ss.rate, cur_sample_size);
          }
          else {
            printf("Unable to open PulseAudio with sample rate %u and sample size %hhu, not playing until next format switch.\n", ss.rate, cur_sample_size);
            ss.rate = 0;
          }
        }
      }
      if (!ss.rate) continue;
      if (pa_simple_write(s, &buf[2], n - 2, &error) < 0) {
        printf("pa_simple_write() failed: %s\n", pa_strerror(error));
        goto BAIL;
      }
    }
  }

  BAIL:
  if (s) pa_simple_free(s);
  return 0;
};
