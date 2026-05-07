#include "ConfigLoader.h"

#include <tinyxml2.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>

namespace cgs {
namespace core {

namespace {

// Forward declaration required: trim is called by textOfChild, defined below.
std::string trim(std::string s);

const char* attrOrEmpty(const tinyxml2::XMLElement* el, const char* name) {
    if (!el) return "";
    const char* a = el->Attribute(name);
    return a ? a : "";
}

int attrInt(const tinyxml2::XMLElement* el, const char* name, int def) {
    if (!el) return def;
    int v = def;
    if (el->QueryIntAttribute(name, &v) == tinyxml2::XML_SUCCESS) return v;
    return def;
}

double attrDouble(const tinyxml2::XMLElement* el, const char* name, double def) {
    if (!el) return def;
    double v = def;
    if (el->QueryDoubleAttribute(name, &v) == tinyxml2::XML_SUCCESS) return v;
    return def;
}

bool attrBool(const tinyxml2::XMLElement* el, const char* name, bool def) {
    if (!el) return def;
    bool v = def;
    if (el->QueryBoolAttribute(name, &v) == tinyxml2::XML_SUCCESS) return v;
    return def;
}

std::string textOfChild(const tinyxml2::XMLElement* parent, const char* childName) {
    if (!parent) return {};
    const tinyxml2::XMLElement* c = parent->FirstChildElement(childName);
    if (!c || !c->GetText()) return {};
    return trim(c->GetText());
}

double childDouble(const tinyxml2::XMLElement* parent, const char* childName, double def) {
    if (!parent) return def;
    const tinyxml2::XMLElement* c = parent->FirstChildElement(childName);
    if (!c || !c->GetText()) return def;
    char* end = nullptr;
    const double v = std::strtod(c->GetText(), &end);
    if (end == c->GetText()) return def;
    return v;
}

int childInt(const tinyxml2::XMLElement* parent, const char* childName, int def) {
    if (!parent) return def;
    const tinyxml2::XMLElement* c = parent->FirstChildElement(childName);
    if (!c || !c->GetText()) return def;
    char* end = nullptr;
    const long v = std::strtol(c->GetText(), &end, 10);
    if (end == c->GetText()) return def;
    return static_cast<int>(v);
}

std::string trim(std::string s) {
    auto notspace = [](unsigned char ch) { return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
    return s;
}

} // namespace

std::string resolveConfigPath(int argc, char** argv) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::strcmp(argv[i], "--config") == 0) return argv[i + 1];
    }
    if (const char* e = std::getenv("CGS_CONFIG_PATH")) {
        if (e[0] != '\0') return std::string(e);
    }
    return "config/config.xml";
}

FusionMode parseFusionModeString(const std::string& in) {
    std::string s;
    for (char c : in) s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    s = trim(std::move(s));
    if (s == "xrayonly") return FusionMode::XRayOnly;
    if (s == "cameraonly") return FusionMode::CameraOnly;
    if (s == "andreject") return FusionMode::AndReject;
    if (s == "orreject") return FusionMode::OrReject;
    return FusionMode::XRayAuthoritative;
}

