/* 文件选择弹窗：默认选可用 output，不支持的类型和失效文件保持禁用并显示原因。 */
#include "ModeSwitchDialog.h"
#include "sandbox/SandboxStore.h"
#include "workspace/WorkspaceMode.h"
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>
#include <exception>

QString ModeSwitchDialog::usablePath(const DocumentItem &document, const QString &mode, bool output,
                                     QString &reason)
{
    try
    {
        const QString path =
            output ? SandboxStore::workspaceOutputPath(document) : document.inputPath;
        if (!WorkspaceMode::supports(mode, WorkspaceMode::kindOf(path)))
            reason = QStringLiteral("此文件格式不支持%1").arg(WorkspaceMode::title(mode));
        else
        {
            const QString validated = SandboxStore::validateWorkspaceFile(document, path);
            if (mode == "edit" && QFileInfo(validated).size() > 16 * 1024 * 1024)
                reason = QStringLiteral("Markdown 超过 16 MiB，无法编辑");
            else
                return validated;
        }
    }
    catch (const std::exception &error)
    {
        reason = QString::fromUtf8(error.what());
    }
    return {};
}

bool ModeSwitchDialog::hasUsableFile(const DocumentItem &document, const QString &mode)
{
    if (document.taskPath.isEmpty())
        return false;
    QString reason;
    return !usablePath(document, mode, true, reason).isEmpty() ||
           !usablePath(document, mode, false, reason).isEmpty();
}

ModeSwitchDialog::ModeSwitchDialog(const DocumentItem &document, const QString &mode,
                                   QWidget *parent)
    : QDialog(parent), input_(new QRadioButton(this)), output_(new QRadioButton(this))
{
    setWindowTitle(QStringLiteral("切换到%1").arg(WorkspaceMode::title(mode)));
    setWindowModality(Qt::WindowModal);
    setMinimumWidth(560);
    auto font = this->font();
    font.setPixelSize(22);
    setFont(font);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(18);
    layout->addWidget(
        new QLabel(QStringLiteral("请选择用于%1的文件").arg(WorkspaceMode::title(mode)), this));
    for (const bool output : {false, true})
    {
        QString reason;
        const QString path = usablePath(document, mode, output, reason);
        (output ? outputPath_ : inputPath_) = path;
        auto *radio = output ? output_ : input_;
        radio->setEnabled(!path.isEmpty());
        const QString name = output ? WorkspaceMode::kindOf(document.inputPath) == "md"
                                          ? document.name
                                          : QFileInfo(document.inputPath).completeBaseName() + ".md"
                                    : document.name;
        const QString fileName = path.isEmpty() ? name : QFileInfo(path).fileName();
        radio->setAccessibleName((output ? QStringLiteral("output ") : QStringLiteral("input ")) +
                                 fileName);
        radio->setToolTip(fileName);
        radio->setText(QStringLiteral("%1    %2%3")
                           .arg(output ? "output" : "input",
                                QFontMetrics(font).elidedText(fileName, Qt::ElideMiddle, 380),
                                reason.isEmpty() ? QString() : "\n" + reason));
        layout->addWidget(radio);
    }
    if (!outputPath_.isEmpty())
        output_->setChecked(true);
    else if (!inputPath_.isEmpty())
        input_->setChecked(true);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("切换"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    buttons->button(QDialogButtonBox::Ok)->setEnabled(!selectedPath().isEmpty());
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QString ModeSwitchDialog::selectedPath() const
{
    return output_->isChecked() ? outputPath_ : input_->isChecked() ? inputPath_ : QString();
}
