#include <jni.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
struct Plane {
    const uint8_t* data;
    jlong capacity;
    int row_stride;
    int pixel_stride;
};
struct Stats {
    uint64_t sum = 0, samples = 0;
    uint8_t minimum = std::numeric_limits<uint8_t>::max();
    uint8_t maximum = std::numeric_limits<uint8_t>::min();
    void add(uint8_t value) {
        sum += value; ++samples;
        minimum = std::min(minimum, value);
        maximum = std::max(maximum, value);
    }
    double mean() const { return samples ? static_cast<double>(sum) / samples : 0.0; }
};
Plane getPlane(JNIEnv* env, jobject buffer, int row, int pixel, const char* name) {
    auto* data = static_cast<const uint8_t*>(env->GetDirectBufferAddress(buffer));
    const jlong capacity = env->GetDirectBufferCapacity(buffer);
    if (!data || capacity <= 0) throw std::invalid_argument(std::string(name) + " is not direct");
    if (row <= 0 || pixel <= 0) throw std::invalid_argument(std::string(name) + " bad stride");
    return {data, capacity, row, pixel};
}
Stats sample(const Plane& plane, int width, int height, int step) {
    Stats stats;
    for (int y = 0; y < height; y += step) {
        const int64_t row = static_cast<int64_t>(y) * plane.row_stride;
        for (int x = 0; x < width; x += step) {
            const int64_t offset = row + static_cast<int64_t>(x) * plane.pixel_stride;
            if (offset < 0 || offset >= plane.capacity)
                throw std::out_of_range("plane stride exceeds buffer capacity");
            stats.add(plane.data[offset]);
        }
    }
    return stats;
}
} // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_dev_portfolio_camera_NativeFrameAnalyzer_analyzeYuv420(
    JNIEnv* env, jobject, jobject y_buffer, jobject u_buffer, jobject v_buffer,
    jint width, jint height, jint y_row_stride, jint u_row_stride,
    jint v_row_stride, jint u_pixel_stride, jint v_pixel_stride,
    jlong timestamp_ns) {
    try {
        if (width <= 0 || height <= 0) throw std::invalid_argument("invalid dimensions");
        const Plane y = getPlane(env, y_buffer, y_row_stride, 1, "Y");
        const Plane u = getPlane(env, u_buffer, u_row_stride, u_pixel_stride, "U");
        const Plane v = getPlane(env, v_buffer, v_row_stride, v_pixel_stride, "V");
        const Stats ys = sample(y, width, height, 8);
        const Stats us = sample(u, (width + 1) / 2, (height + 1) / 2, 4);
        const Stats vs = sample(v, (width + 1) / 2, (height + 1) / 2, 4);
        char output[512];
        std::snprintf(output, sizeof(output),
            "Camera2 + JNI frame\n%dx%d timestamp=%lld ns\n"
            "Y mean=%.2f range=[%u,%u] samples=%llu\n"
            "U mean=%.2f range=[%u,%u]\nV mean=%.2f range=[%u,%u]",
            width, height, static_cast<long long>(timestamp_ns),
            ys.mean(), ys.minimum, ys.maximum,
            static_cast<unsigned long long>(ys.samples),
            us.mean(), us.minimum, us.maximum,
            vs.mean(), vs.minimum, vs.maximum);
        return env->NewStringUTF(output);
    } catch (const std::exception& error) {
        jclass type = env->FindClass("java/lang/IllegalArgumentException");
        if (type) env->ThrowNew(type, error.what());
        return nullptr;
    }
}
