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
        &g_redAlert,
        &g_tremolo,
        &g_grooveExample
};

extern "C" {

JNIEXPORT void JNICALL
Java_com_labsound_examples_Examples_nativeInit(JNIEnv* env, jclass type, jobject assetManager) {
    AAssetManager* mgr = AAssetManager_fromJava(env, assetManager);
}

JNIEXPORT jint JNICALL
Java_com_labsound_examples_Examples_nativeGetExamplesCount(JNIEnv* env, jclass type) {
    return sizeof(g_runnbleExamples) / sizeof(g_runnbleExamples[0]);
}

JNIEXPORT void JNICALL
Java_com_labsound_examples_Examples_nativePlayExample(JNIEnv* env, jclass type, jint index) {
    g_runnbleExamples[index]->PlayExample();
}

}