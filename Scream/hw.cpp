/*++
Copyright (c) 1997-2000  Microsoft Corporation All Rights Reserved

Module Name:
    hw.cpp

Abstract:
    Implementation of MSVAD HW class. 
    MSVAD HW has an array for storing mixer and volume settings
    for the topology.
--*/
#include "scream.h"
#include "hw.h"

//=============================================================================
// CMSVADHW
//=============================================================================

//=============================================================================
#pragma code_seg("PAGE")
CMSVADHW::CMSVADHW() : m_ulMux(0), m_bDevSpecific(FALSE), m_iDevSpecific(0), m_uiDevSpecific(0)
/*++
Routine Description:
  Constructor for MSVADHW. 

Arguments:

Return Value:
  void
--*/
{
    PAGED_CODE();
    
    MixerReset();
} // CMSVADHW
#pragma code_seg()

//=============================================================================
BOOL CMSVADHW::bGetDevSpecific()
/*++
Routine Description:
  Gets the HW (!) Device Specific info

Arguments:
  N/A

Return Value:
  True or False (in this example).
--*/
{
    return m_bDevSpecific;
} // bGetDevSpecific

//=============================================================================
void CMSVADHW::bSetDevSpecific(IN BOOL bDevSpecific)
/*++
Routine Description:
  Sets the HW (!) Device Specific info

Arguments:
  fDevSpecific - true or false for this example.

Return Value:
  void
--*/
{
    m_bDevSpecific = bDevSpecific;
} // bSetDevSpecific

//=============================================================================
INT CMSVADHW::iGetDevSpecific()
/*++
Routine Description:
  Gets the HW (!) Device Specific info

Arguments:
  N/A

Return Value:
  int (in this example).
--*/
{
    return m_iDevSpecific;
} // iGetDevSpecific

//=============================================================================
void CMSVADHW::iSetDevSpecific(IN INT iDevSpecific)
/*++
Routine Description:
  Sets the HW (!) Device Specific info

Arguments:
  fDevSpecific - true or false for this example.

Return Value:
  void
--*/
{
    m_iDevSpecific = iDevSpecific;
} // iSetDevSpecific

//=============================================================================
UINT CMSVADHW::uiGetDevSpecific()
/*++
Routine Description:
  Gets the HW (!) Device Specific info

Arguments:
  N/A

Return Value:
  UINT (in this example).
--*/
{
    return m_uiDevSpecific;
} // uiGetDevSpecific

//=============================================================================
void CMSVADHW::uiSetDevSpecific(IN UINT uiDevSpecific)
/*++
Routine Description:
  Sets the HW (!) Device Specific info

Arguments:
  uiDevSpecific - int for this example.

Return Value:
  void
--*/
{
    m_uiDevSpecific = uiDevSpecific;
} // uiSetDevSpecific


//=============================================================================
BOOL CMSVADHW::GetMixerMute(IN ULONG ulNode)
/*++
Routine Description:
  Gets the HW (!) mute levels for MSVAD

Arguments:
  ulNode - topology node id

Return Value:
  mute setting
--*/
{
    if (ulNode < MAX_TOPOLOGY_NODES) {
        return m_MuteControls[ulNode];
    }

    return 0;
} // GetMixerMute

//=============================================================================
ULONG CMSVADHW::GetMixerMux()
/*++
Routine Description:
  Return the current mux selection

Arguments:

Return Value:
  ULONG
--*/
{
    return m_ulMux;
} // GetMixerMux

//=============================================================================
LONG CMSVADHW::GetMixerVolume(   
    IN  ULONG ulNode,
    IN  LONG  lChannel
)
/*++
Routine Description:
  Gets the HW (!) volume for MSVAD.

Arguments:
  ulNode - topology node id
  lChannel - which channel are we setting?

Return Value:
  LONG - volume level
--*/
{
    UNREFERENCED_PARAMETER(lChannel);

    if (ulNode < MAX_TOPOLOGY_NODES) {
        return m_VolumeControls[ulNode];
    }

    return 0;
} // GetMixerVolume

//=============================================================================
#pragma code_seg("PAGE")
void CMSVADHW::MixerReset()
/*++
Routine Description:
  Resets the mixer registers.

Arguments:

Return Value:
  void
--*/
{
    PAGED_CODE();
    
    RtlFillMemory(m_VolumeControls, sizeof(LONG) * MAX_TOPOLOGY_NODES, 0xFF);
    RtlFillMemory(m_MuteControls, sizeof(BOOL) * MAX_TOPOLOGY_NODES, TRUE);
    
    // BUGBUG change this depending on the topology
    m_ulMux = 2;
} // MixerReset
#pragma code_seg()

//=============================================================================
void CMSVADHW::SetMixerMute(
    IN  ULONG                   ulNode,
    IN  BOOL                    fMute
)
/*++
Routine Description:
  Sets the HW (!) mute levels for MSVAD

Arguments:
  ulNode - topology node id
  fMute - mute flag

Return Value:
  void
--*/
{
    if (ulNode < MAX_TOPOLOGY_NODES) {
        m_MuteControls[ulNode] = fMute;
    }
} // SetMixerMute

//=============================================================================
void CMSVADHW::SetMixerMux(
    IN  ULONG                   ulNode
)
/*++
Routine Description:
  Sets the HW (!) mux selection

Arguments:
  ulNode - topology node id

Return Value:
  void
--*/
{
    m_ulMux = ulNode;
} // SetMixMux

//=============================================================================
void CMSVADHW::SetMixerVolume(   
    IN  ULONG                   ulNode,
    IN  LONG                    lChannel,
    IN  LONG                    lVolume
)
/*++
Routine Description:
  Sets the HW (!) volume for MSVAD.

Arguments:
  ulNode - topology node id
  lChannel - which channel are we setting?
  lVolume - volume level

Return Value:
  void
--*/
{
    UNREFERENCED_PARAMETER(lChannel);

    if (ulNode < MAX_TOPOLOGY_NODES) {
        m_VolumeControls[ulNode] = lVolume;
    }
} // SetMixerVolume
