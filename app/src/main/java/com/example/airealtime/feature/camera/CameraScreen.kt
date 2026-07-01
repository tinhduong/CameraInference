package com.example.airealtime.feature.camera

import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Paint
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.example.airealtime.core.nativebridge.model.NativeStats

@Composable
fun CameraScreen(
    onNavigateToSettings: () -> Unit,
    viewModel: CameraViewModel = hiltViewModel()
) {
    val uiState by viewModel.uiState.collectAsStateWithLifecycle()

    Box(modifier = Modifier.fillMaxSize().background(Color.Black)) {
        
        // 1. Live Camera Preview wrapping SurfaceView
        AndroidView(
            modifier = Modifier.fillMaxSize(),
            factory = { context ->
                SurfaceView(context).apply {
                    holder.addCallback(object : SurfaceHolder.Callback {
                        override fun surfaceCreated(holder: SurfaceHolder) {
                            viewModel.onPreviewSurfaceReady(holder.surface)
                        }

                        override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {}

                        override fun surfaceDestroyed(holder: SurfaceHolder) {
                            viewModel.onPreviewSurfaceDestroyed()
                        }
                    })
                }
            }
        )

        // 2. Real-time Detection Overlay Canvas
        Canvas(modifier = Modifier.fillMaxSize()) {
            val canvasWidth = size.width
            val canvasHeight = size.height

            uiState.inferenceResult?.detections?.forEach { det ->
                // Map camera landscape coordinate space to portrait UI screen
                val left = det.yMin * canvasWidth
                val top = det.xMin * canvasHeight
                val right = det.yMax * canvasWidth
                val bottom = det.xMax * canvasHeight

                drawRect(
                    color = Color(0xFF00FFCC),
                    topLeft = Offset(left, top),
                    size = androidx.compose.ui.geometry.Size(right - left, bottom - top),
                    style = Stroke(width = 6f)
                )

                drawContext.canvas.nativeCanvas.drawText(
                    "${det.label} (${(det.confidence * 100).toInt()}%)",
                    left,
                    top - 15f,
                    android.graphics.Paint().apply {
                        color = Color(0xFF00FFCC).toArgb()
                        textSize = 42f
                        isFakeBoldText = true
                        flags = android.graphics.Paint.ANTI_ALIAS_FLAG
                    }
                )
            }
        }

        // 3. Floating Action buttons (Settings, Clear Stats)
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .statusBarsPadding()
                .padding(16.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            IconButton(
                onClick = { viewModel.clearStats() },
                modifier = Modifier
                    .background(Color.Black.copy(alpha = 0.6f), RoundedCornerShape(50.dp))
                    .border(1.dp, Color.White.copy(alpha = 0.2f), RoundedCornerShape(50.dp))
            ) {
                Text(
                    text = "Clear",
                    color = Color.White,
                    style = MaterialTheme.typography.labelMedium,
                    modifier = Modifier.padding(horizontal = 8.dp)
                )
            }

            IconButton(
                onClick = onNavigateToSettings,
                modifier = Modifier
                    .background(Color.Black.copy(alpha = 0.6f), RoundedCornerShape(50.dp))
                    .border(1.dp, Color.White.copy(alpha = 0.2f), RoundedCornerShape(50.dp))
            ) {
                Icon(
                    imageVector = Icons.Default.Settings,
                    contentDescription = "Open Settings",
                    tint = Color.White
                )
            }
        }

        // 4. Stats dashboard (Glassmorphic Overlay at bottom)
        Column(
            modifier = Modifier
                .align(Alignment.BottomCenter)
                .navigationBarsPadding()
                .padding(16.dp)
                .fillMaxWidth()
        ) {
            // Stats Panel
            StatsDashboard(stats = uiState.stats, uiState = uiState)

            Spacer(modifier = Modifier.height(16.dp))

            // Action Buttons
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceEvenly,
                verticalAlignment = Alignment.CenterVertically
            ) {
                // Photo Capture
                Button(
                    onClick = { viewModel.capturePhoto() },
                    colors = ButtonDefaults.buttonColors(
                        containerColor = Color(0xFFE91E63),
                        contentColor = Color.White
                    ),
                    shape = RoundedCornerShape(12.dp)
                ) {
                    Text("Capture Photo")
                }

                // Video Record Trigger
                Button(
                    onClick = {
                        if (uiState.isRecording) {
                            viewModel.stopVideoRecording()
                        } else {
                            viewModel.startVideoRecording()
                        }
                    },
                    colors = ButtonDefaults.buttonColors(
                        containerColor = if (uiState.isRecording) Color.Red else Color(0xFF2196F3),
                        contentColor = Color.White
                    ),
                    shape = RoundedCornerShape(12.dp)
                ) {
                    Text(if (uiState.isRecording) "Stop Video" else "Record Video")
                }
            }
        }
    }
}

