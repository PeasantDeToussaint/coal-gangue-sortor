#include "ConfigPanel.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace cgs {
namespace gui {

ConfigPanel::ConfigPanel(cgs::core::PipelineEngine* engine, QWidget* parent)
    : QGroupBox(tr("分类与时序参数"), parent), m_engine(engine) {
    buildUi();
    loadFromEngine();
}

void ConfigPanel::buildUi() {
    auto* outer = new QVBoxLayout(this);

    auto* classBox = new QGroupBox(tr("分类阈值"), this);
    auto* classLayout = new QFormLayout(classBox);
    m_xrayEmptyMin   = new QSpinBox(this); m_xrayEmptyMin->setRange(0, 65535);
    m_xrayCoalMin    = new QSpinBox(this); m_xrayCoalMin->setRange(0, 65535);
    m_xrayGangueMax  = new QSpinBox(this); m_xrayGangueMax->setRange(0, 65535);
    m_minObjectWidth = new QSpinBox(this); m_minObjectWidth->setRange(1, 1000);
    classLayout->addRow(tr("X射线 空带最小亮度:"), m_xrayEmptyMin);
    classLayout->addRow(tr("X射线 煤最小亮度:"),  m_xrayCoalMin);
    classLayout->addRow(tr("X射线 矸石最大亮度:"), m_xrayGangueMax);
    classLayout->addRow(tr("最小物体像素数:"),    m_minObjectWidth);

    auto* timingBox = new QGroupBox(tr("喷嘴时序"), this);
    auto* timingLayout = new QFormLayout(timingBox);
    m_sensorToNozzle    = new QDoubleSpinBox(this); m_sensorToNozzle->setRange(0, 5000);    m_sensorToNozzle->setSuffix(" mm");
    m_valveOpenLatency  = new QDoubleSpinBox(this); m_valveOpenLatency->setRange(0, 100);   m_valveOpenLatency->setSuffix(" ms");
    m_valveCloseLatency = new QDoubleSpinBox(this); m_valveCloseLatency->setRange(0, 100);  m_valveCloseLatency->setSuffix(" ms");
    m_pneumaticTravel   = new QDoubleSpinBox(this); m_pneumaticTravel->setRange(0, 50);     m_pneumaticTravel->setSuffix(" ms");
    m_safetyMargin      = new QDoubleSpinBox(this); m_safetyMargin->setRange(0, 50);        m_safetyMargin->setSuffix(" ms");
    m_pixelsPerNozzle   = new QSpinBox(this);       m_pixelsPerNozzle->setRange(1, 200);
    timingLayout->addRow(tr("探测中心→喷嘴中心:"),     m_sensorToNozzle);
    timingLayout->addRow(tr("阀开启延时:"),           m_valveOpenLatency);
    timingLayout->addRow(tr("阀关闭延时:"),           m_valveCloseLatency);
    timingLayout->addRow(tr("气流飞行时间:"),         m_pneumaticTravel);
    timingLayout->addRow(tr("安全余量:"),             m_safetyMargin);
    timingLayout->addRow(tr("每喷嘴覆盖像素:"),       m_pixelsPerNozzle);

    outer->addWidget(classBox);
    outer->addWidget(timingBox);

    auto* btnRow = new QHBoxLayout();
    auto* applyBtn = new QPushButton(tr("应用到运行时"), this);
    btnRow->addStretch();
    btnRow->addWidget(applyBtn);
    outer->addLayout(btnRow);
    outer->addStretch();

    connect(applyBtn, &QPushButton::clicked, this, &ConfigPanel::applyToEngine);
}

void ConfigPanel::loadFromEngine() {
    if (!m_engine) return;
    const auto cfg = m_engine->config();
    m_xrayEmptyMin->setValue(cfg.classifier.xrayEmptyMin);
    m_xrayCoalMin->setValue(cfg.classifier.xrayCoalMin);
    m_xrayGangueMax->setValue(cfg.classifier.xrayGangueMax);
    m_minObjectWidth->setValue(cfg.classifier.minObjectWidthPx);
    m_sensorToNozzle->setValue(cfg.timing.sensorToNozzleMm);
    m_valveOpenLatency->setValue(cfg.timing.valveOpenLatencyMs);
    m_valveCloseLatency->setValue(cfg.timing.valveCloseLatencyMs);
    m_pneumaticTravel->setValue(cfg.timing.pneumaticTravelMs);
    m_safetyMargin->setValue(cfg.timing.safetyMarginMs);
    m_pixelsPerNozzle->setValue((int)cfg.nozzles.pixelsPerNozzle);
}

void ConfigPanel::applyToEngine() {
    if (!m_engine) return;
    auto cfg = m_engine->config();
    cfg.classifier.xrayEmptyMin   = (uint16_t)m_xrayEmptyMin->value();
    cfg.classifier.xrayCoalMin    = (uint16_t)m_xrayCoalMin->value();
    cfg.classifier.xrayGangueMax  = (uint16_t)m_xrayGangueMax->value();
    cfg.classifier.minObjectWidthPx = m_minObjectWidth->value();
    cfg.timing.sensorToNozzleMm    = m_sensorToNozzle->value();
    cfg.timing.valveOpenLatencyMs  = m_valveOpenLatency->value();
    cfg.timing.valveCloseLatencyMs = m_valveCloseLatency->value();
    cfg.timing.pneumaticTravelMs   = m_pneumaticTravel->value();
    cfg.timing.safetyMarginMs      = m_safetyMargin->value();
    cfg.nozzles.pixelsPerNozzle    = m_pixelsPerNozzle->value();
    m_engine->setConfig(cfg);
}

}}
