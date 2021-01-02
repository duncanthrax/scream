#pragma warning (disable : 4127)

#include "scream.h"
#include "savedata.h"

//=============================================================================
// Defines
//=============================================================================
#define MULTICAST_TARGET    "239.255.77.77"
#define MULTICAST_PORT      4010
#define PCM_PAYLOAD_SIZE    1152                        // PCM payload size (divisible by 2, 3 and 4 bytes per sample * 2 channels)
#define HEADER_SIZE         5                           // m_bSamplingFreqMarker, m_bBitsPerSampleMarker, m_bChannels, m_wChannelMask
#define CHUNK_SIZE          (PCM_PAYLOAD_SIZE + HEADER_SIZE)      // Add two bytes so we can send a small header with bytes/sample and sampling freq markers
#define NUM_CHUNKS          800                         // How many payloads in ring buffer
#define BUFFER_SIZE         CHUNK_SIZE * NUM_CHUNKS     // Ring buffer size

#define AC3_PAYLOAD_SIZE    1024    // AC3 Payload size (48 kHz, Stereo, 256 kbps). Currently, this is fixed. Might be extended.
#define RTP_HEADER_SIZE     12
#define RTP_AC3_HEADER_SIZE 2
#define RTP_AC3_CHUNK_SIZE  (AC3_PAYLOAD_SIZE + RTP_HEADER_SIZE + RTP_AC3_HEADER_SIZE)
#define RTP_AC3_BUFFER_SIZE RTP_AC3_CHUNK_SIZE * NUM_CHUNKS

//=============================================================================
// Statics
//=============================================================================

// Client-level callback table
const WSK_CLIENT_DISPATCH WskSampleClientDispatch = {
    MAKE_WSK_VERSION(1, 0), // This sample uses WSK version 1.0
    0, // Reserved
    NULL // WskClientEvent callback is not required in WSK version 1.0
};

//=============================================================================
// Helper Functions
//=============================================================================
// IRP completion routine used for synchronously waiting for completion
NTSTATUS WskSampleSyncIrpCompletionRoutine(__in PDEVICE_OBJECT Reserved, __in PIRP Irp, __in PVOID Context) {    
    PKEVENT compEvent = (PKEVENT)Context;
    
    UNREFERENCED_PARAMETER(Reserved);
    UNREFERENCED_PARAMETER(Irp);
    
    KeSetEvent(compEvent, 2, FALSE);    

    return STATUS_MORE_PROCESSING_REQUIRED;
}

#pragma code_seg("PAGE")
//=============================================================================
// CSaveData
//=============================================================================

//=============================================================================
CSaveData::CSaveData() : m_pBuffer(NULL), m_ulOffset(0), m_ulSendOffset(0), m_fWriteDisabled(FALSE), m_socket(NULL) {
    PAGED_CODE();

    DPF_ENTER(("[CSaveData::CSaveData]"));

    m_ulBufferSize  = g_UseRtpAc3 ? RTP_AC3_BUFFER_SIZE : BUFFER_SIZE;
    m_ulChunkSize   = g_UseRtpAc3 ? RTP_AC3_CHUNK_SIZE : CHUNK_SIZE;
    
    if (!g_UseIVSHMEM) {
        WSK_CLIENT_NPI   wskClientNpi;

        // allocate work item for this stream
        m_pWorkItem = (PSAVEWORKER_PARAM)ExAllocatePoolWithTag(NonPagedPool, sizeof(SAVEWORKER_PARAM), MSVAD_POOLTAG);
        if (m_pWorkItem) {
            m_pWorkItem->WorkItem = IoAllocateWorkItem(GetDeviceObject());
            KeInitializeEvent(&(m_pWorkItem->EventDone), NotificationEvent, TRUE);
        }

        // get us an IRP
        m_irp = IoAllocateIrp(1, FALSE);

        // initialize io completion sychronization event
        KeInitializeEvent(&m_syncEvent, SynchronizationEvent, FALSE);

        // Register with WSK.
        wskClientNpi.ClientContext = NULL;
        wskClientNpi.Dispatch = &WskSampleClientDispatch;
        WskRegister(&wskClientNpi, &m_wskSampleRegistration);
    }
} // CSaveData

