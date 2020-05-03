#ifndef AC3ENCODER_H
#define AC3ENCODER_H

#include <mftransform.h>

class Ac3Encoder
{
public:
    Ac3Encoder(UINT32 sampleRate = 48000,
               UINT32 numChannels = 2);
    ~Ac3Encoder();

    HRESULT Initialize(UINT32 sampleRate, UINT32 numChannels);
    void SetBitrate(UINT32 kbps);

    IMFSample* Process(void *inSamples, UINT32 inSize);
    void ReleaseSample(IMFSample *sample);

    void Drain();

private:
    void ProcessInput(void *sampleData, UINT32 size);
    IMFSample* ProcessOutput();

    UINT32  m_sampleRate = 48000;
    UINT32  m_numChannels = 2;
    UINT32  m_bitrateKbps = 256;
    IMFTransform    *m_transform = nullptr;        // Pointer to the encoder MFT.
    IMFMediaType    *m_inputType = nullptr;  // Input media type of the encoder.
    IMFMediaType    *m_outputType = nullptr;  // Output media type of the encoder.
};

#endif // AC3ENCODER_H
