/*++
Copyright (c) 1997-2000  Microsoft Corporation All Rights Reserved

Module Name:
    adapter.cpp

Abstract:
    Setup and miniport installation.  No resources are used by msvad.
--*/

#pragma warning (disable : 4127)

//
// All the GUIDS for all the miniports end up in this object.
//
#define PUT_GUIDS_HERE

#include "scream.h"
#include "common.h"

//-----------------------------------------------------------------------------
// Defines                                                                    
//-----------------------------------------------------------------------------
// BUGBUG set this to number of miniports
#define MAX_MINIPORTS 3     // Number of maximum miniports.

//-----------------------------------------------------------------------------
// Externals
//-----------------------------------------------------------------------------
NTSTATUS CreateMiniportWaveCyclicMSVAD(OUT PUNKNOWN *, IN  REFCLSID, IN  PUNKNOWN, IN  POOL_TYPE);
NTSTATUS CreateMiniportTopologyMSVAD(OUT PUNKNOWN *, IN  REFCLSID, IN  PUNKNOWN, IN  POOL_TYPE);

//-----------------------------------------------------------------------------
// Referenced forward.
//-----------------------------------------------------------------------------
DRIVER_ADD_DEVICE AddDevice;

NTSTATUS StartDevice(IN PDEVICE_OBJECT, IN PIRP, IN PRESOURCELIST); 

//-----------------------------------------------------------------------------
// Functions
//-----------------------------------------------------------------------------

//=============================================================================
#pragma code_seg("INIT")
extern "C" DRIVER_INITIALIZE DriverEntry;
extern "C" NTSTATUS DriverEntry( 
    IN  PDRIVER_OBJECT          DriverObject,
    IN  PUNICODE_STRING         RegistryPathName
)
/*++
Routine Description:
  Installable driver initialization entry point.
  This entry point is called directly by the I/O system.

  All audio adapter drivers can use this code without change.

Arguments:
  DriverObject - pointer to the driver object
  RegistryPath - pointer to a unicode string representing the path,
                   to driver-specific key in the registry.

Return Value:
  STATUS_SUCCESS if successful,
  STATUS_UNSUCCESSFUL otherwise.

--*/
{
    NTSTATUS ntStatus;

    DPF(D_TERSE, ("[DriverEntry]"));

    // Tell the class driver to initialize the driver.
    ntStatus = PcInitializeAdapterDriver(DriverObject, RegistryPathName, (PDRIVER_ADD_DEVICE)AddDevice);
    
    return ntStatus;
} // DriverEntry
#pragma code_seg()

#pragma code_seg("PAGE")
//=============================================================================
NTSTATUS AddDevice ( 
    IN  PDRIVER_OBJECT          DriverObject,
    IN  PDEVICE_OBJECT          PhysicalDeviceObject 
)
/*++
Routine Description:
  The Plug & Play subsystem is handing us a brand new PDO, for which we
  (by means of INF registration) have been asked to provide a driver.

  We need to determine if we need to be in the driver stack for the device.
  Create a function device object to attach to the stack
  Initialize that device object
  Return status success.

  All audio adapter drivers can use this code without change.
  Set MAX_MINIPORTS depending on the number of miniports that the driver
  uses.

Arguments:
  DriverObject - pointer to a driver object
  PhysicalDeviceObject -  pointer to a device object created by the
                            underlying bus driver.

Return Value:
  NT status code.
--*/
{
    PAGED_CODE();

    NTSTATUS ntStatus;

    DPF(D_TERSE, ("[AddDevice]"));

    // disable prefast warning 28152 because 
    // DO_DEVICE_INITIALIZING is cleared in PcAddAdapterDevice
#pragma warning(disable:28152)

    // Tell the class driver to add the device.
    ntStatus = PcAddAdapterDevice(DriverObject, PhysicalDeviceObject, PCPFNSTARTDEVICE(StartDevice), MAX_MINIPORTS, 0);

    return ntStatus;
} // AddDevice

