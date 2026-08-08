#include "nointernaldraglistview.h"

#include <QDragEnterEvent>
#include <QTimer>

NoInternalDragListView::NoInternalDragListView(QWidget *parent)
    : QListView(parent)
    , mFeedbackTimer(new QTimer(this))
{
    mFeedbackTimer->setSingleShot(true);
    connect(mFeedbackTimer, &QTimer::timeout, this, [this] {
        mDropFeedback.clear();
        viewport()->update();
    });
}

void NoInternalDragListView::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->source() == this)
        e->ignore();
    else {
        emit dropping(true);
        showFeedback("Drop to add");
        QListView::dragEnterEvent(e);
    }
}

void NoInternalDragListView::dragMoveEvent(QDragMoveEvent *e)
{
    if (e->source() == this)
        e->ignore();
    else
        QListView::dragMoveEvent(e);
}

void NoInternalDragListView::dragLeaveEvent(QDragLeaveEvent *e)
{
    emit dropping(false);
    showFeedback({});

    QListView::dragLeaveEvent(e);
}

void NoInternalDragListView::dropEvent(QDropEvent *e)
{
    emit dropping(false);
    showFeedback("Adding...", 2000);

    QListView::dropEvent(e);
}

void NoInternalDragListView::showCopyFinished()
{
    showFeedback("Added", 900);
}

void NoInternalDragListView::showFeedback(const QString &message, int duration)
{
    mDropFeedback = message;
    if (duration)
        mFeedbackTimer->start(duration);
    else
        mFeedbackTimer->stop();
    viewport()->update();
}

void NoInternalDragListView::paintEvent(QPaintEvent *e) {
    QListView::paintEvent(e);

    auto *m = model();
    QPainter p(viewport());

    if (m && m->rowCount(rootIndex()) == 0) {
        QPixmap pm(":/background.png");
        pm = pm.scaled(150, 150,
                       Qt::KeepAspectRatio,
                       Qt::SmoothTransformation);

        const QString text =
            "Drop files, folders, text, links, or images here.";

        QFontMetrics fm(font());
        int textHeight = fm.height();
        int spacing = 8;

        int totalHeight = pm.height() + spacing + textHeight;

        int startY = (height() - totalHeight) / 3;

        QPoint iconPos(width()/2 - pm.width()/2,
                       startY);
        p.drawPixmap(iconPos, pm);

        QRect textRect(0,
                       startY + pm.height() + spacing,
                       width(),
                       textHeight);

        p.setPen(palette().color(QPalette::Text));
        p.drawText(textRect, Qt::AlignHCenter, text);
    }

    if (mDropFeedback.isEmpty())
        return;

    p.fillRect(viewport()->rect(), QColor(0, 0, 0, 96));
    QFont feedbackFont = font();
    feedbackFont.setBold(true);
    feedbackFont.setPointSize(feedbackFont.pointSize() + 2);
    p.setFont(feedbackFont);
    p.setPen(palette().color(QPalette::BrightText));
    p.drawText(viewport()->rect(), Qt::AlignCenter, mDropFeedback);
}
