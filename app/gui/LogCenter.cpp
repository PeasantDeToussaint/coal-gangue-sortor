#include "LogCenter.h"

#include <QDateTime>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextStream>
#include <QVBoxLayout>
#include <QWidget>

namespace cgs {
namespace gui {

LogCenter::LogCenter(QWidget* parent)
    : QDockWidget(tr("日志中心"), parent) {
    setObjectName("LogCenter");

    auto* container = new QWidget(this);
    auto* layout    = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);

    m_view = new QPlainTextEdit(container);
    m_view->setReadOnly(true);
    m_view->setMaximumBlockCount(m_maxLines);
    m_view->setFont(QFont("Courier New", 9));
    m_view->setWordWrapMode(QTextOption::NoWrap);
    layout->addWidget(m_view);

    auto* btnRow = new QHBoxLayout();
    m_clearBtn = new QPushButton(tr("清空"), container);
    m_saveBtn  = new QPushButton(tr("保存日志…"), container);
    btnRow->addStretch();
    btnRow->addWidget(m_clearBtn);
    btnRow->addWidget(m_saveBtn);
    layout->addLayout(btnRow);

    setWidget(container);
    setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);

    // Cross-thread signal/slot: appendMessage() → messageReady → onMessageReady
    connect(this,       &LogCenter::messageReady,
            this,       &LogCenter::onMessageReady,
            Qt::QueuedConnection);
    connect(m_clearBtn, &QPushButton::clicked, this, &LogCenter::onClearClicked);
    connect(m_saveBtn,  &QPushButton::clicked, this, &LogCenter::onSaveClicked);
}

void LogCenter::appendMessage(const QString& msg) {
    emit messageReady(msg);
}

void LogCenter::onMessageReady(const QString& msg) {
    const QString timestamped = QDateTime::currentDateTime()
                                .toString("(yyyy-MM-dd hh:mm:ss) ") + msg;
    m_view->appendPlainText(timestamped);
    // Auto-scroll to bottom
    QScrollBar* sb = m_view->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void LogCenter::onClearClicked() {
    m_view->clear();
}

void LogCenter::onSaveClicked() {
    const QString path = QFileDialog::getSaveFileName(
        this, tr("保存日志"), QString(), tr("日志文件 (*.log *.txt)"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << m_view->toPlainText();
    }
}

}}
