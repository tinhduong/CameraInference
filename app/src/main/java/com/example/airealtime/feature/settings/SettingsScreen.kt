package com.example.airealtime.feature.settings

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle

@Composable
fun SettingsScreen(
    onNavigateBack: () -> Unit,
    viewModel: SettingsViewModel = hiltViewModel()
) {
    val state by viewModel.uiState.collectAsStateWithLifecycle()

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Pipeline Settings") },
                navigationIcon = {
                    IconButton(onClick = onNavigateBack) {
                        Icon(imageVector = Icons.Default.ArrowBack, contentDescription = "Back")
                    }
                },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = MaterialTheme.colorScheme.surfaceVariant,
                    titleContentColor = MaterialTheme.colorScheme.onSurfaceVariant
                )
            )
        }
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .background(MaterialTheme.colorScheme.background)
                .verticalScroll(rememberScrollState())
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            
            // 1. Mode Configuration Card
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text("Processing Mode", style = MaterialTheme.typography.titleMedium)
                    Spacer(modifier = Modifier.height(8.dp))
                    
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Button(
                            onClick = { viewModel.setProcessingMode(0) }, // SYNC
                            colors = ButtonDefaults.buttonColors(
                                containerColor = if (state.config.mode == 0) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surfaceVariant,
                                contentColor = if (state.config.mode == 0) MaterialTheme.colorScheme.onPrimary else MaterialTheme.colorScheme.onSurfaceVariant
                            ),
                            modifier = Modifier.weight(1f)
                        ) {
                            Text("SYNC")
                        }

                        Button(
                            onClick = { viewModel.setProcessingMode(1) }, // ASYNC
                            colors = ButtonDefaults.buttonColors(
                                containerColor = if (state.config.mode == 1) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surfaceVariant,
                                contentColor = if (state.config.mode == 1) MaterialTheme.colorScheme.onPrimary else MaterialTheme.colorScheme.onSurfaceVariant
                            ),
                            modifier = Modifier.weight(1f)
                        ) {
                            Text("ASYNC")
                        }
                    }
                    
                    Spacer(modifier = Modifier.height(8.dp))
                    Text(
                        text = if (state.config.mode == 0) 
                            "SYNC Mode blocks the camera thread until C++ frame processing completes. Best for deterministic benchmarks." 
                            else "ASYNC Mode pushes camera frames into a bounded queue. Worker threads process them. Keeps UI responsive.",
                        style = MaterialTheme.typography.bodySmall,
                        color = Color.Gray
                    )
                }
            }

            // 2. Hardware / Rendering Toggles Card
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                    Text("Hardware Stages", style = MaterialTheme.typography.titleMedium)
                    
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text("GPU conversion (YUV -> RGB)")
                        Switch(
                            checked = state.config.gpuConversionEnabled,
                            onCheckedChange = { viewModel.setGpuConversionEnabled(it) }
                        )
                    }

                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text("AI Inference simulation")
                        Switch(
                            checked = state.config.aiEnabled,
                            onCheckedChange = { viewModel.setAiEnabled(it) }
                        )
                    }

                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text("Mock encode stage")
                        Switch(
                            checked = state.config.encodeEnabled,
                            onCheckedChange = { viewModel.setEncodeEnabled(it) }
                        )
                    }
                }
            }

            // 3. RGB Output Mode Card
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text("RGB Output Target", style = MaterialTheme.typography.titleMedium)
                    Spacer(modifier = Modifier.height(8.dp))
                    
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Button(
                            onClick = { viewModel.setRgbOutputMode(0) }, // GPU_TEXTURE
                            colors = ButtonDefaults.buttonColors(
                                containerColor = if (state.config.rgbMode == 0) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surfaceVariant,
                                contentColor = if (state.config.rgbMode == 0) MaterialTheme.colorScheme.onPrimary else MaterialTheme.colorScheme.onSurfaceVariant
                            ),
                            modifier = Modifier.weight(1f)
                        ) {
                            Text("GPU Texture")
                        }

                        Button(
                            onClick = { viewModel.setRgbOutputMode(1) }, // CPU_RGBA_FROM_GPU
                            colors = ButtonDefaults.buttonColors(
                                containerColor = if (state.config.rgbMode == 1) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surfaceVariant,
                                contentColor = if (state.config.rgbMode == 1) MaterialTheme.colorScheme.onPrimary else MaterialTheme.colorScheme.onSurfaceVariant
                            ),
                            modifier = Modifier.weight(1f)
                        ) {
                            Text("CPU RGBA")
                        }
                    }
                    Spacer(modifier = Modifier.height(8.dp))
                    Text(
                        text = if (state.config.rgbMode == 0) 
                            "Texture output keeps pixels on the GPU (optimal)." 
                            else "CPU output executes glReadPixels from offscreen FBO. Simulates CPU visualizer requirement.",
                        style = MaterialTheme.typography.bodySmall,
                        color = Color.Gray
                    )
                }
            }

            // 4. Processing Resolution Card
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text("Processing Frame Size", style = MaterialTheme.typography.titleMedium)
                    Spacer(modifier = Modifier.height(8.dp))
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Button(
                            onClick = { viewModel.setProcessingResolution(640, 480) },
                            colors = ButtonDefaults.buttonColors(
                                containerColor = if (state.processingWidth == 640) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surfaceVariant
                            ),
                            modifier = Modifier.weight(1f)
                        ) {
                            Text("640x480")
                        }
                        Button(
                            onClick = { viewModel.setProcessingResolution(1280, 720) },
                            colors = ButtonDefaults.buttonColors(
                                containerColor = if (state.processingWidth == 1280) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surfaceVariant
                            ),
                            modifier = Modifier.weight(1f)
                        ) {
                            Text("1280x720")
                        }
                    }
                }
            }

            // 5. Backpressure and queue policy Card (Async only)
            if (state.config.mode == 1) {
                Card(modifier = Modifier.fillMaxWidth()) {
                    Column(modifier = Modifier.padding(16.dp)) {
                        Text("Concurrent Queue Strategy", style = MaterialTheme.typography.titleMedium)
                        Spacer(modifier = Modifier.height(8.dp))

                        Text("Queue Capacity: ${state.config.queueCapacity}")
                        Slider(
                            value = state.config.queueCapacity.toFloat(),
                            onValueChange = { viewModel.setQueueCapacity(it.toInt()) },
                            valueRange = 1f..10f,
                            steps = 8
                        )

                        Spacer(modifier = Modifier.height(8.dp))
                        Text("Drop Policy", style = MaterialTheme.typography.titleMedium)
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            Button(
                                onClick = { viewModel.setDropPolicy(0) }, // DROP_OLDEST
                                colors = ButtonDefaults.buttonColors(
                                    containerColor = if (state.config.dropPolicy == 0) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surfaceVariant
                                ),
                                modifier = Modifier.weight(1f)
                            ) {
                                Text("Drop Oldest")
                            }

                            Button(
                                onClick = { viewModel.setDropPolicy(1) }, // DROP_LATEST
                                colors = ButtonDefaults.buttonColors(
                                    containerColor = if (state.config.dropPolicy == 1) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surfaceVariant
                                ),
                                modifier = Modifier.weight(1f)
                            ) {
                                Text("Drop Latest")
                            }
                        }
                    }
                }
            }

            // 6. Media Export Formats
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                    Text("Media File Outputs", style = MaterialTheme.typography.titleMedium)
                    
                    // Still Format
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text("Still Photo Format")
                        Row {
                            TextButton(
                                onClick = { viewModel.setStillFormat("JPEG") },
                                colors = ButtonDefaults.textButtonColors(
                                    contentColor = if (state.stillFormat == "JPEG") MaterialTheme.colorScheme.primary else Color.Gray
                                )
                            ) { Text("JPEG") }
                            TextButton(
                                onClick = { viewModel.setStillFormat("PNG") },
                                colors = ButtonDefaults.textButtonColors(
                                    contentColor = if (state.stillFormat == "PNG") MaterialTheme.colorScheme.primary else Color.Gray
                                )
                            ) { Text("PNG") }
                        }
                    }

                    // Video Encoder
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text("Video Codec")
                        Row {
                            TextButton(
                                onClick = { viewModel.setVideoFormat("H264") },
                                colors = ButtonDefaults.textButtonColors(
                                    contentColor = if (state.videoFormat == "H264") MaterialTheme.colorScheme.primary else Color.Gray
                                )
                            ) { Text("H.264") }
                            TextButton(
                                onClick = { viewModel.setVideoFormat("HEVC") },
                                colors = ButtonDefaults.textButtonColors(
                                    contentColor = if (state.videoFormat == "HEVC") MaterialTheme.colorScheme.primary else Color.Gray
                                )
                            ) { Text("HEVC") }
                        }
                    }
                }
            }

            // 7. Latency Simulators Card
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text("Simulated Stage Latencies", style = MaterialTheme.typography.titleMedium)
                    Spacer(modifier = Modifier.height(8.dp))

                    Text("AI Inference Delay: ${state.config.mockInferenceDelayMs} ms")
                    Slider(
                        value = state.config.mockInferenceDelayMs.toFloat(),
                        onValueChange = { viewModel.setMockInferenceDelay(it.toInt()) },
                        valueRange = 0f..50f,
                        steps = 10
                    )

                    Spacer(modifier = Modifier.height(8.dp))

                    Text("Encoder Bitrate Delay: ${state.config.mockEncodeDelayMs} ms")
                    Slider(
                        value = state.config.mockEncodeDelayMs.toFloat(),
                        onValueChange = { viewModel.setMockEncodeDelay(it.toInt()) },
                        valueRange = 0f..50f,
                        steps = 10
                    )
                }
            }
        }
    }
}
