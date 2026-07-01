package com.example.airealtime

import com.example.airealtime.core.nativebridge.model.NativeConfig
import org.junit.Assert.assertEquals
import org.junit.Test

class NativeConfigTest {
    @Test
    fun testDefaultConfigValues() {
        val config = NativeConfig()
        assertEquals(1, config.mode) // ASYNC default
        assertEquals(0, config.rgbMode) // GPU_TEXTURE default
        assertEquals(3, config.queueCapacity)
        assertEquals(10, config.mockInferenceDelayMs)
        assertEquals(5, config.mockEncodeDelayMs)
    }
}
