/*++
Copyright (c) 1997-2000  Microsoft Corporation All Rights Reserved

Module Name:
    kshelper.cpp

Abstract:
    Helper functions for msvad
--*/

#include "kshelper.h"

#pragma code_seg("PAGE")

//-----------------------------------------------------------------------------
PWAVEFORMATEX GetWaveFormatEx(IN PKSDATAFORMAT pDataFormat)
/*++
Routine Description:
  Returns the waveformatex for known formats. 

Arguments:
  pDataFormat - data format.

Return Value:
  waveformatex in DataFormat.
  NULL for unknown data formats.
--*/
{
    PAGED_CODE();

    PWAVEFORMATEX pWfx = NULL;
    
    // If this is a known dataformat extract the waveformat info.
    if( pDataFormat && (IsEqualGUIDAligned(pDataFormat->MajorFormat, KSDATAFORMAT_TYPE_AUDIO) && (IsEqualGUIDAligned(pDataFormat->Specifier, KSDATAFORMAT_SPECIFIER_WAVEFORMATEX) || IsEqualGUIDAligned(pDataFormat->Specifier, KSDATAFORMAT_SPECIFIER_DSOUND)))) {
        pWfx = PWAVEFORMATEX(pDataFormat + 1);
        if (IsEqualGUIDAligned(pDataFormat->Specifier, KSDATAFORMAT_SPECIFIER_DSOUND)) {
            PKSDSOUND_BUFFERDESC    pwfxds;
            pwfxds = PKSDSOUND_BUFFERDESC(pDataFormat + 1);
            pWfx = &pwfxds->WaveFormatEx;
        }
    }

    return pWfx;
} // GetWaveFormatEx

//-----------------------------------------------------------------------------
NTSTATUS PropertyHandler_BasicSupport(
    IN PPCPROPERTY_REQUEST         PropertyRequest,
    IN ULONG                       Flags,
    IN DWORD                       PropTypeSetId
)
/*++
Routine Description:
  Default basic support handler. Basic processing depends on the size of data.
  For ULONG it only returns Flags. For KSPROPERTY_DESCRIPTION, the structure   
  is filled.

Arguments:
  PropertyRequest - 
  Flags - Support flags.
  PropTypeSetId - PropTypeSetId

Return Value:
  NT status code.
--*/
{
    PAGED_CODE();

    ASSERT(Flags & KSPROPERTY_TYPE_BASICSUPPORT);

    NTSTATUS ntStatus = STATUS_INVALID_PARAMETER;

    if (PropertyRequest->ValueSize >= sizeof(KSPROPERTY_DESCRIPTION)) {
        // if return buffer can hold a KSPROPERTY_DESCRIPTION, return it
        PKSPROPERTY_DESCRIPTION PropDesc = PKSPROPERTY_DESCRIPTION(PropertyRequest->Value);
        PropDesc->AccessFlags       = Flags;
        PropDesc->DescriptionSize   = sizeof(KSPROPERTY_DESCRIPTION);
        if (VT_ILLEGAL != PropTypeSetId) {
            PropDesc->PropTypeSet.Set   = KSPROPTYPESETID_General;
            PropDesc->PropTypeSet.Id    = PropTypeSetId;
        } else {
            PropDesc->PropTypeSet.Set   = GUID_NULL;
            PropDesc->PropTypeSet.Id    = 0;
        }
        PropDesc->PropTypeSet.Flags = 0;
        PropDesc->MembersListCount  = 0;
        PropDesc->Reserved          = 0;

        PropertyRequest->ValueSize = sizeof(KSPROPERTY_DESCRIPTION);
        ntStatus = STATUS_SUCCESS;
    } else if (PropertyRequest->ValueSize >= sizeof(ULONG)) {
        // if return buffer can hold a ULONG, return the access flags
        *(PULONG(PropertyRequest->Value)) = Flags;

        PropertyRequest->ValueSize = sizeof(ULONG);
        ntStatus = STATUS_SUCCESS;                    
    } else {
        PropertyRequest->ValueSize = 0;
        ntStatus = STATUS_BUFFER_TOO_SMALL;
    }

    return ntStatus;
} // PropertyHandler_BasicSupport

//-----------------------------------------------------------------------------
NTSTATUS ValidatePropertyParams(
    IN PPCPROPERTY_REQUEST      PropertyRequest, 
    IN ULONG                    cbSize,
    IN ULONG                    cbInstanceSize /* = 0  */
)
/*++
Routine Description:
  Validates property parameters.

Arguments:
  PropertyRequest - 
  cbSize -
  cbInstanceSize -

Return Value:
  NT status code.
--*/
{
    PAGED_CODE();

    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;

    if (PropertyRequest && cbSize) {
        if (0 == PropertyRequest->ValueSize)  {
            // If the caller is asking for ValueSize.
            PropertyRequest->ValueSize = cbSize;
            ntStatus = STATUS_BUFFER_OVERFLOW;
        } else if (PropertyRequest->ValueSize < cbSize) {
            // If the caller passed an invalid ValueSize.
            ntStatus = STATUS_BUFFER_TOO_SMALL;
        } else if (PropertyRequest->InstanceSize < cbInstanceSize) {
            ntStatus = STATUS_BUFFER_TOO_SMALL;
        } else if (PropertyRequest->ValueSize == cbSize) {
            // If all parameters are OK.
            if (PropertyRequest->Value) {
                ntStatus = STATUS_SUCCESS;
                //
                // Caller should set ValueSize, if the property 
                // call is successful.
                //
            }
        }
    } else {
        ntStatus = STATUS_INVALID_PARAMETER;
    }
    
    // Clear the ValueSize if unsuccessful.
    if (PropertyRequest && STATUS_SUCCESS != ntStatus && STATUS_BUFFER_OVERFLOW != ntStatus) {
        PropertyRequest->ValueSize = 0;
    }

    return ntStatus;
} // ValidatePropertyParams