@Composable
fun StatsDashboard(stats: NativeStats?, uiState: CameraUiState) {
    Card(
        shape = RoundedCornerShape(16.dp),
        colors = CardDefaults.cardColors(containerColor = Color.Black.copy(alpha = 0.75f)),
        modifier = Modifier
            .fillMaxWidth()
            .border(1.dp, Color.White.copy(alpha = 0.15f), RoundedCornerShape(16.dp))
    ) {
        Column(modifier = Modifier.padding(16.dp)) {
            // Header
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Text(
                    text = "AI Realtime Pipeline Monitor",
                    style = MaterialTheme.typography.titleMedium,
                    color = Color(0xFF00FFCC)
                )
                if (uiState.isRecording) {
                    Box(
                        modifier = Modifier
                            .background(Color.Red, RoundedCornerShape(4.dp))
                            .padding(horizontal = 6.dp, vertical = 2.dp)
                    ) {
                        Text(
                            text = "REC",
                            style = MaterialTheme.typography.labelSmall,
                            color = Color.White
                        )
                    }
                }
            }

            Divider(color = Color.White.copy(alpha = 0.15f), modifier = Modifier.padding(vertical = 8.dp))

            if (stats != null) {
                Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                    Column {
                        StatText(label = "Camera Input", value = "${stats.incomingFps} FPS")
                        StatText(label = "Processed", value = "${stats.processedFps} FPS")
                        StatText(label = "Dropped Frames", value = "${stats.droppedFrames}")
                        StatText(label = "Queue Depth", value = "${stats.queueDepth}")
                    }
                    Column {
                        StatText(label = "GPU Convert", value = String.format("%.1f ms", stats.avgGpuConversionMs))
                        StatText(label = "Inference", value = String.format("%.1f ms", stats.avgInferenceMs))
                        StatText(label = "Encode", value = String.format("%.1f ms", stats.avgEncodeMs))
                        StatText(label = "Latency E2E", value = String.format("%.1f ms", stats.avgEndToEndLatencyMs))
                    }
                }
            } else {
                Text(
                    text = "Initializing pipeline stats...",
                    style = MaterialTheme.typography.bodyMedium,
                    color = Color.Gray
                )
            }

            // Show save indicators
            uiState.lastPhotoPath?.let { path ->
                Spacer(modifier = Modifier.height(4.dp))
                Text(
                    text = "Photo saved: .../${path.substringAfterLast("/")}",
                    style = MaterialTheme.typography.bodySmall,
                    color = Color.LightGray
                )
            }
            uiState.lastVideoPath?.let { path ->
                Spacer(modifier = Modifier.height(4.dp))
                Text(
                    text = "Video saved: .../${path.substringAfterLast("/")}",
                    style = MaterialTheme.typography.bodySmall,
                    color = Color.LightGray
                )
            }
            uiState.errorMessage?.let { error ->
                Spacer(modifier = Modifier.height(4.dp))
                Text(
                    text = "Error: $error",
                    style = MaterialTheme.typography.bodySmall,
                    color = Color.Red
                )
            }
        }
    }
}

@Composable
fun StatText(label: String, value: String) {
    Row(
        modifier = Modifier.padding(vertical = 2.dp),
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Text(text = "$label:", style = MaterialTheme.typography.bodyMedium, color = Color.Gray)
        Text(text = value, style = MaterialTheme.typography.bodyMedium, color = Color.White)
    }
}
