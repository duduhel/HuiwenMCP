/* 标题栏功能区：管理文件、滚动、模式及保存入口，只向工作区发出业务请求。 */
#pragma once
#include <QToolBar>
class FileMenu;
class ModeMenu;
class QAction;
class QToolButton;

class TitleBarActions : public QToolBar
{
    Q_OBJECT
  public:
    explicit TitleBarActions(const QString &mode, QWidget *parent = nullptr);
    FileMenu *fileMenu() const;
    ModeMenu *modeMenu() const;
    void setMode(const QString &mode);
    bool isScrollSynchronized() const;
    QAction *addCommand(const QString &text);

  signals:
    void scrollSyncChanged(bool synchronized);

  private:
    FileMenu *fileMenu_;
    ModeMenu *modeMenu_;
    QAction *synchronizedAction_ = nullptr;
    QToolButton *scrollButton_ = nullptr;
};
