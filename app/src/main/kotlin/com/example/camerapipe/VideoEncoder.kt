package com.example.camerapipe

import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.media.MediaMuxer
import android.util.Log
import android.view.Surface
import java.io.File
import java.io.IOException

class VideoEncoder(
    private val width: Int,
    private val height: Int,
    private val bitRate: Int = 5000000,
    private val frameRate: Int = 30,
    private val iFrameInterval: Int = 1
) {
    private val TAG = "VideoEncoder"
    private val MIME_TYPE = MediaFormat.MIMETYPE_VIDEO_AVC

    private var mediaCodec: MediaCodec? = null
    private var mediaMuxer: MediaMuxer? = null
    private var inputSurface: Surface? = null
    private var trackIndex = -1
    private var isMuxerStarted = false
    private var isRecording = false

    private val bufferInfo = MediaCodec.BufferInfo()
    private var drainThread: Thread? = null

    fun getInputSurface(): Surface? = inputSurface

    @Throws(IOException::class)
    fun start(outputFile: File) {
        if (isRecording) return
        Log.d(TAG, "Starting video encoder for file: ${outputFile.absolutePath}")

        val format = MediaFormat.createVideoFormat(MIME_TYPE, width, height).apply {
            setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface)
            setInteger(MediaFormat.KEY_BIT_RATE, bitRate)
            setInteger(MediaFormat.KEY_FRAME_RATE, frameRate)
            setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, iFrameInterval)
        }

        mediaCodec = MediaCodec.createEncoderByType(MIME_TYPE).apply {
            configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE)
            inputSurface = createInputSurface()
            start()
        }

        mediaMuxer = MediaMuxer(outputFile.absolutePath, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4)
        trackIndex = -1
        isMuxerStarted = false
        isRecording = true

        drainThread = Thread {
            try {
                while (isRecording) {
                    drain(false)
                    Thread.sleep(10)
                }
                // Send end-of-stream signal to codec and drain final output
                mediaCodec?.signalEndOfInputStream()
                drain(true)
            } catch (e: Exception) {
                Log.e(TAG, "Exception in encoder drain thread: ${e.message}", e)
            } finally {
                release()
            }
        }.apply { start() }
    }

    fun stop() {
        if (!isRecording) return
        Log.d(TAG, "Stopping video encoder")
        isRecording = false
        try {
            drainThread?.join()
        } catch (e: InterruptedException) {
            Log.e(TAG, "Encoder drain thread joining interrupted: ${e.message}")
        }
        drainThread = null
    }

    private fun drain(endOfStream: Boolean) {
        val codec = mediaCodec ?: return
        val muxer = mediaMuxer ?: return

        while (true) {
            val encoderStatus = codec.dequeueOutputBuffer(bufferInfo, 10000)
            if (encoderStatus == MediaCodec.INFO_TRY_AGAIN_LATER) {
                if (!endOfStream) {
                    break // Exit the loop and try again later
                }
            } else if (encoderStatus == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                if (isMuxerStarted) {
                    throw RuntimeException("Format changed after muxer started")
                }
                val newFormat = codec.outputFormat
                trackIndex = muxer.addTrack(newFormat)
                muxer.start()
                isMuxerStarted = true
            } else if (encoderStatus < 0) {
                // Ignore other statuses
            } else {
                val encodedData = codec.getOutputBuffer(encoderStatus)
                    ?: throw RuntimeException("EncoderOutputBuffer $encoderStatus was null")

                if (bufferInfo.flags and MediaCodec.BUFFER_FLAG_CODEC_CONFIG != 0) {
                    bufferInfo.size = 0 // Codec config info handles privately
                }

                if (bufferInfo.size != 0) {
                    if (!isMuxerStarted) {
                        throw RuntimeException("Muxer not started before writing sample data")
                    }
                    encodedData.position(bufferInfo.offset)
                    encodedData.limit(bufferInfo.offset + bufferInfo.size)
                    muxer.writeSampleData(trackIndex, encodedData, bufferInfo)
                }

                codec.releaseOutputBuffer(encoderStatus, false)

                if (bufferInfo.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM != 0) {
                    break // Reached end of stream
                }
            }
        }
    }

    private fun release() {
        Log.d(TAG, "Releasing encoder resources")
        try {
            mediaCodec?.stop()
        } catch (e: Exception) {
            Log.e(TAG, "Error stopping mediaCodec: ${e.message}")
        }
        try {
            mediaCodec?.release()
        } catch (e: Exception) {
            Log.e(TAG, "Error releasing mediaCodec: ${e.message}")
        }
        mediaCodec = null

        try {
            if (isMuxerStarted) {
                mediaMuxer?.stop()
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error stopping mediaMuxer: ${e.message}")
        }
        try {
            mediaMuxer?.release()
        } catch (e: Exception) {
            Log.e(TAG, "Error releasing mediaMuxer: ${e.message}")
        }
        mediaMuxer = null
        inputSurface = null
        trackIndex = -1
        isMuxerStarted = false
    }
}
