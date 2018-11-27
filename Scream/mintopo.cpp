/*++
Copyright (c) 1997-2000  Microsoft Corporation All Rights Reserved

Module Name:
    mintopo.cpp

Abstract:
    Implementation of topology miniport.
--*/

#pragma warning (disable : 4127)

#include "scream.h"
#include "common.h"
#include "minwave.h"
#include "mintopo.h"
#include "toptable.h"


PHYSICALCONNECTIONTABLE TopologyPhysicalConnections =
{
    KSPIN_TOPO_WAVEOUT_SOURCE,  // TopologyIn
    (ULONG)-1,
    (ULONG)-1,
    KSPIN_WAVE_RENDER_SOURCE    // WaveOut
};

#pragma code_seg("PAGE")

//=============================================================================
NTSTATUS CreateMiniportTopologyMSVAD (
    OUT PUNKNOWN* Unknown,
    IN  REFCLSID,
    IN  PUNKNOWN  UnknownOuter OPTIONAL,
    IN  POOL_TYPE PoolType 
)
/*++
Routine Description:
    Creates a new topology miniport.

Arguments:
  Unknown - 
  RefclsId -
  UnknownOuter -
  PoolType - 

Return Value:
  NT status code.
--*/
{
    PAGED_CODE();

    ASSERT(Unknown);

    STD_CREATE_BODY(CMiniportTopology, Unknown, UnknownOuter, PoolType);
} // CreateMiniportTopologyMSVAD

//=============================================================================
CMiniportTopology::CMiniportTopology(PUNKNOWN outer) : CUnknown(outer)
/*++
Routine Description:
  Topology miniport constructor

Arguments:

Return Value:
  void
--*/
{
    PAGED_CODE();

    DPF_ENTER(("[%s]",__FUNCTION__));

    m_AdapterCommon = NULL;
    m_FilterDescriptor = NULL;
} // CMiniportTopology

CMiniportTopology::~CMiniportTopology(void)
/*++
Routine Description:
  Topology miniport destructor

Arguments:

Return Value:
  NT status code.
--*/
{
    PAGED_CODE();

    DPF_ENTER(("[%s]",__FUNCTION__));

    if (m_AdapterCommon) {
        m_AdapterCommon->Release();
    }
} // ~CMiniportTopology

