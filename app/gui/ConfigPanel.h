#ifndef CGS_CONFIGPANEL_H
#define CGS_CONFIGPANEL_H

#include <QGroupBox>

#include "../../core/Pipeline/PipelineEngine.h"

QT_BEGIN_NAMESPACE
class QSpinBox;
class QDoubleSpinBox;
QT_END_NAMESPACE

namespace cgs {
namespace gui {

class ConfigPanel : public QGroupBox {
    Q_OBJECT
public:
    explicit ConfigPanel(cgs::core::PipelineEngine* engine, QWidget* parent = nullptr);

private slots:
    void applyToEngine();

private:
    void buildUi();
    void loadFromEngine();

    cgs::core::PipelineEngine* m_engine;

    QSpinBox*       m_xrayEmptyMin = nullptr;
    QSpinBox*       m_xrayCoalMin  = nullptr;
    QSpinBox*       m_xrayGangueMax = nullptr;
    QSpinBox*       m_minObjectWidth = nullptr;
    QDoubleSpinBox* m_sensorToNozzle = nullptr;
    QDoubleSpinBox* m_valveOpenLatency = nullptr;
    QDoubleSpinBox* m_valveCloseLatency = nullptr;
    QDoubleSpinBox* m_pneumaticTravel = nullptr;
    QDoubleSpinBox* m_safetyMargin = nullptr;
    QSpinBox*       m_pixelsPerNozzle = nullptr;
};

}}

#endif
