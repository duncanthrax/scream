/*++

Copyright (c) 1997-2000  Microsoft Corporation All Rights Reserved

Module Name:

    minwave.cpp

Abstract:

    Implementation of wavecyclic miniport.

--*/

#pragma warning (disable : 4127)

#include "scream.h"
#include "common.h"
#include "minstream.h"
#include "minwave.h"
#include "wavtable.h"

// #pragma code_seg("PAGE")

//=============================================================================
// CMiniportWaveCyclic
//=============================================================================

//=============================================================================
NTSTATUS CreateMiniportWaveCyclicMSVAD( 
    OUT PUNKNOWN *              Unknown,
    IN  REFCLSID,
    IN  PUNKNOWN                UnknownOuter OPTIONAL,
    IN  POOL_TYPE               PoolType 
)
/*++
Routine Description:
  Create the wavecyclic miniport.

Arguments:
  Unknown - 
  RefClsId -
  UnknownOuter -
  PoolType -

Return Value:
  NT status code.
--*/
{
    ASSERT(Unknown);

    STD_CREATE_BODY(CMiniportWaveCyclic, Unknown, UnknownOuter, PoolType);
}

//=============================================================================
CMiniportWaveCyclic::CMiniportWaveCyclic(PUNKNOWN other) : CUnknown(other)
/*++
Routine Description:
  Constructor for wavecyclic miniport.

Arguments:

Return Value:
--*/
{
    DPF_ENTER(("[CMiniportWaveCyclic::CMiniportWaveCyclic]"));

    // Initialize members.
    m_AdapterCommon        = NULL;
    m_Port                 = NULL;
    m_FilterDescriptor     = NULL;

    m_NotificationInterval = 0;
    m_SamplingFrequency    = 0;

    m_ServiceGroup         = NULL;
    m_MaxDmaBufferSize     = DMA_BUFFER_SIZE;

    m_MaxOutputStreams     = 0;
    m_MaxInputStreams      = 0;
    m_MaxTotalStreams      = 0;

    m_MinChannels          = 0;
    m_MaxChannelsPcm       = 0;
    m_MinBitsPerSamplePcm  = 0;
    m_MaxBitsPerSamplePcm  = 0;
    m_MinSampleRatePcm     = 0;
    m_MaxSampleRatePcm     = 0;

} // CMiniportWaveCyclic

//=============================================================================
CMiniportWaveCyclic::~CMiniportWaveCyclic(void)
/*++
Routine Description:
  Destructor for wavecyclic miniport

Arguments:

Return Value:
  NT status code.
--*/
{
    // PAGED_CODE();

    DPF_ENTER(("[CMiniportWaveCyclic::~CMiniportWaveCyclic]"));
    
    if (m_Port) {
        m_Port->Release();
    }

    if (m_ServiceGroup) {
        m_ServiceGroup->Release();
    }

    if (m_AdapterCommon) {
        m_AdapterCommon->Release();
    }
} // ~CMiniportWaveCyclic


//=============================================================================
STDMETHODIMP_(NTSTATUS) CMiniportWaveCyclic::DataRangeIntersection( 
    IN  ULONG                       PinId,
    IN  PKSDATARANGE                ClientDataRange,
    IN  PKSDATARANGE                MyDataRange,
    IN  ULONG                       OutputBufferLength,
    OUT PVOID                       ResultantFormat,
    OUT PULONG                      ResultantFormatLength 
)
/*++
Routine Description:
  The DataRangeIntersection function determines the highest quality 
  intersection of two data ranges.

Arguments:
  PinId -           Pin for which data intersection is being determined. 
  ClientDataRange - Pointer to KSDATARANGE structure which contains the data 
                    range submitted by client in the data range intersection 
                    property request. 
  MyDataRange -         Pin's data range to be compared with client's data 
                        range. In this case we actually ignore our own data 
                        range, because we know that we only support one range.
  OutputBufferLength -  Size of the buffer pointed to by the resultant format 
                        parameter. 
  ResultantFormat -     Pointer to value where the resultant format should be 
                        returned. 
  ResultantFormatLength -   Actual length of the resultant format placed in 
                            ResultantFormat. This should be less than or equal 
                            to OutputBufferLength. 

Return Value:
  NT status code.
--*/
{
    UNREFERENCED_PARAMETER(PinId);
    UNREFERENCED_PARAMETER(ClientDataRange);
    UNREFERENCED_PARAMETER(MyDataRange);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(ResultantFormat);
    UNREFERENCED_PARAMETER(ResultantFormatLength);

    // PAGED_CODE();

    // This driver only supports PCM formats.
    // Portcls will handle the request for us.

    return STATUS_NOT_IMPLEMENTED;
} // DataRangeIntersection

