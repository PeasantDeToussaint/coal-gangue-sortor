#ifndef CGS_XRAYSETTINGSDIALOG_H
#define CGS_XRAYSETTINGSDIALOG_H

// X-ray source live control panel.
// Matches the original 光机 subwindow in Gangue.exe:
//   - 连接/断开 (connect/disconnect)
//   - 开启 toggle (enable beam)
//   - 停机时间 + preheat progress
//   - 设定电压/KV and 设定电流/mA with sliders
//   - 光机状态: 10 fault indicator checkboxes
//   - 错误清除 (clear faults) button

#include "../../core/Config/ConfigLoader.h"
#include "../../hardware/XRayInterface/IXRaySource.h"
#include <QDialog>
#include <QTimer>
#include <string>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QSlider;

namespace cgs { namespace gui {

class XRaySettingsDialog : public QDialog {
    Q_OBJECT
public:
    XRaySettingsDialog(cgs::core::HardwareConfig& hw,
                       const std::string& configPath,
                       cgs::hardware::IXRaySource* xray = nullptr,
                       QWidget* parent = nullptr);
    ~XRaySettingsDialog() override;

private slots:
    void onConnect();
    void onDisconnect();
    void onEnableToggled(bool on);
    void onSetKvMa();
    void onClearFaults();
    void onPollTimer();
    void onAccepted();

private:
    void updateFaultDisplay(const cgs::hardware::XRayStatus& s);

    cgs::core::HardwareConfig&      m_hw;
    std::string                     m_configPath;
    cgs::hardware::IXRaySource*     m_xray{nullptr};
    QTimer                          m_pollTimer;

    // Connection controls
    QPushButton* m_connectBtn{nullptr};
    QPushButton* m_disconnectBtn{nullptr};

    // Enable
    QPushButton* m_enableBtn{nullptr};   // 开启 toggle

    // Preheat info
    QLabel*      m_downtimeLabel{nullptr};
    QProgressBar* m_preheatBar{nullptr};

    // kV / mA
    QDoubleSpinBox* m_kvSpin{nullptr};
    QSlider*        m_kvSlider{nullptr};
    QDoubleSpinBox* m_maSpin{nullptr};
    QSlider*        m_maSlider{nullptr};
    QPushButton*    m_setBtn{nullptr};

    // Fault indicators (光机状态)
    QCheckBox* m_faultCathodeOV{nullptr};   // 阴极电压过高
    QCheckBox* m_faultAnodeOV{nullptr};     // 阳极电压过高
    QCheckBox* m_faultArc{nullptr};         // Arc检测错误
    QCheckBox* m_faultTemp{nullptr};        // 温度过热
    QCheckBox* m_faultCurrentOV{nullptr};   // 电流过高
    QCheckBox* m_faultVoltageOV{nullptr};   // 电压过高
    QCheckBox* m_faultPower{nullptr};       // 功率
    QCheckBox* m_faultDutyCycle{nullptr};   // Duty Cycle Mode
    QCheckBox* m_faultInterlock{nullptr};   // 联锁打开
    QCheckBox* m_faultGeneral{nullptr};     // 常规
    QPushButton* m_clearFaultsBtn{nullptr}; // 错误清除

    // Settings fields
    QLineEdit* m_portEdit{nullptr};
    bool m_beamOn{false};
};

}}
#endif
