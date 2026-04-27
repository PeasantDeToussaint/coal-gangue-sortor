#ifndef CGS_NOZZLEMATRIXVIEW_H
#define CGS_NOZZLEMATRIXVIEW_H

#include <QElapsedTimer>
#include <QMutex>
#include <QWidget>

#include <array>

#include "../../core/Pipeline/PipelineEngine.h"

namespace cgs {
namespace gui {

// 8x8 grid of nozzles, each cell flashes when fired.
// Cell color decays linearly back to idle within `m_decayMs` after the last fire.
class NozzleMatrixView : public QWidget {
    Q_OBJECT
public:
    explicit NozzleMatrixView(QWidget* parent = nullptr);

    void pushSnapshot(const cgs::core::PipelineFrameSnapshot& snap);

protected:
    void paintEvent(QPaintEvent* ev) override;

private:
    QElapsedTimer m_clock;
    qint64 m_lastFireMs[64] = {0};
    QMutex m_mtx;
    int m_decayMs = 600;
};

}}

#endif
