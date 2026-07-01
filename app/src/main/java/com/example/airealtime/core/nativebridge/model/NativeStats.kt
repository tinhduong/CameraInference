package com.example.airealtime.core.nativebridge.model

data class NativeStats(
    val incomingFps: Int,
    val processedFps: Int,
    val droppedFrames: Int,
    val queueDepth: Int,
    val avgInferenceMs: Float,
    val avgEncodeMs: Float,
    val avgGpuConversionMs: Float,
    val avgGpuReadbackMs: Float,
    val avgEndToEndLatencyMs: Float
)