//=============================================================================
STDMETHODIMP_(NTSTATUS) CMiniportWaveCyclic::GetDescription( 
    OUT PPCFILTER_DESCRIPTOR * OutFilterDescriptor 
)
/*++
Routine Description:
  The GetDescription function gets a pointer to a filter description. 
  It provides a location to deposit a pointer in miniport's description 
  structure. This is the placeholder for the FromNode or ToNode fields in 
  connections which describe connections to the filter's pins. 

Arguments:
  OutFilterDescriptor - Pointer to the filter description. 

Return Value:
  NT status code.

--*/
{
    // PAGED_CODE();

    ASSERT(OutFilterDescriptor);

    DPF_ENTER(("[CMiniportWaveCyclic::GetDescription]"));

    *OutFilterDescriptor = m_FilterDescriptor;

    return (STATUS_SUCCESS);

} // GetDescription

//=============================================================================
STDMETHODIMP_(NTSTATUS) CMiniportWaveCyclic::Init( 
    IN  PUNKNOWN                UnknownAdapter_,
    IN  PRESOURCELIST           ResourceList_,
    IN  PPORTWAVECYCLIC         Port_ 
)
/*++
Routine Description:
  The Init function initializes the miniport. Callers of this function 
  should run at IRQL PASSIVE_LEVEL

Arguments:
  UnknownAdapter - A pointer to the Iuknown interface of the adapter object. 
  ResourceList - Pointer to the resource list to be supplied to the miniport 
                 during initialization. The port driver is free to examine the 
                 contents of the ResourceList. The port driver will not be 
                 modify the ResourceList contents. 
  Port - Pointer to the topology port object that is linked with this miniport. 

Return Value:
  NT status code.
--*/
{
    UNREFERENCED_PARAMETER(ResourceList_);

    ASSERT(UnknownAdapter_);
    ASSERT(Port_);

    DPF_ENTER(("[CMiniportWaveCyclic::Init]"));

    m_MaxOutputStreams      = MAX_OUTPUT_STREAMS;
    m_MaxInputStreams       = MAX_INPUT_STREAMS;
    m_MaxTotalStreams       = MAX_TOTAL_STREAMS;

    m_MinChannels           = MIN_CHANNELS;
    m_MaxChannelsPcm        = MAX_CHANNELS_PCM;

    m_MinBitsPerSamplePcm   = MIN_BITS_PER_SAMPLE_PCM;
    m_MaxBitsPerSamplePcm   = MAX_BITS_PER_SAMPLE_PCM;
    m_MinSampleRatePcm      = MIN_SAMPLE_RATE;
    m_MaxSampleRatePcm      = MAX_SAMPLE_RATE;

    // AddRef() is required because we are keeping this pointer.
    m_Port = Port_;
    m_Port->AddRef();

    // We want the IAdapterCommon interface on the adapter common object,
    // which is given to us as a IUnknown.  The QueryInterface call gives us
    // an AddRefed pointer to the interface we want.
    //
    NTSTATUS ntStatus = UnknownAdapter_->QueryInterface(IID_IAdapterCommon, (PVOID *) &m_AdapterCommon);

    if (NT_SUCCESS(ntStatus)) {
        KeInitializeMutex(&m_SampleRateSync, 1);
        ntStatus = PcNewServiceGroup(&m_ServiceGroup, NULL);

        if (NT_SUCCESS(ntStatus)) {
            m_AdapterCommon->SetWaveServiceGroup(m_ServiceGroup);
        }
    }

    if (NT_SUCCESS(ntStatus)) {
        // Set filter descriptor.
        m_FilterDescriptor = &MiniportFilterDescriptor;

        m_fCaptureAllocated = FALSE;
        m_fRenderAllocated = FALSE;
    } else {
        // clean up AdapterCommon
        if (m_AdapterCommon) {
            // clean up the service group
            if (m_ServiceGroup) {
                m_AdapterCommon->SetWaveServiceGroup(NULL);
                m_ServiceGroup->Release();
                m_ServiceGroup = NULL;
            }

            m_AdapterCommon->Release();
            m_AdapterCommon = NULL;
        }

        // release the port
        //
        m_Port->Release();
        m_Port = NULL;
    }

    return ntStatus;
} // Init

