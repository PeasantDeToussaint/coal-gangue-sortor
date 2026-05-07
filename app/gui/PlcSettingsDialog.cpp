#include "PlcSettingsDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace cgs { namespace gui {

PlcSettingsDialog::PlcSettingsDialog(cgs::core::HardwareConfig& hw,
                                      const std::string& configPath,
                                      QWidget* parent)
    : QDialog(parent), m_hw(hw), m_configPath(configPath) {
    setWindowTitle(tr("PLC设置"));
    setMinimumWidth(420);
    auto* layout = new QVBoxLayout(this);

    auto* grp  = new QGroupBox(tr("西门子 S7-1500 PLC（皮带控制）"), this);
    auto* form = new QFormLayout(grp);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem("s7-1500 (Siemens S7-1500, snap7)");
    m_typeCombo->addItem("mock (离线测试)");
    m_typeCombo->setCurrentIndex(hw.plcType == "mock" ? 1 : 0);
    form->addRow(tr("PLC 类型:"), m_typeCombo);

    m_endpointEdit = new QLineEdit(QString::fromStdString(hw.plcEndpoint), this);
    m_endpointEdit->setPlaceholderText("s7://192.168.2.100:102");
    form->addRow(tr("端点 (s7://<ip>:<port>):"), m_endpointEdit);

    layout->addWidget(grp);
    layout->addWidget(new QLabel(
        tr("<i>S7-1500: rack=0, slot=1（默认）。<br/>"
           "皮带速度从 PLC 数据块读取，用于计算喷吹延时。<br/>"
           "倍福 EtherCAT 控制气孔，不在此配置。</i>"), this));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &PlcSettingsDialog::onAccepted);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void PlcSettingsDialog::onAccepted() {
    m_hw.plcType     = (m_typeCombo->currentIndex() == 0) ? "s7-1500" : "mock";
    m_hw.plcEndpoint = m_endpointEdit->text().toStdString();
    accept();
}

}}
