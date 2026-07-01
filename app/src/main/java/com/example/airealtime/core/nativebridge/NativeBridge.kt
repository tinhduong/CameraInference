package com.example.airealtime.core.nativebridge

import com.example.airealtime.core.nativebridge.model.InferenceResult
import com.example.airealtime.core.nativebridge.model.NativeConfig
import com.example.airealtime.core.nativebridge.model.NativeStats
import java.nio.ByteBuffer

class NativeBridge {
    companion object {
        init {
            System.loadLibrary("airealtime")
        }
    }

    external fun initNativePipeline(config: NativeConfig): Boolean
    external fun startNativePipeline(): Boolean
    external fun stopNativePipeline()
    external fun releaseNativePipeline()
    external fun updateNativeConfig(config: NativeConfig)

    external fun processFrameSync(
        frameId: Long,
        timestampNs: Long,
        width: Int,
        height: Int,
        rotationDegrees: Int,
        format: Int,
        yBuffer: ByteBuffer,
        yRowStride: Int,
        yPixelStride: Int,
        uBuffer: ByteBuffer,
        uRowStride: Int,
        uPixelStride: Int,
        vBuffer: ByteBuffer,
        vRowStride: Int,
        vPixelStride: Int
    ): InferenceResult?

    external fun enqueueFrameAsync(
        frameId: Long,
        timestampNs: Long,
        width: Int,
        height: Int,
        rotationDegrees: Int,
        format: Int,
        yBuffer: ByteBuffer,
        yRowStride: Int,
        yPixelStride: Int,
        uBuffer: ByteBuffer,
        uRowStride: Int,
        uPixelStride: Int,
        vBuffer: ByteBuffer,
        vRowStride: Int,
        vPixelStride: Int
    ): Boolean

    external fun getLatestResult(): InferenceResult?
    external fun getNativeStats(): NativeStats?
    external fun clearNativeStats()
}
