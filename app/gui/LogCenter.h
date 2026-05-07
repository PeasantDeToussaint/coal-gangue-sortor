#ifndef CGS_LOGCENTER_H
#define CGS_LOGCENTER_H

// Log Center dock widget (Ctrl+M).
// Matches original Gangue.exe "日志中心" feature.
// Displays the last N lines of the application log in a scrolling text view.
// New messages are appended via appendMessage() which is thread-safe
// (posts a Qt signal to ensure UI updates happen on the main thread).

#include <QDockWidget>
#include <QString>

class QPlainTextEdit;
class QPushButton;

namespace cgs {
namespace gui {

class LogCenter : public QDockWidget {
    Q_OBJECT
public:
    explicit LogCenter(QWidget* parent = nullptr);

    // Thread-safe: can be called from any thread (posts to Qt event loop).
    void appendMessage(const QString& msg);

    // Maximum number of lines retained in the view.
    void setMaxLines(int n) { m_maxLines = n; }

signals:
    void messageReady(const QString& msg);

private slots:
    void onMessageReady(const QString& msg);
    void onClearClicked();
    void onSaveClicked();

private:
    QPlainTextEdit* m_view{nullptr};
    QPushButton*    m_clearBtn{nullptr};
    QPushButton*    m_saveBtn{nullptr};
    int             m_maxLines{1000};
};

}}

#endif
