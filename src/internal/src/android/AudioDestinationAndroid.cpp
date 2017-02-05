// License: BSD 3 Clause
// Copyright (C) 2010, Google Inc. All rights reserved.
// Copyright (C) 2015+, The LabSound Authors. All rights reserved.

#include "LabSound/core/AudioIOCallback.h"
#include <LabSound/core/AudioNode.h>

#include "internal/android/AudioDestinationAndroid.h"
#include <internal/VectorMath.h>

#include <libnyquist/Common.h>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <SLES/OpenSLES_AndroidConfiguration.h>

#include <unistd.h>


namespace lab {

const size_t kProcessingSizeInFrames = AudioNode::ProcessingSizeInFrames;
const float kLowThreshold = -1;
const float kHighThreshold = 1;

struct AudioDestinationAndroid::Internals
{
    void * clientdata;

    SLObjectItf openSLEngine;
    SLObjectItf outputMix;
    SLObjectItf outputBufferQueue;
    SLObjectItf inputBufferQueue;
    SLAndroidSimpleBufferQueueItf outputBufferQueueInterface;
    SLAndroidSimpleBufferQueueItf inputBufferQueueInterface;

    short int * fifobuffer;
    short int * silence;

    int sampleRate;
    int bufferFrames;
    int silenceSamples;
    int latencySamples;
    int numBuffers;
    int bufferStep;
    int readBufferIndex;
    int writeBufferIndex;

