#include "DetectorSettingsDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace cgs { namespace gui {

DetectorSettingsDialog::DetectorSettingsDialog(cgs::core::HardwareConfig& hw,
                                                const std::string& configPath,
                                                cgs::hardware::IDetector* detector,
                                                QWidget* parent)
    : QDialog(parent), m_hw(hw), m_configPath(configPath), m_detector(detector) {
    setWindowTitle(tr("探测器"));
    setMinimumWidth(360);

    auto* root = new QVBoxLayout(this);

    // ── Calibration buttons (top, matching original layout) ──────
    m_darkFieldBtn   = new QPushButton(tr("暗场校正"), this);
    m_brightFieldBtn = new QPushButton(tr("亮场校正"), this);
    root->addWidget(m_darkFieldBtn);
    root->addWidget(m_brightFieldBtn);

    // ── Parameters ────────────────────────────────────────────────
    auto* paramLayout = new QHBoxLayout();
    paramLayout->addWidget(new QLabel(tr("积分时间"), this));
    m_intTimeSpin = new QSpinBox(this);
    m_intTimeSpin->setRange(100, 10000);
    m_intTimeSpin->setValue(hw.detectorIntTimeUs);
    m_intTimeSpin->setFixedWidth(80);
    paramLayout->addWidget(m_intTimeSpin);
    root->addLayout(paramLayout);

    auto* lineLayout = new QHBoxLayout();
    lineLayout->addWidget(new QLabel(tr("扫描行数"), this));
    m_lineCountSpin = new QSpinBox(this);
    m_lineCountSpin->setRange(100, 5000);
    m_lineCountSpin->setValue(hw.detectorLineCount);
    m_lineCountSpin->setFixedWidth(80);
    lineLayout->addWidget(m_lineCountSpin);
    root->addLayout(lineLayout);

    // ── Camera calibration ────────────────────────────────────────
    m_cameraCalBtn = new QPushButton(tr("相机校正"), this);
    root->addWidget(m_cameraCalBtn);

    root->addStretch();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(buttons);

    connect(m_darkFieldBtn,   &QPushButton::clicked, this, &DetectorSettingsDialog::onDarkFieldCalibrate);
    connect(m_brightFieldBtn, &QPushButton::clicked, this, &DetectorSettingsDialog::onBrightFieldCalibrate);
    connect(m_cameraCalBtn,   &QPushButton::clicked, this, &DetectorSettingsDialog::onCameraCalibrate);
    connect(buttons, &QDialogButtonBox::accepted, this, &DetectorSettingsDialog::onAccepted);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void DetectorSettingsDialog::onDarkFieldCalibrate() {
    // Dark field (offset) calibration: block X-ray, acquire N frames, average.
    // Requires detector SDK (Aurora/XLibDll) to expose a calibration API.
    // On the real machine this calls DetectorLib::darkFieldCalibrate() which
    // triggers a TwinCAT ADS sequence and saves calibrateImageXray.tif.
    if (!m_detector) {
        QMessageBox::information(this, tr("暗场校正"),
            tr("探测器未连接。\n"
               "请确保探测器已连接（192.168.1.199），X射线源已关闭，"
               "然后重试。\n\n"
               "校正完成后结果保存到 config/calibrateImageXray.tif。"));
        return;
    }
    const auto r = m_detector->calibrateDark();
    if (r)
        QMessageBox::information(this, tr("暗场校正"), tr("暗场校正完成。"));
    else
        QMessageBox::warning(this, tr("暗场校正"), tr("暗场校正失败，请检查探测器连接。"));
}

void DetectorSettingsDialog::onBrightFieldCalibrate() {
    // Bright field (gain) calibration: X-ray ON, no material on belt,
    // acquire N frames, normalise gain profile, save to calibrateImageXray.tif.
    if (!m_detector) {
        QMessageBox::information(this, tr("亮场校正"),
            tr("探测器未连接。\n"
               "请确保探测器已连接，X射线源已开启（200kV/2.3mA），"
               "皮带上无料，然后重试。"));
        return;
    }
    const auto r = m_detector->calibrateBright();
    if (r)
        QMessageBox::information(this, tr("亮场校正"), tr("亮场校正完成。"));
    else
        QMessageBox::warning(this, tr("亮场校正"), tr("亮场校正失败。"));
}

void DetectorSettingsDialog::onCameraCalibrate() {
    QMessageBox::information(this, tr("相机校正"),
        tr("相机校正需要在皮带上放置校正板（白色平板），\n"
           "然后触发采图并保存到 config/calibrateImageCamera.png。\n\n"
           "此功能仅在相机+X射线融合模式下需要。"));
}

void DetectorSettingsDialog::onAccepted() {
    m_hw.detectorIntTimeUs  = m_intTimeSpin->value();
    m_hw.detectorLineCount  = m_lineCountSpin->value();
    accept();
}

}}
