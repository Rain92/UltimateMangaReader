#pragma once

#include <QDialog>

class QLabel;
class QPushButton;

// The questions about an update of the sources, and the wait while it is installed.
class SourceUpdateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SourceUpdateDialog(QWidget *parent);

    // Asks and waits for the answer: true for yes, false for no (also when it is closed by a tap outside).
    bool ask(const QString &title, const QString &text, const QString &yes, const QString &no);
    // Shows the text without buttons until finish(), it can't be closed meanwhile.
    void showBusy(const QString &title, const QString &text);
    void finish();

protected:
    void closeEvent(QCloseEvent *event) override;
    void reject() override;

private:
    QLabel *titleLabel;
    QLabel *textLabel;
    QPushButton *yesButton;
    QPushButton *noButton;
    bool busy;
};
