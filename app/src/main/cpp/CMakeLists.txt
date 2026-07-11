cmake_minimum_required(VERSION 3.22.1)

project("camerapipe")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find standard libraries
find_library(log-lib log)
find_library(android-lib android)
find_library(egl-lib EGL)
find_library(gles3-lib GLESv3)

# Build the native pipeline library
add_library(native-lib SHARED
    native-lib.cpp
    EGLManager.cpp
    Renderer.cpp
    PBOManager.cpp
    AITask.cpp
)

# Link components
target_link_libraries(native-lib
    ${log-lib}
    ${android-lib}
    ${egl-lib}
    ${gles3-lib}
)