bool loadConfigFromFile(const std::string& path,
                        PipelineConfig& pipeline,
                        HardwareConfig& hw) {
    namespace fs = std::filesystem;
    if (path.empty() || !fs::exists(path)) return false;

    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(path.c_str()) != tinyxml2::XML_SUCCESS) return false;

    const tinyxml2::XMLElement* root = doc.RootElement();
    if (!root || std::strcmp(root->Name(), "config") != 0) return false;

    const tinyxml2::XMLElement* hwEl = root->FirstChildElement("hardware");
    if (hwEl) {
        const tinyxml2::XMLElement* xr = hwEl->FirstChildElement("xray");
        if (xr) {
            hw.xrayType = attrOrEmpty(xr, "type");
            if (hw.xrayType.empty()) hw.xrayType = "mock";
            hw.xrayPort = attrOrEmpty(xr, "port");
            hw.xrayKv = attrDouble(xr, "kv", hw.xrayKv);
            hw.xrayMa = attrDouble(xr, "ma", hw.xrayMa);
            // Beifu interlock: notUseBeifu="0" means Beifu IS used.
            const int notUseBeifu = attrInt(xr, "notUseBeifu", 1);
            hw.beifuEnabled = (notUseBeifu == 0);
            const char* beifuNetId = xr->Attribute("beifuNetId");
            if (beifuNetId && beifuNetId[0]) hw.beifuNetId = beifuNetId;
        }
        // ACS motion controller
        const tinyxml2::XMLElement* acs = hwEl->FirstChildElement("acs");
        if (acs) {
            hw.acsIp   = attrOrEmpty(acs, "ip");
            hw.acsPort = attrInt(acs, "port", hw.acsPort);
            if (hw.acsIp.empty()) hw.acsIp = "10.0.0.100";
        }
        const tinyxml2::XMLElement* det = hwEl->FirstChildElement("detector");
        if (det) {
            hw.detectorType    = attrOrEmpty(det, "type");
            if (hw.detectorType.empty()) hw.detectorType = "mock";
            hw.detectorLineRateHz = attrInt(det, "lineRateHz", hw.detectorLineRateHz);
            hw.detectorWidth   = attrInt(det, "width",      hw.detectorWidth);
            hw.detectorLineCount = attrInt(det, "lineCount", hw.detectorLineCount);
            hw.detectorIntTimeUs = attrInt(det, "intTimeUs", hw.detectorIntTimeUs);
        }

        // LastCloseTime stored as attribute on <xray> (matches original format)
        const tinyxml2::XMLElement* xrEl = hwEl->FirstChildElement("xray");
        if (xrEl) {
            const char* lct = xrEl->Attribute("lastCloseTime");
            if (lct && lct[0] != '\0') hw.lastCloseTime = lct;
        }
        const tinyxml2::XMLElement* cam = hwEl->FirstChildElement("camera");
        if (cam) {
            hw.cameraType = attrOrEmpty(cam, "type");
            if (hw.cameraType.empty()) hw.cameraType = "mock";
            hw.cameraIp = attrOrEmpty(cam, "ip");
            if (hw.cameraIp.empty()) hw.cameraIp = "192.168.1.50";
            hw.cameraCcfPath = attrOrEmpty(cam, "ccfPath");
            hw.cameraWidth = attrInt(cam, "width", hw.cameraWidth);
            hw.cameraHeight = attrInt(cam, "height", hw.cameraHeight);
            hw.cameraExposureUs = attrDouble(cam, "exposureUs", hw.cameraExposureUs);
            hw.cameraGainDb = attrDouble(cam, "gainDb", hw.cameraGainDb);
            hw.cameraFrameRateHz = attrInt(cam, "frameRateHz", hw.cameraFrameRateHz);
            hw.cameraHardwareTrigger = attrBool(cam, "hardwareTrigger", hw.cameraHardwareTrigger);
        }
        const tinyxml2::XMLElement* val = hwEl->FirstChildElement("valveDriver");
        if (val) {
            hw.valveType = attrOrEmpty(val, "type");
            if (hw.valveType.empty()) hw.valveType = "mock";
            const char* ep = val->Attribute("endpoint");
            hw.valveEndpoint = ep ? ep : attrOrEmpty(val, "port");
        }
        const tinyxml2::XMLElement* plc = hwEl->FirstChildElement("plc");
        if (plc) {
            hw.plcType = attrOrEmpty(plc, "type");
            if (hw.plcType.empty()) hw.plcType = "mock";
            hw.plcEndpoint = attrOrEmpty(plc, "endpoint");
            if (hw.plcEndpoint.empty()) hw.plcEndpoint = "mock://localhost";
        }
    }

    const tinyxml2::XMLElement* geom = root->FirstChildElement("geometry");
    if (geom) {
        pipeline.nozzles.totalNozzles     = childInt(geom, "nozzleCount",         pipeline.nozzles.totalNozzles);
        pipeline.nozzles.firstNozzleId    = childInt(geom, "firstNozzleId",        pipeline.nozzles.firstNozzleId);
        pipeline.nozzles.pixelsPerNozzle  = childDouble(geom, "pixelsPerNozzle",   pipeline.nozzles.pixelsPerNozzle);
        pipeline.nozzles.beltLeftPaddingPx  = childInt(geom, "beltLeftPaddingPx",  pipeline.nozzles.beltLeftPaddingPx);
        pipeline.nozzles.beltRightPaddingPx = childInt(geom, "beltRightPaddingPx", pipeline.nozzles.beltRightPaddingPx);
        pipeline.timing.sensorToNozzleMm = childDouble(geom, "sensorToNozzleMm",   pipeline.timing.sensorToNozzleMm);

        // Optional QEnd helper: compute rightPadding if caller provided detectorWidth and qEnd
        const int qEnd = childInt(geom, "qEnd", 0);
        if (qEnd > 0 && hw.detectorWidth > qEnd) {
            pipeline.nozzles.beltRightPaddingPx = hw.detectorWidth - qEnd;
        }
    }

    const tinyxml2::XMLElement* belt = root->FirstChildElement("belt");
    if (belt) {
        hw.plcInitialBeltHz = childDouble(belt, "vfdHzAtNominal", hw.plcInitialBeltHz);
    }

    const tinyxml2::XMLElement* tim = root->FirstChildElement("timing");
    if (tim) {
        // timerGapMs: confirmed from decompilation — original uses fixed TimerGap (not dynamic formula).
        // Maps to TimerGap="2456" in the real config.xml DetectorParamsInfo attribute.
        pipeline.timing.timerGapMs = childDouble(tim, "timerGapMs", pipeline.timing.timerGapMs);
        pipeline.timing.valveOpenLatencyMs = childDouble(tim, "valveOpenLatencyMs", pipeline.timing.valveOpenLatencyMs);
        pipeline.timing.valveCloseLatencyMs = childDouble(tim, "valveCloseLatencyMs", pipeline.timing.valveCloseLatencyMs);
        pipeline.timing.pneumaticTravelMs = childDouble(tim, "pneumaticTravelMs", pipeline.timing.pneumaticTravelMs);
        pipeline.timing.safetyMarginMs = childDouble(tim, "safetyMarginMs", pipeline.timing.safetyMarginMs);
        // sensorToNozzleMm is parsed from <geometry><sensorToNozzleMm> above.
    }

    const tinyxml2::XMLElement* clf = root->FirstChildElement("classifier");
    if (clf) {
        const int xe = childInt(clf, "xrayEmptyMin", pipeline.classifier.xrayEmptyMin);
        const int xc = childInt(clf, "xrayCoalMin", pipeline.classifier.xrayCoalMin);
        const int xg = childInt(clf, "xrayGangueMax", pipeline.classifier.xrayGangueMax);
        pipeline.classifier.xrayEmptyMin = static_cast<uint16_t>(std::max(0, std::min(65535, xe)));
        pipeline.classifier.xrayCoalMin = static_cast<uint16_t>(std::max(0, std::min(65535, xc)));
        pipeline.classifier.xrayGangueMax = static_cast<uint16_t>(std::max(0, std::min(65535, xg)));
        pipeline.classifier.cameraEmptyMin = static_cast<uint8_t>(std::max(0, std::min(255, childInt(clf, "cameraEmptyMin", pipeline.classifier.cameraEmptyMin))));
        pipeline.classifier.cameraGangueMax = static_cast<uint8_t>(std::max(0, std::min(255, childInt(clf, "cameraGangueMax", pipeline.classifier.cameraGangueMax))));
        pipeline.classifier.minObjectWidthPx = childInt(clf, "minObjectWidthPx", pipeline.classifier.minObjectWidthPx);
    }

    const tinyxml2::XMLElement* fus = root->FirstChildElement("fusion");
    if (fus) {
        const std::string modeStr = textOfChild(fus, "mode");
        if (!modeStr.empty()) pipeline.fusion.mode = parseFusionModeString(modeStr);
    }

    // Machine configuration (para0_0_x flags)
    const tinyxml2::XMLElement* mach = root->FirstChildElement("machineConfig");
    if (mach) {
        const std::string orient = textOfChild(mach, "orientation");
        if (orient == "vertical")
            pipeline.nozzles.orientation = MachineOrientation::Vertical;
        else
            pipeline.nozzles.orientation = MachineOrientation::Horizontal;

        // XCCR polynomial from <geometry><xccr>
    }

    // XCCR polynomial (geometry section)
    if (geom) {
        const std::string xccrStr = textOfChild(geom, "xccr");
        if (!xccrStr.empty()) {
            pipeline.nozzles.setXccrFromString(xccrStr);
        }
    }

    // Inference engine config
    const tinyxml2::XMLElement* inf = root->FirstChildElement("inference");
    if (inf) {
        pipeline.inference.trtPath    = textOfChild(inf, "trtPath");
        pipeline.inference.onnxPath   = textOfChild(inf, "onnxPath");
        pipeline.inference.imageSize  = childInt(inf, "imageSize", pipeline.inference.imageSize);
        pipeline.inference.iouThreshold   = (float)childDouble(inf, "iouThreshold",   pipeline.inference.iouThreshold);
        pipeline.inference.scoreThreshold = (float)childDouble(inf, "scoreThreshold", pipeline.inference.scoreThreshold);
        pipeline.inference.classCount  = childInt(inf, "classCount", pipeline.inference.classCount);
        pipeline.inference.minDetectionWidthPx = childInt(inf, "minDetectionWidthPx",
                                                          pipeline.inference.minDetectionWidthPx);
        // Parse classEjectMask "1,0,0"
        const std::string maskStr = textOfChild(inf, "classEjectMask");
        if (!maskStr.empty()) {
            pipeline.inference.classEjectMask.clear();
            std::istringstream ss(maskStr);
            std::string tok;
            while (std::getline(ss, tok, ','))
                pipeline.inference.classEjectMask.push_back(std::stoi(tok));
        }
        pipeline.inference.burstScalePercent      = childInt(inf, "burstScalePercent",      pipeline.inference.burstScalePercent);
        pipeline.inference.burstScaleSmallPercent = childInt(inf, "burstScaleSmallPercent", pipeline.inference.burstScaleSmallPercent);
        pipeline.inference.burstScaleLargePercent = childInt(inf, "burstScaleLargePercent", pipeline.inference.burstScaleLargePercent);
        pipeline.inference.smallObjectLenMm = childDouble(inf, "smallObjectLenMm", pipeline.inference.smallObjectLenMm);
        pipeline.inference.largeObjectLenMm = childDouble(inf, "largeObjectLenMm", pipeline.inference.largeObjectLenMm);
    }

    // Line count (detector)
    pipeline.lineCount = hw.detectorLineCount;

    return true;
}

