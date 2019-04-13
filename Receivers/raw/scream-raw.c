#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEFAULT_MULTICAST_GROUP "239.255.77.77"
#define DEFAULT_PORT 4010

#define HEADER_SIZE 5
#define MAX_SO_PACKETSIZE 1152+HEADER_SIZE

#define SNDCHK(call, ret) { \
  if (ret < 0) {            \
    alsa_error(call, ret);  \
    return -1;              \
  }                         \
}


static void show_usage(const char *arg0)
{
  fprintf(stderr, "\n");
  fprintf(stderr, "Usage: %s [-u] [-v] [-p <port>] [-i <iface>] [-g <group>] \n", arg0);
  fprintf(stderr, "\n");
  fprintf(stderr, "         All command line options are optional. Default is to use\n");
  fprintf(stderr, "         multicast with group address 239.255.77.77, port 4010.\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "         -u          : Use unicast instead of multicast.\n");
  fprintf(stderr, "         -v          : Verbose operation.\n");
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
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
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

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
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

int main(int argc, char *argv[])
{
  int sockfd;
  ssize_t n;
  struct sockaddr_in servaddr;
  struct ip_mreq imreq;
 
  unsigned char buf[MAX_SO_PACKETSIZE];
  int opt;
  int verbosity = 0;
  int samples;

  unsigned int bytes_per_sample = 2;
  unsigned int rate = 44100;
  unsigned char cur_server_rate = 0;
  unsigned char cur_server_size = 0;
  unsigned char cur_channels = 2;
  unsigned char cur_channel_map_lsb = 0x03; // stereo
  unsigned char cur_channel_map_msb = 0;
  uint16_t cur_channel_map = (cur_channel_map_msb << 8) | cur_channel_map_lsb;

  // Command line options
  int use_unicast       = 0;
  char *multicast_group = NULL;
  in_addr_t interface   = INADDR_ANY;
  uint16_t port         = DEFAULT_PORT;

  
  while ((opt = getopt(argc, argv, "i:g:p:t:vuh")) != -1) {
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
    case 'v':
      verbosity += 1;
      break;
    default:
      show_usage(argv[0]);
    }
  }
  if (optind < argc) {
    fprintf(stderr, "Expected argument after options\n");
    show_usage(argv[0]);
  }

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);

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
    n = recvfrom(sockfd, &buf, MAX_SO_PACKETSIZE, 0, NULL, 0);
    if (n < HEADER_SIZE) continue;
    
    // Change rate/size?
    if (cur_server_rate != buf[0] || cur_server_size != buf[1] || cur_channels != buf[2] || cur_channel_map_lsb != buf[3] || cur_channel_map_msb != buf[4]) {
        cur_server_rate = buf[0];
        cur_server_size = buf[1];
        cur_channels = buf[2];
        cur_channel_map_lsb = buf[3];
        cur_channel_map_msb = buf[4];
        cur_channel_map = (cur_channel_map_msb << 8) | cur_channel_map_lsb;

        rate = ((cur_server_rate >= 128) ? 44100 : 48000) * (cur_server_rate % 128);
        switch (cur_server_size) {
          case 16:  bytes_per_sample = 2; break;
          case 24:  bytes_per_sample = 3; break;
          case 32:  bytes_per_sample = 4; break;
          default:
            if (verbosity > 0) {
              fprintf(stderr, "Unsupported sample size %hhu, not playing until next format switch.\n", cur_server_size);
            }
            rate = 0;
        }

        if (cur_channels > 2) {
          int k = -1;
          for (int i=0; i<cur_channels; i++) {
            for (int j = k+1; j<=10; j++) {// check the channel map bit by bit from lsb to msb, starting from were we left on the previous step
              if ((cur_channel_map >> j) & 0x01) {// if the bit in j position is set then we have the key for this channel
                k = j;
                break;
              }
            }
            if (verbosity > 0) {
              const char *channel_name;
              switch (k) {
                case  0: channel_name = "Front Left"; break;
                case  1: channel_name = "Front Right"; break;
                case  2: channel_name = "Front Center"; break;
                case  3: channel_name = "LFE / Subwoofer"; break;
                case  4: channel_name = "Rear Left"; break;
                case  5: channel_name = "Rear Right"; break;
                case  6: channel_name = "Front-Left Center"; break;
                case  7: channel_name = "Front-Right Center"; break;
                case  8: channel_name = "Rear Center"; break;
                case  9: channel_name = "Side Left"; break;
                case 10: channel_name = "Side Right"; break;
                default:
                  channel_name = "Unknown. Setted to Center.";
              }
              fprintf(stderr, "Channel %i is %s\n", i, channel_name);
            }
          }
        }
    }
    if (!rate) continue;

    samples = (n - HEADER_SIZE) / (bytes_per_sample * cur_channels);
    fwrite(&buf[HEADER_SIZE],bytes_per_sample*cur_channels,samples,stdout);
 
  }
}
