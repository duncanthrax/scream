//-----------------------------------------------------------------------------
// Author - Marco Martinelli - https://github.com/martinellimarco
//-----------------------------------------------------------------------------

#pragma warning (disable : 4127)

#include "scream.h"
#include "ivshmemsavedata.h"

//=============================================================================
// Defines
//=============================================================================

#define MAGIC_NUM           0x11112014;

#pragma code_seg("PAGE")
//=============================================================================
// CIVSHMEMSaveData
//=============================================================================

//=============================================================================
BOOLEAN CIVSHMEMSaveData::RequestMMAP() {
	if (!m_ivshmem.devObj) {
		return FALSE;
	}

    IO_STATUS_BLOCK ioStatus = { 0 };
    KEVENT event;
    KeInitializeEvent(&event, NotificationEvent, FALSE);
    PIRP irp = IoBuildDeviceIoControlRequest(
        IOCTL_IVSHMEM_REQUEST_MMAP,
        m_ivshmem.devObj,
        NULL, 0,
        &m_ivshmem.mmap, sizeof(IVSHMEM_MMAP),
        FALSE,
        &event,
        &ioStatus
    );
    if (irp) {
        if (IoCallDriver(m_ivshmem.devObj, irp) == STATUS_PENDING) {
            KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
        }
        if (ioStatus.Status == 0) {
            return TRUE;
        }
        else {
            DPF_ENTER(("IVSHMEM: IRP for REQUEST_MMAP failed with status %u\n", ioStatus.Status));
        }
    }
    else {
        DPF_ENTER(("IVSHMEM: Failed to get the IRP for REQUEST_MMAP\n"));
    }
    return FALSE;
}

//=============================================================================
void CIVSHMEMSaveData::ReleaseMMAP() {
	if (!m_ivshmem.devObj) {
		return;
	}

    IO_STATUS_BLOCK ioStatus = { 0 };
    KEVENT event;
    KeInitializeEvent(&event, NotificationEvent, FALSE);
    PIRP irp = IoBuildDeviceIoControlRequest(
        IOCTL_IVSHMEM_RELEASE_MMAP,
        m_ivshmem.devObj,
        NULL, 0,
        NULL, 0,
        FALSE,
        &event,
        &ioStatus
    );
    if (irp) {
        if (IoCallDriver(m_ivshmem.devObj, irp) == STATUS_PENDING) {
            KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
        }
        if (ioStatus.Status != 0) {
            DPF_ENTER(("IVSHMEM: IRP for RELEASE_MMAP failed with status %u\n", ioStatus.Status));
        }
    }
    else {
        DPF_ENTER(("IVSHMEM: Failed to get the IRP for RELEASE_MMAP\n"));
    }
}

