package com.labsound.examples;

import android.content.Context;
import android.content.res.AssetManager;

public final class Examples {

    static {
        System.loadLibrary("labSoundExamples");
    }

    public static Examples create(Context context) {
        return new Examples(context);
    }

    private final int examplesCount;

    private int currentPlayIndex = -1;

    private Examples(Context context) {
        nativeInit(context.getAssets());
        examplesCount = nativeGetExamplesCount();
    }

    public Examples playNext() {
        play(currentPlayIndex >= (examplesCount - 1) ? 0 : currentPlayIndex + 1);
        return this;
    }

    private void play(int index) {
        nativePlayExample(index);
        currentPlayIndex = index;
    }

    private static synchronized native void nativeInit(AssetManager assetManager);
    private static synchronized native void nativePlayExample(int index);
    private static synchronized native int nativeGetExamplesCount();
}
