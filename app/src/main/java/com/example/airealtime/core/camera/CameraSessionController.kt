package com.example.airealtime.core.camera

import android.annotation.SuppressLint
import android.content.Context
import android.hardware.camera2.*
import android.media.ImageReader
import android.os.Handler
import android.os.HandlerThread
import android.util.Log
import android.view.Surface
import com.example.airealtime.core.media.RecordingController
import com.example.airealtime.core.media.StillCaptureController
import dagger.hilt.android.qualifiers.ApplicationContext
import java.util.concurrent.Semaphore
import java.util.concurrent.TimeUnit
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class CameraSessionController @Inject constructor(
    @ApplicationContext private val context: Context,
    private val stillCaptureController: StillCaptureController,
    private val recordingController: RecordingController
) {
    private val TAG = "CameraSessionController"
    private val cameraManager = context.getSystemService(Context.CAMERA_SERVICE) as CameraManager
    
    private var cameraDevice: CameraDevice? = null
    private var captureSession: CameraCaptureSession? = null
    private var cameraThread: HandlerThread? = null
    private var cameraHandler: Handler? = null

    // Processing ImageReader
    private var processingImageReader: ImageReader? = null
    
    // Configured resolution sizes
    var previewSize = android.util.Size(1920, 1080)
        private set
    var processingSize = android.util.Size(640, 480)
        private set
    var stillSize = android.util.Size(1920, 1080)
        private set

    // Camera Lock Semaphore
    private val cameraOpenCloseLock = Semaphore(1)

    // Callback on frame receipt (for VM JNI dispatch)
    var onFrameAvailableListener: ImageReader.OnImageAvailableListener? = null

    fun startBackgroundThread() {
        cameraThread = HandlerThread("CameraBackground").apply { start() }
        cameraHandler = Handler(cameraThread!!.looper)
    }

    fun stopBackgroundThread() {
        cameraThread?.quitSafely()
        try {
            cameraThread?.join()
            cameraThread = null
            cameraHandler = null
        } catch (e: InterruptedException) {
            Log.e(TAG, "Interrupted stopping camera thread", e)
        }
    }

    // Stream-size negotiation
    fun negotiateResolutions(targetProcessingW: Int, targetProcessingH: Int) {
        try {
            val cameraId = getBackCameraId() ?: return
            val chars = cameraManager.getCameraCharacteristics(cameraId)
            val map = chars.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP) ?: return

            // Negotiate processing size (YUV_420_888)
            val yuvSizes = map.getOutputSizes(android.graphics.ImageFormat.YUV_420_888) ?: emptyArray()
            processingSize = selectOptimalSize(yuvSizes, targetProcessingW, targetProcessingH)

            // Negotiate preview size (SurfaceHolder or SurfaceTexture)
            val previewSizes = map.getOutputSizes(android.graphics.SurfaceTexture::class.java) ?: emptyArray()
            previewSize = selectOptimalSize(previewSizes, 1280, 720)

            // Negotiate still capture size (JPEG)
            val jpegSizes = map.getOutputSizes(android.graphics.ImageFormat.JPEG) ?: emptyArray()
            stillSize = jpegSizes.maxByOrNull { it.width * it.height } ?: android.util.Size(1920, 1080)

            Log.i(TAG, "Negotiated resolutions: Preview=$previewSize, Processing=$processingSize, Still=$stillSize")
        } catch (e: CameraAccessException) {
            Log.e(TAG, "Error negotiating stream sizes", e)
        }
    }

    private fun selectOptimalSize(sizes: Array<android.util.Size>, targetW: Int, targetH: Int): android.util.Size {
        return sizes.minByOrNull { Math.abs(it.width - targetW) + Math.abs(it.height - targetH) } ?: sizes[0]
    }

    private fun getBackCameraId(): String? {
        for (id in cameraManager.cameraIdList) {
            val chars = cameraManager.getCameraCharacteristics(id)
            val facing = chars.get(CameraCharacteristics.LENS_FACING)
            if (facing == CameraMetadata.LENS_FACING_BACK) {
                return id
            }
        }
        return cameraManager.cameraIdList.firstOrNull()
    }

    @SuppressLint("MissingPermission")
    fun openCamera(onOpened: () -> Unit, onError: (String) -> Unit) {
        val cameraId = getBackCameraId()
        if (cameraId == null) {
            onError("No camera available")
            return
        }

        startBackgroundThread()
        negotiateResolutions(processingSize.width, processingSize.height)

        try {
            if (!cameraOpenCloseLock.tryAcquire(2500, TimeUnit.MILLISECONDS)) {
                onError("Time out waiting to lock camera opening")
                return
            }
            cameraManager.openCamera(cameraId, object : CameraDevice.StateCallback() {
                override fun onOpened(camera: CameraDevice) {
                    cameraOpenCloseLock.release()
                    cameraDevice = camera
                    onOpened()
                }

                override fun onDisconnected(camera: CameraDevice) {
                    cameraOpenCloseLock.release()
                    camera.close()
                    cameraDevice = null
                }

                override fun onError(camera: CameraDevice, error: Int) {
                    cameraOpenCloseLock.release()
                    camera.close()
                    cameraDevice = null
                    onError("CameraDevice error code: $error")
                }
            }, cameraHandler)
        } catch (e: Exception) {
            cameraOpenCloseLock.release()
            onError("Failed to open camera: ${e.message}")
        }
    }

    fun startCaptureSession(previewSurface: Surface, isRecording: Boolean, recordingSurface: Surface? = null) {
        val device = cameraDevice ?: return
        
        try {
            // Close existing session
            captureSession?.close()
            captureSession = null

            // Re-initialize processing ImageReader
            processingImageReader?.close()
            processingImageReader = ImageReader.newInstance(
                processingSize.width,
                processingSize.height,
                android.graphics.ImageFormat.YUV_420_888,
                3
            )
            onFrameAvailableListener?.let {
                processingImageReader?.setOnImageAvailableListener(it, cameraHandler)
            }

            // Initialize still ImageReader
            stillCaptureController.initReader(stillSize.width, stillSize.height)

            val surfaces = mutableListOf<Surface>().apply {
                add(previewSurface)
                processingImageReader?.surface?.let { add(it) }
                stillCaptureController.stillImageReader?.surface?.let { add(it) }
            }

            if (isRecording && recordingSurface != null) {
                surfaces.add(recordingSurface)
            }

            Log.i(TAG, "Creating Camera2 session with ${surfaces.size} output surfaces")

            device.createCaptureSession(surfaces, object : CameraCaptureSession.StateCallback() {
                override fun onConfigured(session: CameraCaptureSession) {
                    captureSession = session
                    updateRepeatingRequests(previewSurface, isRecording, recordingSurface)
                }

                override fun onConfigureFailed(session: CameraCaptureSession) {
                    Log.e(TAG, "Failed to configure Camera2 capture session")
                }
            }, cameraHandler)

        } catch (e: CameraAccessException) {
            Log.e(TAG, "CameraAccessException creating capture session", e)
        }
    }

    private fun updateRepeatingRequests(previewSurface: Surface, isRecording: Boolean, recordingSurface: Surface?) {
        val device = cameraDevice ?: return
        val session = captureSession ?: return
        val procSurface = processingImageReader?.surface ?: return

        try {
            val template = if (isRecording) {
                CameraDevice.TEMPLATE_RECORD
            } else {
                CameraDevice.TEMPLATE_PREVIEW
            }

            val requestBuilder = device.createCaptureRequest(template).apply {
                addTarget(previewSurface)
                addTarget(procSurface)
                if (isRecording && recordingSurface != null) {
                    addTarget(recordingSurface)
                }
                set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE)
                set(CaptureRequest.CONTROL_AE_MODE, CaptureRequest.CONTROL_AE_MODE_ON)
            }

            session.setRepeatingRequest(requestBuilder.build(), null, cameraHandler)
            Log.i(TAG, "Repeating capture request configured (Recording=$isRecording)")
        } catch (e: CameraAccessException) {
            Log.e(TAG, "Failed to start repeating requests", e)
        }
    }

    fun triggerStillCapture(
        previewSurface: Surface,
        isRecording: Boolean,
        recordingSurface: Surface?,
        onCaptureComplete: () -> Unit
    ) {
        val device = cameraDevice ?: return
        val session = captureSession ?: return
        val stillSurface = stillCaptureController.stillImageReader?.surface ?: return

        try {
            // Lock repeating preview session momentarily
            session.stopRepeating()

            val captureBuilder = device.createCaptureRequest(CameraDevice.TEMPLATE_STILL_CAPTURE).apply {
                addTarget(stillSurface)
                set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE)
                // Determine orientation mapping
                set(CaptureRequest.JPEG_ORIENTATION, 90) // Hardcoded 90 for standard back cameras, customizable
            }

            session.capture(captureBuilder.build(), object : CameraCaptureSession.CaptureCallback() {
                override fun onCaptureCompleted(
                    session: CameraCaptureSession,
                    request: CaptureRequest,
                    result: TotalCaptureResult
                ) {
                    Log.i(TAG, "Still photo capture execution finished")
                    // Resume preview requests
                    updateRepeatingRequests(previewSurface, isRecording, recordingSurface)
                    onCaptureComplete()
                }
            }, cameraHandler)

        } catch (e: CameraAccessException) {
            Log.e(TAG, "Failed to execute still capture request", e)
        }
    }

    fun closeCamera() {
        try {
            cameraOpenCloseLock.acquire()
            captureSession?.close()
            captureSession = null
            cameraDevice?.close()
            cameraDevice = null
            processingImageReader?.close()
            processingImageReader = null
            stillCaptureController.release()
        } catch (e: InterruptedException) {
            Log.e(TAG, "Interrupted closing camera", e)
        } finally {
            cameraOpenCloseLock.release()
            stopBackgroundThread()
        }
    }
}
