
#include <iostream>
#include <windows.h>
#include <math.h>

// Windows multimedia device
#include <Mmdeviceapi.h>

// WASAPI
#include <audioclient.h>

#include <fstream>
#include <iostream>
#include <cmath>

//-----------------------------------------------------------
// Record an audio stream from the default audio capture
// device. The RecordAudioStream function allocates a shared
// buffer big enough to hold one second of PCM audio data.
// The function uses this buffer to stream data from the
// capture device. The main loop runs every 1/2 second.
//-----------------------------------------------------------

//-----------------------------------------------------------
// Record an audio stream from the default audio capture
// device. The RecordAudioStream function allocates a shared
// buffer big enough to hold one second of PCM audio data.
// The function uses this buffer to stream data from the
// capture device. The main loop runs every 1/2 second.
//-----------------------------------------------------------

// REFERENCE_TIME time units per second and per millisecond
#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

#define EXIT_ON_ERROR(hres)  \
              if (FAILED(hres)) { goto Exit; }
#define SAFE_RELEASE(punk)  \
              if ((punk) != NULL)  \
                { (punk)->Release(); (punk) = NULL; }

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);


class MyAudioSink
{

private:

    size_t data_chunk_pos;
    size_t file_length;
    ofstream mainFile;

    //sample format
    WORD  wFormatTag;
    WORD  nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD  nBlockAlign;
    WORD  wBitsPerSample;
    WORD  cbSize;
    int test;

public:

    bool bComplete;

    HRESULT _Initialize_File();
    HRESULT SetFormat(WAVEFORMATEX* pwfx);
    HRESULT CopyData(BYTE* pData, UINT32 numFramesAvailable, BOOL* bDone);
    HRESULT _File_WrapUp();
};

HRESULT MyAudioSink::_Initialize_File() {



    cout << "initializing file";

    // prepare our wav file
    mainFile.open("example.wav", ios::out | ios::binary);

    // Write the file headers and sound format
    mainFile << "RIFF----WAVEfmt ";     // (chunk size to be filled in later)
    write_word(mainFile, 16, 4);  // no extension data
    write_word(mainFile, 1, 2);  // PCM - integer samples
    write_word(mainFile, nChannels, 2);  // two channels (stereo file)
    write_word(mainFile, nSamplesPerSec, 4);  // samples per second (Hz)
    write_word(mainFile, nAvgBytesPerSec, 4);  // (Sample Rate * BitsPerSample * Channels) / 8
    write_word(mainFile, nBlockAlign, 2);  // data block size (size of two integer samples, one for each channel, in bytes)
    write_word(mainFile, wBitsPerSample, 2);  // number of bits per sample (use a multiple of 8)

    // Write the data chunk header
    data_chunk_pos = mainFile.tellp();
    mainFile << "data----";  // (chunk size to be filled in later)..

    //start by setting our complete variable to False, main func will turn to true
    bComplete = false;
    //testing
    test = 0;

    return S_OK;

}

HRESULT MyAudioSink::SetFormat(WAVEFORMATEX* pwfx) {



    //Update our format variables
    wFormatTag = pwfx->wFormatTag;
    nChannels = pwfx->nChannels;
    nSamplesPerSec = pwfx->nSamplesPerSec;
    nAvgBytesPerSec = pwfx->nAvgBytesPerSec;
    nBlockAlign = pwfx->nBlockAlign;
    wBitsPerSample = pwfx->wBitsPerSample;
    cbSize = pwfx->cbSize;

    return S_OK;

}

HRESULT MyAudioSink::CopyData(BYTE* pData, UINT32 numFramesAvailable, BOOL* bDone) {
    //TODO

    //forgot how to do this part, figure it out
    for (int i = 0; i < numFramesAvailable; i++) {
        mainFile.write((const char*) pData+(i* nBlockAlign), nBlockAlign);
    }


    //test
    test++;
    if (test >= nBlockAlign * 120) bComplete = true;

    //check if our main function is done to finish capture
    if (bComplete) *bDone = true;


    return S_OK;
}

