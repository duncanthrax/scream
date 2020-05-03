#include "ac3encoder.h"

#include <mfapi.h>
#include <mferror.h>

template<class T>
void SafeRelease(T **ppT)
{
    if (*ppT) {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

Ac3Encoder::Ac3Encoder(UINT32 sampleRate,
                       UINT32 numChannels) :
    m_sampleRate(sampleRate),
    m_numChannels(numChannels)
{
    Initialize(m_sampleRate, m_numChannels);
}

Ac3Encoder::~Ac3Encoder()
{
    SafeRelease(&m_transform);
    SafeRelease(&m_inputType);
    SafeRelease(&m_outputType);
}

HRESULT Ac3Encoder::Initialize(UINT32 sampleRate, UINT32 numChannels)
{
    m_sampleRate = sampleRate;
    m_numChannels = numChannels;

    SafeRelease(&m_transform);
    SafeRelease(&m_inputType);
    SafeRelease(&m_outputType);

    // Look for a encoder.
    MFT_REGISTER_TYPE_INFO outInfo;
    outInfo.guidMajorType = MFMediaType_Audio;
    outInfo.guidSubtype = MFAudioFormat_Dolby_AC3;
    CLSID *clsids = nullptr;    // Pointer to an array of CLISDs.
    UINT32 clsidsSize = 0;      // Size of the array.
    auto hr = MFTEnum(MFT_CATEGORY_AUDIO_ENCODER,
                      0,            // Flags
                      NULL,         // Input type to match.
                      &outInfo,     // Output type to match.
                      NULL,         // Attributes to match. (None.)
                      &clsids,      // Receives a pointer to an array of CLSIDs.
                      &clsidsSize   // Receives the size of the array.
                      );

    if (SUCCEEDED(hr) && clsidsSize == 0) {
        hr = MF_E_TOPO_CODEC_NOT_FOUND;
    }

    // Create the MFT decoder
    if (SUCCEEDED(hr)) {
        hr = CoCreateInstance(clsids[0], NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_transform));
    }

    // Create and set output type
    MFCreateMediaType(&m_outputType);
    m_outputType->SetGUID(MF_MT_MAJOR_TYPE, outInfo.guidMajorType);
    m_outputType->SetGUID(MF_MT_SUBTYPE, outInfo.guidSubtype);
    m_outputType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate);
    m_outputType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, numChannels);
    m_outputType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, m_bitrateKbps*1000/8);
    hr = m_transform->SetOutputType(0, m_outputType , 0);

    MFCreateMediaType(&m_inputType);
    m_inputType->SetGUID(MF_MT_MAJOR_TYPE, outInfo.guidMajorType);
    m_inputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    m_inputType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    m_inputType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate);
    m_inputType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, numChannels);
    m_inputType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 4);
    m_inputType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, sampleRate*4);
    hr = m_transform->SetInputType(0, m_inputType , 0);

    return hr;
}

void Ac3Encoder::SetBitrate(UINT32 kbps)
{
    if (m_bitrateKbps == kbps) {
        return;
    }

    m_bitrateKbps = kbps;
    Initialize(m_sampleRate, m_numChannels);
}

IMFSample* Ac3Encoder::Process(void *inBuffer, UINT32 inSize)
{
    // Feed inpud data
    ProcessInput(inBuffer, inSize);

    // Check if we have a complete AC3 frame ready for output.
    IMFSample *outSample = ProcessOutput();
    // Once we get an outSample, we have to process output again to put the
    // transform back in accepting state.
    if (outSample) {
        while (auto sample = ProcessOutput()) {
            ReleaseSample(sample);
        }
    }

    return outSample;
}

void Ac3Encoder::ProcessInput(void *inSamples, UINT32 inSize)
{
    // Create buffer
    IMFMediaBuffer *pBuffer = NULL;
    auto hr = MFCreateMemoryBuffer(inSize, &pBuffer);

    // Copy sample data to buffer
    BYTE *pMem = NULL;
    if (SUCCEEDED(hr)) {
        hr = pBuffer->Lock(&pMem, NULL, NULL);
    }

    if (SUCCEEDED(hr)) {
        memcpy(pMem, inSamples, inSize);
    }
    pBuffer->Unlock();

    // Set the data length of the buffer.
    if (SUCCEEDED(hr)) {
        hr = pBuffer->SetCurrentLength(inSize);
    }

    // Create media sample and add the buffer to the sample.
    IMFSample *pSample = NULL;
    if (SUCCEEDED(hr)) {
        hr = MFCreateSample(&pSample);
    }
    if (SUCCEEDED(hr)) {
        hr = pSample->AddBuffer(pBuffer);
    }

    hr = m_transform->ProcessInput(0, pSample, 0);

    SafeRelease(&pSample);
    SafeRelease(&pBuffer);
}

void Ac3Encoder::ReleaseSample(IMFSample *sample)
{
    SafeRelease(&sample);
}

void Ac3Encoder::Drain()
{
    m_transform->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
}

IMFSample* Ac3Encoder::ProcessOutput()
{
    IMFSample* pSample = nullptr;
    MFT_OUTPUT_DATA_BUFFER mftOutputData = { 0, nullptr, 0, nullptr };
    MFT_OUTPUT_STREAM_INFO mftStreamInfo = { 0, 0, 0 };
    IMFMediaBuffer* pBufferOut = nullptr;
    IMFSample* pSampleOut = nullptr;

    // Get output info
    HRESULT hr = m_transform->GetOutputStreamInfo(0, &mftStreamInfo);
    if (FAILED(hr)) {
        goto done;
    }

    // Create the output sample
    hr = MFCreateSample(&pSampleOut);
    if (FAILED(hr)) {
        goto done;
    }
    // Create buffer and add to output sample
    hr = MFCreateMemoryBuffer(mftStreamInfo.cbSize, &pBufferOut);
    if (FAILED(hr)) {
        goto done;
    }
    hr = pSampleOut->AddBuffer(pBufferOut);
    if (FAILED(hr)) {
        goto done;
    }

    // Generate the output sample
    mftOutputData.pSample = pSampleOut;
    DWORD dwStatus;
    hr = m_transform->ProcessOutput(0, 1, &mftOutputData, &dwStatus);
    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
        goto done;
    }
    if (FAILED(hr)) {
        goto done;
    }

    pSample = pSampleOut;
    pSample->AddRef();

done:
    SafeRelease(&pBufferOut);
    SafeRelease(&pSampleOut);

    return pSample;
}
