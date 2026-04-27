#include "XRayWaterfallView.h"

#include <QMutexLocker>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>

#include <algorithm>

namespace cgs {
namespace gui {

XRayWaterfallView::XRayWaterfallView(QWidget* parent) : QWidget(parent) {
    setMinimumSize(640, 240);
    m_canvas = QImage(800, 240, QImage::Format_ARGB32);
    m_canvas.fill(Qt::black);
}

void XRayWaterfallView::pushSnapshot(const cgs::core::PipelineFrameSnapshot& snap) {
    drawSnapshot(snap);
    QMetaObject::invokeMethod(this, [this]{ update(); }, Qt::QueuedConnection);
}

void XRayWaterfallView::resizeEvent(QResizeEvent* ev) {
    QWidget::resizeEvent(ev);
    QMutexLocker lk(&m_mtx);
    QImage scaled = m_canvas.scaled(ev->size(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
    m_canvas = QImage(ev->size(), QImage::Format_ARGB32);
    m_canvas.fill(Qt::black);
    QPainter p(&m_canvas);
    p.drawImage(0, 0, scaled);
}

void XRayWaterfallView::drawSnapshot(const cgs::core::PipelineFrameSnapshot& snap) {
    QMutexLocker lk(&m_mtx);
    if (m_canvas.isNull()) return;
    const int w = m_canvas.width();
    const int h = m_canvas.height();
    if (w <= 0 || h <= 0) return;

    // Scroll up by 1 pixel.
    QImage shifted = m_canvas.copy(0, 1, w, h - 1);
    m_canvas.fill(Qt::black);
    QPainter p(&m_canvas);
    p.drawImage(0, 0, shifted);

    // Draw new bottom row from snapshot.rawRow, mapping width onto image width.
    if (!snap.rawRow.empty()) {
        for (int x = 0; x < w; ++x) {
            const int srcIdx = (int)((double)x / w * snap.rawRow.size());
            const uint16_t v = snap.rawRow[std::min<size_t>(srcIdx, snap.rawRow.size() - 1)];
            const int g = std::min(255, (int)(v / 256));
            m_canvas.setPixelColor(x, h - 1, QColor(g, g, g));
        }
    }

    // Overlay gangue segments in red.
    if (!snap.segments.empty() && snap.width > 0) {
        for (const auto& seg : snap.segments) {
            if (seg.label != cgs::core::Material::Gangue) continue;
            const int x0 = (int)((double)seg.startPx / snap.width * w);
            const int x1 = (int)((double)seg.endPx   / snap.width * w);
            for (int x = x0; x < x1 && x < w; ++x) {
                m_canvas.setPixelColor(x, h - 1, QColor(255, 60, 60));
            }
        }
    }
}

void XRayWaterfallView::paintEvent(QPaintEvent* /*ev*/) {
    QMutexLocker lk(&m_mtx);
    QPainter p(this);
    p.drawImage(rect(), m_canvas);
    p.setPen(Qt::white);
    p.drawText(rect().adjusted(8, 8, -8, -8),
               Qt::AlignTop | Qt::AlignLeft,
               tr("X-ray detection (newest at bottom; red = gangue)"));
}

}}
