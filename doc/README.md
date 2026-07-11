# GPU Camera Pipeline Architecture & UML Diagrams

Tài liệu này mô tả chi tiết kiến trúc hệ thống và luồng dữ liệu (Data Flow) của ứng dụng **GPU Camera Pipeline** giúp người đọc có thể nhanh chóng nắm bắt và thiết kế hệ thống.

---

## 1. Tổng quan kiến trúc hệ thống (System Architecture)

Dự án được phân chia thành **3 phân tầng chính**:
*   **Kotlin Layer (UI & Camera/Encoder)**: Quản lý vòng đời ứng dụng, giao diện UI, cấu hình CameraX và MediaCodec (Bộ mã hóa video).
*   **JNI Layer (JniBridge)**: Làm cầu nối truyền dẫn dữ liệu và lệnh điều khiển giữa Kotlin và C++.
*   **Native C++ Layer (Render & AI Engine)**: Thực hiện xử lý đồ họa GPU hiệu năng cao bằng OpenGL ES, đọc ngược bộ đệm (readback) bất đồng bộ qua PBO và chạy xử lý AI luồng nền.

```mermaid
graph TD
    subgraph Kotlin_Layer ["Kotlin UI Layer"]
        MA[MainActivity] --> CM[CameraManager]
        MA --> VE[VideoEncoder]
        MA --> JB[JniBridge]
        CM --> CX[CameraX API]
    end

    subgraph JNI_Bridge ["JNI Layer"]
        JB <--> NL[native-lib.cpp]
    end

    subgraph Native_CPP_Layer ["Native C++ Layer"]
        NL --> ENG[Engine]
        ENG --> RND[Renderer]
        ENG --> AI[AITask]
        RND --> EGL[EGLManager]
        RND --> PBO[PBOManager]
    end

    CX -- Stream frames --> ST[SurfaceTexture]
    ST -- Bound in GLES --> RND
    RND -- Render to --> PS[Preview Surface]
    RND -- Render to --> ES[Encoder Input Surface]
    RND -- Async Readback --> PBO
    PBO -- RGB Buffer --> AI
    AI -- JNI Callback --> JB
```

---

## 2. Biểu đồ lớp (Class Diagram)

Mô tả mối quan hệ giữa các class trong cả Kotlin và C++:

```mermaid
classDiagram
    class MainActivity {
        -Long nativeEnginePtr
        -CameraManager cameraManager
        -VideoEncoder videoEncoder
        -SurfaceTexture surfaceTexture
        +surfaceCreated(holder)
        +surfaceDestroyed(holder)
        +toggleRecording()
        +onAiResult(label, score, latencyMs)
    }
    
    class CameraManager {
        -Context context
        -LifecycleOwner lifecycleOwner
        -ProcessCameraProvider cameraProvider
        -ExecutorService cameraExecutor
        -Boolean isShutdown
        +startCamera(st, width, height, onCameraStarted)
        +shutdown()
    }
    
    class JniBridge {
        +nativeInit(aiW, aiH)
        +nativeRelease(ptr)
        +nativeStartPipeline(ptr, st)
        +nativeStopPipeline(ptr)
        +nativeOnFrameAvailable(ptr)
        +nativeSetEncoderSurface(ptr, surface, w, h)
    }
    
    class Renderer {
        -std::thread renderThread
        -EGLManager* eglManager
        -PBOManager* pboManager
        -AITask* aiTask
        -GLuint oesTextureId
        -GLuint aiTextureId
        -GLuint fboId
        +start(vm, stObj, aiW, aiH)
        +stop()
        +onFrameAvailable()
        +setPreviewWindow(window)
        +setEncoderWindow(window, w, h)
        -threadLoop()
        -setupGL()
        -drawFrame(env)
    }
    
    class EGLManager {
        -EGLDisplay display
        -EGLContext context
        -EGLSurface previewSurface
        -EGLSurface encoderSurface
        +initEGL()
        +releaseEGL()
        +createPreviewSurface(window)
        +createEncoderSurface(window)
        +destroyPreviewSurface()
        +destroyEncoderSurface()
        +makeCurrent(surface)
        +swapBuffers(surface)
    }
    
    class PBOManager {
        -GLuint pboIds[2]
        -int width
        -int height
        -int activeIndex
        +init(w, h)
        +readback(fboId, outBuffer)
        +release()
    }
    
    class AITask {
        -std::thread aiThread
        -std::vector~uint8_t~ sharedRgbBuffer
        -std::vector~uint8_t~ threadLocalBuffer
        +start(vm, bridgeClass, methodId, w, h)
        +stop()
        +submitFrame(rgbBuffer)
        -threadLoop()
        -runDummyInference(rgbData, label, score)
    }

    MainActivity --> CameraManager : uses
    MainActivity --> JniBridge : calls
    CameraManager --> JniBridge : listener calls
    Renderer --> EGLManager : uses
    Renderer --> PBOManager : uses
    Renderer --> AITask : submits frames to
```

---

## 3. Quy trình khởi tạo (Initialization Sequence)

Mô tả luồng các bước khởi chạy hệ thống khi ứng dụng mở ra và `surfaceCreated` của View chính được gọi:

