/*++
Copyright (c) 1997-2000  Microsoft Corporation All Rights Reserved

Module Name:
    wavtable.h

Abstract:
    Declaration of wave miniport tables.
--*/

#ifndef _MSVAD_WAVTABLE_H_
#define _MSVAD_WAVTABLE_H_

//=============================================================================
static KSDATARANGE_AUDIO PinDataRangesStream[] = {
  {
    {
      sizeof(KSDATARANGE_AUDIO),
      0,
      0,
      0,
      STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
      STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
      STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
    },
    MAX_CHANNELS_PCM,           
    MIN_BITS_PER_SAMPLE_PCM,    
    MAX_BITS_PER_SAMPLE_PCM,    
    MIN_SAMPLE_RATE,            
    MAX_SAMPLE_RATE             
  },
};

static PKSDATARANGE PinDataRangePointersStream[] = {
    PKSDATARANGE(&PinDataRangesStream[0])
};

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

static PKSDATARANGE PinDataRangePointersBridge[] = {
    &PinDataRangesBridge[0]
};

//=============================================================================
static PCPIN_DESCRIPTOR MiniportPins[] = {
  // Wave Out Streaming Pin (Renderer) KSPIN_WAVE_RENDER_SINK
  {
    MAX_INPUT_STREAMS,
    MAX_INPUT_STREAMS, 
    0,
    NULL,
    {
      0,
      NULL,
      0,
      NULL,
      SIZEOF_ARRAY(PinDataRangePointersStream),
      PinDataRangePointersStream,
      KSPIN_DATAFLOW_IN,
      KSPIN_COMMUNICATION_SINK,
      &KSCATEGORY_AUDIO,
      NULL,
      0
    }
  },
  
  // Wave Out Bridge Pin (Renderer) KSPIN_WAVE_RENDER_SOURCE
  {
    0,
    0,
    0,
    NULL,
    {
      0,
      NULL,
      0,
      NULL,
      SIZEOF_ARRAY(PinDataRangePointersBridge),
      PinDataRangePointersBridge,
      KSPIN_DATAFLOW_OUT,
      KSPIN_COMMUNICATION_NONE,
      &KSCATEGORY_AUDIO,
      NULL,
      0
    }
  },
};

//=============================================================================
static PCNODE_DESCRIPTOR MiniportNodes[] = {
  {
    0,                      // Flags
    NULL,                   // AutomationTable
    &KSNODETYPE_DAC,        // Type
    NULL                    // Name
  }
};

//=============================================================================
static PCCONNECTION_DESCRIPTOR MiniportConnections[] = {
  { PCFILTER_NODE,        KSPIN_WAVE_RENDER_SINK,     KSNODE_WAVE_DAC,     1 },    
  { KSNODE_WAVE_DAC,      0,                          PCFILTER_NODE,       KSPIN_WAVE_RENDER_SOURCE },    
};

//=============================================================================
static PCPROPERTY_ITEM PropertiesWaveFilter[] = {
  {
    &KSPROPSETID_General,
    KSPROPERTY_GENERAL_COMPONENTID,
    KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
    PropertyHandler_WaveFilter
  },
  {
    &KSPROPSETID_Pin,
    KSPROPERTY_PIN_PROPOSEDATAFORMAT,
    KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT,
    PropertyHandler_WaveFilter
  }
};
DEFINE_PCAUTOMATION_TABLE_PROP(AutomationWaveFilter, PropertiesWaveFilter);

//=============================================================================
static PCFILTER_DESCRIPTOR MiniportFilterDescriptor = {
  0,                                  // Version
  &AutomationWaveFilter,              // AutomationTable
  sizeof(PCPIN_DESCRIPTOR),           // PinSize
  SIZEOF_ARRAY(MiniportPins),         // PinCount
  MiniportPins,                       // Pins
  sizeof(PCNODE_DESCRIPTOR),          // NodeSize
  SIZEOF_ARRAY(MiniportNodes),        // NodeCount
  MiniportNodes,                      // Nodes
  SIZEOF_ARRAY(MiniportConnections),  // ConnectionCount
  MiniportConnections,                // Connections
  0,                                  // CategoryCount
  NULL                                // Categories  - use defaults (audio, render, capture)
};

#endif
