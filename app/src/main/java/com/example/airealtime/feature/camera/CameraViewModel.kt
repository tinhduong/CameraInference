package com.example.airealtime.feature.camera

import android.media.ImageReader
import android.util.Log
import android.view.Surface
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.example.airealtime.core.camera.CameraSessionController
import com.example.airealtime.core.data.SettingsRepository
import com.example.airealtime.core.media.RecordingController
import com.example.airealtime.core.media.StillCaptureController
import com.example.airealtime.core.nativebridge.NativeBridge
import com.example.airealtime.core.nativebridge.model.InferenceResult
import com.example.airealtime.core.nativebridge.model.NativeConfig
import com.example.airealtime.core.nativebridge.model.NativeStats
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import java.util.concurrent.atomic.AtomicLong
import javax.inject.Inject

data class CameraUiState(
    val isCameraOpened: Boolean = false,
    val isPipelineRunning: Boolean = false,
    val isRecording: Boolean = false,
    val lastPhotoPath: String? = null,
    val lastVideoPath: String? = null,
    val stats: NativeStats? = null,
    val inferenceResult: InferenceResult? = null,
    val errorMessage: String? = null
)

@HiltViewModel
class CameraViewModel @Inject constructor(
    private val cameraSessionController: CameraSessionController,
    private val settingsRepository: SettingsRepository,
    private val stillCaptureController: StillCaptureController,
    private val recordingController: RecordingController
) : ViewModel() {

    private val TAG = "CameraViewModel"
    private val nativeBridge = NativeBridge()
    private val frameIdCounter = AtomicLong(0)

    private val _uiState = MutableStateFlow(CameraUiState())
    val uiState: StateFlow<CameraUiState> = _uiState.asStateFlow()

    private var activePreviewSurface: Surface? = null
    private var statsJob: Job? = null
    private var asyncResultJob: Job? = null
    
    private var currentConfig: NativeConfig? = null
    private var currentStillFormat = "JPEG"
    private var currentVideoFormat = "H264"
    private var currentResWidth = 640
    private var currentResHeight = 480

    init {
        // Handle incoming settings changes
        viewModelScope.launch {
            settingsRepository.nativeConfigFlow.collect { config ->
                handleConfigChange(config)
            }
        }
        viewModelScope.launch {
            settingsRepository.stillFormatFlow.collect { currentStillFormat = it }
        }
        viewModelScope.launch {
            settingsRepository.videoFormatFlow.collect { currentVideoFormat = it }
        }
        viewModelScope.launch {
            settingsRepository.processingResolutionFlow.collect { res ->
                handleResolutionChange(res.first, res.second)
            }
        }
    }

    fun onPreviewSurfaceReady(surface: Surface) {
        activePreviewSurface = surface
        openAndStartCamera()
    }

    fun onPreviewSurfaceDestroyed() {
        activePreviewSurface = null
        closeCameraAndPipeline()
    }

    private fun openAndStartCamera() {
        val surface = activePreviewSurface ?: return
        
        // Re-negotiate resolutions based on current target sizes
        cameraSessionController.negotiateResolutions(currentResWidth, currentResHeight)
        
        cameraSessionController.onFrameAvailableListener = ImageReader.OnImageAvailableListener { reader ->
            val image = reader.acquireNextImage() ?: return@OnImageAvailableListener
            try {
                if (_uiState.value.isPipelineRunning) {
                    val frameId = frameIdCounter.incrementAndGet()
                    val timestamp = image.timestamp
                    val width = image.width
                    val height = image.height
                    
                    val planes = image.planes
                    if (planes.size >= 3) {
                        val yPlane = planes[0]
                        val uPlane = planes[1]
                        val vPlane = planes[2]

                        val config = currentConfig ?: NativeConfig()
                        if (config.mode == 0) { // SYNC mode
                            val result = nativeBridge.processFrameSync(
                                frameId = frameId,
                                timestampNs = timestamp,
                                width = width,
                                height = height,
                                rotationDegrees = 90, // Standard portrait camera tilt
                                format = image.format,
                                yBuffer = yPlane.buffer,
                                yRowStride = yPlane.rowStride,
                                yPixelStride = yPlane.pixelStride,
                                uBuffer = uPlane.buffer,
                                uRowStride = uPlane.rowStride,
                                uPixelStride = uPlane.pixelStride,
                                vBuffer = vPlane.buffer,
                                vRowStride = vPlane.rowStride,
                                vPixelStride = vPlane.pixelStride
                            )
                            result?.let { res ->
                                _uiState.update { it.copy(inferenceResult = res) }
                            }
                        } else { // ASYNC mode
                            nativeBridge.enqueueFrameAsync(
                                frameId = frameId,
                                timestampNs = timestamp,
                                width = width,
                                height = height,
                                rotationDegrees = 90,
                                format = image.format,
                                yBuffer = yPlane.buffer,
                                yRowStride = yPlane.rowStride,
                                yPixelStride = yPlane.pixelStride,
                                uBuffer = uPlane.buffer,
                                uRowStride = uPlane.rowStride,
                                uPixelStride = uPlane.pixelStride,
                                vBuffer = vPlane.buffer,
                                vRowStride = vPlane.rowStride,
                                vPixelStride = vPlane.pixelStride
                            )
                        }
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error routing frame to native pipeline", e)
            } finally {
                image.close()
            }
        }

        cameraSessionController.openCamera(
            onOpened = {
                _uiState.update { it.copy(isCameraOpened = true, errorMessage = null) }
                startPipelineAndSession(surface)
            },
            onError = { err ->
                _uiState.update { it.copy(errorMessage = err) }
            }
        )
    }

    private fun startPipelineAndSession(surface: Surface) {
        val config = currentConfig ?: NativeConfig()
        
        // Initialize native pipeline
        val success = nativeBridge.initNativePipeline(config)
        if (success) {
            nativeBridge.startNativePipeline()
            _uiState.update { it.copy(isPipelineRunning = true) }
            
            // Build camera session
            cameraSessionController.startCaptureSession(surface, isRecording = false)
            
            // Spin up background loops
            startPollingLoops()
            Log.i(TAG, "Pipeline and Session started successfully")
        } else {
            _uiState.update { it.copy(errorMessage = "Native Pipeline Init Failed") }
        }
    }

    private fun startPollingLoops() {
        // Poll stats 4 times per second (250ms)
        statsJob?.cancel()
        statsJob = viewModelScope.launch {
            while (true) {
                val stats = nativeBridge.getNativeStats()
                _uiState.update { it.copy(stats = stats) }
                delay(250)
            }
        }

        // Poll async results at 33ms (approx 30fps)
        asyncResultJob?.cancel()
        asyncResultJob = viewModelScope.launch {
            while (true) {
                val config = currentConfig ?: NativeConfig()
                if (config.mode == 1) { // ASYNC
                    val result = nativeBridge.getLatestResult()
                    result?.let { res ->
                        _uiState.update { it.copy(inferenceResult = res) }
                    }
                }
                delay(33)
            }
        }
    }

    private fun stopPollingLoops() {
        statsJob?.cancel()
        statsJob = null
        asyncResultJob?.cancel()
        asyncResultJob = null
    }

    private fun closeCameraAndPipeline() {
        stopPollingLoops()
        
        if (_uiState.value.isPipelineRunning) {
            nativeBridge.stopNativePipeline()
            nativeBridge.releaseNativePipeline()
            _uiState.update { it.copy(isPipelineRunning = false) }
        }
        
        cameraSessionController.closeCamera()
        _uiState.update { it.copy(isCameraOpened = false) }
        recordingController.release()
    }

    // Settings adjustments handler
    private fun handleConfigChange(newConfig: NativeConfig) {
        val oldConfig = currentConfig
        currentConfig = newConfig

        if (oldConfig == null) return

        // Unsafe parameter changes require a pipeline/session restart
        val modeChanged = oldConfig.mode != newConfig.mode
        val capacityChanged = oldConfig.queueCapacity != newConfig.queueCapacity
        
        if (modeChanged || capacityChanged) {
            Log.i(TAG, "Unsafe config changes. Restarting pipeline/session...")
            viewModelScope.launch {
                val surface = activePreviewSurface
                if (surface != null && _uiState.value.isCameraOpened) {
                    closeCameraAndPipeline()
                    delay(100)
                    openAndStartCamera()
                }
            }
        } else {
            // Safe parameters can be updated live
            nativeBridge.updateNativeConfig(newConfig)
        }
    }

    private fun handleResolutionChange(width: Int, height: Int) {
        val oldW = currentResWidth
        val oldH = currentResHeight
        currentResWidth = width
        currentResHeight = height

        if (oldW == width && oldH == height) return

        Log.i(TAG, "Resolution changed from ${oldW}x${oldH} to ${width}x${height}. Restarting...")
        viewModelScope.launch {
            val surface = activePreviewSurface
            if (surface != null && _uiState.value.isCameraOpened) {
                closeCameraAndPipeline()
                delay(100)
                openAndStartCamera()
            }
        }
    }

    // Still Photo Capture
    fun capturePhoto() {
        val surface = activePreviewSurface ?: return
        val config = currentConfig ?: NativeConfig()
        
        stillCaptureController.capturePhoto(
            format = currentStillFormat,
            onSuccess = { path ->
                _uiState.update { it.copy(lastPhotoPath = path) }
            },
            onFailure = { e ->
                _uiState.update { it.copy(errorMessage = "Photo capture failed: ${e.message}") }
            }
        )

        cameraSessionController.triggerStillCapture(
            previewSurface = surface,
            isRecording = _uiState.value.isRecording,
            recordingSurface = recordingController.recordingSurface,
            onCaptureComplete = {
                Log.i(TAG, "Still photo captured successfully")
            }
        )
    }

    // Video Recording Trigger
    fun startVideoRecording() {
        val surface = activePreviewSurface ?: return
        if (_uiState.value.isRecording) return

        try {
            // Prepare MediaRecorder and extract input surface
            val useHevc = currentVideoFormat == "HEVC"
            val recSurface = recordingController.prepareRecording(
                width = cameraSessionController.previewSize.width,
                height = cameraSessionController.previewSize.height,
                useHevc = useHevc
            )

            // Recreate session incorporating the recording surface
            cameraSessionController.startCaptureSession(
                previewSurface = surface,
                isRecording = true,
                recordingSurface = recSurface
            )

            // Start MediaRecorder writes
            recordingController.startRecording()
            _uiState.update { it.copy(isRecording = true, errorMessage = null) }
        } catch (e: Exception) {
            _uiState.update { it.copy(errorMessage = "Failed to start recording: ${e.message}") }
            recordingController.release()
        }
    }

    fun stopVideoRecording() {
        val surface = activePreviewSurface ?: return
        if (!_uiState.value.isRecording) return

        try {
            val filePath = recordingController.stopRecording()
            _uiState.update { it.copy(isRecording = false, lastVideoPath = filePath) }

            // Recreate camera session without recording surface
            cameraSessionController.startCaptureSession(
                previewSurface = surface,
                isRecording = false
            )
        } catch (e: Exception) {
            _uiState.update { it.copy(errorMessage = "Failed to stop recording: ${e.message}") }
        }
    }

    fun clearStats() {
        nativeBridge.clearNativeStats()
    }

    override fun onCleared() {
        super.onCleared()
        closeCameraAndPipeline()
    }
}