//=============================================================================
NTSTATUS CMiniportTopology::DataRangeIntersection( 
    IN  ULONG        PinId,
    IN  PKSDATARANGE ClientDataRange,
    IN  PKSDATARANGE MyDataRange,
    IN  ULONG        OutputBufferLength,
    OUT PVOID        ResultantFormat OPTIONAL,
    OUT PULONG       ResultantFormatLength 
)
/*++
Routine Description:
  The DataRangeIntersection function determines the highest quality 
  intersection of two data ranges.

Arguments:
  PinId - Pin for which data intersection is being determined. 
  ClientDataRange - Pointer to KSDATARANGE structure which contains the data range 
                    submitted by client in the data range intersection property 
                    request. 
  MyDataRange - Pin's data range to be compared with client's data range. 
  OutputBufferLength - Size of the buffer pointed to by the resultant format 
                       parameter. 
  ResultantFormat - Pointer to value where the resultant format should be 
                    returned. 
  ResultantFormatLength - Actual length of the resultant format that is placed 
                          at ResultantFormat. This should be less than or equal 
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

    PAGED_CODE();

    DPF_ENTER(("[%s]",__FUNCTION__));

    return (STATUS_NOT_IMPLEMENTED);
} // DataRangeIntersection

//=============================================================================
STDMETHODIMP CMiniportTopology::GetDescription(OUT PPCFILTER_DESCRIPTOR * OutFilterDescriptor)
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
    PAGED_CODE();

    ASSERT(OutFilterDescriptor);

    DPF_ENTER(("[%s]",__FUNCTION__));

    *OutFilterDescriptor = m_FilterDescriptor;

    return (STATUS_SUCCESS);
} // GetDescription

//=============================================================================
STDMETHODIMP CMiniportTopology::Init( 
    IN PUNKNOWN      UnknownAdapter,
    IN PRESOURCELIST ResourceList,
    IN PPORTTOPOLOGY Port_ 
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
    UNREFERENCED_PARAMETER(ResourceList);
    UNREFERENCED_PARAMETER(Port_);

    PAGED_CODE();

    ASSERT(UnknownAdapter);
    ASSERT(Port_);

    DPF_ENTER(("[CMiniportTopology::Init]"));

    NTSTATUS ntStatus;

    ntStatus = UnknownAdapter->QueryInterface(IID_IAdapterCommon, (PVOID *) &m_AdapterCommon);
    if (NT_SUCCESS(ntStatus)) {
        m_AdapterCommon->MixerReset();
        m_FilterDescriptor = &MiniportFilterDescriptor;
    } else {
        // clean up AdapterCommon
        if (m_AdapterCommon) {
            m_AdapterCommon->Release();
            m_AdapterCommon = NULL;
        }
    }

    return ntStatus;
} // Init

//=============================================================================
STDMETHODIMP CMiniportTopology::NonDelegatingQueryInterface( 
    IN  REFIID Interface,
    OUT PVOID* Object 
)
/*++
Routine Description:
  QueryInterface for MiniportTopology

Arguments:
  Interface - GUID of the interface
  Object - interface object to be returned.

Return Value:
  NT status code.
--*/
{
    PAGED_CODE();

    ASSERT(Object);

    if (IsEqualGUIDAligned(Interface, IID_IUnknown)) {
        *Object = PVOID(PUNKNOWN(this));
    } else if (IsEqualGUIDAligned(Interface, IID_IMiniport)) {
        *Object = PVOID(PMINIPORT(this));
    } else if (IsEqualGUIDAligned(Interface, IID_IMiniportTopology)) {
        *Object = PVOID(PMINIPORTTOPOLOGY(this));
    } else {
        *Object = NULL;
    }

    if (*Object) {
        // We reference the interface for the caller.
        PUNKNOWN(*Object)->AddRef();
        return(STATUS_SUCCESS);
    }

    return(STATUS_INVALID_PARAMETER);
} // NonDelegatingQueryInterface

//=============================================================================
NTSTATUS CMiniportTopology::PropertyHandlerBasicSupportVolume(IN PPCPROPERTY_REQUEST PropertyRequest)
/*++
Routine Description:
  Handles BasicSupport for Volume nodes.

Arguments:
  PropertyRequest - property request structure

Return Value:
  NT status code.
--*/
{
    PAGED_CODE();

    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG    cbFullProperty = sizeof(KSPROPERTY_DESCRIPTION) + sizeof(KSPROPERTY_MEMBERSHEADER) + sizeof(KSPROPERTY_STEPPING_LONG);

    if (PropertyRequest->ValueSize >= (sizeof(KSPROPERTY_DESCRIPTION))) {
        PKSPROPERTY_DESCRIPTION PropDesc = PKSPROPERTY_DESCRIPTION(PropertyRequest->Value);
        PropDesc->AccessFlags       = KSPROPERTY_TYPE_ALL;
        PropDesc->DescriptionSize   = cbFullProperty;
        PropDesc->PropTypeSet.Set   = KSPROPTYPESETID_General;
        PropDesc->PropTypeSet.Id    = VT_I4;
        PropDesc->PropTypeSet.Flags = 0;
        PropDesc->MembersListCount  = 1;
        PropDesc->Reserved          = 0;

        // if return buffer can also hold a range description, return it too
        if(PropertyRequest->ValueSize >= cbFullProperty) {
            // fill in the members header
            PKSPROPERTY_MEMBERSHEADER Members = PKSPROPERTY_MEMBERSHEADER(PropDesc + 1);
            Members->MembersFlags   = KSPROPERTY_MEMBER_STEPPEDRANGES;
            Members->MembersSize    = sizeof(KSPROPERTY_STEPPING_LONG);
            Members->MembersCount   = 1;
            Members->Flags          = KSPROPERTY_MEMBER_FLAG_BASICSUPPORT_MULTICHANNEL;

            // fill in the stepped range
            PKSPROPERTY_STEPPING_LONG Range = PKSPROPERTY_STEPPING_LONG(Members + 1);
            Range->Bounds.SignedMaximum = 0x00000000;      //   0 dB
            Range->Bounds.SignedMinimum = -96 * 0x10000;   // -96 dB
            Range->SteppingDelta        = 0x08000;         //  .5 dB
            Range->Reserved             = 0;

            // set the return value size
            PropertyRequest->ValueSize = cbFullProperty;
        } else {
            PropertyRequest->ValueSize = sizeof(KSPROPERTY_DESCRIPTION);
        }
    } else if(PropertyRequest->ValueSize >= sizeof(ULONG)) {
        // if return buffer can hold a ULONG, return the access flags
        PULONG AccessFlags = PULONG(PropertyRequest->Value);

        PropertyRequest->ValueSize = sizeof(ULONG);
        *AccessFlags = KSPROPERTY_TYPE_ALL;
    } else {
        PropertyRequest->ValueSize = 0;
        ntStatus = STATUS_BUFFER_TOO_SMALL;
    }

    return ntStatus;
} // PropertyHandlerBasicSupportVolume

