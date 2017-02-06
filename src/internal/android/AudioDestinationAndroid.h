// License: BSD 3 Clause
// Copyright (C) 2010, Google Inc. All rights reserved.
// Copyright (C) 2015+, The LabSound Authors. All rights reserved.

#ifndef AudioDestinationAndroid_h
#define AudioDestinationAndroid_h

#include "internal/AudioBus.h"
#include "internal/AudioDestination.h"

namespace lab {
class AudioDestinationAndroid : public AudioDestination {
public:
    AudioDestinationAndroid(AudioIOCallback&, float sampleRate, size_t bufferFrames, bool enableInput, bool enableOutput);
    virtual ~AudioDestinationAndroid();

    virtual void start();
    virtual void stop();
    bool isPlaying() { return m_isPlaying; }

    float sampleRate() const { return m_sampleRate; }

    bool render(short int* audioIO, size_t numberOfFrames);

    struct Internals; // LabSound
private:
    AudioIOCallback& m_callback;
    AudioBus m_renderBus;
    AudioBus m_inputBus;

    float m_sampleRate;
    bool m_isPlaying;

    Internals* m_internals; // LabSound
};
}

#endif //AudioDestinationAndroid_h
