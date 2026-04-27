#ifndef CGS_STATSPANEL_H
#define CGS_STATSPANEL_H

#include <QGroupBox>

#include "../../core/Pipeline/PipelineEngine.h"

QT_BEGIN_NAMESPACE
class QLabel;
QT_END_NAMESPACE

namespace cgs {
namespace gui {

class StatsPanel : public QGroupBox {
    Q_OBJECT
public:
    explicit StatsPanel(QWidget* parent = nullptr);
    void update(const cgs::core::PipelineStats& s);

private:
    QLabel* m_frames     = nullptr;
    QLabel* m_segments   = nullptr;
    QLabel* m_rejected   = nullptr;
    QLabel* m_scheduled  = nullptr;
    QLabel* m_failed     = nullptr;
    QLabel* m_belt       = nullptr;
};

}}

#endif
