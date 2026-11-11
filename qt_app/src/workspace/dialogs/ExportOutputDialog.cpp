/* 独立导出确认界面：读取 output 文件清单并返回选择，不处理文件复制或平台桥接。 */
#include "ExportOutputDialog.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QStyleOptionComboBox>
#include <QStylePainter>
#include <QVBoxLayout>

namespace
{
// 只移除按钮自身的高亮绘制状态，真实焦点、键盘操作和列表选中态仍由 QComboBox 管理。
class ExportSelectionComboBox final : public QComboBox
{
  public:
    using QComboBox::QComboBox;

  protected:
    void paintEvent(QPaintEvent *) override
    {
        QStyleOptionComboBox option;
        initStyleOption(&option);
        option.state &= ~(QStyle::State_HasFocus | QStyle::State_KeyboardFocusChange |
                          QStyle::State_On | QStyle::State_Sunken | QStyle::State_Selected);
        QStylePainter painter(this);
        painter.setPen(option.palette.color(QPalette::Text));
        painter.drawComplexControl(QStyle::CC_ComboBox, option);
        painter.drawControl(QStyle::CE_ComboBoxLabel, option);
    }
};

QIcon outputFileIcon(const QString &name)
{
    QString kind = QFileInfo(name).suffix().toLower();
    if (kind == "markdown")
        kind = "md";
    else if (QStringList{"png", "jpg", "jpeg", "gif", "webp", "bmp", "svg"}.contains(kind))
        kind = "photo";
    const QIcon icon(QStringLiteral(":/file-icons/%1.png").arg(kind));
    return icon.isNull() ? QIcon(QStringLiteral(":/file-icons/genericfile.png")) : icon;
}
} // namespace

ExportOutputDialog::ExportOutputDialog(const QString &outputPath, QWidget *parent) : QDialog(parent)
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
    nameRow->setSpacing(16);
    nameRow->addWidget(new QLabel(QStringLiteral("名称"), this));

    const QDir directory = QFileInfo(outputPath).dir();
    QStringList files;
    QDirIterator iterator(directory.absolutePath(), QDir::Files | QDir::Hidden | QDir::NoSymLinks,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext())
    {
        const QString relative = directory.relativeFilePath(iterator.next());
        if (!relative.startsWith(".pdf2md-") && !relative.contains("/.pdf2md-"))
            files.append(relative);
    }
    files.sort(Qt::CaseInsensitive);
    if (files.size() == 1)
    {
        onlyFile_ = files.first();
        auto *fileRow = new QHBoxLayout;
        fileRow->setContentsMargins(0, 0, 0, 0);
        fileRow->setSpacing(8);
        auto *icon = new QLabel(this);
        icon->setPixmap(outputFileIcon(onlyFile_).pixmap(22, 22));
        icon->setFixedSize(22, 22);
        fileRow->addWidget(icon);
        auto *name = new QLabel(QFileInfo(onlyFile_).fileName(), this);
        name->setTextFormat(Qt::PlainText);
        name->setWordWrap(true);
        fileRow->addWidget(name, 1);
        nameRow->addLayout(fileRow, 1);
    }
    else
    {
        selection_ = new ExportSelectionComboBox(this);
        selection_->setIconSize(QSize(22, 22));
        selection_->setMaxVisibleItems(8);
        selection_->setMinimumContentsLength(20);
        selection_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        selection_->addItem(style()->standardIcon(QStyle::SP_DirIcon),
                            QFileInfo(outputPath).completeBaseName().left(64) +
                                QStringLiteral("（整个文件夹）"),
                            QString());
        for (const auto &file : files)
        {
            selection_->addItem(outputFileIcon(file), file, file);
            selection_->setItemData(selection_->count() - 1, file, Qt::ToolTipRole);
        }
        nameRow->addWidget(selection_, 1);
    }
    layout->addLayout(nameRow);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save, this);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("选择保存路径"));
    buttons->button(QDialogButtonBox::Save)->setEnabled(!files.isEmpty());
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    layout->addWidget(buttons);
}

QString ExportOutputDialog::selectedRelativePath() const
{
    return selection_ ? selection_->currentData().toString() : onlyFile_;
}
