#include "macosmouseinterceptor.h"
#include <CoreGraphics/CoreGraphics.h>
#include <QCursor>
#include <QDateTime>
#include <QTimer>

static uint64_t nowMs() {
    return static_cast<uint64_t>(QDateTime::currentMSecsSinceEpoch());
}

MacOSMouseInterceptor::MacOSMouseInterceptor(QObject *parent)
    : QObject(parent)
    , mTimer(new QTimer(this))
{
    mTimer->setInterval(50);
    connect(mTimer, &QTimer::timeout,
            this, &MacOSMouseInterceptor::sampleMouse);
}

MacOSMouseInterceptor::~MacOSMouseInterceptor() {
    stop();
}

bool MacOSMouseInterceptor::start() {
    mTimer->start();
    return true;
}

void MacOSMouseInterceptor::stop() {
    mTimer->stop();
    mLeftButtonPressed = false;
    mShakeDetected = false;
    mShakeDetector.reset();
}

void MacOSMouseInterceptor::sampleMouse() {
    const bool leftButtonPressed = CGEventSourceButtonState(
        kCGEventSourceStateCombinedSessionState, kCGMouseButtonLeft);

    if (!leftButtonPressed) {
        const bool didShake = mShakeDetected;
        if (mLeftButtonPressed)
            mTimer->setInterval(50);
        mLeftButtonPressed = false;
        mShakeDetected = false;
        mShakeDetector.reset();
        if (didShake)
            emit shakeEnded();
        return;
    }

    const double x = QCursor::pos().x();
    if (!mLeftButtonPressed) {
        mLeftButtonPressed = true;
        mShakeDetected = false;
        mLastAbsoluteX = x;
        mTimer->setInterval(16);
        return;
    }

    const int dx = static_cast<int>(x - mLastAbsoluteX);
    mLastAbsoluteX = x;

    if (dx == 0)
        return;

    if (!mShakeDetected && mShakeDetector.feed(dx, nowMs())) {
        mShakeDetected = true;
        emit shakeDetected();
    }
}
