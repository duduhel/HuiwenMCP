/* 独立导出确认界面，只返回确认或取消，不处理文件保存与平台桥接。 */
#include "ExportOutputDialog.h"
#include <QDialogButtonBox>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

ExportOutputDialog::ExportOutputDialog(const QString &folderName, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("导出输出文件"));
    setWindowModality(Qt::WindowModal);
    setMinimumWidth(480);
    auto font = this->font();
    font.setPixelSize(22);
    setFont(font);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(24);
    auto *nameRow = new QHBoxLayout;
    nameRow->setContentsMargins(0, 0, 0, 0);
    nameRow->setSpacing(8);
    nameRow->addWidget(new QLabel(QStringLiteral("名称"), this));
    auto *folderIcon = new QLabel(this);
    folderIcon->setPixmap(style()->standardIcon(QStyle::SP_DirIcon).pixmap(20, 20));
    folderIcon->setFixedSize(20, 20);
    nameRow->addWidget(folderIcon);
    auto *name = new QLabel(folderName, this);
    name->setTextFormat(Qt::PlainText);
    name->setWordWrap(true);
    nameRow->addWidget(name, 1);
    layout->addLayout(nameRow);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save, this);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("选择保存路径"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    layout->addWidget(buttons);
}
