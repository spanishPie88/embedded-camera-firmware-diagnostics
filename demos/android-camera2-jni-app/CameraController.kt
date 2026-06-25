package dev.portfolio.camera

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.ImageFormat
import android.graphics.SurfaceTexture
import android.hardware.camera2.*
import android.media.ImageReader
import android.os.Handler
import android.os.HandlerThread
import android.util.Size
import android.view.Surface
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger

/** Camera2 preview + YUV analysis controller with explicit lifecycle ownership. */
class CameraController(
    context: Context,
    private val onStatistics: (String) -> Unit,
    private val onError: (String) -> Unit,
) {
    private val manager = context.getSystemService(Context.CAMERA_SERVICE) as CameraManager
    private val lock = Any()
    private val acceptingFrames = AtomicBoolean(false)
    private val generation = AtomicInteger(0)
    private var thread: HandlerThread? = null
    private var handler: Handler? = null
    private var device: CameraDevice? = null
    private var session: CameraCaptureSession? = null
    private var reader: ImageReader? = null
    private var preview: Surface? = null

    fun start(texture: SurfaceTexture) {
        val token: Int
        synchronized(lock) {
            if (thread != null) return
            thread = HandlerThread("Camera2JniDemo").also { it.start() }
            handler = Handler(thread!!.looper)
            token = generation.incrementAndGet()
        }
        try { openBackCamera(texture, token) }
        catch (error: Exception) { onError("Open failed: ${error.message}"); stop() }
    }

    @SuppressLint("MissingPermission")
    private fun openBackCamera(texture: SurfaceTexture, token: Int) {
        val id = manager.cameraIdList.firstOrNull { cameraId ->
            manager.getCameraCharacteristics(cameraId)
                .get(CameraCharacteristics.LENS_FACING) == CameraCharacteristics.LENS_FACING_BACK
        } ?: manager.cameraIdList.first()
        val map = manager.getCameraCharacteristics(id)
            .get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP)
            ?: error("Missing stream configuration map")
        val previewSize = chooseSize(map.getOutputSizes(SurfaceTexture::class.java).toList(), 1920, 1080)
        val analysisSize = chooseSize(map.getOutputSizes(ImageFormat.YUV_420_888).toList(), 1280, 720)
        texture.setDefaultBufferSize(previewSize.width, previewSize.height)
        preview = Surface(texture)
        reader = ImageReader.newInstance(
            analysisSize.width, analysisSize.height, ImageFormat.YUV_420_888, 3
        ).also { source ->
            source.setOnImageAvailableListener({ onImage(it) }, handler)
        }
        manager.openCamera(id, object : CameraDevice.StateCallback() {
            override fun onOpened(opened: CameraDevice) {
                synchronized(lock) {
                    if (token != generation.get()) { opened.close(); return }
                    device = opened
                }
                createSession(opened, token)
            }
            override fun onDisconnected(disconnected: CameraDevice) {
                disconnected.close(); onError("Camera disconnected")
            }
            override fun onError(failed: CameraDevice, code: Int) {
                failed.close(); onError("CameraDevice error=$code")
            }
        }, handler)
    }

    private fun chooseSize(sizes: List<Size>, width: Int, height: Int): Size =
        sizes.filter { it.width <= width && it.height <= height }
            .maxByOrNull { it.width.toLong() * it.height }
            ?: sizes.minBy { kotlin.math.abs(it.width - width) + kotlin.math.abs(it.height - height) }

    private fun createSession(opened: CameraDevice, token: Int) {
        val previewSurface = preview ?: return
        val analysisSurface = reader?.surface ?: return
        opened.createCaptureSession(listOf(previewSurface, analysisSurface),
            object : CameraCaptureSession.StateCallback() {
                override fun onConfigured(configured: CameraCaptureSession) {
                    synchronized(lock) {
                        if (token != generation.get() || device !== opened) {
                            configured.close(); return
                        }
                        session = configured
                    }
                    val request = opened.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW).apply {
                        addTarget(previewSurface); addTarget(analysisSurface)
                        set(CaptureRequest.CONTROL_AF_MODE,
                            CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE)
                        set(CaptureRequest.CONTROL_AE_MODE, CaptureRequest.CONTROL_AE_MODE_ON)
                    }.build()
                    acceptingFrames.set(true)
                    configured.setRepeatingRequest(request, null, handler)
                }
                override fun onConfigureFailed(failed: CameraCaptureSession) {
                    failed.close(); onError("Session configuration failed")
                }
            }, handler)
    }

    private fun onImage(source: ImageReader) {
        val image = source.acquireLatestImage() ?: return
        try {
            if (!acceptingFrames.get()) return
            require(image.format == ImageFormat.YUV_420_888 && image.planes.size == 3)
            val y = image.planes[0]; val u = image.planes[1]; val v = image.planes[2]
            onStatistics(NativeFrameAnalyzer.analyzeYuv420(
                y.buffer.duplicate().apply { rewind() },
                u.buffer.duplicate().apply { rewind() },
                v.buffer.duplicate().apply { rewind() },
                image.width, image.height,
                y.rowStride, u.rowStride, v.rowStride,
                u.pixelStride, v.pixelStride, image.timestamp
            ))
        } catch (error: RuntimeException) { onError("Frame error: ${error.message}") }
        finally { image.close() }
    }

    fun stop() {
        acceptingFrames.set(false); generation.incrementAndGet()
        val oldThread: HandlerThread?
        synchronized(lock) {
            try { session?.stopRepeating(); session?.abortCaptures() } catch (_: Exception) {}
            session?.close(); session = null
            device?.close(); device = null
            reader?.close(); reader = null
            preview?.release(); preview = null
            oldThread = thread; thread = null; handler = null
        }
        oldThread?.quitSafely(); oldThread?.join(1000)
    }
}

object NativeFrameAnalyzer {
    init { System.loadLibrary("frame_analyzer") }
    external fun analyzeYuv420(
        y: java.nio.ByteBuffer, u: java.nio.ByteBuffer, v: java.nio.ByteBuffer,
        width: Int, height: Int,
        yRowStride: Int, uRowStride: Int, vRowStride: Int,
        uPixelStride: Int, vPixelStride: Int, timestampNs: Long,
    ): String
}