```mermaid
sequenceDiagram
    autonumber
    actor User
    participant MA as MainActivity
    participant CM as CameraManager
    participant JB as JniBridge
    participant CPP as Native Engine
    participant RND as Renderer
    participant AI as AITask

    User->>MA: Mở ứng dụng
    MA->>JB: nativeInit(1280, 720)
    JB->>CPP: Khởi tạo Engine (C++)
    CPP-->>MA: Trả về con trỏ nativeEnginePtr
    MA->>JB: nativeOnSurfaceCreated(enginePtr, surface)
    MA->>MA: Tạo detached SurfaceTexture(st)
    MA->>JB: nativeStartPipeline(enginePtr, st)
    JB->>RND: start(st)
    RND->>AI: start()
    AI->>AI: Khởi chạy AI Thread ngầm
    RND->>RND: Khởi chạy Render Thread (C++)
    Note over RND: Khởi tạo EGL & GLES Context
    RND->>ST: Đính kèm st vào GLES Context (attachToGLContext)
    RND-->>MA: Trả về textureId
    MA->>MA: Đăng ký st.setOnFrameAvailableListener
    MA->>CM: startCamera(st, 720, 1280)
    CM->>CM: Bind CameraX vào Lifecycle và kết nối tới st
```

---

## 4. Luồng xử lý Frame (Runtime Frame Loop Sequence)

Mô tả luồng đi của một frame ảnh từ lúc cảm biến camera bắt được cho tới khi hiển thị lên màn hình, lưu file MP4 và chạy model AI:

```mermaid
sequenceDiagram
    autonumber
    participant CAM as CameraX
    participant ST as SurfaceTexture
    participant MA as MainActivity
    participant JB as JniBridge
    participant RND as Renderer
    participant EGL as EGLManager
    participant PBO as PBOManager
    participant AI as AITask

    CAM->>ST: Đẩy frame mới vào SurfaceTexture
    ST->>MA: onFrameAvailable listener kích hoạt
    MA->>JB: nativeOnFrameAvailable()
    JB->>RND: onFrameAvailable() (Đánh thức Render Thread)
    
    RND->>RND: Render Thread thức dậy
    RND->>ST: updateTexImage() via JNI
    ST-->>RND: Cập nhật dữ liệu OES Texture & Ma trận transform
    
    rect rgba(128, 128, 128, 0.15)
        RND->>EGL: makeCurrent(previewSurface)
        RND->>RND: Vẽ OES Texture ra màn hình chính (Viewport)
        RND->>EGL: swapBuffers() (Hiển thị UI Preview)
        
        opt Nếu đang ghi hình (Is Recording)
            RND->>EGL: makeCurrent(encoderSurface)
            RND->>RND: Vẽ OES Texture sang luồng Encoder (MediaCodec)
            RND->>EGL: swapBuffers() (Gửi frame đi mã hóa)
        end
        
        RND->>RND: Vẽ OES Texture vào Framebuffer (FBO) (Giữ nguyên kích thước gốc của Camera)
        RND->>PBO: readback() (Đọc pixel bất đồng bộ GPU -> CPU)
        PBO-->>RND: Trả về bộ đệm RGB trên CPU (RAM)
    end
    
    RND->>AI: submitFrame(rgbBuffer)
    AI->>AI: Đánh thức AI Thread ngầm
    AI->>AI: runDummyInference() (Chạy mô phỏng trễ 40ms của đối tác)
    AI->>JB: Trả kết quả AI qua CallStaticVoidMethod
    JB->>MA: Callback onAiResult(label, score, latency)
    MA->>MA: Cập nhật kết quả AI & FPS lên TextViews UI
```

---

## 5. Tóm tắt vai trò các Component chính

1.  **`MainActivity`**: Điều phối vòng đời ứng dụng. Lắng nghe callback kết quả phân tích AI gửi về từ Native C++ để hiển thị lên màn hình và nút ghi video.
2.  **`CameraManager`**: Thiết lập camera bằng CameraX. Đưa luồng ảnh đầu ra vào một `SurfaceTexture` tuỳ chỉnh để chuyển quyền kiểm soát buffer cho OpenGL Native.
3.  **`VideoEncoder`**: Khởi tạo luồng mã hóa video qua `MediaCodec`. Cung cấp một `Surface` để C++ Renderer vẽ trực tiếp dữ liệu từ OpenGL vào.
4.  **`Renderer`**: Điểm trung chuyển chính. Luôn chạy trên một Render Thread riêng biệt. Nhiệm vụ của nó là vẽ frame camera nhận được lên màn hình preview, lên luồng ghi hình, và vẽ vào FBO để phục vụ AI (giữ nguyên kích thước gốc, tự động co dãn cấu trúc FBO/PBO khi đổi độ phân giải).
5.  **`EGLManager`**: Thiết lập và quản lý hạ tầng EGL (Context, Display, Surfaces) cần thiết để chạy OpenGL ES trên các luồng native C++ độc lập với luồng UI của Java.
6.  **`PBOManager`**: Tối ưu hóa hiệu năng đọc ngược dữ liệu hình ảnh (Read Pixels) bằng cách sử dụng cơ chế Double PBO. Giúp CPU không bị block chờ đợi GPU hoàn thành tác vụ vẽ, tăng đáng kể FPS.
7.  **`AITask`**: Thực hiện phân tích AI trên luồng ngầm riêng biệt giúp tránh gây giật lag luồng render chính của camera. Có cơ chế thay đổi kích thước bộ đệm động (`resize`) an toàn đa luồng.
