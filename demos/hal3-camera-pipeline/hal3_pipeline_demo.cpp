// Host-buildable C++17 model of a Camera HAL3 request/result pipeline.
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace camera_demo {
enum class Format { Yuv420, Jpeg, Raw10 };
enum class BufferStatus { Ok, Error };
struct Size { uint32_t width, height; };
struct Stream { int id; Size size; Format format; uint32_t max_buffers; };
struct Metadata { std::map<std::string, int64_t> values; };
struct Buffer { int stream_id; uint64_t id; BufferStatus status = BufferStatus::Ok; };
struct Request {
    uint32_t frame;
    std::optional<Metadata> settings;
    std::vector<Buffer> outputs;
};
struct Result {
    uint32_t frame;
    uint32_t partial;
    Metadata metadata;
    std::vector<Buffer> outputs;
};

class Callback {
public:
    virtual ~Callback() = default;
    virtual void result(const Result&) = 0;
    virtual void error(uint32_t frame, const std::string& detail) = 0;
};
class Backend {
public:
    virtual ~Backend() = default;
    virtual bool configure(const std::vector<Stream>&, std::string*) = 0;
    virtual bool capture(const Request&, Metadata*, std::string*) = 0;
    virtual void cancel() = 0;
};

class HalSession {
public:
    HalSession(std::unique_ptr<Backend> backend, std::shared_ptr<Callback> callback)
        : backend_(std::move(backend)), callback_(std::move(callback)),
          worker_(&HalSession::workerLoop, this) {}
    ~HalSession() { close(); }
    HalSession(const HalSession&) = delete;
    HalSession& operator=(const HalSession&) = delete;

    bool configureStreams(const std::vector<Stream>& streams, std::string* error) {
        flush();
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ == State::Closed) { *error = "session closed"; return false; }
        if (!validateStreams(streams, error)) return false;
        if (!backend_->configure(streams, error)) return false;
        stream_ids_.clear();
        for (const auto& stream : streams) stream_ids_.insert(stream.id);
        repeating_settings_.reset();
        has_frame_ = false;
        state_ = State::Configured;
        return true;
    }

    bool processCaptureRequest(Request request, std::string* error) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!validateRequest(request, error)) return false;
            if (request.settings) repeating_settings_ = request.settings;
            else request.settings = repeating_settings_;
            for (const auto& buffer : request.outputs)
                ownership_[{buffer.stream_id, buffer.id}] = Owner::Hal;
            last_frame_ = request.frame;
            has_frame_ = true;
            queue_.push_back(std::move(request));
        }
        work_.notify_one();
        return true;
    }

    void flush() {
        std::deque<Request> cancelled;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (state_ == State::Closed || state_ == State::Unconfigured) return;
            state_ = State::Flushing;
            cancelled.swap(queue_);
            for (const auto& request : cancelled) releaseBuffers(request);
        }
        backend_->cancel();
        for (auto& request : cancelled) returnError(request, "cancelled by flush");
        std::unique_lock<std::mutex> lock(mutex_);
        idle_.wait(lock, [this] { return !worker_busy_; });
        if (state_ != State::Closed) state_ = State::Configured;
    }

    void close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (state_ == State::Closed) return;
        }
        flush();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            state_ = State::Closed;
            stop_ = true;
        }
        work_.notify_all();
        if (worker_.joinable()) worker_.join();
    }