HRESULT MyAudioSink::_File_WrapUp() {



    // (We'll need the final file size to fix the chunk sizes above)
    file_length = mainFile.tellp();

    // Fix the data chunk header to contain the data size
    mainFile.seekp(data_chunk_pos + 4);
    write_word(mainFile, file_length - data_chunk_pos + 8);

    // Fix the file header to contain the proper RIFF chunk size, which is (file size - 8) bytes
    mainFile.seekp(0 + 4);
    write_word(mainFile, file_length - 8, 4);

    mainFile.close();

    cout << "finalized file";

    return S_OK;
}

HRESULT RecordAudioStream(MyAudioSink *pMySink)
{
    HRESULT hr;
    REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
    REFERENCE_TIME hnsActualDuration;
    UINT32 bufferFrameCount;
    UINT32 numFramesAvailable;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;
    IAudioClient *pAudioClient = NULL;
    IAudioCaptureClient *pCaptureClient = NULL;
    WAVEFORMATEX *pwfx = NULL;
    UINT32 packetLength = 0;
    BOOL bDone = FALSE;
    BYTE *pData;
    DWORD flags;

    hr = CoCreateInstance(
           CLSID_MMDeviceEnumerator, NULL,
           CLSCTX_ALL, IID_IMMDeviceEnumerator,
           (void**)&pEnumerator);
    EXIT_ON_ERROR(hr)

    hr = pEnumerator->GetDefaultAudioEndpoint(
                        eCapture, eConsole, &pDevice);
    EXIT_ON_ERROR(hr)

    hr = pDevice->Activate(
                    IID_IAudioClient, CLSCTX_ALL,
                    NULL, (void**)&pAudioClient);
    EXIT_ON_ERROR(hr)

    hr = pAudioClient->GetMixFormat(&pwfx);
    EXIT_ON_ERROR(hr)

    hr = pAudioClient->Initialize(
                         AUDCLNT_SHAREMODE_SHARED,
                         0,
                         hnsRequestedDuration,
                         0,
                         pwfx,
                         NULL);
    EXIT_ON_ERROR(hr)

    // Get the size of the allocated buffer.
    hr = pAudioClient->GetBufferSize(&bufferFrameCount);
    EXIT_ON_ERROR(hr)

    hr = pAudioClient->GetService(
                         IID_IAudioCaptureClient,
                         (void**)&pCaptureClient);
    EXIT_ON_ERROR(hr)

    // Notify the audio sink which format to use.
    hr = pMySink->SetFormat(pwfx);
    EXIT_ON_ERROR(hr)

    // Calculate the actual duration of the allocated buffer.
    hnsActualDuration = (double)REFTIMES_PER_SEC *
                     bufferFrameCount / pwfx->nSamplesPerSec;

    hr = pAudioClient->Start();  // Start recording.
    EXIT_ON_ERROR(hr)

    // Each loop fills about half of the shared buffer.
    while (bDone == FALSE)
    {
        // Sleep for half the buffer duration.
        Sleep(hnsActualDuration/REFTIMES_PER_MILLISEC/2);

        hr = pCaptureClient->GetNextPacketSize(&packetLength);
        EXIT_ON_ERROR(hr)

        while (packetLength != 0)
        {
            // Get the available data in the shared buffer.
            hr = pCaptureClient->GetBuffer(
                                   &pData,
                                   &numFramesAvailable,
                                   &flags, NULL, NULL);
            EXIT_ON_ERROR(hr)

            if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
            {
                pData = NULL;  // Tell CopyData to write silence.
            }

            // Copy the available capture data to the audio sink.
            hr = pMySink->CopyData(
                              pData, numFramesAvailable, &bDone);
            EXIT_ON_ERROR(hr)

            hr = pCaptureClient->ReleaseBuffer(numFramesAvailable);
            EXIT_ON_ERROR(hr)

            hr = pCaptureClient->GetNextPacketSize(&packetLength);
            EXIT_ON_ERROR(hr)
        }
    }

    hr = pAudioClient->Stop();  // Stop recording.
    EXIT_ON_ERROR(hr)

Exit:
    CoTaskMemFree(pwfx);
    SAFE_RELEASE(pEnumerator)
    SAFE_RELEASE(pDevice)
    SAFE_RELEASE(pAudioClient)
    SAFE_RELEASE(pCaptureClient)

    return hr;
}