#pragma once

#include <QDialog>
#include <optional>

class CLineEdit;
class VirtualKeyboard;

// Edits the repository of the sources: a field with the on-screen keyboard (if the device needs one), at the
// bottom of the parent.
class SourceRepositoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SourceRepositoryDialog(QWidget *parent);

    // Asks for the repository. current: the one of the settings, empty for the default one. Returns the new one
    // (empty for the default one), nothing if it was cancelled.
    std::optional<QString> ask(const QString &current, const QString &defaultUrl);

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    CLineEdit *lineEdit;
    VirtualKeyboard *keyboard;

    // at the bottom of the parent, over its width
    void place();
};