//=============================================================================
CSaveData::~CSaveData() {
    PAGED_CODE();

    DPF_ENTER(("[CSaveData::~CSaveData]"));

    if (!g_UseIVSHMEM) {
        // frees the work item
        if (m_pWorkItem->WorkItem != NULL) {
            IoFreeWorkItem(m_pWorkItem->WorkItem);
            m_pWorkItem->WorkItem = NULL;
        }

        // close socket
        if (m_socket) {
            IoReuseIrp(m_irp, STATUS_UNSUCCESSFUL);
            IoSetCompletionRoutine(m_irp, WskSampleSyncIrpCompletionRoutine, &m_syncEvent, TRUE, TRUE, TRUE);
            ((PWSK_PROVIDER_BASIC_DISPATCH)m_socket->Dispatch)->WskCloseSocket(m_socket, m_irp);
            KeWaitForSingleObject(&m_syncEvent, Executive, KernelMode, FALSE, NULL);
        }

        // Deregister with WSK. This call will wait until all the references to
        // the WSK provider NPI are released and all the sockets are closed. Note
        // that if the worker thread has not started yet, then when it eventually
        // starts, its WskCaptureProviderNPI call will fail and the work queue
        // will be flushed and cleaned up properly.
        WskDeregister(&m_wskSampleRegistration);

        // free irp
        IoFreeIrp(m_irp);

        if (m_pBuffer) {
            ExFreePoolWithTag(m_pBuffer, MSVAD_POOLTAG);
            IoFreeMdl(m_pMdl);
        }
    }
} // CSaveData

//=============================================================================
void CSaveData::DestroyWorkItems(void) {
    PAGED_CODE();
    
    DPF_ENTER(("[CSaveData::DestroyWorkItems]"));

    if (m_pWorkItem) {
        ExFreePoolWithTag(m_pWorkItem, MSVAD_POOLTAG);
        m_pWorkItem = NULL;
    }

} // DestroyWorkItems

//=============================================================================
void CSaveData::Disable(BOOL fDisable) {
    PAGED_CODE();

    m_fWriteDisabled = fDisable;
} // Disable

//=============================================================================
NTSTATUS CSaveData::SetDeviceObject(IN PDEVICE_OBJECT DeviceObject) {
    PAGED_CODE();

    ASSERT(DeviceObject);

    NTSTATUS ntStatus = STATUS_SUCCESS;
    
    m_pDeviceObject = DeviceObject;
    return ntStatus;
}

//=============================================================================
PDEVICE_OBJECT CSaveData::GetDeviceObject(void) {
    PAGED_CODE();

    return m_pDeviceObject;
}

#pragma code_seg("PAGE")
//=============================================================================
NTSTATUS CSaveData::Initialize(DWORD nSamplesPerSec, WORD wBitsPerSample, WORD nChannels, DWORD dwChannelMask) {
    PAGED_CODE();

    DPF_ENTER(("[CSaveData::Initialize]"));

    // Only multiples of 44100 and 48000 are supported
    m_bSamplingFreqMarker = (BYTE)((nSamplesPerSec % 44100) ? (0 + (nSamplesPerSec / 48000)) : (128 + (nSamplesPerSec / 44100)));
    m_bBitsPerSampleMarker = (BYTE)(wBitsPerSample);
    m_bChannels = (BYTE)nChannels;
    m_wChannelMask = (WORD)dwChannelMask;

    NTSTATUS ntStatus = STATUS_SUCCESS;
    if (g_UseRtpAc3) {
        if (nSamplesPerSec != 48000) {
            return STATUS_INVALID_PARAMETER_1;
        }
        if (wBitsPerSample != 16) {
            return STATUS_INVALID_PARAMETER_2;
        }
        if (wBitsPerSample != 2) {
            return STATUS_INVALID_PARAMETER_3;
        }

        ntStatus = m_ac3Encoder.Initialize(nSamplesPerSec, nChannels);

        m_rtpHeader.v = 2;
        m_rtpHeader.m = 1;
        m_rtpHeader.pt = 96;
        m_rtpSeq = rand();
        m_rtpTs = rand();
        m_rtpHeader.ssrc = rand();
    }

    if (NT_SUCCESS(ntStatus)) {
        ntStatus = AllocBuffer(m_ulBufferSize);
    }

    return ntStatus;
} // Initialize

//=============================================================================
IO_WORKITEM_ROUTINE SendDataWorkerCallback;

