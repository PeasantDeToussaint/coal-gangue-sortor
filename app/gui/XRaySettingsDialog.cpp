#include "XRaySettingsDialog.h"
#include "../../hardware/XRayInterface/XRaySerial.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

namespace cgs { namespace gui {

XRaySettingsDialog::XRaySettingsDialog(cgs::core::HardwareConfig& hw,
                                        const std::string& configPath,
                                        cgs::hardware::IXRaySource* xray,
                                        QWidget* parent)
    : QDialog(parent), m_hw(hw), m_configPath(configPath), m_xray(xray) {
    setWindowTitle(tr("光机"));
    setMinimumWidth(460);

    auto* root = new QVBoxLayout(this);

    // ── Connection row ──────────────────────────────────────────
    auto* connRow = new QHBoxLayout();
    m_connectBtn    = new QPushButton(tr("连接"), this);
    m_disconnectBtn = new QPushButton(tr("断开"), this);
    connRow->addWidget(m_connectBtn);
    connRow->addWidget(m_disconnectBtn);
    connRow->addStretch();
    root->addLayout(connRow);

    // ── Enable toggle + preheat ─────────────────────────────────
    auto* enableRow = new QHBoxLayout();
    m_enableBtn = new QPushButton(tr("开启"), this);
    m_enableBtn->setCheckable(true);
    m_enableBtn->setStyleSheet(
        "QPushButton:checked { background-color: #27ae60; color: white; font-weight: bold; }");
    enableRow->addWidget(new QLabel(tr("开启"), this));
    enableRow->addWidget(m_enableBtn);

    m_downtimeLabel = new QLabel(
        hw.lastCloseTime.empty()
            ? tr("停机时间 未知")
            : tr("停机时间 %1").arg(QString::fromStdString(hw.lastCloseTime)),
        this);
    enableRow->addWidget(m_downtimeLabel);

    m_preheatBar = new QProgressBar(this);
    m_preheatBar->setRange(0, 100);
    m_preheatBar->setValue(0);
    m_preheatBar->setFixedWidth(80);
    m_preheatBar->setStyleSheet("QProgressBar::chunk { background-color: #3498db; }");
    enableRow->addWidget(new QLabel(tr("预热"), this));
    enableRow->addWidget(m_preheatBar);
    root->addLayout(enableRow);

    // ── kV / mA ──────────────────────────────────────────────────
    auto* kvRow = new QHBoxLayout();
    kvRow->addWidget(new QLabel(tr("设定电压/KV"), this));
    m_kvSpin = new QDoubleSpinBox(this);
    m_kvSpin->setRange(0, 250); m_kvSpin->setSingleStep(10);
    m_kvSpin->setValue(hw.xrayKv); m_kvSpin->setDecimals(1);
    m_kvSpin->setFixedWidth(80);
    m_kvSlider = new QSlider(Qt::Horizontal, this);
    m_kvSlider->setRange(0, 250); m_kvSlider->setValue((int)hw.xrayKv);
    kvRow->addWidget(m_kvSpin);
    kvRow->addWidget(m_kvSlider);
    root->addLayout(kvRow);

    auto* maRow = new QHBoxLayout();
    maRow->addWidget(new QLabel(tr("设定电流/mA"), this));
    m_maSpin = new QDoubleSpinBox(this);
    m_maSpin->setRange(0, 10); m_maSpin->setSingleStep(0.1);
    m_maSpin->setDecimals(2); m_maSpin->setValue(hw.xrayMa);
    m_maSpin->setFixedWidth(80);
    m_maSlider = new QSlider(Qt::Horizontal, this);
    m_maSlider->setRange(0, 1000); m_maSlider->setValue((int)(hw.xrayMa * 100));
    maRow->addWidget(m_maSpin);
    maRow->addWidget(m_maSlider);
    root->addLayout(maRow);

    m_setBtn = new QPushButton(tr("设置"), this);
    root->addWidget(m_setBtn);

    // ── Fault indicators (光机状态) ───────────────────────────────
    auto* faultBox = new QGroupBox(tr("光机状态"), this);
    auto* faultLayout = new QVBoxLayout(faultBox);

    auto makeFault = [&](const QString& label) -> QCheckBox* {
        auto* cb = new QCheckBox(label, this);
        cb->setEnabled(false);          // read-only indicator
        cb->setStyleSheet("QCheckBox::indicator:checked { background-color: red; }");
        faultLayout->addWidget(cb);
        return cb;
    };
    m_faultCathodeOV  = makeFault(tr("阴极电压过高"));
    m_faultAnodeOV    = makeFault(tr("阳极电压过高"));
    m_faultArc        = makeFault(tr("Arc检测错误"));
    m_faultTemp       = makeFault(tr("温度过热"));
    m_faultCurrentOV  = makeFault(tr("电流过高"));
    m_faultVoltageOV  = makeFault(tr("电压过高"));
    m_faultPower      = makeFault(tr("功率"));
    m_faultDutyCycle  = makeFault(tr("Duty Cycle Mode"));
    m_faultInterlock  = makeFault(tr("联锁打开"));
    m_faultGeneral    = makeFault(tr("常规"));
    root->addWidget(faultBox);

    m_clearFaultsBtn = new QPushButton(tr("错误清除"), this);
    root->addWidget(m_clearFaultsBtn);

    // ── Settings (port) ──────────────────────────────────────────
    auto* settGrp  = new QGroupBox(tr("串口设置"), this);
    auto* settForm = new QFormLayout(settGrp);
    m_portEdit = new QLineEdit(QString::fromStdString(hw.xrayPort), this);
    m_portEdit->setPlaceholderText("serial://COM1");
    settForm->addRow(tr("串口:"), m_portEdit);
    root->addWidget(settGrp);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(buttons);

    // ── Connections ───────────────────────────────────────────────
    connect(m_connectBtn,    &QPushButton::clicked,  this, &XRaySettingsDialog::onConnect);
    connect(m_disconnectBtn, &QPushButton::clicked,  this, &XRaySettingsDialog::onDisconnect);
    connect(m_enableBtn,     &QPushButton::toggled,  this, &XRaySettingsDialog::onEnableToggled);
    connect(m_setBtn,        &QPushButton::clicked,  this, &XRaySettingsDialog::onSetKvMa);
    connect(m_clearFaultsBtn,&QPushButton::clicked,  this, &XRaySettingsDialog::onClearFaults);
    connect(buttons,         &QDialogButtonBox::accepted, this, &XRaySettingsDialog::onAccepted);
    connect(buttons,         &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Sync slider ↔ spinbox
    connect(m_kvSlider, &QSlider::valueChanged,
            [this](int v){ m_kvSpin->setValue((double)v); });
    connect(m_kvSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [this](double v){ m_kvSlider->setValue((int)v); });
    connect(m_maSlider, &QSlider::valueChanged,
            [this](int v){ m_maSpin->setValue(v / 100.0); });
    connect(m_maSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [this](double v){ m_maSlider->setValue((int)(v * 100)); });

    // Poll status at 2 Hz when open
    connect(&m_pollTimer, &QTimer::timeout, this, &XRaySettingsDialog::onPollTimer);
    if (m_xray) m_pollTimer.start(500);
}

XRaySettingsDialog::~XRaySettingsDialog() {
    m_pollTimer.stop();
}

void XRaySettingsDialog::onConnect() {
    if (!m_xray) return;
    const std::string port = m_portEdit->text().toStdString();
    if (m_xray->open(port))
        m_connectBtn->setEnabled(false);
    else
        QMessageBox::warning(this, tr("连接失败"), tr("无法连接串口: %1").arg(m_portEdit->text()));
}

void XRaySettingsDialog::onDisconnect() {
    if (m_xray) { m_xray->shutdown(); m_xray->close(); }
    m_connectBtn->setEnabled(true);
    m_enableBtn->setChecked(false);
}

void XRaySettingsDialog::onEnableToggled(bool on) {
    if (!m_xray) return;
    if (on) m_xray->startup(); else m_xray->shutdown();
    m_beamOn = on;
}

void XRaySettingsDialog::onSetKvMa() {
    if (!m_xray) return;
    m_xray->setKv(m_kvSpin->value());
    m_xray->setMa(m_maSpin->value());
    m_hw.xrayKv = m_kvSpin->value();
    m_hw.xrayMa = m_maSpin->value();
}

void XRaySettingsDialog::onClearFaults() {
    if (auto* s = dynamic_cast<cgs::hardware::XRaySerial*>(m_xray))
        s->clearFaults();
}

void XRaySettingsDialog::onPollTimer() {
    if (!m_xray) return;
    const auto st = m_xray->pollStatus();
    updateFaultDisplay(st);
    m_enableBtn->setChecked(st.highVoltageOn);
}

void XRaySettingsDialog::updateFaultDisplay(const cgs::hardware::XRayStatus& s) {
    // Map XRaySerial fault bits to checkboxes
    if (auto* serial = dynamic_cast<cgs::hardware::XRaySerial*>(m_xray)) {
        const auto f = serial->lastFaults();
        m_faultCathodeOV ->setChecked(f.overVoltageCathode);
        m_faultAnodeOV   ->setChecked(f.overVoltageAnode);
        m_faultArc       ->setChecked(f.arcDetect);
        m_faultTemp      ->setChecked(f.overTemperature);
        m_faultCurrentOV ->setChecked(f.overCurrent);
        m_faultVoltageOV ->setChecked(f.overVoltage);
        m_faultPower     ->setChecked(f.powerLimit);
        m_faultInterlock ->setChecked(f.interlockOpen);
        m_faultGeneral   ->setChecked(f.regulation);
    }
    (void)s;
}

void XRaySettingsDialog::onAccepted() {
    m_hw.xrayPort = m_portEdit->text().toStdString();
    m_hw.xrayKv   = m_kvSpin->value();
    m_hw.xrayMa   = m_maSpin->value();
    accept();
}

}}