private:
    enum class State { Unconfigured, Configured, Flushing, Closed };
    enum class Owner { Framework, Hal };

    bool validateStreams(const std::vector<Stream>& streams, std::string* error) {
        if (streams.empty() || streams.size() > 3) {
            *error = "one to three output streams required"; return false;
        }
        std::set<int> ids;
        int raw_count = 0, jpeg_count = 0;
        for (const auto& stream : streams) {
            if (stream.id < 0 || !ids.insert(stream.id).second ||
                stream.size.width == 0 || stream.size.height == 0 ||
                stream.max_buffers == 0) {
                *error = "invalid stream descriptor"; return false;
            }
            if (stream.format == Format::Raw10 && ++raw_count > 1) {
                *error = "only one RAW stream supported"; return false;
            }
            if (stream.format == Format::Jpeg && ++jpeg_count > 1) {
                *error = "only one JPEG stream supported"; return false;
            }
            const bool raw_too_large = stream.format == Format::Raw10 &&
                (stream.size.width > 1920 || stream.size.height > 1080);
            const bool processed_too_large = stream.format != Format::Raw10 &&
                (stream.size.width > 3840 || stream.size.height > 2160);
            if (raw_too_large || processed_too_large) {
                *error = "stream exceeds pipeline capability"; return false;
            }
        }
        return true;
    }

    bool validateRequest(const Request& request, std::string* error) {
        if (state_ != State::Configured) { *error = "not configured"; return false; }
        if (request.outputs.empty()) { *error = "no output buffers"; return false; }
        if (!request.settings && !repeating_settings_) {
            *error = "first request needs settings"; return false;
        }
        if (has_frame_ && request.frame <= last_frame_) {
            *error = "frame numbers must increase"; return false;
        }
        std::set<std::pair<int, uint64_t>> seen;
        for (const auto& buffer : request.outputs) {
            const auto key = std::make_pair(buffer.stream_id, buffer.id);
            if (!stream_ids_.count(buffer.stream_id) || !seen.insert(key).second) {
                *error = "invalid or duplicate buffer"; return false;
            }
            auto existing = ownership_.find(key);
            if (existing != ownership_.end() && existing->second == Owner::Hal) {
                *error = "buffer already owned by HAL"; return false;
            }
        }
        return true;
    }

    void releaseBuffers(const Request& request) {
        for (const auto& buffer : request.outputs)
            ownership_[{buffer.stream_id, buffer.id}] = Owner::Framework;
    }
    void returnError(Request request, const std::string& detail) {
        callback_->error(request.frame, detail);
        for (auto& buffer : request.outputs) buffer.status = BufferStatus::Error;
        callback_->result({request.frame, 0, {}, std::move(request.outputs)});
    }
    void complete(Request request) {
        Result partial{request.frame, 1, {}, {}};
        partial.metadata.values["android.sensor.timestamp"] =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        callback_->result(partial);
        Metadata metadata;
        std::string error;
        if (!backend_->capture(request, &metadata, &error)) returnError(request, error);
        else callback_->result({request.frame, 2, std::move(metadata), request.outputs});
        std::lock_guard<std::mutex> lock(mutex_);
        releaseBuffers(request);
    }
    void workerLoop() {
        for (;;) {
            Request request;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                work_.wait(lock, [this] { return stop_ || !queue_.empty(); });
                if (stop_ && queue_.empty()) return;
                request = std::move(queue_.front()); queue_.pop_front();
                worker_busy_ = true;
            }
            complete(std::move(request));
            {
                std::lock_guard<std::mutex> lock(mutex_);
                worker_busy_ = false;
                if (queue_.empty()) idle_.notify_all();
            }
        }
    }

    std::unique_ptr<Backend> backend_;
    std::shared_ptr<Callback> callback_;
    std::mutex mutex_;
    std::condition_variable work_, idle_;
    std::deque<Request> queue_;
    std::map<std::pair<int, uint64_t>, Owner> ownership_;
    std::set<int> stream_ids_;
    std::optional<Metadata> repeating_settings_;
    std::thread worker_;
    State state_ = State::Unconfigured;
    uint32_t last_frame_ = 0;
    bool has_frame_ = false, worker_busy_ = false, stop_ = false;
};

class FakeV4l2Backend final : public Backend {
public:
    bool configure(const std::vector<Stream>& streams, std::string* error) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (streams.empty()) { *error = "empty configuration"; return false; }
        configured_ = true; return true;
    }
    bool capture(const Request& request, Metadata* metadata, std::string* error) override {
        uint64_t epoch;
        { std::lock_guard<std::mutex> lock(mutex_); if (!configured_) {
            *error = "backend not configured"; return false; } epoch = cancel_epoch_; }
        std::this_thread::sleep_for(std::chrono::milliseconds(12));
        { std::lock_guard<std::mutex> lock(mutex_); if (epoch != cancel_epoch_) {
            *error = "capture cancelled"; return false; } }
        metadata->values["android.request.frameCount"] = request.frame;
        metadata->values["vendor.demo.v4l2Sequence"] = request.frame;
        return true;
    }
    void cancel() override { std::lock_guard<std::mutex> lock(mutex_); ++cancel_epoch_; }
private:
    std::mutex mutex_;
    bool configured_ = false;
    uint64_t cancel_epoch_ = 0;
};

class ConsoleCallback final : public Callback {
public:
    void result(const Result& r) override {
        std::cout << "result frame=" << r.frame << " partial=" << r.partial
                  << " buffers=" << r.outputs.size() << '\n';
    }
    void error(uint32_t frame, const std::string& detail) override {
        std::cout << "error frame=" << frame << " " << detail << '\n';
    }
};
} // namespace camera_demo

int main() {
    using namespace camera_demo;
    auto callback = std::make_shared<ConsoleCallback>();
    HalSession hal(std::make_unique<FakeV4l2Backend>(), callback);
    std::string error;
    if (!hal.configureStreams({{0, {1920,1080}, Format::Yuv420, 4},
                               {1, {3840,2160}, Format::Jpeg, 2}}, &error)) return 1;
    Metadata settings; settings.values["exposure_ns"] = 8000000;
    hal.processCaptureRequest({1, settings, {{0,1001}}}, &error);
    hal.processCaptureRequest({2, std::nullopt, {{0,1002}, {1,2002}}}, &error);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    hal.flush();
}
