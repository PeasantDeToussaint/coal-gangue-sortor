#include "StatsPanel.h"

#include <QFormLayout>
#include <QLabel>

namespace cgs {
namespace gui {

StatsPanel::StatsPanel(QWidget* parent)
    : QGroupBox(tr("运行统计"), parent) {
    auto* layout = new QFormLayout(this);
    m_frames    = new QLabel("0", this);
    m_segments  = new QLabel("0", this);
    m_rejected  = new QLabel("0", this);
    m_scheduled = new QLabel("0", this);
    m_failed    = new QLabel("0", this);
    m_belt      = new QLabel("0.00 m/s", this);
    layout->addRow(tr("已处理帧数:"),       m_frames);
    layout->addRow(tr("识别物体段:"),       m_segments);
    layout->addRow(tr("矸石总数:"),         m_rejected);
    layout->addRow(tr("已下发喷射命令:"),   m_scheduled);
    layout->addRow(tr("命令失败:"),         m_failed);
    layout->addRow(tr("当前皮带速度:"),     m_belt);
}

void StatsPanel::update(const cgs::core::PipelineStats& s) {
    m_frames->setText(QString::number(s.framesProcessed));
    m_segments->setText(QString::number(s.segmentsDetected));
    m_rejected->setText(QString::number(s.gangueRejected));
    m_scheduled->setText(QString::number(s.commandsScheduled));
    m_failed->setText(QString::number(s.commandsRejected));
    m_belt->setText(QString::asprintf("%.2f m/s", s.currentBeltSpeedMps));
}

}}
