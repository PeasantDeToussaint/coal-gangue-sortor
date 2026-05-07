#include "Logger.h"

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/base_sink.h>

#include <filesystem>
#include <mutex>
#include <vector>

namespace cgs {
namespace core {

// ---------------------------------------------------------------------------
// CallbackSink — forwards log records to registered callbacks (Qt LogCenter).
// ---------------------------------------------------------------------------
template <typename Mutex>
class CallbackSink : public spdlog::sinks::base_sink<Mutex> {
public:
    using Callback = Logger::SinkCallback;
    void addCallback(Callback cb) {
        std::lock_guard<Mutex> lk(this->mutex_);
        m_callbacks.push_back(std::move(cb));
    }
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t buf;
        this->formatter_->format(msg, buf);
        const std::string s(buf.data(), buf.size());
        for (auto& cb : m_callbacks) cb(s);
    }
    void flush_() override {}
    std::vector<Callback> m_callbacks;
};

using CallbackSinkMt = CallbackSink<std::mutex>;

// ---------------------------------------------------------------------------
// Statics
// ---------------------------------------------------------------------------
static std::shared_ptr<spdlog::logger>  s_logger;
static std::shared_ptr<CallbackSinkMt>  s_callbackSink;
static std::mutex                        s_initMtx;

void Logger::init(const std::string& logPath, size_t maxSizeBytes, int maxFiles) {
    std::lock_guard<std::mutex> lk(s_initMtx);
    if (s_logger) return;  // already initialised

    // Ensure log directory exists.
    namespace fs = std::filesystem;
    const fs::path dir = fs::path(logPath).parent_path();
    if (!dir.empty()) fs::create_directories(dir);

    std::vector<spdlog::sink_ptr> sinks;

    // Rotating file sink: maxFiles=2 gives gangue_sys.log + gangue_sys.log.1
    // matching the original .log + .log.old scheme.
    try {
        auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            logPath, maxSizeBytes, maxFiles);
        fileSink->set_level(spdlog::level::debug);
        // Pattern matches original: "(2026-05-01 10:16:50 周五) Debug: -----message"
        fileSink->set_pattern("(%Y-%m-%d %H:%M:%S) %l: %v");
        sinks.push_back(fileSink);
    } catch (...) {}

    // Colour console sink (stdout)
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(spdlog::level::info);
    consoleSink->set_pattern("[CGS] %^%l%$ %v");
    sinks.push_back(consoleSink);

    // Callback sink (for Qt LogCenter)
    s_callbackSink = std::make_shared<CallbackSinkMt>();
    s_callbackSink->set_level(spdlog::level::debug);
    s_callbackSink->set_pattern("(%H:%M:%S) %l: %v");
    sinks.push_back(s_callbackSink);

    s_logger = std::make_shared<spdlog::logger>("cgs", sinks.begin(), sinks.end());
    s_logger->set_level(spdlog::level::debug);
    s_logger->flush_on(spdlog::level::warn);
    spdlog::register_logger(s_logger);
}

std::shared_ptr<spdlog::logger> Logger::get() {
    return s_logger;
}

void Logger::flush() {
    if (s_logger) s_logger->flush();
}

void Logger::addCallbackSink(SinkCallback cb) {
    std::lock_guard<std::mutex> lk(s_initMtx);
    if (s_callbackSink) s_callbackSink->addCallback(std::move(cb));
}

}}
