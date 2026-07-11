package com.example.camerapipe

import android.view.Surface

object JniBridge {
    init {
        System.loadLibrary("native-lib")
    }

    interface AiCallback {
        fun onAiResult(label: String, score: Float, latencyMs: Long)
    }

    private var callback: AiCallback? = null

    fun setAiCallback(cb: AiCallback?) {
        callback = cb
    }

    // Invoked by native C++ thread to deliver inference results
    @JvmStatic
    fun triggerAiCallback(label: String, score: Float, latencyMs: Long) {
        callback?.onAiResult(label, score, latencyMs)
    }

    // Native lifecycle methods
    external fun nativeInit(aiWidth: Int, aiHeight: Int): Long
    external fun nativeRelease(enginePtr: Long)
    
    // Preview surface lifecycle
    external fun nativeOnSurfaceCreated(enginePtr: Long, surface: Surface)
    external fun nativeOnSurfaceDestroyed(enginePtr: Long)
    
    // Encoder surface lifecycle
    external fun nativeSetEncoderSurface(enginePtr: Long, surface: Surface?, width: Int, height: Int)
    
    // Render loop controls
    external fun nativeStartPipeline(enginePtr: Long, surfaceTexture: Any): Int // returns generated GL texture ID
    external fun nativeStopPipeline(enginePtr: Long)
    external fun nativeOnFrameAvailable(enginePtr: Long)
    external fun nativeSetCameraResolution(enginePtr: Long, width: Int, height: Int)
}
