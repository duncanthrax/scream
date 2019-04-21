/*++
Copyright (c) 1997-2000  Microsoft Corporation All Rights Reserved

Module Name:
    minstream.cpp

Abstract:
    Implementation of wavecyclic stream.
--*/

#pragma warning (disable : 4127)

#include "scream.h"
#include "common.h"
#include "minwave.h"
#include "minstream.h"
#include "wavtable.h"

// #pragma code_seg("PAGE")

//=============================================================================
// CMiniportWaveStreamCyclic
//=============================================================================
CMiniportWaveCyclicStream::CMiniportWaveCyclicStream(PUNKNOWN other) : CUnknown(other)
{
    // PAGED_CODE();

    m_pMiniport = NULL;
    m_fCapture = FALSE;
    m_fFormat16Bit = FALSE;
    m_usBlockAlign = 0;
    m_ksState = KSSTATE_STOP;
    m_ulPin = (ULONG)-1;

    m_pDpc = NULL;
    m_pTimer = NULL;

    m_fDmaActive = FALSE;
    m_ulDmaPosition = 0;
    m_ullElapsedTimeCarryForward = 0;
    m_ulByteDisplacementCarryForward = 0;
    m_pvDmaBuffer = NULL;
    m_ulDmaBufferSize = 0;
    m_ulDmaMovementRate = 0;
    m_ullDmaTimeStamp = 0;
}

//=============================================================================
CMiniportWaveCyclicStream::~CMiniportWaveCyclicStream(void)
/*++
Routine Description:
  Destructor for wavecyclicstream 

Arguments:

Return Value:
  NT status code.
--*/
{
    // PAGED_CODE();

    DPF_ENTER(("[CMiniportWaveCyclicStream::~CMiniportWaveCyclicStream]"));

    if (m_pTimer) {
        KeCancelTimer(m_pTimer);
        ExFreePoolWithTag(m_pTimer, MSVAD_POOLTAG);
    }

    if (m_pDpc) {
        ExFreePoolWithTag( m_pDpc, MSVAD_POOLTAG );
    }

    // Free the DMA buffer
    FreeBuffer();

    if (NULL != m_pMiniport) {
        if (m_fCapture) {
            m_pMiniport->m_fCaptureAllocated = FALSE;
        } else {
            m_pMiniport->m_fRenderAllocated = FALSE;
        }
    }
} // ~CMiniportWaveCyclicStream

