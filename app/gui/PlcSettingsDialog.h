#ifndef CGS_PLCSETTINGSDIALOG_H
#define CGS_PLCSETTINGSDIALOG_H

#include "../../core/Config/ConfigLoader.h"
#include <QDialog>
#include <string>

class QLineEdit;
class QSpinBox;
class QComboBox;

namespace cgs { namespace gui {

class PlcSettingsDialog : public QDialog {
    Q_OBJECT
public:
    PlcSettingsDialog(cgs::core::HardwareConfig& hw,
                      const std::string& configPath,
                      QWidget* parent = nullptr);
private slots:
    void onAccepted();
private:
    cgs::core::HardwareConfig& m_hw;
    std::string m_configPath;
    QComboBox*  m_typeCombo{nullptr};
    QLineEdit*  m_endpointEdit{nullptr};
};

}}
#endif