//=============================================================================
NTSTATUS CMiniportTopology::PropertyHandlerCpuResources(IN PPCPROPERTY_REQUEST PropertyRequest)
/*++
Routine Description:
  Processes KSPROPERTY_AUDIO_CPURESOURCES

Arguments:
  PropertyRequest - property request structure

Return Value:
  NT status code.
--*/
{
    PAGED_CODE();

    DPF_ENTER(("[%s]",__FUNCTION__));

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    if (PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT) {
        ntStatus = PropertyHandler_BasicSupport(PropertyRequest, KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT, VT_ILLEGAL);
    } else if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET) {
        ntStatus = ValidatePropertyParams(PropertyRequest, sizeof(ULONG));
        if (NT_SUCCESS(ntStatus)) {
            *(PLONG(PropertyRequest->Value)) = KSAUDIO_CPU_RESOURCES_HOST_CPU;
            PropertyRequest->ValueSize = sizeof(LONG);
        }
    }

    return ntStatus;
} // PropertyHandlerCpuResources

//=============================================================================
NTSTATUS CMiniportTopology::PropertyHandlerGeneric(IN PPCPROPERTY_REQUEST PropertyRequest)
/*++
Routine Description:
  Handles all properties for this miniport.

Arguments:
  PropertyRequest - property request structure

Return Value:
  NT status code.
--*/
{
    PAGED_CODE();

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    switch (PropertyRequest->PropertyItem->Id) {
        case KSPROPERTY_AUDIO_VOLUMELEVEL:
            ntStatus = PropertyHandlerVolume(PropertyRequest);
            break;
        
        case KSPROPERTY_AUDIO_CPU_RESOURCES:
            ntStatus = PropertyHandlerCpuResources(PropertyRequest);
            break;

        case KSPROPERTY_AUDIO_MUTE:
            ntStatus = PropertyHandlerMute(PropertyRequest);
            break;

        case KSPROPERTY_AUDIO_MUX_SOURCE:
            ntStatus = PropertyHandlerMuxSource(PropertyRequest);
            break;

        case KSPROPERTY_AUDIO_DEV_SPECIFIC:
            ntStatus = PropertyHandlerDevSpecific(PropertyRequest);
            break;

        default:
            DPF(D_TERSE, ("[PropertyHandlerGeneric: Invalid Device Request]"));
    }

    return ntStatus;
} // PropertyHandlerGeneric

