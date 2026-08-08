#ifndef MACOSMOUSEINTERCEPTOR_H
#define MACOSMOUSEINTERCEPTOR_H

#include <QObject>
#include "horizontalshakedetector.h"

class QTimer;

class MacOSMouseInterceptor : public QObject {
    Q_OBJECT
public:
    explicit MacOSMouseInterceptor(QObject *parent = nullptr);
    ~MacOSMouseInterceptor();

    bool start();
    void stop();

signals:
    void shakeDetected();
    void shakeEnded();

private:
    void sampleMouse();

    QTimer *mTimer;
    bool mLeftButtonPressed = false;
    bool mShakeDetected = false;
    double mLastAbsoluteX = 0.0;

    HorizontalShakeDetector mShakeDetector;
};

#endif // MACOSMOUSEINTERCEPTOR_H
