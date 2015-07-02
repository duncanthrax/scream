
/*++

Copyright (c) 1997-2000  Microsoft Corporation All Rights Reserved

Module Name:

    minitopo.h

Abstract:

    Declaration of topology miniport.

--*/

#ifndef _MSVAD_MINTOPO_H_
#define _MSVAD_MINTOPO_H_

//=============================================================================
// Classes
//=============================================================================

///////////////////////////////////////////////////////////////////////////////
// CMiniportTopology 
//   

class CMiniportTopology : public IMiniportTopology, public CUnknown {

protected:
	PADAPTERCOMMON              m_AdapterCommon;    // Adapter common object.
	PPCFILTER_DESCRIPTOR        m_FilterDescriptor; // Filter descriptor.

public:
    DECLARE_STD_UNKNOWN();
    CMiniportTopology(PUNKNOWN outer);
    ~CMiniportTopology();

    IMP_IMiniportTopology;

	NTSTATUS Init(IN PUNKNOWN UnknownAdapter, IN PPORTTOPOLOGY Port_);

    // PropertyHandlers.
	NTSTATUS PropertyHandlerJackDescription(IN PPCPROPERTY_REQUEST PropertyRequest);
    NTSTATUS PropertyHandlerBasicSupportVolume(IN PPCPROPERTY_REQUEST PropertyRequest);
    NTSTATUS PropertyHandlerCpuResources(IN PPCPROPERTY_REQUEST PropertyRequest);
    NTSTATUS PropertyHandlerGeneric(IN PPCPROPERTY_REQUEST PropertyRequest);
    NTSTATUS PropertyHandlerMute(IN PPCPROPERTY_REQUEST PropertyRequest);
    NTSTATUS PropertyHandlerMuxSource(IN PPCPROPERTY_REQUEST PropertyRequest);
    NTSTATUS PropertyHandlerVolume(IN PPCPROPERTY_REQUEST PropertyRequest);
    NTSTATUS PropertyHandlerDevSpecific(IN PPCPROPERTY_REQUEST PropertyRequest);
};
typedef CMiniportTopology *PCMiniportTopology;

extern NTSTATUS PropertyHandler_TopoFilter(IN PPCPROPERTY_REQUEST PropertyRequest);

#endif

