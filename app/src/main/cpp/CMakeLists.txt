cmake_minimum_required(VERSION 3.22.1)

project("airealtime")

# Recursively locate C++ pipeline, GPU, inference, and encoder sources
file(GLOB_RECURSE PIPELINE_SOURCES
    "pipeline/*.cpp"
    "pipeline/*.h"
    "gpu/*.cpp"
    "gpu/*.h"
    "inference/*.cpp"
    "inference/*.h"
    "encoder/*.cpp"
    "encoder/*.h"
)

add_library(airealtime SHARED
    native-lib.cpp
    ${PIPELINE_SOURCES}
)

# Find Android support libraries
find_library(log-lib log)
find_library(android-lib android)

# Link hardware buffer, graphics, and system libraries
target_link_libraries(airealtime
    android
    EGL
    GLESv3
    ${log-lib}
)
