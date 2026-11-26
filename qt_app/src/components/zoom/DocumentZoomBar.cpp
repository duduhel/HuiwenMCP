#include "DocumentZoomBar.h"
#include "styles/StyleSheet.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>

DocumentZoomBar::DocumentZoomBar(QWidget *parent)
    : QWidget(parent), slider_(new QSlider(Qt::Horizontal, this)),
      decrease_(new QPushButton(QStringLiteral("−"), this)),
      increase_(new QPushButton(QStringLiteral("+"), this)), percentage_(new QLabel(this))
{
    setObjectName(QStringLiteral("documentZoomBar"));
    setStyleSheet(loadStyle(QStringLiteral(":/styles/document/DocumentZoomBar.qss")));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 2, 8, 2);
    layout->setSpacing(8);
    decrease_->setFixedSize(30, 30);
    increase_->setFixedSize(30, 30);
    decrease_->setAccessibleName(QStringLiteral("缩小文档"));
    increase_->setAccessibleName(QStringLiteral("放大文档"));
    slider_->setAccessibleName(QStringLiteral("文档缩放比例"));
    slider_->setRange(25, 400);
    slider_->setSingleStep(1);
    slider_->setPageStep(10);
    slider_->setFixedSize(180, 30);
    percentage_->setFixedWidth(64);
    percentage_->setAlignment(Qt::AlignCenter);
    layout->addWidget(decrease_);
    layout->addWidget(slider_);
    layout->addWidget(increase_);
    layout->addWidget(percentage_);
    connect(slider_, &QSlider::valueChanged, this,
            [this](int percent)
            {
                setPercent(percent);
                emit percentRequested(percent);
            });
    connect(decrease_, &QPushButton::clicked, this,
            [this] { slider_->setValue(slider_->value() - 10); });
    connect(increase_, &QPushButton::clicked, this,
            [this] { slider_->setValue(slider_->value() + 10); });
    setPercent(100);
}

void DocumentZoomBar::setPercent(int percent)
{
    const QSignalBlocker blocked(slider_);
    slider_->setValue(percent);
    percentage_->setText(QStringLiteral("%1%").arg(slider_->value()));
    decrease_->setEnabled(slider_->value() > slider_->minimum());
    increase_->setEnabled(slider_->value() < slider_->maximum());
}

void DocumentZoomBar::setMaximumPercent(int percent)
{
    const QSignalBlocker blocked(slider_);
    slider_->setMaximum(percent);
    setPercent(slider_->value());
}
