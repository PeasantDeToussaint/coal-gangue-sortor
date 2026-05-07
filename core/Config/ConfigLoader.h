#ifndef CGS_CONFIGLOADER_H
#define CGS_CONFIGLOADER_H

#include "../Classifier/Classifier.h"
#include "../Fusion/FusionPolicy.h"
#include "../NozzleMapping/NozzleMapper.h"
#include "../Pipeline/PipelineEngine.h"
#include "../Timing/TimingCalculator.h"

#include <string>

namespace cgs {
namespace core {

// Parsed from <hardware> — used by app entry points to construct factories.
struct HardwareConfig {
    std::string xrayType{"mock"};
    std::string xrayPort{};           // vj-serial: device path or COM name
    double xrayKv{200.0};             // real machine: 200 kV
    double xrayMa{2.3};               // real machine: 2.3 mA

    // LastCloseTime: written on shutdown, read at startup for preheat calc.
    // Format: "YYYYMMDD_HHMMSS" (matching original Gangue.exe convention)
    std::string lastCloseTime{};

    std::string detectorType{"mock"};
    int  detectorWidth{2180};         // 17 modules × 128 px = 2176 (rounded to 2180)
    int  detectorLineRateHz{1000};
    int  detectorLineCount{1150};     // LineNumber: scan lines per 2D frame
    int  detectorIntTimeUs{540};      // intTime in µs (config stores as "540" ms-ish)

    std::string cameraType{"mock"};
    std::string cameraIp{"192.168.1.50"};
    std::string cameraCcfPath{};          // Hikvision: path to .ccf feature file
    int cameraWidth{4096};
    int cameraHeight{1};                  // line-scan: 1 row per acquisition
    double cameraExposureUs{800.0};
    double cameraGainDb{6.0};
    int cameraFrameRateHz{1000};
    bool cameraHardwareTrigger{false};

    std::string valveType{"mock"};
    std::string valveEndpoint{};      // el2828: ads://... ; mock: ignored (open uses "mock")

    std::string plcType{"mock"};
    std::string plcEndpoint{"mock://localhost"};

    double plcInitialBeltHz{40.0};    // from <belt><vfdHzAtNominal> when present

    // Beifu X-ray interlock ADS device.
    // NotUseBeifu="0" in real config.xml → Beifu IS required.
    // BeifuNetID="2.192.168.0.102.1.1" — AMS Net ID on the EtherCAT bus.
    bool        beifuEnabled{false};
    std::string beifuNetId{};         // AMS Net ID string "n1.n2.n3.n4.n5.n6"

    // ACS motion controller TCP endpoint.
    std::string acsIp{"10.0.0.100"};
    int         acsPort{7070};
};

// Search order: argv "--config <path>", env CGS_CONFIG_PATH, then "config/config.xml".
std::string resolveConfigPath(int argc, char** argv);

// If file is missing or invalid XML, returns false (caller should use applyBuiltinDefaults).
bool loadConfigFromFile(const std::string& path,
                        PipelineConfig& pipeline,
                        HardwareConfig& hw);

void applyBuiltinDefaults(bool realHardwarePreset,
                          PipelineConfig& pipeline,
                          HardwareConfig& hw);

FusionMode parseFusionModeString(const std::string& s);

}}

#endif