//=============================================================================
CIVSHMEMSaveData::CIVSHMEMSaveData() : m_pBuffer(NULL), m_ulOffset(0), m_ulSendOffset(0), m_fWriteDisabled(FALSE) {
    PAGED_CODE();

    DPF_ENTER(("[CIVSHMEMSaveData::CIVSHMEMSaveData]"));
    
    if (g_UseIVSHMEM) {
        // allocate work item for this stream
        m_pWorkItem = (PIVSHMEM_SAVEWORKER_PARAM)ExAllocatePoolWithTag(NonPagedPool, sizeof(IVSHMEM_SAVEWORKER_PARAM), MSVAD_POOLTAG);
        if (m_pWorkItem) {
            m_pWorkItem->WorkItem = IoAllocateWorkItem(GetDeviceObject());
            KeInitializeEvent(&(m_pWorkItem->EventDone), NotificationEvent, TRUE);
        }

        // Enumerate IVSHMEM devices
        GUID interfaceClassGuid = { 0xdf576976,0x569d,0x4672,0x95,0xa0,0xf5,0x7e,0x4e,0xa0,0xb2,0x10 };
        PWSTR symbolicLinkList;
        NTSTATUS ntStatus;
        ntStatus = IoGetDeviceInterfaces(
            &interfaceClassGuid,
            NULL,
            0,
            &symbolicLinkList
        );

        if (NT_SUCCESS(ntStatus) && NULL != symbolicLinkList) {
            PFILE_OBJECT fileObj;
            PDEVICE_OBJECT devObj;
            UNICODE_STRING objName;

            KEVENT event;
            IO_STATUS_BLOCK ioStatus = { 0 };

            PIRP irp;
            UINT64 ivshmemSize = 0;
            
            for (PWSTR symbolicLink = symbolicLinkList;
                symbolicLink[0] != NULL && symbolicLink[1] != NULL;
                symbolicLink += wcslen(symbolicLink) + 1) {
                RtlInitUnicodeString(&objName, symbolicLink),

                ntStatus = IoGetDeviceObjectPointer(
                    &objName,
                    FILE_ALL_ACCESS,
                    &fileObj,
                    &devObj
                );
                if (NT_SUCCESS(ntStatus)) {
                    ObDereferenceObject(fileObj);
                    ntStatus = ObReferenceObjectByPointer(devObj, FILE_ALL_ACCESS, NULL, KernelMode);
                    if (NT_SUCCESS(ntStatus)) {
                        ivshmemSize = 0;
                        KeInitializeEvent(&event, NotificationEvent, FALSE);
                        irp = IoBuildDeviceIoControlRequest(
                            IOCTL_IVSHMEM_REQUEST_SIZE,
                            devObj,
                            NULL, 0,
                            &ivshmemSize, sizeof(ivshmemSize),
                            FALSE,
                            &event,
                            &ioStatus
                        );
                        if (irp) {
                            ntStatus = IoCallDriver(devObj, irp);
                            if (ntStatus == STATUS_PENDING) {
                                KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
                            }
                            if (ioStatus.Status == 0 && ivshmemSize == g_UseIVSHMEM*1024*1024) { //we suppose that we are the only g_UseIVSHMEM*1MiB IVSHMEM device around
                                DPF_ENTER(("IVSHMEM Device Detected!\n"));
                                
                                RtlZeroMemory(&m_ivshmem, sizeof(IVSHMEM_OBJECT));
                                m_ivshmem.devObj = devObj;
                                // we have found a suitable device, we skip the others, and we DON'T dereference devObj, it will be dereferenced in the destructor
                                break;
                            }
                        }
                        else {
                            DPF_ENTER(("IVSHMEM: Failed to get the IRP for REQUEST_SIZE\n"));
                        }
                    }

                    ObDereferenceObject(devObj);
                }
            }
        }

        ExFreePool(symbolicLinkList);
    }
} // CIVSHMEMSaveData

//=============================================================================
CIVSHMEMSaveData::~CIVSHMEMSaveData() {
    PAGED_CODE();

    DPF_ENTER(("[CIVSHMEMSaveData::~CIVSHMEMSaveData]"));
    if (g_UseIVSHMEM) {
        // frees the work item
        if (m_pWorkItem->WorkItem != NULL) {
            IoFreeWorkItem(m_pWorkItem->WorkItem);
            m_pWorkItem->WorkItem = NULL;
        }

        //release mmap
        if (m_ivshmem.initialized) {
            m_ivshmem.initialized = false;
            if (RequestMMAP()) {
                //set to 0 the header so the receiver know we terminated
                RtlZeroMemory(m_ivshmem.mmap.ptr, sizeof(IVSHMEM_SCREAM_HEADER));
            }
            ReleaseMMAP();
        }
        if (m_ivshmem.devObj) {
            ObDereferenceObject(m_ivshmem.devObj);
        }

        if (m_pBuffer) {
            ExFreePoolWithTag(m_pBuffer, MSVAD_POOLTAG);
        }
    }
} // CIVSHMEMSaveData

//=============================================================================
void CIVSHMEMSaveData::DestroyWorkItems(void) {
    PAGED_CODE();
    
    DPF_ENTER(("[CIVSHMEMSaveData::DestroyWorkItems]"));

    if (m_pWorkItem) {
        ExFreePoolWithTag(m_pWorkItem, MSVAD_POOLTAG);
        m_pWorkItem = NULL;
    }

} // DestroyWorkItems

//=============================================================================
void CIVSHMEMSaveData::Disable(BOOL fDisable) {
    PAGED_CODE();

    m_fWriteDisabled = fDisable;
} // Disable

