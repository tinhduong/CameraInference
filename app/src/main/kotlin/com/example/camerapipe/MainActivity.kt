package com.example.camerapipe

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.graphics.SurfaceTexture
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.SurfaceHolder
import android.widget.Button
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import java.io.File

class MainActivity : AppCompatActivity(), SurfaceHolder.Callback, JniBridge.AiCallback {
    private val TAG = "MainActivity"
    private val CAMERA_PERMISSION_CODE = 101

    private var nativeEnginePtr: Long = 0
    private var cameraManager: CameraManager? = null
    private var videoEncoder: VideoEncoder? = null
    private var surfaceTexture: SurfaceTexture? = null
    private var isRecording = false

    private lateinit var tvFps: TextView
    private lateinit var tvAiResult: TextView
    private lateinit var btnRecord: Button
    private lateinit var tvResolution: TextView

    private val mainHandler = Handler(Looper.getMainLooper())

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        tvFps = findViewById(R.id.tvFps)
        tvAiResult = findViewById(R.id.tvAiResult)
        btnRecord = findViewById(R.id.btnRecord)
        tvResolution = findViewById(R.id.tvResolution)

        btnRecord.setOnClickListener {
            toggleRecording()
        }

        tvResolution.setOnClickListener {
            showResolutionSelector()
        }

        val drawerLayout = findViewById<androidx.drawerlayout.widget.DrawerLayout>(R.id.drawerLayout)
        val btnOpenDrawer = findViewById<android.view.View>(R.id.btnOpenDrawer)
        btnOpenDrawer.setOnClickListener {
            drawerLayout.openDrawer(androidx.core.view.GravityCompat.START)
        }

        JniBridge.setAiCallback(this)

        if (checkPermissions()) {
            setupSurfaceView()
        } else {
            requestPermissions()
        }