//=============================================================================
STDMETHODIMP_(NTSTATUS) CMiniportWaveCyclic::NewStream( 
    OUT PMINIPORTWAVECYCLICSTREAM * OutStream,
    IN  PUNKNOWN                OuterUnknown,
    IN  POOL_TYPE               PoolType,
    IN  ULONG                   Pin,
    IN  BOOLEAN                 Capture,
    IN  PKSDATAFORMAT           DataFormat,
    OUT PDMACHANNEL *           OutDmaChannel,
    OUT PSERVICEGROUP *         OutServiceGroup 
)
/*++
Routine Description:
  The NewStream function creates a new instance of a logical stream 
  associated with a specified physical channel. Callers of NewStream should 
  run at IRQL PASSIVE_LEVEL.

Arguments:
  OutStream -
  OuterUnknown -
  PoolType - 
  Pin - 
  Capture - 
  DataFormat -
  OutDmaChannel -
  OutServiceGroup -

Return Value:
  NT status code.
--*/
{
    UNREFERENCED_PARAMETER(PoolType);

    // PAGED_CODE();

    ASSERT(OutStream);
    ASSERT(DataFormat);
    ASSERT(OutDmaChannel);
    ASSERT(OutServiceGroup);

    DPF_ENTER(("[CMiniportWaveCyclic::NewStream]"));

    NTSTATUS                    ntStatus = STATUS_SUCCESS;
    PCMiniportWaveCyclicStream  stream = NULL;

    // Check if we have enough streams.
    if (Capture) {
        if (m_fCaptureAllocated) {
            DPF(D_TERSE, ("[Only one capture stream supported]"));
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    } else {
        if (m_fRenderAllocated) {
            DPF(D_TERSE, ("[Only one render stream supported]"));
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    // Instantiate a stream. Stream must be in
    // NonPagedPool because of file saving.
    //
    if (NT_SUCCESS(ntStatus)) {
        stream = new (NonPagedPool, MSVAD_POOLTAG) CMiniportWaveCyclicStream(OuterUnknown);

        if (stream) {
            stream->AddRef();

            ntStatus = stream->Init(this, Pin, Capture, DataFormat);
        } else {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    if (NT_SUCCESS(ntStatus)) {
        if (Capture) {
            m_fCaptureAllocated = TRUE;
        } else {
            m_fRenderAllocated = TRUE;
        }

        *OutStream = PMINIPORTWAVECYCLICSTREAM(stream);
        (*OutStream)->AddRef();
        
        *OutDmaChannel = PDMACHANNEL(stream);
        (*OutDmaChannel)->AddRef();

        *OutServiceGroup = m_ServiceGroup;
        (*OutServiceGroup)->AddRef();

        // The stream, the DMA channel, and the service group have
        // references now for the caller.  The caller expects these
        // references to be there.
    }

    // This is our private reference to the stream.  The caller has
    // its own, so we can release in any case.
    if (stream) {
        stream->Release();
    }
    
    return ntStatus;
} // NewStream

//=============================================================================
STDMETHODIMP_(NTSTATUS) CMiniportWaveCyclic::NonDelegatingQueryInterface( 
    IN  REFIID  Interface,
    OUT PVOID * Object 
)
/*++
Routine Description:
  QueryInterface

Arguments:
  Interface - GUID
  Object - interface pointer to be returned.

Return Value:
  NT status code.
--*/
{
    // PAGED_CODE();

    ASSERT(Object);

    if (IsEqualGUIDAligned(Interface, IID_IUnknown)) {
        *Object = PVOID(PUNKNOWN(PMINIPORTWAVECYCLIC(this)));
    } else if (IsEqualGUIDAligned(Interface, IID_IMiniport)) {
        *Object = PVOID(PMINIPORT(this));
    } else if (IsEqualGUIDAligned(Interface, IID_IMiniportWaveCyclic)) {
        *Object = PVOID(PMINIPORTWAVECYCLIC(this));
    } else {
        *Object = NULL;
    }

    if (*Object) {
        // We reference the interface for the caller.

        PUNKNOWN(*Object)->AddRef();
        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_PARAMETER;
} // NonDelegatingQueryInterface

//=============================================================================
NTSTATUS CMiniportWaveCyclic::PropertyHandlerComponentId(
    IN PPCPROPERTY_REQUEST      PropertyRequest
)
/*++
Routine Description:
  Handles KSPROPERTY_GENERAL_COMPONENTID

Arguments:
  PropertyRequest - 

Return Value:
  NT status code.
--*/
{
    // PAGED_CODE();

    DPF_ENTER(("[PropertyHandlerComponentId]"));

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    if (PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT) {
        ntStatus = PropertyHandler_BasicSupport(PropertyRequest, KSPROPERTY_TYPE_BASICSUPPORT | KSPROPERTY_TYPE_GET, VT_ILLEGAL);
    } else {
        ntStatus = ValidatePropertyParams(PropertyRequest, sizeof(KSCOMPONENTID), 0);
        if (NT_SUCCESS(ntStatus)) {
            if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET) {
                PKSCOMPONENTID pComponentId = (PKSCOMPONENTID) PropertyRequest->Value;

                INIT_MMREG_MID(&pComponentId->Manufacturer, MM_MICROSOFT);
                pComponentId->Product   = PID_MSVAD;
                pComponentId->Name      = NAME_MSVAD_SIMPLE;
                pComponentId->Component = GUID_NULL; // Not used for extended caps.
                pComponentId->Version   = MSVAD_VERSION;
                pComponentId->Revision  = MSVAD_REVISION;

                PropertyRequest->ValueSize = sizeof(KSCOMPONENTID);
                ntStatus = STATUS_SUCCESS;
            }
        } else {
            DPF(D_TERSE, ("[PropertyHandlerComponentId - Invalid parameter]"));
            ntStatus = STATUS_INVALID_PARAMETER;
        }
    }

    return ntStatus;
} // PropertyHandlerComponentId

//=============================================================================
NTSTATUS CMiniportWaveCyclic::PropertyHandlerProposedFormat(
    IN PPCPROPERTY_REQUEST      PropertyRequest
)
/*++
Routine Description:
  Handles KSPROPERTY_GENERAL_COMPONENTID

Arguments:
  PropertyRequest - 

Return Value:
  NT status code.
--*/
{
    // PAGED_CODE();

    DPF_ENTER(("[PropertyHandlerProposedFormat]"));

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    if (PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT) {
        ntStatus = PropertyHandler_BasicSupport(PropertyRequest, KSPROPERTY_TYPE_BASICSUPPORT | KSPROPERTY_TYPE_SET, VT_ILLEGAL);
    } else {
        ULONG cbMinSize = sizeof(KSDATAFORMAT_WAVEFORMATEX);

        if (PropertyRequest->ValueSize == 0) {
            PropertyRequest->ValueSize = cbMinSize;
            ntStatus = STATUS_BUFFER_OVERFLOW;
        } else if (PropertyRequest->ValueSize < cbMinSize) {
            ntStatus = STATUS_BUFFER_TOO_SMALL;
        } else {
            if (PropertyRequest->Verb & KSPROPERTY_TYPE_SET) {
                KSDATAFORMAT_WAVEFORMATEX* pKsFormat = (KSDATAFORMAT_WAVEFORMATEX*)PropertyRequest->Value;
                ntStatus = STATUS_NO_MATCH;
                if ((pKsFormat->DataFormat.MajorFormat == KSDATAFORMAT_TYPE_AUDIO) && (pKsFormat->DataFormat.SubFormat == KSDATAFORMAT_SUBTYPE_PCM) && (pKsFormat->DataFormat.Specifier == KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)) {
                    WAVEFORMATEX* pWfx = (WAVEFORMATEX*)&pKsFormat->WaveFormatEx;
                    // We support from 1 to 8 channels at freq >= 44100, sampling size >= 16bits
                    if ( ((pWfx->wBitsPerSample == 16) || (pWfx->wBitsPerSample == 24) || (pWfx->wBitsPerSample == 32)) &&
                         ((pWfx->nSamplesPerSec == 44100) || (pWfx->nSamplesPerSec == 48000) || (pWfx->nSamplesPerSec == 96000) || (pWfx->nSamplesPerSec == 192000)) ) {
                        if ((pWfx->wFormatTag == WAVE_FORMAT_PCM) && (pWfx->cbSize == 0)) {
                            if ((pWfx->nChannels >= 1) && (pWfx->nChannels <= 8)) {
                                ntStatus = STATUS_SUCCESS;
                            }
                        }
                        else if ((pWfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) && (pWfx->cbSize == CB_EXTENSIBLE)) {
                            WAVEFORMATEXTENSIBLE* pWfxT = (WAVEFORMATEXTENSIBLE*)pWfx;
                            // The channel mask can be greater than 0x07FF, but only in exotic configurations.
                            // It's important that the number of channels is even, to fit in PCM_PAYLOAD_SIZE. Mono is an exception.
                            // This restriction doesn't apply if IVSHMEM is used, because the packet size is dynamic there.
                            if ((pWfx->nChannels >= 1) && (pWfx->nChannels <= 8) && (pWfxT->dwChannelMask <= 0x07FF) && (g_UseIVSHMEM || pWfx->nChannels == 1 || pWfx->nChannels % 2 == 0)) {
                                ntStatus = STATUS_SUCCESS;
                            }
                        }
                    }
                }
            } else {
                ntStatus = STATUS_INVALID_PARAMETER;
            }
        }
    }

    return ntStatus;
} // PropertyHandlerProposedFormat

//=============================================================================
NTSTATUS CMiniportWaveCyclic::PropertyHandlerCpuResources(
    IN  PPCPROPERTY_REQUEST     PropertyRequest
)
/*++
Routine Description:
  Processes KSPROPERTY_AUDIO_CPURESOURCES

Arguments:
  PropertyRequest - property request structure

Return Value:
  NT status code.
--*/
{
    // PAGED_CODE();

    ASSERT(PropertyRequest);

    DPF_ENTER(("[CMiniportWaveCyclic::PropertyHandlerCpuResources]"));

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    if (PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT) {
        ntStatus = PropertyHandler_BasicSupport(PropertyRequest, KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT, VT_I4);
    } else if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET) {
        ntStatus = ValidatePropertyParams(PropertyRequest, sizeof(LONG), 0);
        if (NT_SUCCESS(ntStatus)) {
            *(PLONG(PropertyRequest->Value)) = KSAUDIO_CPU_RESOURCES_HOST_CPU;
            PropertyRequest->ValueSize = sizeof(LONG);
        }
    } 

    return ntStatus;
} // PropertyHandlerCpuResources

//=============================================================================
NTSTATUS CMiniportWaveCyclic::PropertyHandlerGeneric(
    IN  PPCPROPERTY_REQUEST     PropertyRequest
)
/*++
Routine Description:
  Handles all properties for this miniport.

Arguments:
  PropertyRequest - property request structure

Return Value:
  NT status code.
--*/
{
    // PAGED_CODE();

    ASSERT(PropertyRequest);
    ASSERT(PropertyRequest->PropertyItem);

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    switch (PropertyRequest->PropertyItem->Id) {
        case KSPROPERTY_AUDIO_CPU_RESOURCES:
            ntStatus = PropertyHandlerCpuResources(PropertyRequest);
            break;

        default:
            DPF(D_TERSE, ("[PropertyHandlerGeneric: Invalid Device Request]"));
            ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    }

    return ntStatus;
} // PropertyHandlerGeneric

//=============================================================================
void TimerNotify(
    IN  PKDPC                   Dpc,
    IN  PVOID                   DeferredContext,
    IN  PVOID                   SA1,
    IN  PVOID                   SA2
)
/*++
Routine Description:
  Dpc routine. This simulates an interrupt service routine. The Dpc will be
  called whenever CMiniportWaveCyclicStreamMSVAD::m_pTimer triggers.

Arguments:
  Dpc - the Dpc object
  DeferredContext - Pointer to a caller-supplied context to be passed to
                    the DeferredRoutine when it is called
  SA1 - System argument 1
  SA2 - System argument 2

Return Value:
  NT status code.
--*/
{
    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SA1);
    UNREFERENCED_PARAMETER(SA2);

    PCMiniportWaveCyclic pMiniport = (PCMiniportWaveCyclic) DeferredContext;

    if (pMiniport && pMiniport->m_Port) {
        pMiniport->m_Port->Notify(pMiniport->m_ServiceGroup);
    }
} // TimerNotify


//=============================================================================
NTSTATUS PropertyHandler_WaveFilter( 
    IN PPCPROPERTY_REQUEST      PropertyRequest 
)
/*++
Routine Description:
  Redirects general property request to miniport object

Arguments:
  PropertyRequest - 

Return Value:
  NT status code.
--*/
{
    // PAGED_CODE();

    NTSTATUS                    ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    PCMiniportWaveCyclic        pWave = (PCMiniportWaveCyclic) PropertyRequest->MajorTarget;

    switch (PropertyRequest->PropertyItem->Id) {
        case KSPROPERTY_GENERAL_COMPONENTID:
            ntStatus = pWave->PropertyHandlerComponentId(PropertyRequest);
            break;

        case KSPROPERTY_PIN_PROPOSEDATAFORMAT:
            ntStatus = pWave->PropertyHandlerProposedFormat(PropertyRequest);
            break;
        
        default:
            DPF(D_TERSE, ("[PropertyHandler_WaveFilter: Invalid Device Request]"));
    }

    return ntStatus;
} // PropertyHandler_WaveFilter

#pragma code_seg()

