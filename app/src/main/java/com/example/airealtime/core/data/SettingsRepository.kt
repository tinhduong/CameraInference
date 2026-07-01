package com.example.airealtime.core.data

import android.content.Context
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.*
import androidx.datastore.preferences.preferencesDataStore
import com.example.airealtime.core.nativebridge.model.NativeConfig
import dagger.hilt.android.qualifiers.ApplicationContext
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map
import javax.inject.Inject
import javax.inject.Singleton

val Context.dataStore: DataStore<Preferences> by preferencesDataStore(name = "ai_camera_settings")

@Singleton
class SettingsRepository @Inject constructor(
    @ApplicationContext private val context: Context
) {
    private object PreferencesKeys {
        val PROCESSING_MODE = intPreferencesKey("processing_mode")
        val RGB_OUTPUT_MODE = intPreferencesKey("rgb_output_mode")
        val AI_ENABLED = booleanPreferencesKey("ai_enabled")
        val ENCODE_ENABLED = booleanPreferencesKey("encode_enabled")
        val GPU_CONVERSION_ENABLED = booleanPreferencesKey("gpu_conversion_enabled")
        val QUEUE_CAPACITY = intPreferencesKey("queue_capacity")
        val DROP_POLICY = intPreferencesKey("drop_policy")
        val MOCK_INFERENCE_DELAY = intPreferencesKey("mock_inference_delay")
        val MOCK_ENCODE_DELAY = intPreferencesKey("mock_encode_delay")
        
        val STILL_CAPTURE_FORMAT = stringPreferencesKey("still_capture_format")
        val VIDEO_RECORD_FORMAT = stringPreferencesKey("video_record_format")
        
        val PROCESSING_WIDTH = intPreferencesKey("processing_width")
        val PROCESSING_HEIGHT = intPreferencesKey("processing_height")
    }

    val nativeConfigFlow: Flow<NativeConfig> = context.dataStore.data.map { preferences ->
        NativeConfig(
            mode = preferences[PreferencesKeys.PROCESSING_MODE] ?: 1, // ASYNC
            rgbMode = preferences[PreferencesKeys.RGB_OUTPUT_MODE] ?: 0, // GPU_TEXTURE
            aiEnabled = preferences[PreferencesKeys.AI_ENABLED] ?: true,
            encodeEnabled = preferences[PreferencesKeys.ENCODE_ENABLED] ?: true,
            gpuConversionEnabled = preferences[PreferencesKeys.GPU_CONVERSION_ENABLED] ?: true,
            queueCapacity = preferences[PreferencesKeys.QUEUE_CAPACITY] ?: 3,
            dropPolicy = preferences[PreferencesKeys.DROP_POLICY] ?: 0, // DROP_OLDEST
            mockInferenceDelayMs = preferences[PreferencesKeys.MOCK_INFERENCE_DELAY] ?: 10,
            mockEncodeDelayMs = preferences[PreferencesKeys.MOCK_ENCODE_DELAY] ?: 5
        )
    }

    val stillFormatFlow: Flow<String> = context.dataStore.data.map { preferences ->
        preferences[PreferencesKeys.STILL_CAPTURE_FORMAT] ?: "JPEG"
    }

    val videoFormatFlow: Flow<String> = context.dataStore.data.map { preferences ->
        preferences[PreferencesKeys.VIDEO_RECORD_FORMAT] ?: "H264"
    }

    val processingResolutionFlow: Flow<Pair<Int, Int>> = context.dataStore.data.map { preferences ->
        Pair(
            preferences[PreferencesKeys.PROCESSING_WIDTH] ?: 640,
            preferences[PreferencesKeys.PROCESSING_HEIGHT] ?: 480
        )
    }

    suspend fun updateProcessingMode(mode: Int) {
        context.dataStore.edit { it[PreferencesKeys.PROCESSING_MODE] = mode }
    }

    suspend fun updateRgbOutputMode(rgbMode: Int) {
        context.dataStore.edit { it[PreferencesKeys.RGB_OUTPUT_MODE] = rgbMode }
    }

    suspend fun updateAiEnabled(enabled: Boolean) {
        context.dataStore.edit { it[PreferencesKeys.AI_ENABLED] = enabled }
    }

    suspend fun updateEncodeEnabled(enabled: Boolean) {
        context.dataStore.edit { it[PreferencesKeys.ENCODE_ENABLED] = enabled }
    }

    suspend fun updateGpuConversionEnabled(enabled: Boolean) {
        context.dataStore.edit { it[PreferencesKeys.GPU_CONVERSION_ENABLED] = enabled }
    }

    suspend fun updateQueueCapacity(capacity: Int) {
        context.dataStore.edit { it[PreferencesKeys.QUEUE_CAPACITY] = capacity }
    }

    suspend fun updateDropPolicy(policy: Int) {
        context.dataStore.edit { it[PreferencesKeys.DROP_POLICY] = policy }
    }

    suspend fun updateMockInferenceDelay(delayMs: Int) {
        context.dataStore.edit { it[PreferencesKeys.MOCK_INFERENCE_DELAY] = delayMs }
    }

    suspend fun updateMockEncodeDelay(delayMs: Int) {
        context.dataStore.edit { it[PreferencesKeys.MOCK_ENCODE_DELAY] = delayMs }
    }

    suspend fun updateStillCaptureFormat(format: String) {
        context.dataStore.edit { it[PreferencesKeys.STILL_CAPTURE_FORMAT] = format }
    }

    suspend fun updateVideoRecordFormat(format: String) {
        context.dataStore.edit { it[PreferencesKeys.VIDEO_RECORD_FORMAT] = format }
    }

    suspend fun updateProcessingResolution(width: Int, height: Int) {
        context.dataStore.edit {
            it[PreferencesKeys.PROCESSING_WIDTH] = width
            it[PreferencesKeys.PROCESSING_HEIGHT] = height
        }
    }
}
