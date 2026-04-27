#include "MainWindow.h"

#include "ConfigPanel.h"
#include "NozzleMatrixView.h"
#include "StatsPanel.h"
#include "XRayWaterfallView.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

namespace cgs {
namespace gui {

MainWindow::MainWindow(cgs::core::PipelineEngine* engine, QWidget* parent)
    : QMainWindow(parent), m_engine(engine) {
    setWindowTitle(tr("煤矸石分选机 - 上位机"));
    resize(1280, 800);
    buildUi();
    wireSignals();

    connect(&m_statsTimer, &QTimer::timeout, this, &MainWindow::refreshStats);
    m_statsTimer.start(200);
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi() {
    auto* central = new QWidget(this);
    auto* topRow = new QHBoxLayout();

    m_waterfall = new XRayWaterfallView(this);
    m_matrix    = new NozzleMatrixView(this);
    topRow->addWidget(m_waterfall, 2);
    topRow->addWidget(m_matrix, 1);

    auto* bottomRow = new QHBoxLayout();
    m_configPanel = new ConfigPanel(m_engine, this);
    m_statsPanel  = new StatsPanel(this);
    bottomRow->addWidget(m_configPanel, 2);
    bottomRow->addWidget(m_statsPanel, 1);

    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->addLayout(topRow, 3);
    mainLayout->addLayout(bottomRow, 2);
    setCentralWidget(central);

    auto* tb = addToolBar(tr("控制"));
    m_armBtn    = new QPushButton(tr("启动 / 武装"), this);
    m_disarmBtn = new QPushButton(tr("解除武装"), this);
    m_estopBtn  = new QPushButton(tr("急停"), this);
    m_estopBtn->setStyleSheet("background-color: #c0392b; color: white; font-weight: bold;");
    tb->addWidget(m_armBtn);
    tb->addWidget(m_disarmBtn);
    tb->addSeparator();
    tb->addWidget(m_estopBtn);

    m_statusLabel = new QLabel(tr("未武装"), this);
    statusBar()->addWidget(m_statusLabel);
}

void MainWindow::wireSignals() {
    connect(m_armBtn,    &QPushButton::clicked, this, &MainWindow::onArmClicked);
    connect(m_disarmBtn, &QPushButton::clicked, this, &MainWindow::onDisarmClicked);
    connect(m_estopBtn,  &QPushButton::clicked, this, &MainWindow::onEmergencyStopClicked);

    if (m_engine) {
        m_engine->setSnapshotCallback([this](const cgs::core::PipelineFrameSnapshot& snap){
            m_waterfall->pushSnapshot(snap);
            m_matrix->pushSnapshot(snap);
        });
    }
}

void MainWindow::onArmClicked() {
    if (!m_engine) return;
    m_engine->arm();
    m_statusLabel->setText(tr("已武装 - 实时分选中"));
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
    m_statusLabel->setText(tr("急停！"));
    m_statusLabel->setStyleSheet("color: #c0392b; font-weight: bold;");
}

void MainWindow::refreshStats() {
    if (!m_engine || !m_statsPanel) return;
    m_statsPanel->update(m_engine->stats());
}

}}
