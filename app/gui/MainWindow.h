#ifndef CGS_MAINWINDOW_H
#define CGS_MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>

#include <memory>

#include "../../core/Pipeline/PipelineEngine.h"

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
QT_END_NAMESPACE

namespace cgs {
namespace gui {

class XRayWaterfallView;
class NozzleMatrixView;
class ConfigPanel;
class StatsPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(cgs::core::PipelineEngine* engine,
                        QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onArmClicked();
    void onDisarmClicked();
    void onEmergencyStopClicked();
    void refreshStats();

private:
    void buildUi();
    void wireSignals();

    cgs::core::PipelineEngine* m_engine;
    XRayWaterfallView*   m_waterfall = nullptr;
    NozzleMatrixView*    m_matrix    = nullptr;
    ConfigPanel*         m_configPanel = nullptr;
    StatsPanel*          m_statsPanel  = nullptr;
    QPushButton*         m_armBtn      = nullptr;
    QPushButton*         m_disarmBtn   = nullptr;
    QPushButton*         m_estopBtn    = nullptr;
    QLabel*              m_statusLabel = nullptr;
    QTimer               m_statsTimer;
};

}}

#endif
