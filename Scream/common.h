/*++
Copyright (c) 1997-2000  Microsoft Corporation All Rights Reserved

Module Name:
    Common.h

Abstract:
    CAdapterCommon class declaration.
--*/

#ifndef _MSVAD_COMMON_H_
#define _MSVAD_COMMON_H_

//=============================================================================
// Defines
//=============================================================================
DEFINE_GUID(IID_IAdapterCommon, 0x7eda2950, 0xbf9f, 0x11d0, 0x87, 0x1f, 0x0, 0xa0, 0xc9, 0x11, 0xb5, 0x44);

//=============================================================================
// Interfaces
//=============================================================================

///////////////////////////////////////////////////////////////////////////////
// IAdapterCommon
//
DECLARE_INTERFACE_(IAdapterCommon, IUnknown) {
    STDMETHOD_(NTSTATUS,        Init)                (THIS_ IN PDEVICE_OBJECT DeviceObject) PURE;
    STDMETHOD_(PDEVICE_OBJECT,  GetDeviceObject)     (THIS) PURE;
    STDMETHOD_(VOID,            SetWaveServiceGroup) (THIS_ IN PSERVICEGROUP ServiceGroup) PURE;
    STDMETHOD_(PUNKNOWN *,      WavePortDriverDest)  (THIS) PURE;

    STDMETHOD_(BOOL,            bDevSpecificRead)    (THIS_) PURE;
    STDMETHOD_(VOID,            bDevSpecificWrite)   (THIS_ IN  BOOL bDevSpecific);

    STDMETHOD_(INT,             iDevSpecificRead)    (THIS_) PURE;
    STDMETHOD_(VOID,            iDevSpecificWrite)   (THIS_ IN INT iDevSpecific);

    STDMETHOD_(UINT,            uiDevSpecificRead)   (THIS_) PURE;
    STDMETHOD_(VOID,            uiDevSpecificWrite)  (THIS_ IN UINT uiDevSpecific);

    STDMETHOD_(BOOL,            MixerMuteRead)       (THIS_ IN ULONG Index) PURE;
    STDMETHOD_(VOID,            MixerMuteWrite)      (THIS_ IN ULONG Index, IN BOOL Value);
    STDMETHOD_(ULONG,           MixerMuxRead)        (THIS);
    STDMETHOD_(VOID,            MixerMuxWrite)       (THIS_ IN ULONG Index);
    STDMETHOD_(LONG,            MixerVolumeRead)     (THIS_ IN ULONG Index, IN LONG Channel) PURE;
    STDMETHOD_(VOID,            MixerVolumeWrite)    (THIS_ IN ULONG Index, IN LONG Channel, IN LONG Value) PURE;
    STDMETHOD_(VOID,            MixerReset)          (THIS) PURE;
};
typedef IAdapterCommon *PADAPTERCOMMON;

//=============================================================================
// Function Prototypes
//=============================================================================
NTSTATUS NewAdapterCommon( 
    OUT PUNKNOWN* Unknown,
    IN  REFCLSID,
    IN  PUNKNOWN  UnknownOuter OPTIONAL,
    IN  POOL_TYPE PoolType 
);

#endif  //_COMMON_H_
