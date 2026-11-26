/* 文档缩放条：只管理比例选择，不依赖 PDF、Word 或工作区业务。 */
#pragma once
#include <QWidget>

class QLabel;
class QPushButton;
class QSlider;

class DocumentZoomBar : public QWidget
{
    Q_OBJECT
  public:
    explicit DocumentZoomBar(QWidget *parent = nullptr);
    void setPercent(int percent);
    void setMaximumPercent(int percent);

  signals:
    void percentRequested(int percent);

  private:
    QSlider *slider_;
    QPushButton *decrease_;
    QPushButton *increase_;
    QLabel *percentage_;
};
