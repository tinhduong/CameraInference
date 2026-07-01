package com.example.airealtime.feature.settings

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.example.airealtime.core.data.SettingsRepository
import com.example.airealtime.core.nativebridge.model.NativeConfig
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch
import javax.inject.Inject

data class SettingsUiState(
    val config: NativeConfig = NativeConfig(),
    val stillFormat: String = "JPEG",
    val videoFormat: String = "H264",
    val processingWidth: Int = 640,
    val processingHeight: Int = 480
)

@HiltViewModel
class SettingsViewModel @Inject constructor(
    private val settingsRepository: SettingsRepository
) : ViewModel() {

    val uiState: StateFlow<SettingsUiState> = combine(
        settingsRepository.nativeConfigFlow,
        settingsRepository.stillFormatFlow,
        settingsRepository.videoFormatFlow,
        settingsRepository.processingResolutionFlow
    ) { config, stillFormat, videoFormat, resolution ->
        SettingsUiState(
            config = config,
            stillFormat = stillFormat,
            videoFormat = videoFormat,
            processingWidth = resolution.first,
            processingHeight = resolution.second
        )
    }.stateIn(
        scope = viewModelScope,
        started = SharingStarted.WhileSubscribed(5000),
        initialValue = SettingsUiState()
    )

    fun setProcessingMode(mode: Int) {
        viewModelScope.launch {
            settingsRepository.updateProcessingMode(mode)
        }
    }

    fun setRgbOutputMode(mode: Int) {
        viewModelScope.launch {
            settingsRepository.updateRgbOutputMode(mode)
        }
    }

    fun setAiEnabled(enabled: Boolean) {
        viewModelScope.launch {
            settingsRepository.updateAiEnabled(enabled)
        }
    }

    fun setEncodeEnabled(enabled: Boolean) {
        viewModelScope.launch {
            settingsRepository.updateEncodeEnabled(enabled)
        }
    }

    fun setGpuConversionEnabled(enabled: Boolean) {
        viewModelScope.launch {
            settingsRepository.updateGpuConversionEnabled(enabled)
        }
    }

    fun setQueueCapacity(capacity: Int) {
        viewModelScope.launch {
            settingsRepository.updateQueueCapacity(capacity)
        }
    }

    fun setDropPolicy(policy: Int) {
        viewModelScope.launch {
            settingsRepository.updateDropPolicy(policy)
        }
    }

    fun setMockInferenceDelay(delayMs: Int) {
        viewModelScope.launch {
            settingsRepository.updateMockInferenceDelay(delayMs)
        }
    }

    fun setMockEncodeDelay(delayMs: Int) {
        viewModelScope.launch {
            settingsRepository.updateMockEncodeDelay(delayMs)
        }
    }

    fun setStillFormat(format: String) {
        viewModelScope.launch {
            settingsRepository.updateStillCaptureFormat(format)
        }
    }

    fun setVideoFormat(format: String) {
        viewModelScope.launch {
            settingsRepository.updateVideoRecordFormat(format)
        }
    }

    fun setProcessingResolution(width: Int, height: Int) {
        viewModelScope.launch {
            settingsRepository.updateProcessingResolution(width, height)
        }
    }
}