        // Query and log supported camera resolutions on startup for troubleshooting
        getSupportedPortraitResolutions()
    }

    private fun checkPermissions(): Boolean {
        val cameraGranted = ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED
        val audioGranted = ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED
        return cameraGranted && audioGranted
    }

    private fun requestPermissions() {
        ActivityCompat.requestPermissions(
            this,
            arrayOf(Manifest.permission.CAMERA, Manifest.permission.RECORD_AUDIO),
            CAMERA_PERMISSION_CODE
        )
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == CAMERA_PERMISSION_CODE) {
            if (grantResults.isNotEmpty() && grantResults.all { it == PackageManager.PERMISSION_GRANTED }) {
                setupSurfaceView()
            } else {
                Toast.makeText(this, "Permissions are required to run the pipeline", Toast.LENGTH_LONG).show()
                finish()
            }
        }
    }

    private fun setupSurfaceView() {
        val surfaceView = findViewById<android.view.SurfaceView>(R.id.surfaceView)
        surfaceView.holder.addCallback(this)
    }

    // SurfaceHolder.Callback implementation
    override fun surfaceCreated(holder: SurfaceHolder) {
        Log.d(TAG, "surfaceCreated")
        // Initialize the native engine pointer (AI input size is matching camera default 1280x720)
        nativeEnginePtr = JniBridge.nativeInit(1280, 720)
        
        // Connect UI Surface to the native EGL system
        JniBridge.nativeOnSurfaceCreated(nativeEnginePtr, holder.surface)

        // Create a dummy SurfaceTexture. We will attach it in C++ EGL Thread
        val st = SurfaceTexture(0)
        st.detachFromGLContext()
        surfaceTexture = st

        // Start the native render thread and attach the SurfaceTexture
        val textureId = JniBridge.nativeStartPipeline(nativeEnginePtr, st)
        Log.d(TAG, "Native started pipeline, bound textureId: $textureId")

        // Trigger native update when camera frames arrive
        st.setOnFrameAvailableListener({
            JniBridge.nativeOnFrameAvailable(nativeEnginePtr)
        }, mainHandler)

        // Fire up CameraX bound to the SurfaceTexture
        cameraManager = CameraManager(this, this)
        cameraManager?.startCamera(nativeEnginePtr, st, 720, 1280) {
            Log.d(TAG, "Camera initialised and bound to custom SurfaceTexture")
        }
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        Log.d(TAG, "surfaceChanged: ${width}x${height}")
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        Log.d(TAG, "surfaceDestroyed")
        cleanupPipeline()
    }

    private fun toggleRecording() {
        if (nativeEnginePtr == 0L) return

        if (!isRecording) {
            val recordFile = File(getExternalFilesDir(null), "recording_${System.currentTimeMillis()}.mp4")
            videoEncoder = VideoEncoder(1280, 720)
            try {
                videoEncoder?.start(recordFile)
                val inputSurface = videoEncoder?.getInputSurface()
                if (inputSurface != null) {
                    JniBridge.nativeSetEncoderSurface(nativeEnginePtr, inputSurface, 1280, 720)
                    isRecording = true
                    btnRecord.text = "Stop"
                    btnRecord.setBackgroundColor(ContextCompat.getColor(this, android.R.color.darker_gray))
                    Toast.makeText(this, "Recording to: ${recordFile.name}", Toast.LENGTH_SHORT).show()
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error starting recording: ${e.message}", e)
                Toast.makeText(this, "Could not start recording", Toast.LENGTH_SHORT).show()
            }
        } else {
            JniBridge.nativeSetEncoderSurface(nativeEnginePtr, null, 0, 0)
            videoEncoder?.stop()
            videoEncoder = null
            isRecording = false
            btnRecord.text = "Record"
            btnRecord.setBackgroundColor(ContextCompat.getColor(this, android.R.color.holo_red_dark))
            Toast.makeText(this, "Recording stopped. File saved.", Toast.LENGTH_SHORT).show()
        }
    }

    private fun cleanupPipeline() {
        if (isRecording) {
            toggleRecording()
        }
        cameraManager?.shutdown()
        cameraManager = null

        if (nativeEnginePtr != 0L) {
            JniBridge.nativeStopPipeline(nativeEnginePtr)
            JniBridge.nativeOnSurfaceDestroyed(nativeEnginePtr)
            JniBridge.nativeRelease(nativeEnginePtr)
            nativeEnginePtr = 0L
        }

        surfaceTexture?.release()
        surfaceTexture = null
    }

    override fun onDestroy() {
        super.onDestroy()
        cleanupPipeline()
        JniBridge.setAiCallback(null)
    }

    override fun onAiResult(label: String, score: Float, latencyMs: Long) {
        runOnUiThread {
            tvAiResult.text = String.format("AI Inference: %s (%.1f%%)", label, score * 100)
            tvFps.text = String.format("Inference latency: %d ms", latencyMs)
        }
    }

    fun updateResolutionText(width: Int, height: Int) {
        runOnUiThread {
            val portW = minOf(width, height)
            val portH = maxOf(width, height)
            tvResolution.text = "${portW}x${portH}"
        }
    }

    private fun getSupportedPortraitResolutions(): List<Pair<Int, Int>> {
        val list = mutableListOf<Pair<Int, Int>>()
        try {
            val cameraManagerService = getSystemService(Context.CAMERA_SERVICE) as android.hardware.camera2.CameraManager
            val cameraId = cameraManagerService.cameraIdList.firstOrNull { id ->
                val chars = cameraManagerService.getCameraCharacteristics(id)
                chars.get(android.hardware.camera2.CameraCharacteristics.LENS_FACING) == 
                        android.hardware.camera2.CameraCharacteristics.LENS_FACING_BACK
            } ?: cameraManagerService.cameraIdList.firstOrNull() ?: return emptyList()
            
            val chars = cameraManagerService.getCameraCharacteristics(cameraId)
            val map = chars.get(android.hardware.camera2.CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP)
            val sizes = map?.getOutputSizes(android.graphics.SurfaceTexture::class.java) ?: emptyArray()
            
            Log.d(TAG, "Supported sizes for SurfaceTexture count: ${sizes.size}")
            for (size in sizes) {
                Log.d(TAG, "  - Supported: ${size.width}x${size.height}")
            }
            
            for (size in sizes) {
                val w = minOf(size.width, size.height)
                val h = maxOf(size.width, size.height)
                if (w < h) {
                    list.add(Pair(w, h))
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error querying camera resolutions: ${e.message}", e)
        }
        
        return list.distinct().sortedByDescending { it.first * it.second }
    }

    private fun changeCameraResolution(portWidth: Int, portHeight: Int) {
        if (isRecording) {
            Toast.makeText(this, "Cannot change resolution while recording!", Toast.LENGTH_SHORT).show()
            return
        }
        
        val st = surfaceTexture ?: return
        
        cameraManager?.shutdown()
        cameraManager = null
        
        cameraManager = CameraManager(this, this)
        cameraManager?.startCamera(nativeEnginePtr, st, portWidth, portHeight) {
            runOnUiThread {
                tvResolution.text = "${portWidth}x${portHeight}"
                Toast.makeText(this, "Changed resolution to ${portWidth}x${portHeight}", Toast.LENGTH_SHORT).show()
            }
        }
    }

    private fun showResolutionSelector() {
        if (isRecording) {
            Toast.makeText(this, "Cannot change resolution while recording!", Toast.LENGTH_SHORT).show()
            return
        }

        val resolutions = getSupportedPortraitResolutions()
        if (resolutions.isEmpty()) {
            Toast.makeText(this, "Could not retrieve supported resolutions", Toast.LENGTH_SHORT).show()
            return
        }

        val items = resolutions.map { "${it.first}x${it.second}" }.toTypedArray()
        
        androidx.appcompat.app.AlertDialog.Builder(this)
            .setTitle("Select Camera Resolution")
            .setItems(items) { _, which ->
                val selected = resolutions[which]
                changeCameraResolution(selected.first, selected.second)
            }
            .setNegativeButton("Cancel", null)
            .show()
    }
}