//=============================================================================
NTSTATUS CMiniportTopology::PropertyHandlerMute(IN PPCPROPERTY_REQUEST PropertyRequest)
/*++
Routine Description:
  Property handler for KSPROPERTY_AUDIO_MUTE

Arguments:
  PropertyRequest - property request structure

Return Value:
  NT status code.
--*/
{
    PAGED_CODE();

    DPF_ENTER(("[%s]",__FUNCTION__));

    NTSTATUS ntStatus;
    LONG     lChannel;
    PBOOL    pfMute;

    if (PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT) {
        ntStatus = PropertyHandler_BasicSupport(PropertyRequest, KSPROPERTY_TYPE_ALL, VT_BOOL);
    } else {
        ntStatus = ValidatePropertyParams(PropertyRequest, sizeof(BOOL), sizeof(LONG));
        if (NT_SUCCESS(ntStatus)) {
            lChannel = * PLONG (PropertyRequest->Instance);
            pfMute   = PBOOL (PropertyRequest->Value);

            if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET) {
                *pfMute = m_AdapterCommon->MixerMuteRead(PropertyRequest->Node);
                PropertyRequest->ValueSize = sizeof(BOOL);
            } else if (PropertyRequest->Verb & KSPROPERTY_TYPE_SET) {
                m_AdapterCommon->MixerMuteWrite(PropertyRequest->Node, *pfMute);
            }
        } else {
            DPF(D_TERSE, ("[%s - ntStatus=0x%08x]", __FUNCTION__, ntStatus));
        }
    }

    return ntStatus;
} // PropertyHandlerMute

//=============================================================================
NTSTATUS CMiniportTopology::PropertyHandlerMuxSource(IN PPCPROPERTY_REQUEST PropertyRequest)
/*++
Routine Description:
  PropertyHandler for KSPROPERTY_AUDIO_MUX_SOURCE.

Arguments:
  PropertyRequest - property request structure

Return Value:
  NT status code.
--*/
{
    PAGED_CODE();

    DPF_ENTER(("[%s]",__FUNCTION__));

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    //
    // Validate node
    // This property is only valid for WAVEIN_MUX node.
    //
    // TODO if (WAVEIN_MUX == PropertyRequest->Node)
    {
        if (PropertyRequest->ValueSize >= sizeof(ULONG)) {
            PULONG pulMuxValue = PULONG(PropertyRequest->Value);
            
            if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET) {
                *pulMuxValue = m_AdapterCommon->MixerMuxRead();
                PropertyRequest->ValueSize = sizeof(ULONG);
                ntStatus = STATUS_SUCCESS;
            } else if (PropertyRequest->Verb & KSPROPERTY_TYPE_SET) {
                m_AdapterCommon->MixerMuxWrite(*pulMuxValue);
                ntStatus = STATUS_SUCCESS;
            } else if (PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT) {
                ntStatus = PropertyHandler_BasicSupport(PropertyRequest, KSPROPERTY_TYPE_ALL, VT_I4);
            }
        } else {
            DPF(D_TERSE, ("[PropertyHandlerMuxSource - Invalid parameter]"));
            ntStatus = STATUS_INVALID_PARAMETER;
        }
    }

    return ntStatus;
} // PropertyHandlerMuxSource

//=============================================================================
NTSTATUS CMiniportTopology::PropertyHandlerVolume(IN PPCPROPERTY_REQUEST PropertyRequest)
/*++
Routine Description:
  Property handler for KSPROPERTY_AUDIO_VOLUMELEVEL

Arguments:
  PropertyRequest - property request structure

Return Value:
  NT status code.
--*/
{
    PAGED_CODE();

    DPF_ENTER(("[%s]",__FUNCTION__));

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    LONG     lChannel;
    PULONG   pulVolume;

    if (PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT) {
        ntStatus = PropertyHandlerBasicSupportVolume(PropertyRequest);
    } else {
        // volume value is a ULONG, instance is the channel number
        ntStatus = ValidatePropertyParams(PropertyRequest, sizeof(ULONG), sizeof(LONG));
        if (NT_SUCCESS(ntStatus)) {
            lChannel  = * (PLONG (PropertyRequest->Instance));
            pulVolume = PULONG (PropertyRequest->Value);

            if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET) {
                *pulVolume = m_AdapterCommon->MixerVolumeRead(PropertyRequest->Node, lChannel);
                PropertyRequest->ValueSize = sizeof(ULONG);                
            } else if (PropertyRequest->Verb & KSPROPERTY_TYPE_SET) {
                m_AdapterCommon->MixerVolumeWrite(PropertyRequest->Node, lChannel, *pulVolume);
            }
        } else {
            DPF(D_TERSE, ("[%s - ntStatus=0x%08x]",__FUNCTION__,ntStatus));
        }
    }

    return ntStatus;
} // PropertyHandlerVolume