//=============================================================================
NTSTATUS CMiniportWaveCyclicStream::Init( 
    IN PCMiniportWaveCyclic         Miniport_,
    IN ULONG                        Pin_,
    IN BOOLEAN                      Capture_,
    IN PKSDATAFORMAT                DataFormat_
)
/*++
Routine Description:
  Initializes the stream object. Allocate a DMA buffer, timer and DPC

Arguments:
  Miniport_ -
  Pin_ -
  Capture_ -
  DataFormat -
  DmaChannel_ -

Return Value:
  NT status code.
--*/
{
    // PAGED_CODE();

    ASSERT(Miniport_);
    ASSERT(DataFormat_);


    NTSTATUS      ntStatus = STATUS_SUCCESS;
    PWAVEFORMATEX pWfx;

    pWfx = GetWaveFormatEx(DataFormat_);
    if (!pWfx) {
        DPF(D_TERSE, ("Invalid DataFormat param in NewStream"));
        ntStatus = STATUS_INVALID_PARAMETER;
    }

    if (NT_SUCCESS(ntStatus)) {
        m_pMiniport                       = Miniport_;

        m_ulPin                           = Pin_;
        m_fCapture                        = Capture_;
        m_usBlockAlign                    = pWfx->nBlockAlign;
        m_fFormat16Bit                    = (pWfx->wBitsPerSample == 16);
        m_ksState                         = KSSTATE_STOP;
        m_ulDmaPosition                   = 0;
        m_ullElapsedTimeCarryForward      = 0;
        m_ulByteDisplacementCarryForward  = 0;
        m_fDmaActive                      = FALSE;
        m_pDpc                            = NULL;
        m_pTimer                          = NULL;
        m_pvDmaBuffer                     = NULL;
        // If this is not the capture stream, create the output file.
        if (!m_fCapture) {
            if (NT_SUCCESS(ntStatus)) {
                DWORD dwChannelMask = KSAUDIO_SPEAKER_STEREO;
                if ((pWfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) && (pWfx->cbSize == CB_EXTENSIBLE)) {
                    WAVEFORMATEXTENSIBLE* pWfxT = (WAVEFORMATEXTENSIBLE*)pWfx;
                    dwChannelMask = pWfxT->dwChannelMask;
                }
                if (g_UseIVSHMEM) {
                    ntStatus = m_IVSHMEMSaveData.Initialize(pWfx->nSamplesPerSec, pWfx->wBitsPerSample, pWfx->nChannels, dwChannelMask);
                }
                else {
                    ntStatus = m_SaveData.Initialize(pWfx->nSamplesPerSec, pWfx->wBitsPerSample, pWfx->nChannels, dwChannelMask);
                }
            }
        }
    }

    // Allocate DMA buffer for this stream.
    if (NT_SUCCESS(ntStatus)) {
        ntStatus = AllocateBuffer(m_pMiniport->m_MaxDmaBufferSize, NULL);
    }

    // Set sample frequency. Note that m_SampleRateSync access should
    // be syncronized.
    if (NT_SUCCESS(ntStatus)) {
        ntStatus = KeWaitForSingleObject(&m_pMiniport->m_SampleRateSync, Executive, KernelMode, FALSE, NULL);
        if (STATUS_SUCCESS == ntStatus) {
            m_pMiniport->m_SamplingFrequency = pWfx->nSamplesPerSec;
            KeReleaseMutex(&m_pMiniport->m_SampleRateSync, FALSE);
        } else {
            DPF(D_TERSE, ("[SamplingFrequency Sync failed: %08X]", ntStatus));
        }
    }

    if (NT_SUCCESS(ntStatus)) {
        ntStatus = SetFormat(DataFormat_);
    }

    if (NT_SUCCESS(ntStatus)) {
        m_pDpc = (PRKDPC) ExAllocatePoolWithTag(NonPagedPool, sizeof(KDPC), MSVAD_POOLTAG);
        if (!m_pDpc) {
            DPF(D_TERSE, ("[Could not allocate memory for DPC]"));
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    if (NT_SUCCESS(ntStatus)) {
        m_pTimer = (PKTIMER) ExAllocatePoolWithTag(NonPagedPool, sizeof(KTIMER), MSVAD_POOLTAG);
        if (!m_pTimer) {
            DPF(D_TERSE, ("[Could not allocate memory for Timer]"));
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    if (NT_SUCCESS(ntStatus)) {
        KeInitializeDpc(m_pDpc, TimerNotify, m_pMiniport);
        KeInitializeTimerEx(m_pTimer, NotificationTimer);
    }

    return ntStatus;
} // Init

//=============================================================================
STDMETHODIMP_(NTSTATUS) CMiniportWaveCyclicStream::NonDelegatingQueryInterface( 
    IN  REFIID  Interface,
    OUT PVOID * Object 
)
/*++
Routine Description:
  QueryInterface

Arguments:
  Interface - GUID
  Object - interface pointer to be returned

Return Value:
  NT status code.
--*/
{
    // PAGED_CODE();

    ASSERT(Object);

    if (IsEqualGUIDAligned(Interface, IID_IUnknown)) {
        *Object = PVOID(PUNKNOWN(PMINIPORTWAVECYCLICSTREAM(this)));
    } else if (IsEqualGUIDAligned(Interface, IID_IMiniportWaveCyclicStream)) {
        *Object = PVOID(PMINIPORTWAVECYCLICSTREAM(this));
    } else if (IsEqualGUIDAligned(Interface, IID_IDmaChannel)) {
        *Object = PVOID(PDMACHANNEL(this));
    } else {
        *Object = NULL;
    }

    if (*Object) {
        PUNKNOWN(*Object)->AddRef();
        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_PARAMETER;
} // NonDelegatingQueryInterface

//=============================================================================
STDMETHODIMP CMiniportWaveCyclicStream::GetPosition(
    OUT PULONG                  Position
)
/*++
Routine Description:
  The GetPosition function gets the current position of the DMA read or write
  pointer for the stream. Callers of GetPosition should run at
  IRQL <= DISPATCH_LEVEL.

Arguments:
  Position - Position of the DMA pointer

Return Value:
  NT status code.
--*/
{
    if (m_fDmaActive) {
        // Get the current time
        ULONGLONG CurrentTime = KeQueryInterruptTime();

        // Calculate the time elapsed since the last call to GetPosition() or since the
        // DMA engine started.  Note that the division by 10000 to convert to milliseconds
        // may cause us to lose some of the time, so we will carry the remainder forward 
        // to the next GetPosition() call.
        ULONG TimeElapsedInMS = ((ULONG) (CurrentTime - m_ullDmaTimeStamp + m_ullElapsedTimeCarryForward)) / 10000;

        // Carry forward the remainder of this division so we don't fall behind with our position.
        m_ullElapsedTimeCarryForward = (CurrentTime - m_ullDmaTimeStamp + m_ullElapsedTimeCarryForward) % 10000;

        // Calculate how many bytes in the DMA buffer would have been processed in the elapsed
        // time.  Note that the division by 1000 to convert to milliseconds may cause us to 
        // lose some bytes, so we will carry the remainder forward to the next GetPosition() call.
        ULONG ByteDisplacement = ((m_ulDmaMovementRate * TimeElapsedInMS) + m_ulByteDisplacementCarryForward) / 1000;

        // Carry forward the remainder of this division so we don't fall behind with our position.
        m_ulByteDisplacementCarryForward = ((m_ulDmaMovementRate * TimeElapsedInMS) + m_ulByteDisplacementCarryForward) % 1000;

        // Increment the DMA position by the number of bytes displaced since the last
        // call to GetPosition() and ensure we properly wrap at buffer length.
        m_ulDmaPosition = (m_ulDmaPosition + ByteDisplacement) % m_ulDmaBufferSize;

        // Return the new DMA position
        *Position = m_ulDmaPosition;

        // Update the DMA time stamp for the next call to GetPosition()
        m_ullDmaTimeStamp = CurrentTime;
    } else {
        // DMA is inactive so just return the current DMA position.
        *Position = m_ulDmaPosition;
    }

    return STATUS_SUCCESS;
} // GetPosition

//=============================================================================
STDMETHODIMP CMiniportWaveCyclicStream::NormalizePhysicalPosition(
    IN OUT PLONGLONG            PhysicalPosition
)
/*++
Routine Description:
  Given a physical position based on the actual number of bytes transferred,
  NormalizePhysicalPosition converts the position to a time-based value of
  100 nanosecond units. Callers of NormalizePhysicalPosition can run at any IRQL.

Arguments:
  PhysicalPosition - On entry this variable contains the value to convert.
                     On return it contains the converted value

Return Value:
  NT status code.
--*/
{
    ASSERT(PhysicalPosition);

    *PhysicalPosition = (_100NS_UNITS_PER_SECOND / m_usBlockAlign * *PhysicalPosition) / m_pMiniport->m_SamplingFrequency;
    
    return STATUS_SUCCESS;
} // NormalizePhysicalPosition

//=============================================================================
STDMETHODIMP_(NTSTATUS) CMiniportWaveCyclicStream::SetFormat(
    IN  PKSDATAFORMAT           Format
)
/*++
Routine Description:
  The SetFormat function changes the format associated with a stream.
  Callers of SetFormat should run at IRQL PASSIVE_LEVEL

Arguments:
  Format - Pointer to a KSDATAFORMAT structure which indicates the new format
           of the stream.

Return Value:
  NT status code.
--*/
{
    // PAGED_CODE();

    ASSERT(Format);

    DPF_ENTER(("[CMiniportWaveCyclicStream::SetFormat]"));

    NTSTATUS      ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    PWAVEFORMATEX pWfx;

    if (m_ksState != KSSTATE_RUN) {
        // MSVAD does not validate the format.
        pWfx = GetWaveFormatEx(Format);
        if (pWfx) {
            ntStatus = KeWaitForSingleObject(&m_pMiniport->m_SampleRateSync, Executive, KernelMode, FALSE, NULL);
            if (STATUS_SUCCESS == ntStatus) {
                m_usBlockAlign = pWfx->nBlockAlign;
                m_fFormat16Bit = (pWfx->wBitsPerSample == 16);
                m_pMiniport->m_SamplingFrequency = pWfx->nSamplesPerSec;
                m_ulDmaMovementRate = pWfx->nAvgBytesPerSec;

                DPF(D_TERSE, ("New Format - SpS: %d - BpS: %d - aBypS: %d - C: %d", pWfx->nSamplesPerSec, pWfx->wBitsPerSample, pWfx->nAvgBytesPerSec, pWfx->nChannels));
            }

            KeReleaseMutex(&m_pMiniport->m_SampleRateSync, FALSE);
        }
    }

    return ntStatus;
} // SetFormat

//=============================================================================
STDMETHODIMP_(ULONG) CMiniportWaveCyclicStream::SetNotificationFreq(
    IN  ULONG                   Interval,
    OUT PULONG                  FramingSize
)
/*++
Routine Description:
  The SetNotificationFrequency function sets the frequency at which
  notification interrupts are generated. Callers of SetNotificationFrequency
  should run at IRQL PASSIVE_LEVEL.

Arguments:
  Interval - Value indicating the interval between interrupts,
             expressed in milliseconds
  FramingSize - Pointer to a ULONG value where the number of bytes equivalent
                to Interval milliseconds is returned

Return Value:
  NT status code.
--*/
{
    // PAGED_CODE();

    ASSERT(FramingSize);

    DPF_ENTER(("[CMiniportWaveCyclicStream::SetNotificationFreq]"));

    m_pMiniport->m_NotificationInterval = Interval;

    *FramingSize = m_usBlockAlign * m_pMiniport->m_SamplingFrequency * Interval / 1000;

    return m_pMiniport->m_NotificationInterval;
} // SetNotificationFreq

//=============================================================================
STDMETHODIMP CMiniportWaveCyclicStream::SetState(
    IN  KSSTATE                 NewState
)
/*++
Routine Description:
  The SetState function sets the new state of playback or recording for the
  stream. SetState should run at IRQL PASSIVE_LEVEL

Arguments:
  NewState - KSSTATE indicating the new state for the stream.

Return Value:
  NT status code.
--*/
{
    // PAGED_CODE();

    DPF_ENTER(("[CMiniportWaveCyclicStream::SetState]"));

    NTSTATUS ntStatus = STATUS_SUCCESS;

    // The acquire state is not distinguishable from the stop state for our
    // purposes.
    if (NewState == KSSTATE_ACQUIRE) {
        NewState = KSSTATE_STOP;
    }

    if (m_ksState != NewState) {
        switch(NewState) {
            case KSSTATE_PAUSE:
                DPF(D_TERSE, ("KSSTATE_PAUSE"));
                
                m_fDmaActive = FALSE;
                break;

            case KSSTATE_RUN:
                DPF(D_TERSE, ("KSSTATE_RUN"));

                LARGE_INTEGER   delay;

                // Set the timer for DPC.
                m_ullDmaTimeStamp             = KeQueryInterruptTime();
                m_ullElapsedTimeCarryForward  = 0;
                m_fDmaActive                  = TRUE;
                delay.HighPart                = 0;
                delay.LowPart                 = m_pMiniport->m_NotificationInterval;

                KeSetTimerEx(m_pTimer, delay, m_pMiniport->m_NotificationInterval, m_pDpc);
                break;

            case KSSTATE_STOP:
                DPF(D_TERSE, ("KSSTATE_STOP"));
    
                m_fDmaActive                      = FALSE;
                m_ulDmaPosition                   = 0;
                m_ullElapsedTimeCarryForward      = 0;
                m_ulByteDisplacementCarryForward  = 0;
    
                KeCancelTimer(m_pTimer);
    
                // Wait until all work items are completed.
                if (!m_fCapture) {
                    if (g_UseIVSHMEM) {
                        m_IVSHMEMSaveData.WaitAllWorkItems();
                    }
                    else {
                        m_SaveData.WaitAllWorkItems();
                    }
                }
                break;
        }

        m_ksState = NewState;
    }

    return ntStatus;
} // SetState

//=============================================================================
STDMETHODIMP_(void) CMiniportWaveCyclicStream::Silence(
    __out_bcount(ByteCount) PVOID Buffer,
    IN ULONG                    ByteCount
)
/*++
Routine Description:
  The Silence function is used to copy silence samplings to a certain location.
  Callers of Silence can run at any IRQL

Arguments:
  Buffer - Pointer to the buffer where the silence samplings should
           be deposited.
  ByteCount - Size of buffer indicating number of bytes to be deposited.

Return Value:
  NT status code.
--*/
{
    RtlFillMemory(Buffer, ByteCount, m_fFormat16Bit ? 0 : 0x80);
} // Silence


// #pragma code_seg("PAGE")
//=============================================================================
STDMETHODIMP_(NTSTATUS) CMiniportWaveCyclicStream::AllocateBuffer(
    IN ULONG                    BufferSize,
    IN PPHYSICAL_ADDRESS        PhysicalAddressConstraint OPTIONAL
)
/*++
Routine Description:
  The AllocateBuffer function allocates a buffer associated with the DMA object.
  The buffer is nonPaged.
  Callers of AllocateBuffer should run at a passive IRQL.

Arguments:
  BufferSize - Size in bytes of the buffer to be allocated.
  PhysicalAddressConstraint - Optional constraint to place on the physical
                              address of the buffer. If supplied, only the bits
                              that are set in the constraint address may vary
                              from the beginning to the end of the buffer.
                              For example, if the desired buffer should not
                              cross a 64k boundary, the physical address
                              constraint 0x000000000000ffff should be specified

Return Value:
  NT status code.

--*/
{
    UNREFERENCED_PARAMETER(PhysicalAddressConstraint);

    // PAGED_CODE();

    DPF_ENTER(("[CMiniportWaveCyclicStream::AllocateBuffer]"));

    NTSTATUS ntStatus = STATUS_SUCCESS;

    // Adjust this cap as needed...
    ASSERT (BufferSize <= DMA_BUFFER_SIZE);

    m_pvDmaBuffer = (PVOID) ExAllocatePoolWithTag(NonPagedPool, BufferSize, MSVAD_POOLTAG);
    if (!m_pvDmaBuffer) {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    } else {
        m_ulDmaBufferSize = BufferSize;
    }

    return ntStatus;
} // AllocateBuffer
#pragma code_seg()

//=============================================================================
STDMETHODIMP_(ULONG) CMiniportWaveCyclicStream::AllocatedBufferSize(void)
/*++
Routine Description:
  AllocatedBufferSize returns the size of the allocated buffer.
  Callers of AllocatedBufferSize can run at any IRQL.

Arguments:

Return Value:
  ULONG
--*/
{
    DPF_ENTER(("[CMiniportWaveCyclicStream::AllocatedBufferSize]"));

    return m_ulDmaBufferSize;
} // AllocatedBufferSize

//=============================================================================
STDMETHODIMP_(ULONG) CMiniportWaveCyclicStream::BufferSize(void)
/*++
Routine Description:
  BufferSize returns the size set by SetBufferSize or the allocated buffer size
  if the buffer size has not been set. The DMA object does not actually use
  this value internally. This value is maintained by the object to allow its
  various clients to communicate the intended size of the buffer. This call
  is often used to obtain the map size parameter to the Start member
  function. Callers of BufferSize can run at any IRQL

Arguments:

Return Value:
  ULONG
--*/
{
    return m_ulDmaBufferSize;
} // BufferSize

//=============================================================================
STDMETHODIMP_(void) CMiniportWaveCyclicStream::CopyFrom(
    IN  PVOID                   Destination,
    IN  PVOID                   Source,
    IN  ULONG                   ByteCount
)
/*++
Routine Description:
  The CopyFrom function copies sample data from the DMA buffer.
  Callers of CopyFrom can run at any IRQL

Arguments:
  Destination - Points to the destination buffer.
  Source - Points to the source buffer.
  ByteCount - Points to the source buffer.

Return Value:
  void
--*/
{
    UNREFERENCED_PARAMETER(Destination);
    UNREFERENCED_PARAMETER(Source);
    UNREFERENCED_PARAMETER(ByteCount);
} // CopyFrom

//=============================================================================
STDMETHODIMP_(void) CMiniportWaveCyclicStream::CopyTo(
    IN  PVOID                   Destination,
    IN  PVOID                   Source,
    IN  ULONG                   ByteCount
)
/*++
Routine Description:
  The CopyTo function copies sample data to the DMA buffer.
  Callers of CopyTo can run at any IRQL.

Arguments:
  Destination - Points to the destination buffer.
  Source - Points to the source buffer
  ByteCount - Number of bytes to be copied

Return Value:
  void
--*/
{
    UNREFERENCED_PARAMETER(Destination);

    if (g_UseIVSHMEM) {
        m_IVSHMEMSaveData.WriteData((PBYTE)Source, ByteCount);
    }
    else {
        m_SaveData.WriteData((PBYTE)Source, ByteCount);
    }
} // CopyTo

//=============================================================================
// #pragma code_seg("PAGE")
STDMETHODIMP_(void) CMiniportWaveCyclicStream::FreeBuffer(void)
/*++
Routine Description:
  The FreeBuffer function frees the buffer allocated by AllocateBuffer. Because
  the buffer is automatically freed when the DMA object is deleted, this
  function is not normally used. Callers of FreeBuffer should run at
  IRQL PASSIVE_LEVEL.

Arguments:

Return Value:
  void
--*/
{
    // PAGED_CODE();

    DPF_ENTER(("[CMiniportWaveCyclicStream::FreeBuffer]"));

    if ( m_pvDmaBuffer ) {
        ExFreePoolWithTag( m_pvDmaBuffer, MSVAD_POOLTAG );
        m_ulDmaBufferSize = 0;
    }
} // FreeBuffer
#pragma code_seg()

//=============================================================================
STDMETHODIMP_(PADAPTER_OBJECT) CMiniportWaveCyclicStream::GetAdapterObject(void)
/*++
Routine Description:
  The GetAdapterObject function returns the DMA object's internal adapter
  object. Callers of GetAdapterObject can run at any IRQL.

Arguments:

Return Value:
  PADAPTER_OBJECT - The return value is the object's internal adapter object.
--*/
{
    DPF_ENTER(("[CMiniportWaveCyclicStream::GetAdapterObject]"));

    // MSVAD does not have need a physical DMA channel. Therefore it
    // does not have physical DMA structure.

    return NULL;
} // GetAdapterObject

//=============================================================================
STDMETHODIMP_(ULONG) CMiniportWaveCyclicStream::MaximumBufferSize(void)
/*++
Routine Description:

Arguments:

Return Value:
  NT status code.
--*/
{
    DPF_ENTER(("[CMiniportWaveCyclicStream::MaximumBufferSize]"));

    return m_pMiniport->m_MaxDmaBufferSize;
} // MaximumBufferSize

//=============================================================================
STDMETHODIMP_(PHYSICAL_ADDRESS) CMiniportWaveCyclicStream::PhysicalAddress(void)
/*++
Routine Description:
  MaximumBufferSize returns the size in bytes of the largest buffer this DMA
  object is configured to support. Callers of MaximumBufferSize can run
  at any IRQL

Arguments:

Return Value:
  PHYSICAL_ADDRESS - The return value is the size in bytes of the largest
                     buffer this DMA object is configured to support.
--*/
{
    DPF_ENTER(("[CMiniportWaveCyclicStream::PhysicalAddress]"));

    PHYSICAL_ADDRESS pAddress;
    pAddress.QuadPart = (LONGLONG) m_pvDmaBuffer;

    return pAddress;
} // PhysicalAddress

//=============================================================================
STDMETHODIMP_(void) CMiniportWaveCyclicStream::SetBufferSize(
    IN ULONG                    BufferSize
)
/*++
Routine Description:
  The SetBufferSize function sets the current buffer size. This value is set to
  the allocated buffer size when AllocateBuffer is called. The DMA object does
  not actually use this value internally. This value is maintained by the object
  to allow its various clients to communicate the intended size of the buffer.
  Callers of SetBufferSize can run at any IRQL.

Arguments:
  BufferSize - Current size in bytes.

Return Value:
  void
--*/
{
    DPF_ENTER(("[CMiniportWaveCyclicStream::SetBufferSize]"));

    if ( BufferSize <= m_ulDmaBufferSize ) {
        m_ulDmaBufferSize = BufferSize;
    } else {
        DPF(D_ERROR, ("Tried to enlarge dma buffer size"));
    }
} // SetBufferSize

//=============================================================================
STDMETHODIMP_(PVOID) CMiniportWaveCyclicStream::SystemAddress(void)
/*++
Routine Description:
  The SystemAddress function returns the virtual system address of the
  allocated buffer. Callers of SystemAddress can run at any IRQL.

Arguments:

Return Value:
  PVOID - The return value is the virtual system address of the
          allocated buffer.
--*/
{
    return m_pvDmaBuffer;
} // SystemAddress

//=============================================================================
STDMETHODIMP_(ULONG) CMiniportWaveCyclicStream::TransferCount(void)
/*++
Routine Description:
  The TransferCount function returns the size in bytes of the buffer currently
  being transferred by a DMA object. Callers of TransferCount can run
  at any IRQL.

Arguments:

Return Value:
  ULONG - The return value is the size in bytes of the buffer currently
          being transferred.
--*/
{
    DPF_ENTER(("[CMiniportWaveCyclicStream::TransferCount]"));

    return m_ulDmaBufferSize;
}
