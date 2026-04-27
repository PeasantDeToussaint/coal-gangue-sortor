#include "NozzleMatrixView.h"

#include <QMutexLocker>
#include <QPainter>
#include <QTimer>

namespace cgs {
namespace gui {

NozzleMatrixView::NozzleMatrixView(QWidget* parent) : QWidget(parent) {
    setMinimumSize(360, 360);
    m_clock.start();
    auto* t = new QTimer(this);
    connect(t, &QTimer::timeout, this, [this]{ update(); });
    t->start(50);
}

void NozzleMatrixView::pushSnapshot(const cgs::core::PipelineFrameSnapshot& snap) {
    QMutexLocker lk(&m_mtx);
    const qint64 now = m_clock.elapsed();
    for (int n : snap.firedNozzleIds) {
        if (n >= 1 && n <= 64) m_lastFireMs[n - 1] = now;
    }
}

void NozzleMatrixView::paintEvent(QPaintEvent* /*ev*/) {
    QMutexLocker lk(&m_mtx);
    const qint64 now = m_clock.elapsed();
    QPainter p(this);
    p.fillRect(rect(), QColor(20, 20, 30));

    const int rows = 8, cols = 8;
    const int margin = 12;
    const int cw = (width()  - margin * 2) / cols;
    const int ch = (height() - margin * 2) / rows;

    p.setFont(QFont("Menlo", std::max(8, std::min(cw, ch) / 5)));
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const int idx = r * cols + c;
            const int nozzleId = idx + 1;
            const qint64 since = now - m_lastFireMs[idx];
            int intensity = 0;
            if (m_lastFireMs[idx] > 0 && since < m_decayMs) {
                intensity = (int)(255 * (1.0 - (double)since / m_decayMs));
            }

            const QRect cell(margin + c * cw + 2,
                             margin + r * ch + 2,
                             cw - 4, ch - 4);

            QColor base(60, 60, 80);
            QColor active(255, 80, 60);
            QColor mix(
                base.red()   + (active.red()   - base.red())   * intensity / 255,
                base.green() + (active.green() - base.green()) * intensity / 255,
                base.blue()  + (active.blue()  - base.blue())  * intensity / 255);

            p.setPen(QColor(80, 80, 100));
            p.setBrush(mix);
            p.drawRoundedRect(cell, 4, 4);

            p.setPen(intensity > 100 ? Qt::white : QColor(180, 180, 200));
            p.drawText(cell, Qt::AlignCenter, QString::number(nozzleId));
        }
    }

    p.setPen(Qt::white);
    p.drawText(rect().adjusted(margin, 0, -margin, -margin / 2),
               Qt::AlignBottom | Qt::AlignLeft,
               tr("64 路气枪喷嘴 — 红=刚触发"));
}

}}
