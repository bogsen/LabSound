// License: BSD 2 Clause
// Copyright (C) 2015+, The LabSound Authors. All rights reserved.

#pragma once

#include "LabSound/extended/LabSound.h"

#include <string>

using namespace lab;

struct SoundBufferFactory
{
    virtual SoundBuffer Create(const char * path, float sampleRate) const
    {
        return SoundBuffer(path, sampleRate);
    }

    virtual ~SoundBufferFactory() {}
};
