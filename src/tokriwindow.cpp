#include "tokriwindow.h"
#include "./ui_tokriwindow.h"
#include "standardpaths.h"
#include "listitemdelegate.h"
#include "closebutton.h"
#include "sleekscrollbar.h"
#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QFileSystemModel>
#include <QMenu>
#include <QPropertyAnimation>
#include <QScreen>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#ifdef Q_OS_MAC
#include "MacQuickLook.h"
#include "MacWindowLevel.h"
#endif

TokriWindow::TokriWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::TokriWindow)
{
    ui->setupUi(this);

    init();
    setWindowFlags(windowFlags()
                   | Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);

    mDockAnimation = new QPropertyAnimation(this, "pos", this);
    mDockAnimation->setDuration(180);
    mDockAnimation->setEasingCurve(QEasingCurve::OutCubic);

    mEdgeHoverTimer = new QTimer(this);
    mEdgeHoverTimer->setInterval(50);
    connect(mEdgeHoverTimer, &QTimer::timeout,
            this, &TokriWindow::updateEdgeHover);

    mEdgeHideTimer = new QTimer(this);
    mEdgeHideTimer->setInterval(700);
    mEdgeHideTimer->setSingleShot(true);
    connect(mEdgeHideTimer, &QTimer::timeout, this, [this] {
        if (mDockedAtEdge && !mEdgeHidden &&
            qApp->applicationState() != Qt::ApplicationActive &&
            !frameGeometry().contains(QCursor::pos())) {
            moveToScreenEdge(true, true);
        }
    });

    connect(qApp, &QGuiApplication::applicationStateChanged,
            this, [this](Qt::ApplicationState state) {
                if (state == Qt::ApplicationInactive) {
                    if (!mDragWakeActive)
                        dockAtScreenEdge();
                } else if (state == Qt::ApplicationActive) {
                    wakeUp();
                }
            });

    ui->listView->setStyleSheet(R"(
        QListView {
            padding: 0px;
            margin: 0px;
        }
    )");

    mCloseButton = new CloseButton(this);
    mCloseButton->setParent(this);
    mCloseButton->raise();
    connect(
        mCloseButton,
        &QAbstractButton::clicked,
        this,
        &TokriWindow::sleep
        );
    renderCloseButton();


    ui->listView->setVerticalScrollBar(new SleekScrollBar(Qt::Vertical, ui->listView));
    const auto delegate = new ListItemDelegate(ui->listView);
    ui->listView->setItemDelegate(delegate);
    ui->listView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(ui->listView, &QListView::doubleClicked,
            this, [this](const QModelIndex &idx){
                if (!idx.isValid())
                    return;

                const QString filePath =
                    idx.data(QFileSystemModel::FileInfoRole)
                        .value<QFileInfo>()
                        .filePath();
                openItem(filePath);
    });

    ui->listView->setViewMode(QListView::IconMode);
    ui->listView->setGridSize({100, 130});
    ui->listView->setFlow(QListView::LeftToRight);
    ui->listView->setWrapping(true);
    ui->listView->setUniformItemSizes(true);
    ui->listView->setSpacing(8);
    ui->listView->setMouseTracking(true);
    ui->listView->setFocusPolicy(Qt::NoFocus);
    ui->listView->setDropIndicatorShown(false);
    ui->listView->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->listView, &QWidget::customContextMenuRequested, this,
            [this](const QPoint &pos) {
                auto *view = ui->listView;
                auto *sel  = view->selectionModel();
                const auto selected = sel->selectedIndexes();
                const int count = selected.size();

                QMenu menu;
                menu.setPalette(this->palette());

                QAction *open = nullptr, *reveal = nullptr, *rename = nullptr;
                QAction *copy = nullptr, *del = nullptr, *selectAll = nullptr;

                if (count == 1) {
                    open   = menu.addAction("&Open");
                    reveal= menu.addAction("Reveal in &Explorer");
                    rename= menu.addAction("&Rename");
                }
                if (count > 0) {
                    copy = menu.addAction("&Copy");
                    del  = menu.addAction("&Delete");
                }
                QAction *undoDelete = menu.addAction("&Undo Delete");
                undoDelete->setEnabled(canUndoDelete());
                selectAll = menu.addAction("Select &All");

                QAction *chosen = menu.exec(view->viewport()->mapToGlobal(pos));
                if (!chosen) return;

                auto fileInfoAt = [](const QModelIndex &idx) {
                    return idx.data(QFileSystemModel::FileInfoRole).value<QFileInfo>();
                };

                if (chosen == selectAll) {
                    view->selectAll();
                    return;
                }

                if (count == 1 && chosen == open) {
                    QString filePath = fileInfoAt(selected[0]).filePath();
                    openItem(filePath);
                    return;
                }

                if (count == 1 && chosen == reveal) {
                    QDesktopServices::openUrl(
                        QUrl::fromLocalFile(fileInfoAt(selected[0]).absolutePath()));
                    return;
                }

                if (count == 1 && chosen == rename) {
                    view->edit(selected[0]);
                    return;
                }

                if (chosen == copy) {
                    QList<QUrl> urls;
                    for (const auto &idx : selected) {
                        const auto fi = fileInfoAt(idx);
                        if (fi.exists())
                            urls << QUrl::fromLocalFile(fi.absoluteFilePath());
                    }
                    if (!urls.isEmpty()) {
                        auto *mime = new QMimeData;
                        mime->setUrls(urls);
                        QGuiApplication::clipboard()->setMimeData(mime);
                    }
                    return;
                }

                if (chosen == del) {
                    deleteSelectedItems();
                    return;
                }

                if (chosen == undoDelete) {
                    undoLastDelete();
                }
            });

    connect(
        ui->listView,
        &NoInternalDragListView::dropping,
        this,
        &TokriWindow::setDropping
        );
}

