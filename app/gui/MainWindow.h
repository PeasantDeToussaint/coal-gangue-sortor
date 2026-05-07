#ifndef CGS_MAINWINDOW_H
#define CGS_MAINWINDOW_H

#include "../../core/Pipeline/PipelineEngine.h"
#include "../../core/Config/ConfigLoader.h"
#include "../../hardware/XRayInterface/IXRaySource.h"
#include "../../hardware/DetectorInterface/IDetector.h"

#include <QMainWindow>
#include <QTimer>

class QLabel;
class QPushButton;
class QAction;

namespace cgs {
namespace gui {

class XRayWaterfallView;
class NozzleMatrixView;
class ConfigPanel;
class StatsPanel;
class LogCenter;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(cgs::core::PipelineEngine* engine,
               const std::string& configPath,
               QWidget* parent = nullptr);
    ~MainWindow() override;

    void setHardwareConfig(const cgs::core::HardwareConfig& hw) { m_hw = hw; }
    void setHardwarePointers(cgs::hardware::IXRaySource* xray,
                             cgs::hardware::IDetector*   detector) {
        m_xray = xray; m_detector = detector;
    }

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onArmClicked();
    void onDisarmClicked();
    void onEmergencyStopClicked();
    void refreshStats();

    // Menu actions
    void onXRaySettings();
    void onDetectorSettings();
    void onPlcSettings();
    void onRefreshConfig();
    void onDetectorCalibration();
    void onShowLogCenter();
    void onAbout();

private:
    void buildUi();
    void buildMenuBar();
    void wireSignals();
    void writeLastCloseTime();

    cgs::core::PipelineEngine*  m_engine{nullptr};
    std::string                  m_configPath;
    cgs::core::HardwareConfig    m_hw;
    cgs::hardware::IXRaySource* m_xray{nullptr};
    cgs::hardware::IDetector*   m_detector{nullptr};

    // Central widgets
    XRayWaterfallView* m_waterfall{nullptr};
    NozzleMatrixView*  m_matrix{nullptr};
    ConfigPanel*       m_configPanel{nullptr};
    StatsPanel*        m_statsPanel{nullptr};
    LogCenter*         m_logCenter{nullptr};

    // Toolbar buttons
    QPushButton* m_armBtn{nullptr};
    QPushButton* m_disarmBtn{nullptr};
    QPushButton* m_estopBtn{nullptr};
    QLabel*      m_statusLabel{nullptr};

    // Menu actions
    QAction* m_actXRaySettings{nullptr};
    QAction* m_actDetectorSettings{nullptr};
    QAction* m_actPlcSettings{nullptr};
    QAction* m_actRefreshConfig{nullptr};
    QAction* m_actDetectorCalib{nullptr};
    QAction* m_actLogCenter{nullptr};

    QTimer m_statsTimer;
};

}}

#endif