//=============================================================================
NTSTATUS CIVSHMEMSaveData::SetDeviceObject(IN PDEVICE_OBJECT DeviceObject) {
    PAGED_CODE();

    ASSERT(DeviceObject);

    NTSTATUS ntStatus = STATUS_SUCCESS;
    
    m_pDeviceObject = DeviceObject;
    return ntStatus;
}

//=============================================================================
PDEVICE_OBJECT CIVSHMEMSaveData::GetDeviceObject(void) {
    PAGED_CODE();

    return m_pDeviceObject;
}

#pragma code_seg("PAGE")
//=============================================================================
NTSTATUS CIVSHMEMSaveData::Initialize(DWORD nSamplesPerSec, WORD wBitsPerSample, WORD nChannels, DWORD dwChannelMask) {
    PAGED_CODE();

    NTSTATUS          ntStatus = STATUS_SUCCESS;

    DPF_ENTER(("[CIVSHMEMSaveData::Initialize]"));

    m_ivshmem.chunkSize = (UINT32)((wBitsPerSample>>3)*nChannels*nSamplesPerSec/50);
    if (RequestMMAP()) {
        PIVSHMEM_SCREAM_HEADER hdr = (PIVSHMEM_SCREAM_HEADER)m_ivshmem.mmap.ptr;

        RtlZeroMemory(hdr, sizeof(IVSHMEM_SCREAM_HEADER));
        m_ivshmem.writeIdx = 0;
        hdr->offset = m_ivshmem.offset = (UINT8)sizeof(IVSHMEM_SCREAM_HEADER);
        hdr->chunkSize = m_ivshmem.chunkSize;
        hdr->maxChunks = m_ivshmem.maxChunks = (UINT16)(m_ivshmem.mmap.size / m_ivshmem.chunkSize);
        m_ivshmem.bufferSize = m_ivshmem.chunkSize*m_ivshmem.maxChunks;

        // Only multiples of 44100 and 48000 are supported
        hdr->sampleRate = (UINT8)((nSamplesPerSec % 44100) ? (0 + (nSamplesPerSec / 48000)) : (128 + (nSamplesPerSec / 44100)));
        hdr->sampleSize = (UINT8)(wBitsPerSample);
        hdr->channels = (UINT8)(nChannels);
        hdr->channelMap = (UINT16)(dwChannelMask);

        hdr->magic = MAGIC_NUM;
        m_ivshmem.initialized = true;
    }
    else {
        m_ivshmem.initialized = false;
    }
    ReleaseMMAP();

    // Allocate memory for data buffer.
    if (NT_SUCCESS(ntStatus)) {
        m_pBuffer = (PBYTE)ExAllocatePoolWithTag(NonPagedPool, m_ivshmem.bufferSize, MSVAD_POOLTAG);
        if (!m_pBuffer) {
            DPF(D_TERSE, ("[Could not allocate memory for sending data]"));
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    return ntStatus;
} // Initialize

//=============================================================================
IO_WORKITEM_ROUTINE IVSHMEMSendDataWorkerCallback;

VOID IVSHMEMSendDataWorkerCallback(PDEVICE_OBJECT pDeviceObject, IN  PVOID  Context) {
    UNREFERENCED_PARAMETER(pDeviceObject);

    PAGED_CODE();

    ASSERT(Context);
    
    PIVSHMEM_SAVEWORKER_PARAM pParam = (PIVSHMEM_SAVEWORKER_PARAM) Context;
    PCIVSHMEMSaveData        pSaveData;

    ASSERT(pParam->pSaveData);

    if (pParam->WorkItem) {
        pSaveData = pParam->pSaveData;
        pSaveData->IVSHMEMSendData();
    }

    KeSetEvent(&(pParam->EventDone), 0, FALSE);
} // SendDataWorkerCallback

//=============================================================================
void CIVSHMEMSaveData::IVSHMEMSendData() {
    ULONG storeOffset;
    PBYTE mmap = NULL;
    if (RequestMMAP()) {
        mmap = (PBYTE)m_ivshmem.mmap.ptr + m_ivshmem.offset;
    }

    PIVSHMEM_SCREAM_HEADER hdr = (PIVSHMEM_SCREAM_HEADER)m_ivshmem.mmap.ptr;
    
    while (1) {
        // Read latest storeOffset. There might be new data.
        storeOffset = m_ulOffset;

        // Abort if there's nothing to send. Note: When storeOffset < sendOffset, we can always send a chunk.
        if ((storeOffset >= m_ulSendOffset) && ((storeOffset - m_ulSendOffset) < m_ivshmem.chunkSize))
            break;

        // Send a chunk
        if (mmap) {
            if (++m_ivshmem.writeIdx >= m_ivshmem.maxChunks) {
                m_ivshmem.writeIdx = 0;
            }
            RtlCopyMemory(&mmap[m_ivshmem.writeIdx*m_ivshmem.chunkSize], &m_pBuffer[m_ulSendOffset], m_ivshmem.chunkSize);
            hdr->writeIdx = m_ivshmem.writeIdx;
        }

        m_ulSendOffset += m_ivshmem.chunkSize; if (m_ulSendOffset >= m_ivshmem.bufferSize) m_ulSendOffset = 0;
    }
    ReleaseMMAP();
}

#pragma code_seg("PAGE")
//=============================================================================
void CIVSHMEMSaveData::WaitAllWorkItems(void) {
    PAGED_CODE();

    DPF_ENTER(("[CIVSHMEMSaveData::WaitAllWorkItems]"));

    DPF(D_VERBOSE, ("[Waiting for WorkItem]"));
    KeWaitForSingleObject(&(m_pWorkItem->EventDone), Executive, KernelMode, FALSE, NULL);
    
} // WaitAllWorkItems

#pragma code_seg()
//=============================================================================
void CIVSHMEMSaveData::WriteData(IN PBYTE pBuffer, IN ULONG ulByteCount) {
    ASSERT(pBuffer);

    if (!m_ivshmem.initialized) {
        return;
    }

    LARGE_INTEGER timeOut = { 0 };
    NTSTATUS ntStatus;
    ULONG offset;
    ULONG toWrite;
    ULONG w;

    if (m_fWriteDisabled) {
        return;
    }

    //DPF_ENTER(("[CIVSHMEMSaveData::WriteData ulByteCount=%lu]", ulByteCount));

    // Undersized (paranoia)
    if (0 == ulByteCount) {
        return;
    }

    // Oversized (paranoia)
    if (ulByteCount > (m_ivshmem.bufferSize / 2)) {
        return;
    }

    // Append to ring buffer. Don't write intermediate states to m_ulOffset,
    // but update it once at the end.
    offset = m_ulOffset;
    toWrite = ulByteCount;
    while (toWrite > 0) {
        w = offset % m_ivshmem.chunkSize;
        if (w > 0) {
            // Fill up last chunk
            w = (m_ivshmem.chunkSize - w);
            w = (toWrite < w) ? toWrite : w;
            RtlCopyMemory(&(m_pBuffer[offset]), &(pBuffer[ulByteCount - toWrite]), w);
        }
        else {
            // Start a new chunk
            w = ((m_ivshmem.bufferSize - offset) < toWrite) ? (m_ivshmem.bufferSize - offset) : toWrite;
            w = (w > m_ivshmem.chunkSize) ? m_ivshmem.chunkSize : w;
            RtlCopyMemory(&(m_pBuffer[offset]), &(pBuffer[ulByteCount - toWrite]), w);
        }
        toWrite -= w;
        offset += w;  if (offset >= m_ivshmem.bufferSize) offset = 0;
    }
    m_ulOffset = offset;

    // If I/O worker was done, relaunch it
    ntStatus = KeWaitForSingleObject(&(m_pWorkItem->EventDone), Executive, KernelMode, FALSE, &timeOut);
    if (STATUS_SUCCESS == ntStatus) {
        m_pWorkItem->pSaveData = this;
        KeResetEvent(&(m_pWorkItem->EventDone));
        IoQueueWorkItem(m_pWorkItem->WorkItem, IVSHMEMSendDataWorkerCallback, CriticalWorkQueue, (PVOID)m_pWorkItem);
    }
} // WriteData
