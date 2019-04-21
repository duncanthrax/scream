//-----------------------------------------------------------------------------
// Author - Marco Martinelli - https://github.com/martinellimarco
//-----------------------------------------------------------------------------

#ifndef _MSVAD_IVSHMEM_SAVEDATA_H
#define _MSVAD_IVSHMEM_SAVEDATA_H

#pragma warning(push)
#pragma warning(disable:4201) // nameless struct/union
#pragma warning(disable:4214) // bit field types other than int

#include <ntddk.h>

#pragma warning(pop)

//-----------------------------------------------------------------------------
//  IVSHMEM IOCTLs
//-----------------------------------------------------------------------------

#define IOCTL_IVSHMEM_REQUEST_SIZE   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IVSHMEM_REQUEST_MMAP   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IVSHMEM_RELEASE_MMAP   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)

//-----------------------------------------------------------------------------
//  Forward declaration
//-----------------------------------------------------------------------------
class CIVSHMEMSaveData;
typedef CIVSHMEMSaveData *PCIVSHMEMSaveData;

//-----------------------------------------------------------------------------
//  Structs
//-----------------------------------------------------------------------------

// Parameter to workitem.
#include <pshpack1.h>
typedef struct _IVSHMEM_SAVEWORKER_PARAM {
    PIO_WORKITEM        WorkItem;
    PCIVSHMEMSaveData   pSaveData;
    KEVENT              EventDone;
} IVSHMEM_SAVEWORKER_PARAM;
typedef IVSHMEM_SAVEWORKER_PARAM *PIVSHMEM_SAVEWORKER_PARAM;
#include <poppack.h>

typedef struct IVSHMEM_MMAP
{
    UINT16         peerID;  // our peer id
    UINT64         size;    // the size of the memory region
    PVOID          ptr;     // pointer to the memory region
    UINT16         vectors; // the number of vectors available
}
IVSHMEM_MMAP, *PIVSHMEM_MMAP;

typedef struct IVSHMEM_OBJECT
{
    BOOLEAN initialized;
    PDEVICE_OBJECT devObj;
    IVSHMEM_MMAP mmap;
    UINT16 writeIdx;
    UINT8  offset;    //position of the 1st chunk
    UINT16 maxChunks; //how many chunks
    UINT32 chunkSize; //the size of a chunk
    UINT32 bufferSize; 
}
IVSHMEM_OBJECT, *PIVSHMEM_OBJECT;

typedef struct IVSHMEM_SCREAM_HEADER
{
    UINT32 magic;
    UINT16 writeIdx;
    UINT8  offset;    //position of the 1st chunk
    UINT16 maxChunks; //how many chunks
    UINT32 chunkSize; //the size of a chunk
    UINT8  sampleRate;
    UINT8  sampleSize;
    UINT8  channels;
    UINT16 channelMap;
}
IVSHMEM_SCREAM_HEADER, *PIVSHMEM_SCREAM_HEADER;

//-----------------------------------------------------------------------------
//  Classes
//-----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
// CIVSHMEMSaveData
//   Saves the wave data to disk.
//
IO_WORKITEM_ROUTINE IVSHMEMSendDataWorkerCallback;

class CIVSHMEMSaveData {
protected:
    PBYTE                       m_pBuffer;
    ULONG                       m_ulOffset;
    ULONG                       m_ulSendOffset;
    
    static PDEVICE_OBJECT       m_pDeviceObject;
    static PIVSHMEM_SAVEWORKER_PARAM    m_pWorkItem;

    BOOL                        m_fWriteDisabled;

    IVSHMEM_OBJECT              m_ivshmem;
public:
    CIVSHMEMSaveData();
    ~CIVSHMEMSaveData();

    NTSTATUS                    Initialize(DWORD nSamplesPerSec, WORD wBitsPerSample, WORD nChannels, DWORD dwChannelMask);
    void                        Disable(BOOL fDisable);
    
    static void                 DestroyWorkItems(void);
    void                        WaitAllWorkItems(void);
    
    static NTSTATUS             SetDeviceObject(IN PDEVICE_OBJECT DeviceObject);
    static PDEVICE_OBJECT       GetDeviceObject(void);
    
    void                        WriteData(IN PBYTE pBuffer, IN ULONG ulByteCount);

private:
    static NTSTATUS             InitializeWorkItem(IN PDEVICE_OBJECT DeviceObject);

    void                        IVSHMEMSendData();
    friend VOID                 IVSHMEMSendDataWorkerCallback(PDEVICE_OBJECT pDeviceObject, IN PVOID Context);

    BOOLEAN                     RequestMMAP();
    void                        ReleaseMMAP();
};
typedef CIVSHMEMSaveData *PCIVSHMEMSaveData;

#endif
