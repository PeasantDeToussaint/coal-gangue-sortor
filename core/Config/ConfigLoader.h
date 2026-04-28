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
    std::string xrayPort{};           // vj-serial: device path or COM name; normalized at open time
    double xrayKv{80.0};
    double xrayMa{5.0};

    std::string detectorType{"mock"};
    int detectorWidth{1024};
    int detectorLineRateHz{1000};

    std::string cameraType{"mock"};
    std::string cameraIp{"192.168.1.50"};
    int cameraWidth{2048};
    int cameraHeight{1024};
    double cameraExposureUs{800.0};
    double cameraGainDb{6.0};
    int cameraFrameRateHz{30};
    bool cameraHardwareTrigger{false};

    std::string valveType{"mock"};
    std::string valveEndpoint{};      // el2828: ads://... ; mock: ignored (open uses "mock")

    std::string plcType{"mock"};
    std::string plcEndpoint{"mock://localhost"};

    double plcInitialBeltHz{40.0};    // from <belt><vfdHzAtNominal> when present
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
