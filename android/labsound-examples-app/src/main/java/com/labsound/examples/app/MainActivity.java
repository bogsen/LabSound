package com.labsound.examples.app;

import android.content.Context;
import android.media.AudioManager;
import android.os.Build;
import android.support.annotation.NonNull;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;

import com.labsound.examples.Examples;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.FutureTask;
import java.util.concurrent.ThreadFactory;

public class MainActivity extends AppCompatActivity {

    private static final ExecutorService EXECUTOR_SERVICE = Executors.newSingleThreadExecutor();

    private Future<?> playExampleTask;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        String samplerateString = null;
        String buffersizeString = null;

        if (Build.VERSION.SDK_INT >= 17) {
            AudioManager audioManager = (AudioManager) this.getSystemService(Context.AUDIO_SERVICE);
            samplerateString = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
            buffersizeString = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
        }
        if (samplerateString == null) {
            samplerateString = "44100";
        }
        if (buffersizeString == null) {
            buffersizeString = "512";
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        final Context context = getApplicationContext();
        playExampleTask = EXECUTOR_SERVICE.submit(new Runnable() {
            @Override
            public void run() {
                Examples examples = Examples.create(context);
                while (!Thread.currentThread().isInterrupted()) {
                    examples.playNext();
                }
            }
        });
    }

    @Override
    protected void onPause() {
        if (playExampleTask != null) {
            playExampleTask.cancel(true);
            playExampleTask = null;
        }
        super.onPause();
    }
}
