#ifndef CGS_XRAYWATERFALLVIEW_H
#define CGS_XRAYWATERFALLVIEW_H

#include <QImage>
#include <QMutex>
#include <QWidget>

#include "../../core/Pipeline/PipelineEngine.h"

namespace cgs {
namespace gui {

// Scrolling waterfall plot of the line-scan detector. Each new row appears
// at the bottom; older rows scroll up. Gangue segments overlay in red.
class XRayWaterfallView : public QWidget {
    Q_OBJECT
public:
    explicit XRayWaterfallView(QWidget* parent = nullptr);

    // Thread-safe ingest from the pipeline callback.
    void pushSnapshot(const cgs::core::PipelineFrameSnapshot& snap);

protected:
    void paintEvent(QPaintEvent* ev) override;
    void resizeEvent(QResizeEvent* ev) override;

private:
    void drawSnapshot(const cgs::core::PipelineFrameSnapshot& snap);

    QImage m_canvas;
    QMutex m_mtx;
};

}}

#endif
