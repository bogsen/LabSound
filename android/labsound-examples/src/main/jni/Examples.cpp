#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include "ExampleBaseApp.h"

#include "Simple.h"
#include "ConnectDisconnect.h"
#include "OfflineRender.h"
#include "ConvolutionReverb.h"
#include "MicrophoneDalek.h"
#include "MicrophoneLoopback.h"
#include "MicrophoneReverb.h"
#include "Rhythm.h"
#include "RhythmAndFilters.h"
#include "PeakCompressor.h"
#include "StereoPanning.h"
#include "Spatialization.h"
#include "Tremolo.h"
#include "RedAlert.h"
#include "InfiniteFM.h"
#include "Groove.h"
#include "Validation.h"


SimpleApp g_simpleExample;
ConnectDisconnectApp g_connectApp;
OfflineRenderApp g_offlineRenderApp;
ConvolutionReverbApp g_convolutionReverbExample;
MicrophoneDalekApp g_microphoneDalekApp;
MicrophoneLoopbackApp g_microphoneLoopback;
MicrophoneReverbApp g_microphoneReverb;
PeakCompressorApp g_peakCompressor;
RedAlertApp g_redAlert;
RhythmApp g_rhythm;
RhythmAndFiltersApp g_rhythmAndFilters;
SpatializationApp g_spatialization;
TremoloApp g_tremolo;
ValidationApp g_validation;
InfiniteFMApp g_infiniteFM;
StereoPanningApp g_stereoPanning;
GrooveApp g_grooveExample;

LabSoundExampleApp * g_runnbleExamples[] = {
//        &g_simpleExample,
        // Disabled, requires loading ogg from disk,
        // see https://github.com/ddiakopoulos/libnyquist/blob/9130de77b745d4d6f1ab1dfa447420efa1e16980/src/VorbisDecoder.cpp#L31
        // &g_convolutionReverbExample,
        &g_microphoneLoopback,
//        &g_microphoneReverb,
        &g_peakCompressor,
        &g_rhythm,
        &g_rhythmAndFilters,
//        // Disabled, loads hrtf files from disk internally
//        // &g_spatialization,
        &g_stereoPanning,
//        &g_redAlert,
//        &g_tremolo,
//        &g_grooveExample
};

AAssetManager * g_assetManager;

static int android_read(void * cookie, char * buf, int size)
{
    return AAsset_read((AAsset*)cookie, buf, size);
}

static int android_write(void * cookie, const char * buf, int size)
{
    return EACCES; // can't provide write access to the apk
}

static fpos_t android_seek(void * cookie, fpos_t offset, int whence)
{
    return AAsset_seek((AAsset*)cookie, offset, whence);
}

static int android_close(void * cookie) {
    AAsset_close((AAsset*)cookie);
    return 0;
}

static FILE* android_fopen(const char * filename, const char * mode)
{
    if (mode[0] == 'w') {
        return NULL;
    }

    AAsset* asset = AAssetManager_open(g_assetManager, filename, 0);
    if (!asset) {
        return NULL;
    }
    return funopen(asset, android_read, android_write, android_seek, android_close);
}

struct AssetManagerSoundBufferFactory : public SoundBufferFactory
{
    virtual lab::SoundBuffer Create(const char * path, float sampleRate) const override
    {
        FILE * audioFile = android_fopen(path, "rb");

        if (!audioFile)
        {
            throw std::runtime_error("file not found");
        }

        fseek(audioFile, 0, SEEK_END);
        size_t lengthInBytes = (size_t) ftell(audioFile);
        fseek(audioFile, 0, SEEK_SET);

        // Allocate temporary buffer
        std::vector<uint8_t> fileBuffer(lengthInBytes);

        size_t elementsRead = fread(fileBuffer.data(), 1, lengthInBytes, audioFile);

        if (elementsRead == 0 || fileBuffer.size() < 64)
        {
            throw std::runtime_error("error reading file or file too small");
        }

        fclose(audioFile);

        return lab::SoundBuffer(std::move(fileBuffer), ParsePathForExtension(path), sampleRate);
    }

    static std::string ParsePathForExtension(const std::string & path)
    {
        if (path.find_last_of(".") != std::string::npos)
            return path.substr(path.find_last_of(".") + 1);

        return std::string("");
    }
};

extern "C"
{

JNIEXPORT void JNICALL
Java_com_labsound_examples_Examples_nativeInit(JNIEnv * env, jclass type, jobject assetManager)
{
    g_assetManager = AAssetManager_fromJava(env, assetManager);
}

JNIEXPORT jint JNICALL
Java_com_labsound_examples_Examples_nativeGetExamplesCount(JNIEnv * env, jclass type)
{
    return sizeof(g_runnbleExamples) / sizeof(g_runnbleExamples[0]);
}

JNIEXPORT void JNICALL
Java_com_labsound_examples_Examples_nativePlayExample(JNIEnv * env, jclass type, jint index)
{
    g_runnbleExamples[index]->PlayExample(AssetManagerSoundBufferFactory());
}

}