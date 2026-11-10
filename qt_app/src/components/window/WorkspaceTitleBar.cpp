#include "WorkspaceTitleBar.h"
#include "cust/framelesswidget.h"
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QToolButton>

namespace
{
QIcon windowControlIcon(const QString &path, const QSize &size)
{
    const QImage source(path);
    QRect ink;
    int maxAlpha = 0;
    for (int y = 0; y < source.height(); ++y)
        for (int x = 0; x < source.width(); ++x)
        {
            const int alpha = qAlpha(source.pixel(x, y));
            if (alpha > 0)
                ink = ink.united(QRect(x, y, 1, 1));
            maxAlpha = qMax(maxAlpha, alpha);
        }
    if (ink.isEmpty())
        return QIcon(path);
    // 去除 SDK 图片的透明外边距，最小化横线仍保持原来的宽高比。
    const int side = qMax(ink.width(), ink.height());
    QImage image(side, side, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.drawImage(QPoint((side - ink.width()) / 2, (side - ink.height()) / 2),
                      source.copy(ink));
    painter.end();
    // SDK 失焦 PNG 的主体透明度只有 40%；恢复实心黑色，按比例保留边缘抗锯齿。
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            image.setPixel(x, y, qRgba(0, 0, 0, qAlpha(image.pixel(x, y)) * 255 / maxAlpha));
    // QIcon 的默认位图引擎只缩小图像，提前按按钮的目标尺寸缩放。
    return QIcon(
        QPixmap::fromImage(image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
}
} // namespace

WorkspaceTitleBar::WorkspaceTitleBar(QWidget *window)
    : QWidget(window), window_(window), title_(new QLabel(window->windowTitle(), this))
{
    setObjectName(QStringLiteral("workspaceTitleBar"));
    setAttribute(Qt::WA_StyledBackground);
    constexpr int buttonHeight = 40;
    constexpr int topInset = 12;
    constexpr int rightInset = 16;
    setFixedHeight(buttonHeight + topInset);
    setStyleSheet(QStringLiteral(
        "#workspaceTitleBar { background: transparent; }"
        "#workspaceTitleBar QLabel { background: transparent; color: #000000; "
        "padding-left: 16px; font-size: 16px; font-weight: bold; }"
        "#workspaceTitleBar QToolButton { border: none; background: transparent; }"
        "#workspaceTitleBar QToolButton:hover { background: #DCE1E8; }"
        "#workspaceTitleBar QToolButton#closeWindow:hover { background: #E86969; }"));
    auto *layout = new QHBoxLayout(this);
    // 给鸿蒙小窗口右上角的系统缩放热区留出空间。
    layout->setContentsMargins(0, topInset, rightInset, 0);
    layout->setSpacing(0);
    title_->setMinimumWidth(0);
    title_->setTextFormat(Qt::RichText);
    title_->setTextInteractionFlags(Qt::NoTextInteraction);
    layout->addWidget(title_, 1);
    auto *windowActions = new FramelessWidget(window_, title_);
    const QSize iconSize(28, 28);
    const auto makeButton = [this, layout, iconSize](const QString &name, const QString &icon)
    {
        auto *button = new QToolButton(this);
        button->setFixedSize(48, buttonHeight);
        button->setIconSize(iconSize);
        button->setIcon(windowControlIcon(icon, iconSize));
        button->setToolTip(name);
        button->setAccessibleName(name);
        layout->addWidget(button);
        return button;
    };
    maximize_ =
        makeButton(QStringLiteral("最大化"), QStringLiteral(":/window-controls/maximize.png"));
    auto *minimize =
        makeButton(QStringLiteral("最小化"), QStringLiteral(":/window-controls/minimize.png"));
    auto *close =
        makeButton(QStringLiteral("关闭工作区"), QStringLiteral(":/window-controls/close.png"));
    close->setObjectName(QStringLiteral("closeWindow"));
    connect(minimize, &QToolButton::clicked, window_, &QWidget::showMinimized);
    connect(maximize_, &QToolButton::clicked, windowActions, &FramelessWidget::toggleMaximized);
    connect(close, &QToolButton::clicked, this, &WorkspaceTitleBar::closeRequested);
    window_->installEventFilter(this);
    updateWindowState();
}

void WorkspaceTitleBar::updateWindowState()
{
    const bool restore = window_->isMaximized() || window_->isFullScreen();
    // 与首页使用同一品牌素材；图标和文字仍共用原有拖动／双击区域。
    const QString branding =
        restore ? QStringLiteral(
                      "<img src=\":/branding/harmonyos_logo.png\" width=\"108\" height=\"20\" "
                      "style=\"vertical-align: middle;\"> &nbsp;×&nbsp; ")
                : QStringLiteral("<img src=\":/branding/app_icon.png\" width=\"20\" height=\"20\" "
                                 "style=\"vertical-align: middle;\"> &nbsp;");
    title_->setText(branding + window_->windowTitle().toHtmlEscaped());
    title_->setAccessibleName(window_->windowTitle());
    const QString text = restore ? QStringLiteral("还原") : QStringLiteral("最大化");
    const QString iconPath = restore ? QStringLiteral(":/window-controls/recover.png")
                                     : QStringLiteral(":/window-controls/maximize.png");
    maximize_->setIcon(windowControlIcon(iconPath, maximize_->iconSize()));
    maximize_->setToolTip(text);
    maximize_->setAccessibleName(text);
}

bool WorkspaceTitleBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == window_)
    {
        if (event->type() == QEvent::WindowStateChange ||
            event->type() == QEvent::WindowTitleChange)
            updateWindowState();
    }
    return QWidget::eventFilter(watched, event);
}