//=============================================================================
NTSTATUS CMiniportTopology::PropertyHandlerDevSpecific(IN PPCPROPERTY_REQUEST PropertyRequest)
/*++
Routine Description:
  Property handler for KSPROPERTY_AUDIO_DEV_SPECIFIC

Arguments:
  PropertyRequest - property request structure

Return Value:
  NT status code.
--*/
{
    PAGED_CODE();

    DPF_ENTER(("[%s]",__FUNCTION__));

    NTSTATUS ntStatus = STATUS_SUCCESS;

    if (PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT) {
        if (DEV_SPECIFIC_VT_BOOL == PropertyRequest->Node ) {
            ntStatus = PropertyHandler_BasicSupport(PropertyRequest,KSPROPERTY_TYPE_ALL,VT_BOOL);
        } else {
            ULONG ExpectedSize = sizeof( KSPROPERTY_DESCRIPTION ) + sizeof( KSPROPERTY_MEMBERSHEADER ) + sizeof( KSPROPERTY_BOUNDS_LONG );
            DWORD ulPropTypeSetId;

            if( DEV_SPECIFIC_VT_I4 == PropertyRequest->Node ) {
                ulPropTypeSetId = VT_I4;
            } else if ( DEV_SPECIFIC_VT_UI4 == PropertyRequest->Node ) {
                ulPropTypeSetId = VT_UI4;
            } else {
                ulPropTypeSetId = VT_ILLEGAL;
                ntStatus = STATUS_INVALID_PARAMETER;
            }

            if( NT_SUCCESS(ntStatus)) {
                if ( !PropertyRequest->ValueSize ) {
                    PropertyRequest->ValueSize = ExpectedSize;
                    ntStatus = STATUS_BUFFER_OVERFLOW;
                } else if (PropertyRequest->ValueSize >= sizeof(KSPROPERTY_DESCRIPTION)) {
                    // if return buffer can hold a KSPROPERTY_DESCRIPTION, return it
                    //
                    PKSPROPERTY_DESCRIPTION PropDesc = PKSPROPERTY_DESCRIPTION(PropertyRequest->Value);
                    PropDesc->AccessFlags       = KSPROPERTY_TYPE_ALL;
                    PropDesc->DescriptionSize   = ExpectedSize;
                    PropDesc->PropTypeSet.Set   = KSPROPTYPESETID_General;
                    PropDesc->PropTypeSet.Id    = ulPropTypeSetId;
                    PropDesc->PropTypeSet.Flags = 0;
                    PropDesc->MembersListCount  = 0;
                    PropDesc->Reserved          = 0;

                    if ( PropertyRequest->ValueSize >= ExpectedSize ) {
                        // Extra information to return
                        PropDesc->MembersListCount  = 1;

                        PKSPROPERTY_MEMBERSHEADER MembersHeader = ( PKSPROPERTY_MEMBERSHEADER )( PropDesc + 1);
                        MembersHeader->MembersFlags = KSPROPERTY_MEMBER_RANGES;
                        MembersHeader->MembersCount  = 1;
                        MembersHeader->MembersSize   = sizeof( KSPROPERTY_BOUNDS_LONG );
                        MembersHeader->Flags = 0;

                        PKSPROPERTY_BOUNDS_LONG PeakMeterBounds = (PKSPROPERTY_BOUNDS_LONG)( MembersHeader + 1);
                        if(VT_I4 == ulPropTypeSetId ) {
                            PeakMeterBounds->SignedMinimum = 0;
                            PeakMeterBounds->SignedMaximum = 0x7fffffff;
                        } else {
                            PeakMeterBounds->UnsignedMinimum = 0;
                            PeakMeterBounds->UnsignedMaximum = 0xffffffff;
                        }

                        // set the return value size
                        PropertyRequest->ValueSize = ExpectedSize;
                    } else {
                        // No extra information to return.
                        PropertyRequest->ValueSize = sizeof(KSPROPERTY_DESCRIPTION);
                    }

                    ntStatus = STATUS_SUCCESS;
                } else if (PropertyRequest->ValueSize >= sizeof(ULONG)) {
                    // if return buffer can hold a ULONG, return the access flags
                    //
                    *(PULONG(PropertyRequest->Value)) = KSPROPERTY_TYPE_ALL;

                    PropertyRequest->ValueSize = sizeof(ULONG);
                    ntStatus = STATUS_SUCCESS;                    
                } else {
                    PropertyRequest->ValueSize = 0;
                    ntStatus = STATUS_BUFFER_TOO_SMALL;
                }
            }
        }
    } else {
        // switch on node id
        switch( PropertyRequest->Node ) {
            case DEV_SPECIFIC_VT_BOOL:
                PBOOL pbDevSpecific;

                ntStatus = ValidatePropertyParams(PropertyRequest, sizeof(BOOL), 0);

                if (NT_SUCCESS(ntStatus)) {
                    pbDevSpecific = PBOOL (PropertyRequest->Value);

                    if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET) {
                        *pbDevSpecific = m_AdapterCommon->bDevSpecificRead();
                        PropertyRequest->ValueSize = sizeof(BOOL);
                    } else if (PropertyRequest->Verb & KSPROPERTY_TYPE_SET) {
                        m_AdapterCommon->bDevSpecificWrite(*pbDevSpecific);
                    } else {
                        ntStatus = STATUS_INVALID_PARAMETER;
                    }
                }
                break;
            case DEV_SPECIFIC_VT_I4:
                INT* piDevSpecific;

                ntStatus = ValidatePropertyParams(PropertyRequest, sizeof(int), 0);

                if (NT_SUCCESS(ntStatus)) {
                    piDevSpecific = PINT (PropertyRequest->Value);

                    if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET) {
                        *piDevSpecific = m_AdapterCommon->iDevSpecificRead();
                        PropertyRequest->ValueSize = sizeof(int);
                    } else if (PropertyRequest->Verb & KSPROPERTY_TYPE_SET) {
                        m_AdapterCommon->iDevSpecificWrite(*piDevSpecific);
                    } else {
                        ntStatus = STATUS_INVALID_PARAMETER;
                    }
                }
                break;
            case DEV_SPECIFIC_VT_UI4:
                UINT* puiDevSpecific;

                ntStatus = ValidatePropertyParams(PropertyRequest, sizeof(UINT), 0);

                if (NT_SUCCESS(ntStatus)) {
                    puiDevSpecific = PUINT (PropertyRequest->Value);

                    if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET) {
                        *puiDevSpecific = m_AdapterCommon->uiDevSpecificRead();
                        PropertyRequest->ValueSize = sizeof(UINT);
                    } else if (PropertyRequest->Verb & KSPROPERTY_TYPE_SET) {
                        m_AdapterCommon->uiDevSpecificWrite(*puiDevSpecific);
                    } else {
                        ntStatus = STATUS_INVALID_PARAMETER;
                    }
                }
                break;
            default:
                ntStatus = STATUS_INVALID_PARAMETER;
                break;
        }

        if( !NT_SUCCESS(ntStatus)) {
            DPF(D_TERSE, ("[%s - ntStatus=0x%08x]",__FUNCTION__,ntStatus));
        }
    }

    return ntStatus;
} // PropertyHandlerDevSpecific

