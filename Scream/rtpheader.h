#ifndef RTPHEADER_H
#define RTPHEADER_H

#pragma pack(push, 1)
struct RtpHeader
{
    unsigned char cc  : 4;    /* CSRC count (always 0) */
    unsigned char x   : 1;    /* header extension flag (always 0) */
    unsigned char p   : 1;    /* padding flag (always 0) */
    unsigned char v   : 2;    /* protocol version (always 2) */

    unsigned char pt  : 7;    /* payload type (always 10, meaning L16 linear PCM, 2 channels) */
    unsigned char m : 1;    /* marker bit (always 0) */

    unsigned short seq : 16;   /* sequence number (monotonically incrementing, will just wrap) */

    // Bytes 4-7
    unsigned int ts : 32;		/* timestamp of 1st sample in packet, increase by 1 for each 4 bytes sent */

    // Bytes 8-11
    unsigned int ssrc : 32;		/* synchronization source (always 0) */
};
#pragma pack(pop)

#endif // RTPHEADER_H
