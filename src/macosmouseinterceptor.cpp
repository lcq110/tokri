#include "macosmouseinterceptor.h"
#include <QDateTime>

static uint64_t nowMs() {
    return static_cast<uint64_t>(QDateTime::currentMSecsSinceEpoch());
}

CGEventRef MacOSMouseInterceptor::callback(CGEventTapProxy,
                                           CGEventType type,
                                           CGEventRef event,
                                           void *userInfo) {
    auto *self = static_cast<MacOSMouseInterceptor *>(userInfo);

    if (type == kCGEventTapDisabledByTimeout ||
        type == kCGEventTapDisabledByUserInput) {
        CGEventTapEnable(self->eventTap, true);
        return event;
    }

    self->handleEvent(type, event);
    return event;
}

MacOSMouseInterceptor::MacOSMouseInterceptor(QObject *parent)
    : QObject(parent) {}

MacOSMouseInterceptor::~MacOSMouseInterceptor() {
    stop();
}

bool MacOSMouseInterceptor::start() {
    if (!CGPreflightListenEventAccess() && !CGRequestListenEventAccess())
        return false;

    CGEventMask mask =
        CGEventMaskBit(kCGEventLeftMouseDragged) |
                       CGEventMaskBit(kCGEventLeftMouseDown) |
                       CGEventMaskBit(kCGEventLeftMouseUp);

    eventTap = CGEventTapCreate(
        kCGSessionEventTap,
        kCGHeadInsertEventTap,
        kCGEventTapOptionListenOnly,
        mask,
        &MacOSMouseInterceptor::callback,
        this);

    if (!eventTap) return false;

    runLoopSource =
        CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);

    CFRunLoopAddSource(CFRunLoopGetMain(),
                       runLoopSource,
                       kCFRunLoopCommonModes);
    CGEventTapEnable(eventTap, true);
    return true;
}

void MacOSMouseInterceptor::stop() {
    if (runLoopSource) {
        CFRunLoopRemoveSource(CFRunLoopGetMain(),
                              runLoopSource,
                              kCFRunLoopCommonModes);
        CFRelease(runLoopSource);
        runLoopSource = nullptr;
    }
    if (eventTap) {
        CFRelease(eventTap);
        eventTap = nullptr;
    }
}

void MacOSMouseInterceptor::handleEvent(CGEventType type, CGEventRef event) {
    if (type == kCGEventLeftMouseDown) {
        mLeftButtonClicked = true;
        mShakeDetector.reset();
        mLastAbsoluteX = CGEventGetLocation(event).x;
        mHaveLastAbsoluteX = true;
        return;
    }
    if (type == kCGEventLeftMouseUp) {
        mLeftButtonClicked = false;
        mShakeDetector.reset();
        mHaveLastAbsoluteX = false;
        return;
    }

    if (type != kCGEventLeftMouseDragged)
        return;

    if (!mLeftButtonClicked) {
        mLeftButtonClicked = true;
        mShakeDetector.reset();
        mHaveLastAbsoluteX = false;
    }

    CGPoint p = CGEventGetLocation(event);

    int dx = 0;
    if (mHaveLastAbsoluteX) {
        dx = static_cast<int>(p.x - mLastAbsoluteX);
    }
    mLastAbsoluteX = p.x;
    mHaveLastAbsoluteX = true;

    if (dx == 0)
        return;

    if (mShakeDetector.feed(dx, nowMs())) {
        emit shakeDetected();
    }
}
