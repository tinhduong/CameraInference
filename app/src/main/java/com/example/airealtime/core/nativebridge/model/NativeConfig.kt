package com.example.airealtime.core.nativebridge.model

data class NativeConfig(
    val mode: Int = 1,              // 0: SYNC, 1: ASYNC
    val rgbMode: Int = 0,           // 0: GPU_TEXTURE, 1: CPU_RGBA_FROM_GPU
    val aiEnabled: Boolean = true,
    val encodeEnabled: Boolean = true,
    val gpuConversionEnabled: Boolean = true,
    val queueCapacity: Int = 3,
    val dropPolicy: Int = 0,         // 0: DROP_OLDEST, 1: DROP_LATEST
    val mockInferenceDelayMs: Int = 10,
    val mockEncodeDelayMs: Int = 5
)
