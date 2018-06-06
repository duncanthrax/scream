#ifndef _MSVAD_SAVEDATA_H
#define _MSVAD_SAVEDATA_H

#pragma warning(push)
#pragma warning(disable:4201) // nameless struct/union
#pragma warning(disable:4214) // bit field types other than int

// fix strange warnings from wsk.h
#pragma warning(disable:4510)
#pragma warning(disable:4512)
#pragma warning(disable:4610)

#include <ntddk.h>
#include <wsk.h>


//=============================================================================
// Defines
//=============================================================================
#define MULTICAST_TARGET	"239.255.77.77"
#define MULTICAST_PORT		4010
#define CHUNK_SIZE			980							// UDP payload size (44100Hz stereo, 16 bit, 1/180 sec)
#define NUM_CHUNKS			20							// How many payloads in ring buffer
#define BUFFER_SIZE			CHUNK_SIZE * NUM_CHUNKS		// Ring buffer size


#pragma warning(pop)

//-----------------------------------------------------------------------------
//  Forward declaration
//-----------------------------------------------------------------------------
class CSaveData;
typedef CSaveData *PCSaveData;

//-----------------------------------------------------------------------------
//  Structs
//-----------------------------------------------------------------------------

// Parameter to workitem.
#include <pshpack1.h>
typedef struct _SAVEWORKER_PARAM {
    PIO_WORKITEM     WorkItem;
    PCSaveData       pSaveData;
    KEVENT           EventDone;
} SAVEWORKER_PARAM;
typedef SAVEWORKER_PARAM *PSAVEWORKER_PARAM;
#include <poppack.h>


// RTP Packet
#pragma pack(push, 1)
typedef struct
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
    
    // Data
    BYTE data[CHUNK_SIZE];
} RTPPacket;
#pragma pack(pop)


//-----------------------------------------------------------------------------
//  Classes
//-----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
// CSaveData
//   Saves the wave data to disk.
//
IO_WORKITEM_ROUTINE SendDataWorkerCallback;

class CSaveData {
protected:
    WSK_REGISTRATION			m_wskSampleRegistration;
    PWSK_SOCKET					m_socket;
    PIRP						m_irp;
    KEVENT						m_syncEvent;
    
    PBYTE						m_pBuffer;
    ULONG						m_ulOffset;
    ULONG						m_ulSendOffset;
    PMDL						m_pMdl;
    
    static PDEVICE_OBJECT       m_pDeviceObject;
    static PSAVEWORKER_PARAM    m_pWorkItem;

    BOOL                        m_fWriteDisabled;
    
    SOCKADDR_STORAGE       		m_sServerAddr;

    RTPPacket					*m_pRtpPacket;
    PMDL						m_pPacketMdl;
    unsigned short				m_usRtpSeq;
    ULONG						m_uiRtpTs;

public:
    CSaveData();
    ~CSaveData();

    NTSTATUS                    Initialize();
    void                        Disable(BOOL fDisable);
    
    static void                 DestroyWorkItems(void);
    void                        WaitAllWorkItems(void);
    
    static NTSTATUS             SetDeviceObject(IN PDEVICE_OBJECT DeviceObject);
    static PDEVICE_OBJECT       GetDeviceObject(void);
    
    void                        WriteData(IN PBYTE pBuffer, IN ULONG ulByteCount);

private:
    static NTSTATUS             InitializeWorkItem(IN PDEVICE_OBJECT DeviceObject);

    void                        CreateSocket(void);
    void						SendData();
    friend VOID                 SendDataWorkerCallback(PDEVICE_OBJECT pDeviceObject, IN PVOID Context);
};
typedef CSaveData *PCSaveData;

#endif
