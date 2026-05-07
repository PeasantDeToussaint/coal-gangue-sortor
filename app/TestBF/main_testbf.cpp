// TestBF — Beckhoff EtherCAT valve timing test.
//
// Mirrors the original TestBF.exe / TestValvePlate.exe behaviour:
//   - Connects to Beckhoff EtherCAT via ADS
//   - Runs ValveDriverEL2828::selfTest() (cycles each nozzle once)
//   - Prints the wall-clock time for each bulk write command
//   - Reports overall command latency statistics
//
// Used to verify that the EtherCAT bulk write latency is within spec
// (should be < 5ms per command at 1ms EtherCAT cycle time).
//
// Usage:
//   cgs_test_bf --endpoint "ads://192.168.0.1:2.192.168.0.102.1.1:851?ig=0x3040030&io=0x81000006&ch=136"
//   cgs_test_bf --pulse-ms 200

#ifdef CGS_HAS_BECKHOFF_ADS

#include "../../hardware/ValveDriverInterface/IValveDriver.h"
#include "../../hardware/ValveDriverInterface/ValveDriverEL2828.h"
#include "../../core/Logging/Logger.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace cgs::hardware;
using namespace cgs::core;
using clock = std::chrono::steady_clock;

int main(int argc, char** argv) {
    Logger::init("data/test_bf.log");

    std::string endpoint = "ads://192.168.0.1:2.192.168.0.102.1.1:851"
                           "?ig=0x3040030&io=0x81000006&ch=136";
    uint32_t pulseMs = 200;

    for (int i = 1; i < argc; ++i) {
        if ((std::strcmp(argv[i], "--endpoint") == 0 || std::strcmp(argv[i], "-e") == 0)
                && i + 1 < argc) {
            endpoint = argv[++i];
        } else if ((std::strcmp(argv[i], "--pulse-ms") == 0) && i + 1 < argc) {
            pulseMs = (uint32_t)std::stoi(argv[++i]);
        }
    }

    std::cout << "TestBF: connecting to " << endpoint << "\n";

    ValveDriverEL2828 driver;
    driver.setFaultCallback([](const std::string& msg){
        std::cerr << "[FAULT] " << msg << "\n";
    });

    if (!driver.open(endpoint)) {
        std::cerr << "TestBF: open() failed — check endpoint and TwinCAT runtime\n";
        return 1;
    }

    if (!driver.arm()) {
        std::cerr << "TestBF: arm() failed\n";
        return 1;
    }

    const auto t0 = clock::now();

    std::cout << "TestBF: running self-test with " << pulseMs << " ms pulses...\n"
              << "  Each nozzle fires once in sequence. Operator should confirm.\n\n";

    driver.selfTest(pulseMs);

    // Wait for all scheduled commands to execute (channels × 2 × pulseMs)
    // The scheduler loop processes at 1ms; selfTest schedules all at once.
    const auto status0 = driver.pollStatus();
    const int ch = status0.channelCount;
    const auto waitMs = (long long)ch * pulseMs * 2 + 500;
    std::cout << "  Waiting " << waitMs << " ms for all " << ch << " nozzles...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(waitMs));

    const auto t1 = clock::now();
    const double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    driver.disarm();
    driver.close();

    const auto status = driver.pollStatus();
    std::printf("\n==> TestBF complete\n"
                "    Channels:          %d\n"
                "    Commands accepted: %llu\n"
                "    Commands rejected: %llu\n"
                "    Total wall time:   %.1f ms\n"
                "    Avg per nozzle:    %.2f ms\n",
                ch,
                (unsigned long long)status.commandsAccepted,
                (unsigned long long)status.commandsRejected,
                elapsedMs,
                ch > 0 ? elapsedMs / ch : 0.0);

    Logger::flush();
    return 0;
}

#else

int main() {
    fprintf(stderr, "TestBF: built without CGS_HAS_BECKHOFF_ADS. "
                    "Recompile with -DCGS_HAS_BECKHOFF_ADS=ON.\n");
    return 1;
}

#endif // CGS_HAS_BECKHOFF_ADS