VOID SendDataWorkerCallback(PDEVICE_OBJECT pDeviceObject, IN  PVOID  Context) {
    UNREFERENCED_PARAMETER(pDeviceObject);

    PAGED_CODE();

    ASSERT(Context);

    PSAVEWORKER_PARAM pParam = (PSAVEWORKER_PARAM) Context;
    PCSaveData        pSaveData;

    ASSERT(pParam->pSaveData);

    if (pParam->WorkItem) {
        pSaveData = pParam->pSaveData;
        pSaveData->SendData();
    }

    KeSetEvent(&(pParam->EventDone), 0, FALSE);
} // SendDataWorkerCallback

NTSTATUS CSaveData::AllocBuffer(SIZE_T nBufferSize)
{
    NTSTATUS ntStatus = STATUS_SUCCESS;

    // Allocate memory for data buffer.
    if (NT_SUCCESS(ntStatus)) {
        m_pBuffer = (PBYTE)ExAllocatePoolWithTag(NonPagedPool, nBufferSize, MSVAD_POOLTAG);
        if (!m_pBuffer) {
            DPF(D_TERSE, ("[Could not allocate memory for sending data]"));
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    // Allocate MDL for the data buffer
    if (NT_SUCCESS(ntStatus)) {
        m_pMdl = IoAllocateMdl(m_pBuffer, nBufferSize, FALSE, FALSE, NULL);
        if (m_pMdl == NULL) {
            DPF(D_TERSE, ("[Failed to allocate MDL]"));
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
        else {
            MmBuildMdlForNonPagedPool(m_pMdl);
        }
    }

    return ntStatus;
}

#pragma code_seg()
//=============================================================================
void CSaveData::CreateSocket(void) {
    NTSTATUS            status;
    WSK_PROVIDER_NPI    pronpi;
    LPCTSTR             terminator;
    SOCKADDR_IN         locaddr4 = { AF_INET, RtlUshortByteSwap((USHORT)g_UnicastPort), 0, 0 };
    SOCKADDR_IN         sockaddr = { AF_INET, RtlUshortByteSwap((USHORT)g_UnicastPort), 0, 0 };
    
    DPF_ENTER(("[CSaveData::CreateSocket]"));
    
    // capture WSK provider
    status = WskCaptureProviderNPI(&m_wskSampleRegistration, WSK_INFINITE_WAIT, &pronpi);
    if(!NT_SUCCESS(status)){
        DPF(D_TERSE, ("Failed to capture provider NPI: 0x%X\n", status));
        return;
    }

    RtlIpv4StringToAddress(g_UnicastIPv4, true, &terminator, &(sockaddr.sin_addr));
    RtlCopyMemory(&m_sServerAddr, &sockaddr, sizeof(SOCKADDR_IN));
    
    // create socket
    IoReuseIrp(m_irp, STATUS_UNSUCCESSFUL);
    IoSetCompletionRoutine(m_irp, WskSampleSyncIrpCompletionRoutine, &m_syncEvent, TRUE, TRUE, TRUE);    
    pronpi.Dispatch->WskSocket(
        pronpi.Client,
        m_sServerAddr.ss_family,
        SOCK_DGRAM,
        IPPROTO_UDP,
        WSK_FLAG_DATAGRAM_SOCKET,
        NULL, // socket context
        NULL, // dispatch
        NULL, // Process
        NULL, // Thread
        NULL, // SecurityDescriptor
        m_irp);
    KeWaitForSingleObject(&m_syncEvent, Executive, KernelMode, FALSE, NULL);
    
    DPF(D_TERSE, ("WskSocket: %x", m_irp->IoStatus.Status));
    
    if (!NT_SUCCESS(m_irp->IoStatus.Status)) {
        DPF(D_TERSE, ("Failed to create socket: %x", m_irp->IoStatus.Status));
        
        if(m_socket) {
            IoReuseIrp(m_irp, STATUS_UNSUCCESSFUL);
            IoSetCompletionRoutine(m_irp, WskSampleSyncIrpCompletionRoutine, &m_syncEvent, TRUE, TRUE, TRUE);
            ((PWSK_PROVIDER_BASIC_DISPATCH)m_socket->Dispatch)->WskCloseSocket(m_socket, m_irp);
            KeWaitForSingleObject(&m_syncEvent, Executive, KernelMode, FALSE, NULL);
        }
        
        // release the provider again, as we are finished with it
        WskReleaseProviderNPI(&m_wskSampleRegistration);
        
        return;
    }
    
    // save the socket
    m_socket = (PWSK_SOCKET)m_irp->IoStatus.Information;
    
    // release the provider again, as we are finished with it
    WskReleaseProviderNPI(&m_wskSampleRegistration);

    // bind the socket
    IoReuseIrp(m_irp, STATUS_UNSUCCESSFUL);
    IoSetCompletionRoutine(m_irp, WskSampleSyncIrpCompletionRoutine, &m_syncEvent, TRUE, TRUE, TRUE);
    status = ((PWSK_PROVIDER_DATAGRAM_DISPATCH)(m_socket->Dispatch))->WskBind(m_socket, (PSOCKADDR)(&locaddr4), 0, m_irp);
    KeWaitForSingleObject(&m_syncEvent, Executive, KernelMode, FALSE, NULL);
    
    DPF(D_TERSE, ("WskBind: %x", m_irp->IoStatus.Status));
    
    if (!NT_SUCCESS(m_irp->IoStatus.Status)) {
        DPF(D_TERSE, ("Failed to bind socket: %x", m_irp->IoStatus.Status));
        if(m_socket) {
            IoReuseIrp(m_irp, STATUS_UNSUCCESSFUL);
            IoSetCompletionRoutine(m_irp, WskSampleSyncIrpCompletionRoutine, &m_syncEvent, TRUE, TRUE, TRUE);
            ((PWSK_PROVIDER_BASIC_DISPATCH)m_socket->Dispatch)->WskCloseSocket(m_socket, m_irp);
            KeWaitForSingleObject(&m_syncEvent, Executive, KernelMode, FALSE, NULL);
        }
        
        return;
    }
}

//=============================================================================
BOOL CSaveData::WriteScreamData(IN PBYTE pBuffer, IN ULONG ulByteCount) {
    // Append to ring buffer. Don't write intermediate states to m_ulOffset,
    // but update it once at the end.
    auto offset = m_ulOffset;
    auto toWrite = ulByteCount;
    ULONG w = 0;
    while (toWrite > 0) {
        w = offset % m_ulChunkSize;
        if (w > 0) {
            // Fill up last chunk
            w = (m_ulChunkSize - w);
            w = (toWrite < w) ? toWrite : w;
            RtlCopyMemory(&(m_pBuffer[offset]), &(pBuffer[ulByteCount - toWrite]), w);
        }
        else {
            // Start a new chunk
            m_pBuffer[offset] = m_bSamplingFreqMarker;
            m_pBuffer[offset + 1] = m_bBitsPerSampleMarker;
            m_pBuffer[offset + 2] = m_bChannels;
            m_pBuffer[offset + 3] = (BYTE)(m_wChannelMask & 0xFF);
            m_pBuffer[offset + 4] = (BYTE)(m_wChannelMask >> 8 & 0xFF);
            offset += HEADER_SIZE;
            w = ((m_ulBufferSize - offset) < toWrite) ? (m_ulBufferSize - offset) : toWrite;
            w = (w > PCM_PAYLOAD_SIZE) ? PCM_PAYLOAD_SIZE : w;
            RtlCopyMemory(&(m_pBuffer[offset]), &(pBuffer[ulByteCount - toWrite]), w);
        }
        toWrite -= w;
        offset += w;  if (offset >= m_ulBufferSize) offset = 0;
    }
    m_ulOffset = offset;

    return TRUE;
}

//=============================================================================
BOOL CSaveData::WriteRtpAc3Data(IN PBYTE pBuffer, IN ULONG ulByteCount) {
    auto outSample = m_ac3Encoder.Process(pBuffer, ulByteCount);
    if (!outSample) {
        return FALSE;
    }

    m_rtpHeader.seq = _byteswap_ushort(m_rtpSeq);
    m_rtpHeader.ts = _byteswap_ulong(m_rtpTs);

    IMFMediaBuffer* buffer = nullptr;
    outSample->GetBufferByIndex(0, &buffer);
    BYTE* ac3Data;
    DWORD maxAc3Length, currentAc3Length;
    buffer->Lock(&ac3Data, &maxAc3Length, &currentAc3Length);

    RtlCopyMemory(&(m_pBuffer[m_ulOffset]), &m_rtpHeader, sizeof(RtpHeader));
    m_ulOffset += RTP_HEADER_SIZE;
    m_pBuffer[m_ulOffset] = 0;
    m_pBuffer[m_ulOffset + 1] = 1;
    m_ulOffset += 2;
    RtlCopyMemory(&(m_pBuffer[m_ulOffset]), ac3Data, currentAc3Length);
    m_ulOffset += currentAc3Length;
    
    buffer->Unlock();
    buffer->Release();
    outSample->Release();

    ++m_rtpSeq;
    m_rtpTs += 1536;

    return TRUE;
}

//=============================================================================
void CSaveData::SendData() {
    WSK_BUF wskbuf;

    ULONG storeOffset;
    
    if (!m_socket) {
        CreateSocket();
    }
    
    if (m_socket) {
        while (1) {
            // Read latest storeOffset. There might be new data.
            storeOffset = m_ulOffset;

            // Abort if there's nothing to send. Note: When storeOffset < sendOffset, we can always send a chunk.
            if ((storeOffset >= m_ulSendOffset) && ((storeOffset - m_ulSendOffset) < m_ulChunkSize))
                break;

            // Send a chunk
            wskbuf.Mdl = m_pMdl;
            wskbuf.Length = m_ulChunkSize;
            wskbuf.Offset = m_ulSendOffset;
            IoReuseIrp(m_irp, STATUS_UNSUCCESSFUL);
            IoSetCompletionRoutine(m_irp, WskSampleSyncIrpCompletionRoutine, &m_syncEvent, TRUE, TRUE, TRUE);
            ((PWSK_PROVIDER_DATAGRAM_DISPATCH)(m_socket->Dispatch))->WskSendTo(m_socket, &wskbuf, 0, (PSOCKADDR)&m_sServerAddr, 0, NULL, m_irp);
            KeWaitForSingleObject(&m_syncEvent, Executive, KernelMode, FALSE, NULL);
            DPF(D_TERSE, ("WskSendTo: %x", m_irp->IoStatus.Status));

            m_ulSendOffset += m_ulChunkSize; if (m_ulSendOffset >= m_ulBufferSize) m_ulSendOffset = 0;
        }
    }
}

#pragma code_seg("PAGE")
//=============================================================================
void CSaveData::WaitAllWorkItems(void) {
    PAGED_CODE();

    DPF_ENTER(("[CSaveData::WaitAllWorkItems]"));

    DPF(D_VERBOSE, ("[Waiting for WorkItem]"));
    KeWaitForSingleObject(&(m_pWorkItem->EventDone), Executive, KernelMode, FALSE, NULL);
    
} // WaitAllWorkItems

#pragma code_seg()
//=============================================================================
void CSaveData::WriteData(IN PBYTE pBuffer, IN ULONG ulByteCount) {
    ASSERT(pBuffer);

    LARGE_INTEGER timeOut = { 0 };
    NTSTATUS ntStatus;
    ULONG offset;
    ULONG toWrite;
    ULONG w;
    
    if (m_fWriteDisabled) {
        return;
    }

    DPF_ENTER(("[CSaveData::WriteData ulByteCount=%lu]", ulByteCount));

    // Undersized (paranoia)
    if (0 == ulByteCount) {
        return;
    }

    // Oversized (paranoia)
    if (ulByteCount > (m_ulChunkSize * NUM_CHUNKS / 2)) {
        return;
    }

    BOOL dataWritten = false;
    if (g_UseRtpAc3) {
        dataWritten = WriteRtpAc3Data(pBuffer, ulByteCount);
    }
    else {
        dataWritten = WriteScreamData(pBuffer, ulByteCount);
    }

    // If I/O worker was done, relaunch it
    if (dataWritten) {
        ntStatus = KeWaitForSingleObject(&(m_pWorkItem->EventDone), Executive, KernelMode, FALSE, &timeOut);
        if (STATUS_SUCCESS == ntStatus) {
            m_pWorkItem->pSaveData = this;
            KeResetEvent(&(m_pWorkItem->EventDone));
            IoQueueWorkItem(m_pWorkItem->WorkItem, SendDataWorkerCallback, CriticalWorkQueue, (PVOID)m_pWorkItem);
        }
    }
} // WriteData
