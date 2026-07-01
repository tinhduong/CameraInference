package com.example.airealtime.core.nativebridge.model

data class DetectionItem(
    val xMin: Float,
    val yMin: Float,
    val xMax: Float,
    val yMax: Float,
    val confidence: Float,
    val labelId: Int,
    val label: String
)

data class InferenceResult(
    val frameId: Long,
    val timestampNs: Long,
    val width: Int,
    val height: Int,
    val rotationDegrees: Int,
    val durationMs: Float,
    val detections: List<DetectionItem>
)
