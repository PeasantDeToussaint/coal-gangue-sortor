#include "PreheatDialog.h"

#include "../../hardware/XRayInterface/IXRaySource.h"
#include "../../hardware/XRayInterface/XRaySerial.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace cgs {
namespace gui {

// ---------------------------------------------------------------------------
// computePreheatSeconds — static utility
// lastCloseTimeStr format: "YYYYMMDD_HHMMSS"
// ---------------------------------------------------------------------------
int PreheatDialog::computePreheatSeconds(const std::string& lastCloseTimeStr) {
    if (lastCloseTimeStr.empty()) return 600;  // unknown idle → 10 min (safest)

    // Parse "YYYYMMDD_HHMMSS"
    struct tm then{};
    if (lastCloseTimeStr.size() < 15) return 600;
    try {
        then.tm_year = std::stoi(lastCloseTimeStr.substr(0, 4)) - 1900;
        then.tm_mon  = std::stoi(lastCloseTimeStr.substr(4, 2)) - 1;
        then.tm_mday = std::stoi(lastCloseTimeStr.substr(6, 2));
        then.tm_hour = std::stoi(lastCloseTimeStr.substr(9, 2));
        then.tm_min  = std::stoi(lastCloseTimeStr.substr(11, 2));
        then.tm_sec  = std::stoi(lastCloseTimeStr.substr(13, 2));
    } catch (...) {
        return 600;
    }
    then.tm_isdst = -1;

    const std::time_t thenT = std::mktime(&then);
    const std::time_t nowT  = std::time(nullptr);
    const double hours = std::difftime(nowT, thenT) / 3600.0;

    if (hours < 12.0)       return 0;
    if (hours < 24.0)       return 30;
    if (hours < 7.0 * 24)   return 120;
    if (hours < 30.0 * 24)  return 300;
    return 600;
}

// ---------------------------------------------------------------------------
// PreheatDialog
// ---------------------------------------------------------------------------
PreheatDialog::PreheatDialog(cgs::hardware::IXRaySource* xray,
                              const std::string& lastCloseTimeStr,
                              QWidget* parent)
    : QDialog(parent, Qt::Dialog | Qt::WindowTitleHint)
    , m_xray(xray)
    , m_lastCloseTime(lastCloseTimeStr) {
    setWindowTitle(tr("X射线源预热"));
    setModal(true);
    setMinimumWidth(420);

    m_preheatSeconds = computePreheatSeconds(lastCloseTimeStr);

    auto* layout = new QVBoxLayout(this);

    // Info label
    QString idleStr;
    if (lastCloseTimeStr.empty()) {
        idleStr = tr("（上次关机时间未知）");
    } else {
        idleStr = tr("上次关机：%1").arg(QString::fromStdString(lastCloseTimeStr));
    }

    QString preheatStr;
    if (m_preheatSeconds == 0) {
        preheatStr = tr("停机不足12小时，无需预热。");
    } else {
        preheatStr = tr("需要预热 %1 秒（约 %2 分钟）。")
                     .arg(m_preheatSeconds)
                     .arg(m_preheatSeconds / 60.0, 0, 'f', 1);
    }

    m_infoLabel = new QLabel(
        tr("<b>X射线源预热</b><br/>%1<br/>%2<br/><br/>"
           "为保护X射线管寿命，请严格按停机天数选择预热时间。")
        .arg(idleStr).arg(preheatStr), this);
    m_infoLabel->setWordWrap(true);
    layout->addWidget(m_infoLabel);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, std::max(1, m_preheatSeconds));
    m_progress->setValue(0);
    m_progress->setStyleSheet("QProgressBar::chunk { background-color: #27ae60; }");
    layout->addWidget(m_progress);

    m_countdownLabel = new QLabel(tr("就绪"), this);
    m_countdownLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_countdownLabel);

    auto* btnRow = new QHBoxLayout();
    m_startBtn = new QPushButton(
        m_preheatSeconds > 0 ? tr("开始预热") : tr("开机（无需预热）"), this);
    m_skipBtn  = new QPushButton(tr("跳过（不推荐）"), this);
    m_skipBtn->setStyleSheet("color: #c0392b;");
    btnRow->addWidget(m_startBtn);
    btnRow->addWidget(m_skipBtn);
    layout->addLayout(btnRow);

    connect(m_startBtn, &QPushButton::clicked, this, &PreheatDialog::onStartClicked);
    connect(m_skipBtn,  &QPushButton::clicked, this, &PreheatDialog::onSkipClicked);
    connect(&m_tickTimer, &QTimer::timeout,    this, &PreheatDialog::onTick);
}

void PreheatDialog::onStartClicked() {
    if (m_preheatSeconds == 0) {
        // No preheat needed — still send 0 s preheat to source, then enable.
        if (auto* serial = dynamic_cast<cgs::hardware::XRaySerial*>(m_xray)) {
            serial->setPreheatSeconds(0);
        }
        if (m_xray) m_xray->startup();
        m_completed = true;
        accept();
        return;
    }
    startPreheat(m_preheatSeconds);
}

void PreheatDialog::onSkipClicked() {
    // Allow arm() even without preheat (operator's responsibility).
    m_completed = true;
    accept();
}

void PreheatDialog::startPreheat(int seconds) {
    m_startBtn->setEnabled(false);
    m_skipBtn->setEnabled(false);
    m_elapsedSeconds = 0;
    m_progress->setRange(0, seconds);
    m_progress->setValue(0);

    if (auto* serial = dynamic_cast<cgs::hardware::XRaySerial*>(m_xray)) {
        serial->setPreheatSeconds(seconds);
    }
    if (m_xray) m_xray->startup();  // ENBL1 after preheat duration is configured

    m_tickTimer.start(1000);  // 1-second ticks
    m_countdownLabel->setText(tr("预热中… 剩余 %1 秒").arg(seconds));
}

void PreheatDialog::onTick() {
    ++m_elapsedSeconds;
    m_progress->setValue(m_elapsedSeconds);
    const int remaining = m_preheatSeconds - m_elapsedSeconds;
    if (remaining > 0) {
        m_countdownLabel->setText(tr("预热中… 剩余 %1 秒").arg(remaining));
    } else {
        m_tickTimer.stop();
        m_countdownLabel->setText(tr("预热完成！"));
        m_completed = true;
        QMessageBox::information(this, tr("预热完成"), tr("X射线源已就绪，可以开始分选。"));
        accept();
    }
}

}}