TokriWindow::~TokriWindow()
{
    delete ui;
}

Ui::TokriWindow *TokriWindow::uiHandle()
{
    return ui;
}

void TokriWindow::sleep()
{
    mDockAnimation->stop();
    mEdgeHoverTimer->stop();
    mEdgeHideTimer->stop();
    mDockedAtEdge = false;
    mEdgeHidden = false;
    mDragWakeActive = false;
    hide();
}

void TokriWindow::wakeUp()
{
    const bool docked = mDockedAtEdge;
    const bool minimized = isMinimized();
    const bool hidden = !isVisible();

    mDockAnimation->stop();
    mEdgeHoverTimer->stop();
    mEdgeHideTimer->stop();
    mDockedAtEdge = false;
    mEdgeHidden = false;

    if (docked || minimized || hidden)
        moveNearCursor();

    if (minimized) {
        showNormal();
    } else if (hidden) {
        show();
    }

#ifdef Q_OS_MAC
    MacWindowLevel::activateApplication();
#endif
    raise();
    activateWindow();
}

void TokriWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = QRectF(rect()).adjusted(2.0, 2.0, -2.0, -2.0);

    // background
    p.setPen(Qt::NoPen);
    p.setBrush(palette().color(QPalette::Window));
    p.drawRoundedRect(r, 16.0, 16.0);

    // border / drop indicator
    QColor color = palette().color(
        mDropping ? QPalette::Accent : QPalette::Shadow
        );

    QPen pen(color);
    pen.setWidthF(2.0);
    if (mDropping){
        pen.setWidthF(8.0);
    }
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);

    p.setBrush(Qt::NoBrush);
    p.setPen(pen);
    p.drawRoundedRect(r, 16.0, 16.0);
}

void TokriWindow::setDropping(bool status)
{
    mDropping = status;
    update();
}

void TokriWindow::resizeEvent(QResizeEvent *e)
{
    QMainWindow::resizeEvent(e);
    renderCloseButton();
}

void TokriWindow::init()
{
    QString tokriDir = StandardPaths::getPath(StandardPaths::TokriDir);
    QDir dir(tokriDir);
    if (!dir.exists()){
        bool success = dir.mkpath(tokriDir);
        if (!success){
            // FIXME handle error
        }
    }
#ifdef Q_OS_MAC
    MacWindowLevel::hideFromDock();
#endif
}

void TokriWindow::moveNearCursor()
{
    const QPoint cursor = QCursor::pos();
    const QSize  winSize = size();
    QPoint p(cursor.x() + 20, cursor.y() + 20);

    const QRect screen = QGuiApplication::screenAt(cursor)->availableGeometry();

    if (p.x() + winSize.width() > screen.right())
        p.setX(screen.right() - winSize.width());
    if (p.y() + winSize.height() > screen.bottom())
        p.setY(screen.bottom() - winSize.height());
    if (p.x() < screen.left())
        p.setX(screen.left());
    if (p.y() < screen.top())
        p.setY(screen.top());

    move(p);
}

void TokriWindow::onShakeDetect()
{
    wakeUp();
}

void TokriWindow::beginDragWake()
{
    mDragWakeActive = true;
    wakeUp();
}

void TokriWindow::endDragWake()
{
    mDragWakeActive = false;
    wakeUp();
}

void TokriWindow::previewSelectedItem()
{
#ifdef Q_OS_MAC
    const auto selected = ui->listView->selectionModel()->selectedIndexes();
    if (selected.isEmpty())
        return;

    const QModelIndex current = ui->listView->currentIndex();
    const QModelIndex index = selected.contains(current) ? current : selected.first();
    const QString filePath = index.data(QFileSystemModel::FileInfoRole)
                                 .value<QFileInfo>()
                                 .filePath();
    MacQuickLook::toggle(filePath);
#endif
}

