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
    WSK_REGISTRATION            m_wskSampleRegistration;
    PWSK_SOCKET                 m_socket;
    PIRP                        m_irp;
    KEVENT                      m_syncEvent;
    
    PBYTE                       m_pBuffer;
    ULONG                       m_ulOffset;
    ULONG                       m_ulSendOffset;
    PMDL                        m_pMdl;
    
    static PDEVICE_OBJECT       m_pDeviceObject;
    static PSAVEWORKER_PARAM    m_pWorkItem;

    BOOL                        m_fWriteDisabled;
    
    SOCKADDR_STORAGE            m_sServerAddr;

    BYTE                        m_bSamplingFreqMarker;
    BYTE                        m_bBitsPerSampleMarker;
    BYTE                        m_bChannels;
    WORD                        m_wChannelMask;

public:
    CSaveData();
    ~CSaveData();

    NTSTATUS                    Initialize(DWORD nSamplesPerSec, WORD wBitsPerSample, WORD nChannels, DWORD dwChannelMask);
    void                        Disable(BOOL fDisable);
    
    static void                 DestroyWorkItems(void);
    void                        WaitAllWorkItems(void);
    
    static NTSTATUS             SetDeviceObject(IN PDEVICE_OBJECT DeviceObject);
    static PDEVICE_OBJECT       GetDeviceObject(void);
    
    void                        WriteData(IN PBYTE pBuffer, IN ULONG ulByteCount);

private:
    static NTSTATUS             InitializeWorkItem(IN PDEVICE_OBJECT DeviceObject);

    void                        CreateSocket(void);
    void                        SendData();
    friend VOID                 SendDataWorkerCallback(PDEVICE_OBJECT pDeviceObject, IN PVOID Context);
};
typedef CSaveData *PCSaveData;

#endif
