#pragma once
// DetectorLibInterface.h — RECONSTRUCTED from DetectorLib.lib symbol table.
//
// Original header was not available. Every class, method, and signal here was
// decoded from the MSVC-mangled names in DetectorLib.lib using:
//   strings DetectorLib.lib | grep __imp_
//
// Mangling key (MSVC x64):
//   PEAG = unsigned short*    H = int    I = unsigned int
//   PEBD = const char*        G = unsigned short
//   VQString@@ = QString (by value)
//   XZ = no parameters    _N = bool
//
// DetectorLib.dll is 袁工's wrapper around the Aurora/XLibDll SDK.
// It is a Qt library — DetectorLibInterface inherits QObject and emits
// signals when scan-line data arrives.
//
// Usage:
//   auto* factory = DetectorLibFactory::getInstance();
//   DetectorLibInterface* det = factory->create("XRay");  // name is arbitrary
//   connect(det, &DetectorLibInterface::signalAcqDataArray, this, &MyClass::onFrame);
//   // configure XDevice, then start acquisition

#ifndef DETECTORLIB_INTERFACE_H
#define DETECTORLIB_INTERFACE_H

#include <QObject>
#include <QString>

// ---------------------------------------------------------------------------
// XDevice — per-detector hardware configuration
// Methods decoded from XDevice@@ mangled symbols.
// ---------------------------------------------------------------------------
class XSystem;

class XDevice {
public:
    XDevice();
    XDevice(XSystem* system);
    ~XDevice();

    // Network configuration
    const char*    GetIP() const;
    void           SetIP(const char* ip);
    unsigned short GetCmdPort() const;
    void           SetCmdPort(unsigned short port);
    unsigned short GetImgPort() const;
    void           SetImgPort(unsigned short port);

    // Hardware identity
    char*          GetSerialNum();
    unsigned char* GetMAC();
    void           SetMAC(unsigned char* mac);
    unsigned int   GetCardNumber() const;
    unsigned int   GetCardType() const;
    unsigned int   GetSerialPort() const;
    void           SetSerialPort(unsigned int port);

    // Detector geometry / config
    unsigned int   GetPixelNumber() const;      // total pixels per row (e.g. 2180)
    unsigned int   GetDMPixelNumber() const;    // pixels per detector module (e.g. 128)
    unsigned int   GetPixelDepth() const;       // bits per pixel (16)
    unsigned int   GetBinningMode() const;
    unsigned int   GetEnergyMode() const;       // single / dual energy
    unsigned int   GetOPMode() const;           // operation mode

    XSystem*       GetSystem();
};

// ---------------------------------------------------------------------------
// XImage — one scan-line image buffer
// ---------------------------------------------------------------------------
class XImage {
public:
    XImage();
    ~XImage();

    // Returns pointer to pixel data for line lineIdx (0-based).
    // Cast to uint16_t* for 16-bit depth.
    unsigned char* GetLineAddr(unsigned int lineIdx);

    unsigned int   GetPixelVal(unsigned int row, unsigned int col);
    void           SetPixelVal(unsigned int row, unsigned int col, unsigned int val);
};

// ---------------------------------------------------------------------------
// DetectorLibInterface — Qt QObject with scan-line data signals.
// This is the main interface used by Gangue.exe.
// ---------------------------------------------------------------------------
class DetectorLibInterface : public QObject {
    Q_OBJECT
public:
    explicit DetectorLibInterface(QObject* parent = nullptr);
    ~DetectorLibInterface() override;

signals:
    // Emitted for every acquired scan-line batch.
    // data   : uint16_t* pixel array, width × height pixels
    // width  : pixels per scan line (e.g. 2180)
    // height : number of scan lines in this callback (usually 1)
    // info   : status / timestamp string (format unspecified)
    void signalAcqDataArray(unsigned short* data, int width, int height, QString info);

    // Emitted when a full acquisition sequence finishes.
    void signalAcqFinished();

    // Internal test signal.
    void signalTest();
};

// ---------------------------------------------------------------------------
// DetectorLibFactory — singleton that creates DetectorLibInterface instances.
// ---------------------------------------------------------------------------
class DetectorLibFactory {
public:
    // Singleton accessor.
    static DetectorLibFactory* getInstance();

    // Create a new DetectorLibInterface. The `name` argument is passed to the
    // underlying Aurora SDK (exact value unknown; try "" or "XRay").
    DetectorLibInterface* create(QString name);

    // Get the most recently created interface (or nullptr).
    DetectorLibInterface* get();

    DetectorLibFactory(const DetectorLibFactory&) = delete;
    DetectorLibFactory& operator=(const DetectorLibFactory&) = delete;
    DetectorLibFactory(DetectorLibFactory&&) = delete;
    DetectorLibFactory& operator=(DetectorLibFactory&&) = delete;

private:
    DetectorLibFactory();
};

#endif // DETECTORLIB_INTERFACE_H