//=============================================================================
NTSTATUS CMiniportTopology::PropertyHandlerJackDescription(IN PPCPROPERTY_REQUEST PropertyRequest)
/*++
Routine Description:
  Handles ( KSPROPSETID_Jack, KSPROPERTY_JACK_DESCRIPTION )

Arguments:
  PropertyRequest - 

Return Value:
  NT status code.
--*/
{
    PAGED_CODE();

    ASSERT(PropertyRequest);

    DPF_ENTER(("[PropertyHandlerJackDescription]"));

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    ULONG    nPinId   = (ULONG)-1;

    if (PropertyRequest->InstanceSize >= sizeof(ULONG)) {
        nPinId = *(PULONG(PropertyRequest->Instance));

        if ((nPinId < ARRAYSIZE(JackDescriptions)) && (JackDescriptions[nPinId] != NULL)) {
            if (PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT) {
                ntStatus = PropertyHandler_BasicSupport(PropertyRequest, KSPROPERTY_TYPE_BASICSUPPORT | KSPROPERTY_TYPE_GET, VT_ILLEGAL);
            } else {
                ULONG cbNeeded = sizeof(KSMULTIPLE_ITEM) + sizeof(KSJACK_DESCRIPTION);

                if (PropertyRequest->ValueSize == 0) {
                    PropertyRequest->ValueSize = cbNeeded;
                    ntStatus = STATUS_BUFFER_OVERFLOW;
                } else if (PropertyRequest->ValueSize < cbNeeded) {
                    ntStatus = STATUS_BUFFER_TOO_SMALL;
                } else {
                    if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET) {
                        PKSMULTIPLE_ITEM pMI = (PKSMULTIPLE_ITEM)PropertyRequest->Value;
                        PKSJACK_DESCRIPTION pDesc = (PKSJACK_DESCRIPTION)(pMI+1);

                        pMI->Size = cbNeeded;
                        pMI->Count = 1;

                        RtlCopyMemory(pDesc, JackDescriptions[nPinId], sizeof(KSJACK_DESCRIPTION));
                        ntStatus = STATUS_SUCCESS;
                    }
                }
            }
        }
    }

    return ntStatus;
}