void applyBuiltinDefaults(bool realHardwarePreset,
                          PipelineConfig& pipeline,
                          HardwareConfig& hw) {
    hw = HardwareConfig{};
    pipeline = PipelineConfig{};

    if (!realHardwarePreset) {
        hw.detectorWidth = 2180;
        hw.detectorLineRateHz = 100;
        hw.detectorLineCount = 1150;
        hw.plcInitialBeltHz = 40.0;
    } else {
        hw.xrayType       = "XRayLib";   // matches real config.xml Type="XRayLib"
        hw.xrayPort       = "serial://COM1";
        hw.xrayKv         = 200.0;
        hw.xrayMa         = 2.3;
        hw.detectorType   = "aurora";
        hw.detectorWidth  = 2180;
        hw.detectorLineRateHz = 1000;
        hw.detectorLineCount  = 1150;
        hw.detectorIntTimeUs  = 540;
        hw.cameraType     = "mock";
        hw.valveType      = "el2828";
        hw.plcType        = "s7-1500";
        hw.plcEndpoint    = "s7://192.168.2.100:102";
        hw.valveEndpoint  = "ads://192.168.X.X:NETID:851?ig=0x3040030&io=0x81000006&ch=136";
        hw.plcInitialBeltHz = 40.0;
    }

    // Real machine: QNum=136, QStart=11, QEnd=1066, detectorWidth=2180
    pipeline.nozzles.totalNozzles     = 136;
    pipeline.nozzles.firstNozzleId    = 1;
    pipeline.nozzles.beltLeftPaddingPx = 11;   // QStart
    // QEnd=1066 → rightPadding = detectorWidth - QEnd = 2180 - 1066 = 1114
    pipeline.nozzles.beltRightPaddingPx = hw.detectorWidth - 1066;
    // pixelsPerNozzle = (QEnd - QStart) / QNum = (1066-11)/136 ≈ 7.757
    pipeline.nozzles.pixelsPerNozzle =
        static_cast<double>(1066 - 11) / static_cast<double>(136);
    pipeline.timing.sensorToNozzleMm = 2498.0;  // DQ value from config.xml
    pipeline.timing.timerGapMs = 2456.0;        // TimerGap from real config.xml (confirmed)
}

}}
