#ifndef CGS_LOGGER_H
#define CGS_LOGGER_H

// Application-wide logger wrapping spdlog.
//
// Matches the original gangue_sys.log pattern:
//   (YYYY-MM-DD HH:MM:SS 周X) Debug: File:() Line:(0) function:() -----message
//
// Usage:
//   CGS_LOG_DEBUG("-----begin process: \"{}\"", timeStr);
//   CGS_LOG_INFO("connected to detector at {}", ip);
//   CGS_LOG_WARN("DQ timing negative: {}ms — belt too fast", delay);
//   CGS_LOG_ERROR("ADS write failed: {}", errCode);
//
// Call Logger::init() once at application startup before any other code.
// The rotating file sink writes to data/gangue_sys.log with a .old backup,
// matching the original two-file rotation scheme.

#include <spdlog/spdlog.h>
#include <spdlog/logger.h>

#include <functional>
#include <memory>
#include <string>

namespace cgs {
namespace core {

class Logger {
public:
    // Init: creates rotating file sink + console sink.
    // logPath: e.g. "data/gangue_sys.log" (directory is created if absent).
    // maxSizeBytes: rotate at this size (default 50 MB = original .log + .old pattern).
    static void init(const std::string& logPath   = "data/gangue_sys.log",
                     size_t maxSizeBytes           = 50 * 1024 * 1024,
                     int maxFiles                  = 2);

    static std::shared_ptr<spdlog::logger> get();

    // Flush all sinks (call on shutdown).
    static void flush();

    // Register an additional Qt-based sink (for LogCenter dock widget).
    // Callback is called on the thread that logs the message; it must be
    // thread-safe (use Qt::QueuedConnection on the receiving side).
    using SinkCallback = std::function<void(const std::string& formattedMsg)>;
    static void addCallbackSink(SinkCallback cb);
};

}}

// Convenience macros — same style as the original log output
#define CGS_LOG_TRACE(...)   if (auto _l = cgs::core::Logger::get()) _l->trace(__VA_ARGS__)
#define CGS_LOG_DEBUG(...)   if (auto _l = cgs::core::Logger::get()) _l->debug(__VA_ARGS__)
#define CGS_LOG_INFO(...)    if (auto _l = cgs::core::Logger::get()) _l->info(__VA_ARGS__)
#define CGS_LOG_WARN(...)    if (auto _l = cgs::core::Logger::get()) _l->warn(__VA_ARGS__)
#define CGS_LOG_ERROR(...)   if (auto _l = cgs::core::Logger::get()) _l->error(__VA_ARGS__)

#endif
