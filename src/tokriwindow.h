#ifndef TOKRIWINDOW_H
#define TOKRIWINDOW_H

#include "closebutton.h"

#include <QListView>
#include <QMainWindow>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QMimeData>
#include <QPainter>

class QPropertyAnimation;
class QTimer;

QT_BEGIN_NAMESPACE
namespace Ui {
class TokriWindow;
}
QT_END_NAMESPACE

class TokriWindow : public QMainWindow
{
    Q_OBJECT

public:
    TokriWindow(QWidget *parent = nullptr);
    ~TokriWindow();
    Ui::TokriWindow* uiHandle();
    void sleep();
    void wakeUp();
    bool canUndoDelete() const;

public slots:
    void onShakeDetect();
    void beginDragWake();
    void endDragWake();
    void previewSelectedItem();
    void deleteSelectedItems();
    void undoLastDelete();

signals:
    void undoDeleteAvailabilityChanged(bool available);

private:
    Ui::TokriWindow *ui;
    bool mDropping = false;
    bool mDockedAtEdge = false;
    bool mEdgeHidden = false;
    bool mDragWakeActive = false;
    CloseButton *mCloseButton;
    QPropertyAnimation *mDockAnimation;
    QTimer *mEdgeHoverTimer;
    QTimer *mEdgeHideTimer;

    void init();
    void moveNearCursor();
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *e) override;
    void setDropping(bool status);
    void showEvent(QShowEvent *e) override;
    void openItem(QString filePath);
    void renderCloseButton();
    void updateEdgeHover();
    QPoint screenEdgePosition(bool hidden) const;
    void moveToScreenEdge(bool hidden, bool animated);
    void dockAtScreenEdge();
    void revealFromScreenEdge();
};
#endif // TOKRIWINDOW_H
