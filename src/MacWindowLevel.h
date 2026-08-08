#ifndef MACWINDOWLEVEL_H
#define MACWINDOWLEVEL_H

#include <QtGui/QWindow>

namespace MacWindowLevel {
    void makeAlwaysOnTop(QWindow *w);
    void hideFromDock();
    void activateApplication();
}

#endif // MACWINDOWLEVEL_H
