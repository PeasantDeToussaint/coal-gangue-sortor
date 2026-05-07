#ifndef CGS_DETECTORSETTINGSDIALOG_H
#define CGS_DETECTORSETTINGSDIALOG_H

// Detector control panel.
// Matches the original 探测器 subwindow in Gangue.exe:
//   - 暗场校正 (dark field / offset calibration) button
//   - 亮场校正 (bright field / gain calibration) button
//   - 积分时间 (integration time) editable field
//   - 扫描行数 (scan line count) editable field
//   - 相机校正 (camera calibration) button

#include "../../core/Config/ConfigLoader.h"
#include "../../hardware/DetectorInterface/IDetector.h"
#include <QDialog>
#include <string>

class QSpinBox;
class QLineEdit;
class QPushButton;

namespace cgs { namespace gui {

class DetectorSettingsDialog : public QDialog {
    Q_OBJECT
public:
    DetectorSettingsDialog(cgs::core::HardwareConfig& hw,
                           const std::string& configPath,
                           cgs::hardware::IDetector* detector = nullptr,
                           QWidget* parent = nullptr);

private slots:
    void onDarkFieldCalibrate();
    void onBrightFieldCalibrate();
    void onCameraCalibrate();
    void onAccepted();

private:
    cgs::core::HardwareConfig&  m_hw;
    std::string                 m_configPath;
    cgs::hardware::IDetector*   m_detector{nullptr};

    QPushButton* m_darkFieldBtn{nullptr};    // 暗场校正
    QPushButton* m_brightFieldBtn{nullptr};  // 亮场校正
    QSpinBox*    m_intTimeSpin{nullptr};     // 积分时间
    QSpinBox*    m_lineCountSpin{nullptr};   // 扫描行数
    QPushButton* m_cameraCalBtn{nullptr};    // 相机校正
};

}}
#endif