//=============================================================================
NTSTATUS PropertyHandler_TopoFilter(IN PPCPROPERTY_REQUEST PropertyRequest)
/*++
Routine Description:
  Redirects property request to miniport object

Arguments:
  PropertyRequest - 

Return Value:
  NT status code.
--*/
{
    PAGED_CODE();

    ASSERT(PropertyRequest);

    DPF_ENTER(("[PropertyHandler_TopoFilter]"));

    // PropertryRequest structure is filled by portcls. 
    // MajorTarget is a pointer to miniport object for miniports.
    //
    NTSTATUS            ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    PCMiniportTopology  pMiniport = (PCMiniportTopology)PropertyRequest->MajorTarget;

    if (IsEqualGUIDAligned(*PropertyRequest->PropertyItem->Set, KSPROPSETID_Jack) && (PropertyRequest->PropertyItem->Id == KSPROPERTY_JACK_DESCRIPTION)) {
        ntStatus = pMiniport->PropertyHandlerJackDescription(PropertyRequest);
    }

    return ntStatus;
} // PropertyHandler_TopoFilter

//=============================================================================
NTSTATUS PropertyHandler_Topology(IN PPCPROPERTY_REQUEST PropertyRequest)
/*++
Routine Description:
  Redirects property request to miniport object

Arguments:
  PropertyRequest - 

Return Value:
  NT status code.
--*/
{
    PAGED_CODE();

    ASSERT(PropertyRequest);

    DPF_ENTER(("[PropertyHandler_Topology]"));

    // PropertryRequest structure is filled by portcls. 
    // MajorTarget is a pointer to miniport object for miniports.
    //
    return ((PCMiniportTopology) (PropertyRequest->MajorTarget))->PropertyHandlerGeneric(PropertyRequest);
} // PropertyHandler_Topology

#pragma code_seg()