    bool hasOutput = false;
    bool hasInput = false;
};

// The entire operation is based on two Android Simple Buffer Queues, one for the audio input and one for the audio output.
static void startQueues(AudioDestinationAndroid::Internals* internals)
{
    if (internals->inputBufferQueue)
    {
        SLRecordItf recordInterface;
        (*internals->inputBufferQueue)->GetInterface(internals->inputBufferQueue, SL_IID_RECORD, &recordInterface);
        (*recordInterface)->SetRecordState(recordInterface, SL_RECORDSTATE_RECORDING);
    }
    if (internals->outputBufferQueue)
    {
        SLPlayItf outputPlayInterface;
        (*internals->outputBufferQueue)->GetInterface(internals->outputBufferQueue, SL_IID_PLAY, &outputPlayInterface);
        (*outputPlayInterface)->SetPlayState(outputPlayInterface, SL_PLAYSTATE_PLAYING);
    }
}

// Stopping the Simple Buffer Queues.
static void stopQueues(AudioDestinationAndroid::Internals* internals)
{
    if (internals->outputBufferQueue)
    {
        SLPlayItf outputPlayInterface;
        (*internals->outputBufferQueue)->GetInterface(internals->outputBufferQueue, SL_IID_PLAY, &outputPlayInterface);
        (*outputPlayInterface)->SetPlayState(outputPlayInterface, SL_PLAYSTATE_STOPPED);
        usleep(50000);
    }
    if (internals->inputBufferQueue)
    {
        SLRecordItf recordInterface;
        (*internals->inputBufferQueue)->GetInterface(internals->inputBufferQueue, SL_IID_RECORD, &recordInterface);
        (*recordInterface)->SetRecordState(recordInterface, SL_RECORDSTATE_STOPPED);
        usleep(50000);
    }
}

// This is called periodically by the input audio queue. Audio input is received from the media server at this point.
static void AudioDestinationAndroid_InputCallback(SLAndroidSimpleBufferQueueItf caller, void *pContext)
{
    AudioDestinationAndroid::Internals* internals = static_cast<AudioDestinationAndroid::Internals*>(pContext);
    AudioDestinationAndroid* destination = static_cast<AudioDestinationAndroid*>(internals->clientdata);

    short int *buffer = internals->fifobuffer + internals->writeBufferIndex * internals->bufferStep;
    if (internals->writeBufferIndex < internals->numBuffers - 1)
    {
        internals->writeBufferIndex++;
    }
    else
    {
        internals->writeBufferIndex = 0;
    }

    if (!internals->hasOutput)
    { // When there is no audio output configured.
        int buffersAvailable = internals->writeBufferIndex - internals->readBufferIndex;
        if (buffersAvailable < 0)
        {
            buffersAvailable = internals->numBuffers - (internals->readBufferIndex - internals->writeBufferIndex);
        }
        if (buffersAvailable * internals->bufferFrames >= internals->latencySamples)
        { // if we have enough audio input available
            destination->render(internals->fifobuffer + internals->readBufferIndex * internals->bufferStep, static_cast<size_t>(internals->bufferFrames));
            if (internals->readBufferIndex < internals->numBuffers - 1)
            {
                internals->readBufferIndex++;
            }
            else
            {
                internals->readBufferIndex = 0;
            }
        }
    }

    (*caller)->Enqueue(caller, buffer, (SLuint32)internals->bufferFrames * sizeof(short));
}

// This is called periodically by the output audio queue. Audio for the user should be provided here.
static void AudioDestinationAndroid_OutputCallback(SLAndroidSimpleBufferQueueItf caller, void *pContext)
{
    AudioDestinationAndroid::Internals* internals = static_cast<AudioDestinationAndroid::Internals*>(pContext);
    AudioDestinationAndroid* destination = static_cast<AudioDestinationAndroid*>(internals->clientdata);

    int buffersAvailable = internals->writeBufferIndex - internals->readBufferIndex;
    if (buffersAvailable < 0)
    {
        buffersAvailable = internals->numBuffers - (internals->readBufferIndex - internals->writeBufferIndex);
    }
    short int* output = internals->fifobuffer + internals->readBufferIndex * internals->bufferStep;

    if (internals->hasInput)
    { // If audio input is enabled.
        if (buffersAvailable * internals->bufferFrames >= internals->latencySamples)
        { // if we have enough audio input available
            if (!destination->render(output, static_cast<size_t>(internals->bufferFrames)))
            {
                memset(output, 0, (size_t)internals->bufferFrames * sizeof(short));
                internals->silenceSamples += internals->bufferFrames;
            }
            else
            {
                internals->silenceSamples = 0;
            }
        }
        else
        {
            output = NULL; // dropout, not enough audio input
        }
    }
    else
    { // If audio input is not enabled.
        short int *audioToGenerate = internals->fifobuffer + internals->writeBufferIndex * internals->bufferStep;
        if (!destination->render(audioToGenerate, static_cast<size_t>(internals->bufferFrames)))
        {
            memset(audioToGenerate, 0, (size_t)internals->bufferFrames * sizeof(short));
            internals->silenceSamples += internals->bufferFrames;
        }
        else
        {
            internals->silenceSamples = 0;
        }
        if (internals->writeBufferIndex < internals->numBuffers - 1)
        {
            internals->writeBufferIndex++;
        }
        else
        {
            internals->writeBufferIndex = 0;
        }
        if ((buffersAvailable + 1) * internals->bufferFrames < internals->latencySamples)
        {
            output = NULL; // dropout, not enough audio generated
        }
    }

    if (output)
    {
        if (internals->readBufferIndex < internals->numBuffers - 1)
        {
            internals->readBufferIndex++;
        }
        else
        {
            internals->readBufferIndex = 0;
        }
    }

    (*caller)->Enqueue(caller, output ? output : internals->silence, (SLuint32)internals->bufferFrames * sizeof(short));
}

AudioDestination* AudioDestination::MakePlatformAudioDestination(AudioIOCallback& callback, unsigned numberOfOutputChannels, float sampleRate)
{
    return new AudioDestinationAndroid(callback, sampleRate, kProcessingSizeInFrames, false, true);
}

float AudioDestination::hardwareSampleRate()
{
    return 44100;
}

unsigned long AudioDestination::maxChannelCount()
{
    return 2;
}

AudioDestinationAndroid::AudioDestinationAndroid(AudioIOCallback& callback, float sampleRate, size_t bufferFrames, bool enableInput, bool enableOutput)
        : m_sampleRate(sampleRate)
        , m_callback(callback)
        , m_renderBus(2, kProcessingSizeInFrames, false)
        , m_isPlaying(false)
{
    m_renderBuffer.reset(new AudioFloatArray(bufferFrames * 2));

    static const SLboolean requireds[2] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_FALSE };

    m_internals = new Internals();
    memset(m_internals, 0, sizeof(Internals));
    m_internals->clientdata = this;
    m_internals->sampleRate = static_cast<int>(sampleRate);
    m_internals->bufferFrames = bufferFrames;
    m_internals->silence = (short int *)malloc((size_t)bufferFrames * sizeof(short));
    memset(m_internals->silence, 0, (size_t)bufferFrames * sizeof(short));
    m_internals->latencySamples = bufferFrames * 2;

    m_internals->numBuffers = (m_internals->latencySamples / bufferFrames) * 2;
    if (m_internals->numBuffers < 16) {
        m_internals->numBuffers = 16;
    }
    m_internals->bufferStep = (bufferFrames + 64) * 2;
    size_t fifoBufferSizeBytes = m_internals->numBuffers * m_internals->bufferStep * sizeof(short);
    m_internals->fifobuffer = (short int *)malloc(fifoBufferSizeBytes);
    memset(m_internals->fifobuffer, 0, fifoBufferSizeBytes);

    SLresult result;

