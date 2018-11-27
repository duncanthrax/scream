/*++
Copyright (c) 1997-2000  Microsoft Corporation All Rights Reserved

Module Name:
    toptable.h

Abstract:
    Declaration of topology tables.
--*/

#ifndef _MSVAD_TOPTABLE_H_
#define _MSVAD_TOPTABLE_H_

//=============================================================================
static KSDATARANGE PinDataRangesBridge[] = {
 {
   sizeof(KSDATARANGE),
   0,
   0,
   0,
   STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
   STATICGUIDOF(KSDATAFORMAT_SUBTYPE_ANALOG),
   STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)
 }
};

//=============================================================================
static PKSDATARANGE PinDataRangePointersBridge[] = {
  &PinDataRangesBridge[0]
};

//=============================================================================
static PCPIN_DESCRIPTOR MiniportPins[] = {
  // KSPIN_TOPO_WAVEOUT_SOURCE
  {
    0,
    0,
    0,                                              // InstanceCount
    NULL,                                           // AutomationTable
    {                                               // KsPinDescriptor
      0,                                            // InterfacesCount
      NULL,                                         // Interfaces
      0,                                            // MediumsCount
      NULL,                                         // Mediums
      SIZEOF_ARRAY(PinDataRangePointersBridge),     // DataRangesCount
      PinDataRangePointersBridge,                   // DataRanges
      KSPIN_DATAFLOW_IN,                            // DataFlow
      KSPIN_COMMUNICATION_NONE,                     // Communication
      &KSCATEGORY_AUDIO,                            // Category
      NULL,                                         // Name
      0                                             // Reserved
    }
  },

  // KSPIN_TOPO_LINEOUT_DEST
  {
    0,
    0,
    0,                                              // InstanceCount
    NULL,                                           // AutomationTable
    {                                               // KsPinDescriptor
      0,                                            // InterfacesCount
      NULL,                                         // Interfaces
      0,                                            // MediumsCount
      NULL,                                         // Mediums
      SIZEOF_ARRAY(PinDataRangePointersBridge),     // DataRangesCount
      PinDataRangePointersBridge,                   // DataRanges
      KSPIN_DATAFLOW_OUT,                           // DataFlow
      KSPIN_COMMUNICATION_NONE,                     // Communication
      &KSNODETYPE_SPEAKER,                          // Category
      NULL,                                         // Name
      0                                             // Reserved
    }
  }

};


static PKSJACK_DESCRIPTION JackDescriptions[] = {
    NULL,
    NULL
};

//=============================================================================
static PCPROPERTY_ITEM PropertiesVolume[] = {
  {
    &KSPROPSETID_Audio,
    KSPROPERTY_AUDIO_CPU_RESOURCES,
    KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
    PropertyHandler_Topology
  }
};

DEFINE_PCAUTOMATION_TABLE_PROP(AutomationVolume, PropertiesVolume);

//=============================================================================
static PCPROPERTY_ITEM PropertiesMute[] = {
  {
    &KSPROPSETID_Audio,
    KSPROPERTY_AUDIO_MUTE,
    KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT,
    PropertyHandler_Topology
  },
  {
    &KSPROPSETID_Audio,
    KSPROPERTY_AUDIO_CPU_RESOURCES,
    KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
    PropertyHandler_Topology
  }
};

DEFINE_PCAUTOMATION_TABLE_PROP(AutomationMute, PropertiesMute);

//=============================================================================
static PCPROPERTY_ITEM PropertiesMux[] = {
  {
    &KSPROPSETID_Audio,
    KSPROPERTY_AUDIO_MUX_SOURCE,
    KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT,
    PropertyHandler_Topology
  },
  {
    &KSPROPSETID_Audio,
    KSPROPERTY_AUDIO_CPU_RESOURCES,
    KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
    PropertyHandler_Topology
  }
};

DEFINE_PCAUTOMATION_TABLE_PROP(AutomationMux, PropertiesMux);

//=============================================================================
static PCNODE_DESCRIPTOR TopologyNodes[] = {
  // KSNODE_TOPO_WAVEOUT_VOLUME
  {
    0,                      // Flags
    &AutomationVolume,      // AutomationTable
    &KSNODETYPE_VOLUME,     // Type
    &KSAUDFNAME_WAVE_VOLUME // Name
  },

  // KSNODE_TOPO_WAVEOUT_MUTE
  {
    0,                      // Flags
    &AutomationMute,        // AutomationTable
    &KSNODETYPE_MUTE,       // Type
    &KSAUDFNAME_WAVE_MUTE   // Name
  },

  // KSNODE_TOPO_LINEOUT_MIX
  {
    0,                      // Flags
    NULL,                   // AutomationTable
    &KSNODETYPE_SUM,        // Type
    NULL                    // Name
  },

  // KSNODE_TOPO_LINEOUT_VOLUME
  {
    0,                      // Flags
    &AutomationVolume,      // AutomationTable
    &KSNODETYPE_VOLUME,     // Type
    &KSAUDFNAME_MASTER_VOLUME // Name
  }

};

C_ASSERT( KSNODE_TOPO_WAVEOUT_VOLUME  == 0 );
C_ASSERT( KSNODE_TOPO_WAVEOUT_MUTE    == 1 );
C_ASSERT( KSNODE_TOPO_LINEOUT_MIX     == 2 );
C_ASSERT( KSNODE_TOPO_LINEOUT_VOLUME  == 3 );

//=============================================================================
static PCCONNECTION_DESCRIPTOR MiniportConnections[] = {
  //  FromNode,                     FromPin,                        ToNode,                      ToPin
  {   PCFILTER_NODE,                KSPIN_TOPO_WAVEOUT_SOURCE,      KSNODE_TOPO_WAVEOUT_VOLUME,    1 },
  {   KSNODE_TOPO_WAVEOUT_VOLUME,   0,                              KSNODE_TOPO_WAVEOUT_MUTE,      1 },
  {   KSNODE_TOPO_WAVEOUT_MUTE,     0,                              KSNODE_TOPO_LINEOUT_MIX,       1 },

  {   KSNODE_TOPO_LINEOUT_MIX,      0,                              KSNODE_TOPO_LINEOUT_VOLUME,    1 },
  {   KSNODE_TOPO_LINEOUT_VOLUME,   0,                              PCFILTER_NODE,                 KSPIN_TOPO_LINEOUT_DEST }
};


//=============================================================================
static PCPROPERTY_ITEM PropertiesTopoFilter[] = {
  {
    &KSPROPSETID_Jack,
    KSPROPERTY_JACK_DESCRIPTION,
    KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
    PropertyHandler_TopoFilter
  }
};
DEFINE_PCAUTOMATION_TABLE_PROP(AutomationTopoFilter, PropertiesTopoFilter);

//=============================================================================
static PCFILTER_DESCRIPTOR MiniportFilterDescriptor = {
  0,                                  // Version
  &AutomationTopoFilter,              // AutomationTable
  sizeof(PCPIN_DESCRIPTOR),           // PinSize
  SIZEOF_ARRAY(MiniportPins),         // PinCount
  MiniportPins,                       // Pins
  sizeof(PCNODE_DESCRIPTOR),          // NodeSize
  SIZEOF_ARRAY(TopologyNodes),        // NodeCount
  TopologyNodes,                      // Nodes
  SIZEOF_ARRAY(MiniportConnections),  // ConnectionCount
  MiniportConnections,                // Connections
  0,                                  // CategoryCount
  NULL                                // Categories
};

#endif

