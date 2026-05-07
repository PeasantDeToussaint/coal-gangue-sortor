#include "MainWindow.h"

#include "ConfigPanel.h"
#include "LogCenter.h"
#include "NozzleMatrixView.h"
#include "StatsPanel.h"
#include "XRayWaterfallView.h"
#include "XRaySettingsDialog.h"
#include "DetectorSettingsDialog.h"
#include "PlcSettingsDialog.h"

#include <QAction>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace cgs {
namespace gui {

MainWindow::MainWindow(cgs::core::PipelineEngine* engine,
                       const std::string& configPath,
                       QWidget* parent)
    : QMainWindow(parent)
    , m_engine(engine)
    , m_configPath(configPath) {
    setWindowTitle(tr("煤矸石分选机 — 上位机软件"));
    resize(1280, 800);
    buildUi();
    buildMenuBar();
    wireSignals();

    connect(&m_statsTimer, &QTimer::timeout, this, &MainWindow::refreshStats);
    m_statsTimer.start(200);
}

MainWindow::~MainWindow() = default;

// ---------------------------------------------------------------------------
// buildUi
// ---------------------------------------------------------------------------
void MainWindow::buildUi() {
    auto* central   = new QWidget(this);
    auto* topRow    = new QHBoxLayout();
    auto* bottomRow = new QHBoxLayout();

    m_waterfall  = new XRayWaterfallView(this);
    m_matrix     = new NozzleMatrixView(this);
    topRow->addWidget(m_waterfall, 2);
    topRow->addWidget(m_matrix,   1);

    m_configPanel = new ConfigPanel(m_engine, this);
    m_statsPanel  = new StatsPanel(this);
    bottomRow->addWidget(m_configPanel, 2);
    bottomRow->addWidget(m_statsPanel,  1);

    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->addLayout(topRow,    3);
    mainLayout->addLayout(bottomRow, 2);
    setCentralWidget(central);

    // Log center dock (Ctrl+M)
    m_logCenter = new LogCenter(this);
    addDockWidget(Qt::BottomDockWidgetArea, m_logCenter);
    m_logCenter->hide();

    // Control toolbar
    auto* tb = addToolBar(tr("控制"));
    tb->setMovable(false);

    m_armBtn    = new QPushButton(tr("启动 / 武装"), this);
    m_disarmBtn = new QPushButton(tr("解除武装"), this);
    m_estopBtn  = new QPushButton(tr("急停 ⚠"), this);
    m_estopBtn->setStyleSheet(
        "background-color: #c0392b; color: white; font-weight: bold; font-size: 14px;");

    tb->addWidget(m_armBtn);
    tb->addWidget(m_disarmBtn);
    tb->addSeparator();
    tb->addWidget(m_estopBtn);

    m_statusLabel = new QLabel(tr("未武装"), this);
    statusBar()->addWidget(m_statusLabel);
}

// ---------------------------------------------------------------------------
// buildMenuBar — matches config/menu.xml exactly
//   文件 → 退出(x)       Ctrl+X
//   设置 → 光机设置 | 探测器设置 | PLC设置 | 配置文件刷新
//   系统维护 → 探测器校正
//   帮助 → 使用帮助 | 日志中心   Ctrl+M | 关于我们
// ---------------------------------------------------------------------------
void MainWindow::buildMenuBar() {
    // 文件 (File)
    QMenu* fileMenu = menuBar()->addMenu(tr("文件"));
    QAction* actExit = fileMenu->addAction(tr("退出(x)"));
    actExit->setShortcut(QKeySequence("Ctrl+X"));
    connect(actExit, &QAction::triggered, this, &QWidget::close);

    // 设置 (Settings)
    QMenu* setMenu = menuBar()->addMenu(tr("设置"));
    m_actXRaySettings     = setMenu->addAction(tr("光机设置"));
    m_actDetectorSettings = setMenu->addAction(tr("探测器设置"));
    m_actPlcSettings      = setMenu->addAction(tr("PLC设置"));
    setMenu->addSeparator();
    m_actRefreshConfig    = setMenu->addAction(tr("配置文件刷新"));

    // 系统维护 (System Maintenance)
    QMenu* maintMenu = menuBar()->addMenu(tr("系统维护"));
    m_actDetectorCalib = maintMenu->addAction(tr("探测器校正"));

    // 帮助 (Help)
    QMenu* helpMenu = menuBar()->addMenu(tr("帮助"));
    QAction* actGuide = helpMenu->addAction(tr("使用帮助"));
    m_actLogCenter = helpMenu->addAction(tr("日志中心"));
    m_actLogCenter->setShortcut(QKeySequence("Ctrl+M"));
    helpMenu->addSeparator();
    QAction* actAbout = helpMenu->addAction(tr("关于我们"));

    connect(actGuide,  &QAction::triggered, [](){ /* open help PDF */ });
    connect(actAbout,  &QAction::triggered, this, &MainWindow::onAbout);
}

// ---------------------------------------------------------------------------
// wireSignals
// ---------------------------------------------------------------------------
void MainWindow::wireSignals() {
    connect(m_armBtn,    &QPushButton::clicked, this, &MainWindow::onArmClicked);
    connect(m_disarmBtn, &QPushButton::clicked, this, &MainWindow::onDisarmClicked);
    connect(m_estopBtn,  &QPushButton::clicked, this, &MainWindow::onEmergencyStopClicked);

    connect(m_actXRaySettings,     &QAction::triggered, this, &MainWindow::onXRaySettings);
    connect(m_actDetectorSettings, &QAction::triggered, this, &MainWindow::onDetectorSettings);
    connect(m_actPlcSettings,      &QAction::triggered, this, &MainWindow::onPlcSettings);
    connect(m_actRefreshConfig,    &QAction::triggered, this, &MainWindow::onRefreshConfig);
    connect(m_actDetectorCalib,    &QAction::triggered, this, &MainWindow::onDetectorCalibration);
    connect(m_actLogCenter,        &QAction::triggered, this, &MainWindow::onShowLogCenter);

    if (m_engine) {
        m_engine->setSnapshotCallback([this](const cgs::core::PipelineFrameSnapshot& snap){
            // These calls are from the detector thread → must be thread-safe Qt signals.
            QMetaObject::invokeMethod(m_waterfall, [this, snap](){
                m_waterfall->pushSnapshot(snap);
            }, Qt::QueuedConnection);
            QMetaObject::invokeMethod(m_matrix, [this, snap](){
                m_matrix->pushSnapshot(snap);
            }, Qt::QueuedConnection);
        });
    }
}

// ---------------------------------------------------------------------------
// Arm / Disarm / E-Stop
// ---------------------------------------------------------------------------
void MainWindow::onArmClicked() {
    if (!m_engine) return;
    m_engine->arm();
    m_statusLabel->setText(tr("已武装 — 实时分选中"));
    m_statusLabel->setStyleSheet("color: #27ae60; font-weight: bold;");
}

void MainWindow::onDisarmClicked() {
    if (!m_engine) return;
    m_engine->disarm();
    m_statusLabel->setText(tr("未武装"));
    m_statusLabel->setStyleSheet("");
}

void MainWindow::onEmergencyStopClicked() {
    onDisarmClicked();
    m_statusLabel->setText(tr("急停！所有喷吹已停止。"));
    m_statusLabel->setStyleSheet("color: #c0392b; font-weight: bold;");
}

// ---------------------------------------------------------------------------
// Menu actions
// ---------------------------------------------------------------------------
void MainWindow::onXRaySettings() {
    XRaySettingsDialog dlg(m_hw, m_configPath, m_xray, this);
    dlg.exec();
}

void MainWindow::onDetectorSettings() {
    DetectorSettingsDialog dlg(m_hw, m_configPath, m_detector, this);
    dlg.exec();
}

void MainWindow::onPlcSettings() {
    PlcSettingsDialog dlg(m_hw, m_configPath, this);
    dlg.exec();
}

void MainWindow::onRefreshConfig() {
    if (!m_engine) return;
    cgs::core::PipelineConfig pipe;
    cgs::core::HardwareConfig hw;
    if (cgs::core::loadConfigFromFile(m_configPath, pipe, hw)) {
        m_hw = hw;
        m_engine->setConfig(pipe);
        statusBar()->showMessage(tr("配置已刷新"), 3000);
    } else {
        QMessageBox::warning(this, tr("错误"), tr("配置文件加载失败：%1")
                             .arg(QString::fromStdString(m_configPath)));
    }
}

void MainWindow::onDetectorCalibration() {
    QMessageBox::information(this, tr("探测器校正"),
        tr("请确保皮带停止且无物料，然后确认开始平场校正。\n"
           "校正完成后将保存到 config/calibrateImageXray.tif。"));
    // TODO: Trigger DetectorAurora flat-field calibration sequence.
}

void MainWindow::onShowLogCenter() {
    m_logCenter->setVisible(!m_logCenter->isVisible());
}

void MainWindow::onAbout() {
    QMessageBox::about(this, tr("关于"),
        tr("<b>煤矸石分选机 上位机软件</b><br/>"
           "基于 Qt5 / C++17 / TensorRT<br/>"
           "Rebuilt from 袁工's Gangue.exe system<br/><br/>"
           "Protocol: Liong LN/VJ-compatible (RS-232)<br/>"
           "Detector: Detection Technology Aurora SDK<br/>"
           "Valve control: Beckhoff EtherCAT ADS (136 ch)<br/>"
           "Belt PLC: Siemens S7-1500"));
}

void MainWindow::refreshStats() {
    if (!m_engine || !m_statsPanel) return;
    m_statsPanel->update(m_engine->stats());
}

// ---------------------------------------------------------------------------
// closeEvent — write LastCloseTime to config.xml for preheat calc on next start.
// ---------------------------------------------------------------------------
void MainWindow::closeEvent(QCloseEvent* event) {
    if (m_engine) {
        m_engine->disarm();
    }
    writeLastCloseTime();
    QMainWindow::closeEvent(event);
}

void MainWindow::writeLastCloseTime() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const std::time_t t = system_clock::to_time_t(now);
    struct tm tm_info{};
#ifdef _WIN32
    localtime_s(&tm_info, &t);
#else
    localtime_r(&t, &tm_info);
#endif
    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm_info);
    m_hw.lastCloseTime = buf;
    // TODO: Write lastCloseTime attribute back to config.xml via tinyxml2.
    // (Requires implementing a config writer in ConfigLoader.)
}

}}