    SLEngineOption option[] = {{ SL_ENGINEOPTION_THREADSAFE, static_cast<SLuint32>(SL_BOOLEAN_TRUE) }};
    result = slCreateEngine(&m_internals->openSLEngine, 1, option, 0, NULL, NULL);
    assert(SL_RESULT_SUCCESS == result);
    result = (*m_internals->openSLEngine)->Realize(m_internals->openSLEngine, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    SLEngineItf openSLEngineInterface = NULL;
    result = (*m_internals->openSLEngine)->GetInterface(m_internals->openSLEngine, SL_IID_ENGINE, &openSLEngineInterface);
    assert(SL_RESULT_SUCCESS == result);

    result = (*openSLEngineInterface)->CreateOutputMix(openSLEngineInterface, &m_internals->outputMix, 0, NULL, NULL);
    assert(SL_RESULT_SUCCESS == result);
    result = (*m_internals->outputMix)->Realize(m_internals->outputMix, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    SLDataLocator_OutputMix outputMixLocator = { SL_DATALOCATOR_OUTPUTMIX, m_internals->outputMix };

    if (enableInput)
    {
        SLDataLocator_IODevice deviceInputLocator = { SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT, SL_DEFAULTDEVICEID_AUDIOINPUT, NULL };
        SLDataSource inputSource = { &deviceInputLocator, NULL };
        SLDataLocator_AndroidSimpleBufferQueue inputLocator = { SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2 };
        SLDataFormat_PCM inputFormat = { SL_DATAFORMAT_PCM, 2, (SLuint32)sampleRate * 1000, SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16, SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT, SL_BYTEORDER_LITTLEENDIAN };
        SLDataSink inputSink = { &inputLocator, &inputFormat };
        const SLInterfaceID inputInterfaces[2] = { SL_IID_ANDROIDSIMPLEBUFFERQUEUE, SL_IID_ANDROIDCONFIGURATION };
        result = (*openSLEngineInterface)->CreateAudioRecorder(openSLEngineInterface, &m_internals->inputBufferQueue, &inputSource, &inputSink, 2, inputInterfaces, requireds);
        assert(SL_RESULT_SUCCESS == result);

        int inputStreamType = (int)SL_ANDROID_RECORDING_PRESET_VOICE_RECOGNITION; // Configure the voice recognition preset which has no signal processing for lower latency.
        SLAndroidConfigurationItf inputConfiguration;
        if ((*m_internals->inputBufferQueue)->GetInterface(m_internals->inputBufferQueue, SL_IID_ANDROIDCONFIGURATION, &inputConfiguration) == SL_RESULT_SUCCESS) {
            SLuint32 st = (SLuint32)inputStreamType;
            (*inputConfiguration)->SetConfiguration(inputConfiguration, SL_ANDROID_KEY_RECORDING_PRESET, &st, sizeof(SLuint32));
        };

        result = (*m_internals->inputBufferQueue)->Realize(m_internals->inputBufferQueue, SL_BOOLEAN_FALSE);
        assert(SL_RESULT_SUCCESS == result);

        m_internals->hasInput = true;
    }

    if (enableOutput)
    {
        SLDataLocator_AndroidSimpleBufferQueue outputLocator = { SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2 };
        SLDataFormat_PCM outputFormat = { SL_DATAFORMAT_PCM, 2, (SLuint32)sampleRate * 1000, SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16, SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT, SL_BYTEORDER_LITTLEENDIAN };
        SLDataSource outputSource = { &outputLocator, &outputFormat };
        const SLInterfaceID outputInterfaces[3] = { SL_IID_BUFFERQUEUE, SL_IID_ANDROIDCONFIGURATION, SL_IID_PLAYBACKRATE };
        SLDataSink outputSink = { &outputMixLocator, NULL };
        result = (*openSLEngineInterface)->CreateAudioPlayer(openSLEngineInterface, &m_internals->outputBufferQueue, &outputSource, &outputSink, 3, outputInterfaces, requireds);
        assert(SL_RESULT_SUCCESS == result);

        // Configure the stream type.
        int outputStreamType = SL_ANDROID_STREAM_MEDIA;
        SLAndroidConfigurationItf outputConfiguration;
        if ((*m_internals->outputBufferQueue)->GetInterface(m_internals->outputBufferQueue, SL_IID_ANDROIDCONFIGURATION, &outputConfiguration) == SL_RESULT_SUCCESS) {
            SLint32 st = (SLint32)outputStreamType;
            (*outputConfiguration)->SetConfiguration(outputConfiguration, SL_ANDROID_KEY_STREAM_TYPE, &st, sizeof(SLint32));
        };

        result = (*m_internals->outputBufferQueue)->Realize(m_internals->outputBufferQueue, SL_BOOLEAN_FALSE);
        assert(SL_RESULT_SUCCESS == result);

        SLPlaybackRateItf playerPlaybackRate;
        result = (*m_internals->outputBufferQueue)->GetInterface(m_internals->outputBufferQueue, SL_IID_PLAYBACKRATE, &playerPlaybackRate);
        assert(SL_RESULT_SUCCESS == result);
        SLpermille defaultRate;
        result = (*playerPlaybackRate)->GetRate(playerPlaybackRate, &defaultRate);
        assert(SL_RESULT_SUCCESS == result);
        SLuint32 defaultProperties;
        result = (*playerPlaybackRate)->GetProperties(playerPlaybackRate, &defaultProperties);
        assert(SL_RESULT_SUCCESS == result);

        SLpermille halfRate = 1000 / 2;
        if (defaultRate != halfRate)
        {
            result = (*playerPlaybackRate)->SetRate(playerPlaybackRate, halfRate);
            assert(SL_RESULT_SUCCESS == result);
        }

        m_internals->hasOutput = true;
    }

    if (m_internals->hasInput) {
        result = (*m_internals->inputBufferQueue)->GetInterface(m_internals->inputBufferQueue, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &m_internals->inputBufferQueueInterface);
        assert(SL_RESULT_SUCCESS == result);
        result = (*m_internals->inputBufferQueueInterface)->RegisterCallback(m_internals->inputBufferQueueInterface, AudioDestinationAndroid_InputCallback, m_internals);
        result = (*m_internals->inputBufferQueueInterface)->Enqueue(m_internals->inputBufferQueueInterface, m_internals->fifobuffer, (SLuint32)bufferFrames * sizeof(short));
        assert(SL_RESULT_SUCCESS == result);
    }

    if (m_internals->hasOutput) {
        result = (*m_internals->outputBufferQueue)->GetInterface(m_internals->outputBufferQueue, SL_IID_BUFFERQUEUE, &m_internals->outputBufferQueueInterface);
        assert(SL_RESULT_SUCCESS == result);
        result = (*m_internals->outputBufferQueueInterface)->RegisterCallback(m_internals->outputBufferQueueInterface, AudioDestinationAndroid_OutputCallback, m_internals);
        assert(SL_RESULT_SUCCESS == result);
        result = (*m_internals->outputBufferQueueInterface)->Enqueue(m_internals->outputBufferQueueInterface, m_internals->fifobuffer, (SLuint32)bufferFrames * sizeof(short));
        assert(SL_RESULT_SUCCESS == result);
    }
}

AudioDestinationAndroid::~AudioDestinationAndroid()
{
    stopQueues(m_internals);
    if (m_internals->outputBufferQueue)
    {
        (*m_internals->outputBufferQueue)->Destroy(m_internals->outputBufferQueue);
    }
    if (m_internals->inputBufferQueue)
    {
        (*m_internals->inputBufferQueue)->Destroy(m_internals->inputBufferQueue);
    }
    (*m_internals->outputMix)->Destroy(m_internals->outputMix);
    (*m_internals->openSLEngine)->Destroy(m_internals->openSLEngine);
    free(m_internals->fifobuffer);
    free(m_internals->silence);
    delete m_internals;
}

void AudioDestinationAndroid::start()
{
    m_isPlaying = true;
    startQueues(m_internals);
}

void AudioDestinationAndroid::stop()
{
    m_isPlaying = false;
    stopQueues(m_internals);
}

bool AudioDestinationAndroid::render(short int* audioIO, size_t numberOfFrames)
{
    if (!m_isPlaying)
    {
        return false;
    }

    if (m_renderBus.isFirstTime())
    {
        m_renderBus.setChannelMemory(0, m_renderBuffer->data(), numberOfFrames);
        m_renderBus.setChannelMemory(1, m_renderBuffer->data() + (numberOfFrames), numberOfFrames);
    }

    m_callback.render(0, &m_renderBus, numberOfFrames);

    // Clamp values at 0db (i.e., [-1.0, 1.0])
    for (size_t i = 0; i < m_renderBus.numberOfChannels(); ++i)
    {
        AudioChannel* channel = m_renderBus.channel(i);
        VectorMath::vclip(
                channel->data(), 1, &kLowThreshold, &kHighThreshold,
                channel->mutableData(), 1, numberOfFrames);
    }

    nqr::ConvertFromFloat32(
            (uint8_t *) audioIO,
            m_renderBuffer->data(),
            numberOfFrames,
            nqr::PCMFormat::PCM_16,
            nqr::DitherType::DITHER_NONE
    );

    return true;
}


}