//=============================================================================
NTSTATUS InstallSubdevice( 
    __in        PDEVICE_OBJECT    DeviceObject,
    __in        PIRP              Irp,
    __in        PWSTR             Name,
    __in        REFGUID           PortClassId,
    __in        REFGUID           MiniportClassId,
    __in_opt    PFNCREATEINSTANCE MiniportCreate,
    __in_opt    PUNKNOWN          UnknownAdapter,
    __in_opt    PRESOURCELIST     ResourceList,
    __in        REFGUID           PortInterfaceId,
    __out_opt   PUNKNOWN *        OutPortInterface,
    __out_opt   PUNKNOWN *        OutPortUnknown
)
/*++
Routine Description:
    This function creates and registers a subdevice consisting of a port       
    driver, a minport driver and a set of resources bound together.  It will   
    also optionally place a pointer to an interface on the port driver in a    
    specified location before initializing the port driver.  This is done so   
    that a common ISR can have access to the port driver during 
    initialization, when the ISR might fire.                                   

Arguments:
    DeviceObject - pointer to the driver object
    Irp - pointer to the irp object.
    Name - name of the miniport. Passes to PcRegisterSubDevice
    PortClassId - port class id. Passed to PcNewPort.
    MiniportClassId - miniport class id. Passed to PcNewMiniport.
    MiniportCreate - pointer to a miniport creation function. If NULL, 
                     PcNewMiniport is used.
    UnknownAdapter - pointer to the adapter object. 
                     Used for initializing the port.
    ResourceList - pointer to the resource list.
    PortInterfaceId - GUID that represents the port interface.
    OutPortInterface - pointer to store the port interface
    OutPortUnknown - pointer to store the unknown port interface.

Return Value:
    NT status code.
--*/
{
    PAGED_CODE();

    ASSERT(DeviceObject);
    ASSERT(Irp);
    ASSERT(Name);

    NTSTATUS ntStatus;
    PPORT    port = NULL;
    PUNKNOWN miniport = NULL;
     
    DPF_ENTER(("[InstallSubDevice %S]", Name));

    // Create the port driver object
    ntStatus = PcNewPort(&port, PortClassId);

    // Create the miniport object
    if (NT_SUCCESS(ntStatus)) {
        if (MiniportCreate) {
            ntStatus = MiniportCreate(&miniport, MiniportClassId, NULL, NonPagedPool  );
        } else {
            ntStatus = PcNewMiniport((PMINIPORT *) &miniport, MiniportClassId);
        }
    }

    // Init the port driver and miniport in one go.
    if (NT_SUCCESS(ntStatus)) {
        ntStatus = port->Init(DeviceObject, Irp, miniport, UnknownAdapter, ResourceList);

        if (NT_SUCCESS(ntStatus)) {
            // Register the subdevice (port/miniport combination).
            ntStatus = PcRegisterSubdevice(DeviceObject, Name, port);
        }

        // We don't need the miniport any more.  Either the port has it,
        // or we've failed, and it should be deleted.
        miniport->Release();
    }

    // Deposit the port interfaces if it's needed.
    if (NT_SUCCESS(ntStatus)) {
        if (OutPortUnknown) {
            ntStatus = port->QueryInterface(IID_IUnknown, (PVOID *)OutPortUnknown);
        }

        if (OutPortInterface) {
            ntStatus = port->QueryInterface(PortInterfaceId, (PVOID *) OutPortInterface);
        }
    }

    if (port) {
        port->Release();
    }

    return ntStatus;
} // InstallSubDevice

