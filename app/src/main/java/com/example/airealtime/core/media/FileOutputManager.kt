package com.example.airealtime.core.media

import android.content.ContentValues
import android.content.Context
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.os.ParcelFileDescriptor
import android.provider.MediaStore
import dagger.hilt.android.qualifiers.ApplicationContext
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class FileOutputManager @Inject constructor(
    @ApplicationContext private val context: Context
) {
    // Saves raw photo bytes directly into MediaStore (DCIM/Camera)
    fun savePhotoToGallery(bytes: ByteArray, format: String = "JPEG"): Uri? {
        val timeStamp = SimpleDateFormat("yyyyMMdd_HHmmss", Locale.getDefault()).format(Date())
        val displayName = "IMG_$timeStamp"
        val mimeType = if (format.uppercase() == "PNG") "image/png" else "image/jpeg"
        val relativePath = "${Environment.DIRECTORY_DCIM}/Camera"

        val contentValues = ContentValues().apply {
            put(MediaStore.Images.Media.DISPLAY_NAME, displayName)
            put(MediaStore.Images.Media.MIME_TYPE, mimeType)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                put(MediaStore.Images.Media.RELATIVE_PATH, relativePath)
                put(MediaStore.Images.Media.IS_PENDING, 1)
            }
        }

        val resolver = context.contentResolver
        val uri = resolver.insert(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, contentValues)

        if (uri != null) {
            try {
                resolver.openOutputStream(uri)?.use { out ->
                    out.write(bytes)
                }
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                    contentValues.clear()
                    contentValues.put(MediaStore.Images.Media.IS_PENDING, 0)
                    resolver.update(uri, contentValues, null, null)
                }
            } catch (e: Exception) {
                resolver.delete(uri, null, null)
                return null
            }
        }
        return uri
    }

    data class VideoTarget(val uri: Uri, val pfd: ParcelFileDescriptor)

    // Inserts a new video row in MediaStore and opens a ParcelFileDescriptor for MediaRecorder
    fun createVideoTargetInGallery(): VideoTarget? {
        val timeStamp = SimpleDateFormat("yyyyMMdd_HHmmss", Locale.getDefault()).format(Date())
        val displayName = "VID_$timeStamp"
        val mimeType = "video/mp4"
        val relativePath = "${Environment.DIRECTORY_DCIM}/Camera"

        val contentValues = ContentValues().apply {
            put(MediaStore.Video.Media.DISPLAY_NAME, displayName)
            put(MediaStore.Video.Media.MIME_TYPE, mimeType)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                put(MediaStore.Video.Media.RELATIVE_PATH, relativePath)
                put(MediaStore.Video.Media.IS_PENDING, 1)
            }
        }

        val resolver = context.contentResolver
        val uri = resolver.insert(MediaStore.Video.Media.EXTERNAL_CONTENT_URI, contentValues) ?: return null

        return try {
            val pfd = resolver.openFileDescriptor(uri, "rw") ?: throw Exception("Failed to open FileDescriptor")
            VideoTarget(uri, pfd)
        } catch (e: Exception) {
            resolver.delete(uri, null, null)
            null
        }
    }

    // Toggles the IS_PENDING flag to make the recorded video scan-visible to gallery apps
    fun publishVideo(uri: Uri) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            val resolver = context.contentResolver
            val contentValues = ContentValues().apply {
                put(MediaStore.Video.Media.IS_PENDING, 0)
            }
            resolver.update(uri, contentValues, null, null)
        }
    }
}