void TokriWindow::deleteSelectedItems()
{
    QFileInfoList items;
    for (const QModelIndex &index : ui->listView->selectionModel()->selectedIndexes()) {
        const QFileInfo item = index.data(QFileSystemModel::FileInfoRole)
                                   .value<QFileInfo>();
        if (item.exists())
            items.append(item);
    }

    if (items.isEmpty())
        return;

    const QString undoPath = StandardPaths::getPath(StandardPaths::UndoDir);
    QDir(undoPath).removeRecursively();
    QDir().mkpath(undoPath);

    for (const QFileInfo &item : items) {
        const QString destination = QDir(undoPath).filePath(item.fileName());
        if (item.isDir())
            QDir().rename(item.absoluteFilePath(), destination);
        else
            QFile::rename(item.absoluteFilePath(), destination);
    }

    emit undoDeleteAvailabilityChanged(canUndoDelete());
}

void TokriWindow::undoLastDelete()
{
    const QString undoPath = StandardPaths::getPath(StandardPaths::UndoDir);
    QDir undoDir(undoPath);
    const QFileInfoList items = undoDir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    const QString basketPath = StandardPaths::getPath(StandardPaths::TokriDir);

    for (const QFileInfo &item : items) {
        const QString destination = QDir(basketPath).filePath(item.fileName());
        if (item.isDir())
            QDir().rename(item.absoluteFilePath(), destination);
        else
            QFile::rename(item.absoluteFilePath(), destination);
    }

    emit undoDeleteAvailabilityChanged(canUndoDelete());
}

bool TokriWindow::canUndoDelete() const
{
    const QDir undoDir(StandardPaths::getPath(StandardPaths::UndoDir));
    return !undoDir.entryList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System
        ).isEmpty();
}


void TokriWindow::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
#ifdef Q_OS_MAC
    MacWindowLevel::makeAlwaysOnTop(windowHandle());
#endif
}

void TokriWindow::updateEdgeHover()
{
    if (!mDockedAtEdge || !isVisible() ||
        mDockAnimation->state() != QAbstractAnimation::Stopped)
        return;

    const bool cursorOverWindow = frameGeometry().contains(QCursor::pos());
    if (mEdgeHidden && cursorOverWindow) {
        mEdgeHideTimer->stop();
        revealFromScreenEdge();
    } else if (!mEdgeHidden) {
        if (cursorOverWindow ||
            qApp->applicationState() == Qt::ApplicationActive) {
            mEdgeHideTimer->stop();
        } else if (!mEdgeHideTimer->isActive()) {
            mEdgeHideTimer->start();
        }
    }
}

QPoint TokriWindow::screenEdgePosition(bool hidden) const
{
    QScreen *rightmostScreen = QGuiApplication::primaryScreen();
    for (QScreen *screen : QGuiApplication::screens()) {
        if (screen->geometry().right() > rightmostScreen->geometry().right())
            rightmostScreen = screen;
    }

    const QRect available = rightmostScreen->availableGeometry();
    const int y = qBound(available.top(), pos().y(),
                         available.bottom() - height() + 1);
    const int edgeHandleWidth = 14;
    const int x = hidden
        ? available.right() - edgeHandleWidth + 1
        : available.right() - width() + 1;
    return {x, y};
}

void TokriWindow::moveToScreenEdge(bool hidden, bool animated)
{
    const QPoint destination = screenEdgePosition(hidden);
    mDockAnimation->stop();
    mEdgeHidden = hidden;

    if (!animated) {
        move(destination);
        return;
    }

    mDockAnimation->setStartValue(pos());
    mDockAnimation->setEndValue(destination);
    mDockAnimation->start();
}

void TokriWindow::dockAtScreenEdge()
{
    if (!isVisible())
        return;

    mDockedAtEdge = true;
    mEdgeHideTimer->stop();
    mEdgeHoverTimer->start();
    moveToScreenEdge(true, true);
}

void TokriWindow::revealFromScreenEdge()
{
    moveToScreenEdge(false, true);
}

void TokriWindow::openItem(QString filePath) {
    if (filePath.endsWith(".url.txt"))
    {
        QFile file(filePath);
        if (file.open(QIODeviceBase::ReadOnly | QIODeviceBase::Text))
        {
            QTextStream in(&file);
            QString line = in.readLine();
            if (!line.isEmpty())
            {
                qDebug() << "Opening url" << line;
                QDesktopServices::openUrl(line);
                return;
            }
        }
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
}

void TokriWindow::renderCloseButton()
{
        const int m = 8;
#ifdef Q_OS_MAC
        mCloseButton->move(m, m);
#else
        mCloseButton->move(width() - mCloseButton->width() - m, m);
#endif
}
