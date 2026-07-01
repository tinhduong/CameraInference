package com.example.airealtime.core.media

import android.graphics.ImageFormat
import android.media.ImageReader
import android.util.Log
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import java.io.FileOutputStream
import java.io.IOException
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class StillCaptureController @Inject constructor(
    private val fileOutputManager: FileOutputManager
) {
    private val TAG = "StillCaptureController"
    private val scope = CoroutineScope(Dispatchers.IO)

    var stillImageReader: ImageReader? = null
        private set

    fun initReader(width: Int, height: Int) {
        release()
        stillImageReader = ImageReader.newInstance(width, height, ImageFormat.JPEG, 2)
    }

    fun release() {
        stillImageReader?.close()
        stillImageReader = null
    }

    fun capturePhoto(
        format: String,
        onSuccess: (filePath: String) -> Unit,
        onFailure: (exception: Exception) -> Unit
    ) {
        val reader = stillImageReader
        if (reader == null) {
            onFailure(IllegalStateException("Still ImageReader is not initialized"))
            return
        }

        reader.setOnImageAvailableListener({ readerTarget ->
            val image = readerTarget.acquireNextImage() ?: return@setOnImageAvailableListener
            scope.launch {
                try {
                    val buffer = image.planes[0].buffer
                    val bytes = ByteArray(buffer.remaining())
                    buffer.get(bytes)
                    
                    val uri = fileOutputManager.savePhotoToGallery(bytes, format)
                    if (uri != null) {
                        onSuccess(uri.toString())
                    } else {
                        onFailure(IOException("Failed to save image to MediaStore"))
                    }
                } catch (e: Exception) {
                    Log.e(TAG, "Failed to save still capture", e)
                    onFailure(e)
                } finally {
                    image.close()
                }
            }
        }, null)
    }
}
