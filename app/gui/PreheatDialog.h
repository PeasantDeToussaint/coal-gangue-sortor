#ifndef CGS_PREHEATDIALOG_H
#define CGS_PREHEATDIALOG_H

// X-Ray source preheat dialog.
//
// Reads LastCloseTime from config, computes required preheat duration per the
// Liong/VJ preheat table, then runs the preheat sequence before arm() is
// allowed.
//
// Preheat table (from X光机简易操作说明2.pdf):
//   < 12 hours idle  → no preheat required
//   12–24 hours      → 30 seconds
//   1–7 days         → 2 minutes
//   7–30 days        → 5 minutes
//   > 30 days        → 10 minutes

#include <QDialog>
#include <QString>
#include <QTimer>

class QProgressBar;
class QLabel;
class QPushButton;

namespace cgs {
namespace hardware { class IXRaySource; }
namespace gui {

class PreheatDialog : public QDialog {
    Q_OBJECT
public:
    explicit PreheatDialog(cgs::hardware::IXRaySource* xray,
                           const std::string& lastCloseTimeStr,
                           QWidget* parent = nullptr);

    // Call after dialog accepted to check if preheat completed successfully.
    bool preheatCompleted() const { return m_completed; }

    // Compute preheat seconds from idle time (static utility).
    static int computePreheatSeconds(const std::string& lastCloseTimeStr);

private slots:
    void onStartClicked();
    void onSkipClicked();
    void onTick();

private:
    void startPreheat(int seconds);

    cgs::hardware::IXRaySource* m_xray;
    std::string m_lastCloseTime;
    int         m_preheatSeconds{0};
    int         m_elapsedSeconds{0};
    bool        m_completed{false};

    QLabel*      m_infoLabel{nullptr};
    QProgressBar* m_progress{nullptr};
    QLabel*      m_countdownLabel{nullptr};
    QPushButton* m_startBtn{nullptr};
    QPushButton* m_skipBtn{nullptr};
    QTimer       m_tickTimer;
};

}}

#endif