//=============================================================================
NTSTATUS StartDevice(
    IN  PDEVICE_OBJECT          DeviceObject,     
    IN  PIRP                    Irp,              
    IN  PRESOURCELIST           ResourceList      
)  
/*++
Routine Description:
  This function is called by the operating system when the device is 
  started.
  It is responsible for starting the miniports.  This code is specific to    
  the adapter because it calls out miniports for functions that are specific 
  to the adapter.                                                            

Arguments:
  DeviceObject - pointer to the driver object
  Irp - pointer to the irp 
  ResourceList - pointer to the resource list assigned by PnP manager

Return Value:
  NT status code.
--*/
{
    UNREFERENCED_PARAMETER(ResourceList);

    PAGED_CODE();
    
    ASSERT(DeviceObject);
    ASSERT(Irp);
    ASSERT(ResourceList);

    NTSTATUS       ntStatus        = STATUS_SUCCESS;
    PUNKNOWN       unknownTopology = NULL;
    PUNKNOWN       unknownWave     = NULL;
    PADAPTERCOMMON pAdapterCommon  = NULL;
    PUNKNOWN       pUnknownCommon  = NULL;

    DPF_ENTER(("[StartDevice]"));

    // create a new adapter common object
    ntStatus = NewAdapterCommon(&pUnknownCommon, IID_IAdapterCommon, NULL, NonPagedPool);
    if (NT_SUCCESS(ntStatus)) {
        ntStatus = pUnknownCommon->QueryInterface(IID_IAdapterCommon, (PVOID *) &pAdapterCommon);
        if (NT_SUCCESS(ntStatus)) {
            ntStatus = pAdapterCommon->Init(DeviceObject);
            if (NT_SUCCESS(ntStatus)) {
                // register with PortCls for power-management services
                ntStatus = PcRegisterAdapterPowerManagement(PUNKNOWN(pAdapterCommon), DeviceObject);
            }
        }
    }

    // install MSVAD topology miniport.
    if (NT_SUCCESS(ntStatus)) {
        ntStatus = InstallSubdevice(DeviceObject, Irp, L"Topology", CLSID_PortTopology, CLSID_PortTopology, CreateMiniportTopologyMSVAD, pAdapterCommon, NULL, IID_IPortTopology, NULL, &unknownTopology);
    }

    // install MSVAD wavecyclic miniport.
    if (NT_SUCCESS(ntStatus)) {
        ntStatus = InstallSubdevice(DeviceObject, Irp, L"Wave", CLSID_PortWaveCyclic, CLSID_PortWaveCyclic, CreateMiniportWaveCyclicMSVAD, pAdapterCommon, NULL, IID_IPortWaveCyclic, pAdapterCommon->WavePortDriverDest(), &unknownWave);
    }

    if (unknownTopology && unknownWave) {
        // register wave <=> topology connections
        // This will connect bridge pins of wavecyclic and topology
        // miniports.
        if ((TopologyPhysicalConnections.ulTopologyOut != (ULONG)-1) && (TopologyPhysicalConnections.ulWaveIn != (ULONG)-1)) {
            ntStatus = PcRegisterPhysicalConnection(DeviceObject, unknownTopology, TopologyPhysicalConnections.ulTopologyOut, unknownWave, TopologyPhysicalConnections.ulWaveIn);
        }

        if (NT_SUCCESS(ntStatus)) {
            if ((TopologyPhysicalConnections.ulWaveOut != (ULONG)-1) && (TopologyPhysicalConnections.ulTopologyIn != (ULONG)-1)) {
                ntStatus = PcRegisterPhysicalConnection(DeviceObject, unknownWave, TopologyPhysicalConnections.ulWaveOut, unknownTopology, TopologyPhysicalConnections.ulTopologyIn);
            }
        }
    }

    // Release the adapter common object.  It either has other references,
    // or we need to delete it anyway.
    if (pAdapterCommon) {
        pAdapterCommon->Release();
    }

    if (pUnknownCommon) {
        pUnknownCommon->Release();
    }
    
    if (unknownTopology) {
        unknownTopology->Release();
    }

    if (unknownWave) {
        unknownWave->Release();
    }

    return ntStatus;
} // StartDevice
#pragma code_seg()
