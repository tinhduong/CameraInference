package com.example.airealtime.core.media

import android.content.Context
import android.media.MediaRecorder
import android.net.Uri
import android.os.Build
import android.os.ParcelFileDescriptor
import android.util.Log
import android.view.Surface
import dagger.hilt.android.qualifiers.ApplicationContext
import java.io.IOException
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class RecordingController @Inject constructor(
    @ApplicationContext private val context: Context,
    private val fileOutputManager: FileOutputManager
) {
    private val TAG = "RecordingController"
    private var mediaRecorder: MediaRecorder? = null
    private var recordingUri: Uri? = null
    private var recordingPfd: ParcelFileDescriptor? = null
    var recordingSurface: Surface? = null
        private set
    var isRecording = false
        private set

    fun prepareRecording(width: Int, height: Int, useHevc: Boolean): Surface {
        release()

        val target = fileOutputManager.createVideoTargetInGallery() 
            ?: throw IOException("Failed to create video target in MediaStore")
        
        recordingUri = target.uri
        recordingPfd = target.pfd

        val recorder = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            MediaRecorder(context)
        } else {
            @Suppress("DEPRECATION")
            MediaRecorder()
        }

        recorder.setAudioSource(MediaRecorder.AudioSource.MIC)
        recorder.setVideoSource(MediaRecorder.VideoSource.SURFACE)
        recorder.setOutputFormat(MediaRecorder.OutputFormat.MPEG_4)
        recorder.setOutputFile(target.pfd.fileDescriptor)
        
        recorder.setVideoSize(width, height)
        recorder.setVideoFrameRate(30)
        recorder.setVideoEncodingBitRate(8_000_000) // 8 Mbps
        
        val videoEncoder = if (useHevc) {
            MediaRecorder.VideoEncoder.HEVC
        } else {
            MediaRecorder.VideoEncoder.H264
        }
        recorder.setVideoEncoder(videoEncoder)
        recorder.setAudioEncoder(MediaRecorder.AudioEncoder.AAC)
        recorder.setAudioEncodingBitRate(128000)
        recorder.setAudioSamplingRate(44100)

        recorder.prepare()
        mediaRecorder = recorder
        recordingSurface = recorder.surface
        
        Log.i(TAG, "MediaRecorder prepared with descriptor for Uri: ${target.uri}")
        return recorder.surface
    }

    fun startRecording() {
        try {
            mediaRecorder?.start()
            isRecording = true
            Log.i(TAG, "Recording started")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to start MediaRecorder", e)
            isRecording = false
            throw e
        }
    }

    fun stopRecording(): String? {
        if (!isRecording) return null
        var path: String? = null
        try {
            mediaRecorder?.stop()
            recordingUri?.let { uri ->
                fileOutputManager.publishVideo(uri)
                path = uri.toString()
            }
            Log.i(TAG, "Recording stopped. Uri: $path")
        } catch (e: Exception) {
            Log.e(TAG, "Error stopping MediaRecorder", e)
            recordingUri?.let { uri ->
                try {
                    context.contentResolver.delete(uri, null, null)
                } catch (ex: Exception) {
                    Log.e(TAG, "Failed to delete temporary video entry", ex)
                }
            }
        } finally {
            try {
                recordingPfd?.close()
            } catch (e: Exception) {
                Log.e(TAG, "Failed to close video PFD", e)
            }
            mediaRecorder?.reset()
            mediaRecorder = null
            recordingSurface = null
            recordingPfd = null
            recordingUri = null
            isRecording = false
        }
        return path
    }

    fun release() {
        if (isRecording) {
            stopRecording()
        }
        mediaRecorder?.release()
        mediaRecorder = null
        recordingSurface = null
        try {
            recordingPfd?.close()
        } catch (e: Exception) {}
        recordingPfd = null
        recordingUri = null
        isRecording = false
    }
}
