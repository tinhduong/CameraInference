package com.example.camerapipe

import android.content.Context
import android.graphics.SurfaceTexture
import android.util.Log
import android.util.Size
import android.view.Surface
import androidx.camera.core.CameraSelector
import androidx.camera.core.Preview
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.core.content.ContextCompat
import androidx.lifecycle.LifecycleOwner
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import androidx.camera.core.resolutionselector.ResolutionSelector
import androidx.camera.core.resolutionselector.ResolutionStrategy
import androidx.camera.core.AspectRatio
import androidx.camera.core.resolutionselector.AspectRatioStrategy

class CameraManager(
    private val context: Context,
    private val lifecycleOwner: LifecycleOwner
) {
    private val TAG = "CameraManager"
    private var cameraProvider: ProcessCameraProvider? = null
    private var cameraExecutor: ExecutorService = Executors.newSingleThreadExecutor()
    private var surfaceTexture: SurfaceTexture? = null
    private var cameraSurface: Surface? = null
    private var isShutdown = false
    private var enginePtr: Long = 0L

    fun startCamera(enginePtr: Long, st: SurfaceTexture, width: Int, height: Int, onCameraStarted: () -> Unit) {
        this.enginePtr = enginePtr
        this.surfaceTexture = st
        isShutdown = false
        
        val cameraProviderFuture = ProcessCameraProvider.getInstance(context)
        cameraProviderFuture.addListener({
            if (isShutdown) {
                Log.d(TAG, "ProcessCameraProvider initialization completed after shutdown; ignoring.")
                return@addListener
            }
            try {
                cameraProvider = cameraProviderFuture.get()
                bindCameraUseCases(width, height)
                onCameraStarted()
            } catch (e: Exception) {
                Log.e(TAG, "Use case binding failed: ${e.message}", e)
            }
        }, ContextCompat.getMainExecutor(context))
    }

    private fun bindCameraUseCases(width: Int, height: Int) {
        val provider = cameraProvider ?: return
        provider.unbindAll()

        val cameraSelector = CameraSelector.DEFAULT_BACK_CAMERA

        val ratio = width.toFloat() / height.toFloat()
        val targetAspectRatio = if (Math.abs(ratio - 9f / 16f) < 0.1) {
            AspectRatio.RATIO_16_9
        } else {
            AspectRatio.RATIO_4_3
        }

        val resolutionSelector = ResolutionSelector.Builder()
            .setResolutionStrategy(
                ResolutionStrategy(
                    Size(maxOf(width, height), minOf(width, height)),
                    ResolutionStrategy.FALLBACK_RULE_CLOSEST_LOWER_THEN_HIGHER
                )
            )
            .setAspectRatioStrategy(
                AspectRatioStrategy(
                    targetAspectRatio,
                    AspectRatioStrategy.FALLBACK_RULE_AUTO
                )
            )
            .build()

        val preview = Preview.Builder()
            .setResolutionSelector(resolutionSelector)
            .build()

        preview.setSurfaceProvider(cameraExecutor) { request ->
            val texture = surfaceTexture
            if (texture != null) {
                texture.setDefaultBufferSize(request.resolution.width, request.resolution.height)
                
                // Set camera resolution in native code
                JniBridge.nativeSetCameraResolution(enginePtr, request.resolution.width, request.resolution.height)
                
                // Update resolution TextView in MainActivity
                (context as? MainActivity)?.updateResolutionText(request.resolution.width, request.resolution.height)
                
                cameraSurface?.release()
                val surface = Surface(texture)
                cameraSurface = surface
                
                Log.d(TAG, "Providing camera surface: ${request.resolution.width}x${request.resolution.height}")
                request.provideSurface(surface, cameraExecutor) { result ->
                    Log.d(TAG, "Surface request result code: ${result.resultCode}")
                }
            } else {
                request.willNotProvideSurface()
            }
        }

        try {
            provider.bindToLifecycle(lifecycleOwner, cameraSelector, preview)
        } catch (e: Exception) {
            Log.e(TAG, "Binding camera use cases to lifecycle failed: ${e.message}", e)
        }
    }

    fun shutdown() {
        isShutdown = true
        cameraProvider?.unbindAll()
        cameraExecutor.shutdown()
        cameraSurface?.release()
        cameraSurface = null
        surfaceTexture = null
    }
}
