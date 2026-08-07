#include "horizontalshakedetector.h"

#include <cstdlib>

bool HorizontalShakeDetector::feed(int dx, uint64_t tsMs)
{
    if (dx == 0)
        return false;

    if (lastDir != 0 && tsMs - lastTsMs > maxGapMs)
        reset();

    lastTsMs = tsMs;
    const int dir = dx > 0 ? 1 : -1;

    if (lastDir == 0) {
        lastDir = dir;
        strokeDistance = std::abs(dx);
        return false;
    }

    if (dir == lastDir) {
        strokeDistance += std::abs(dx);
        return false;
    }

    if (strokeDistance < minStrokeDistance)
        return false;

    ++flips;
    lastDir = dir;
    strokeDistance = std::abs(dx);

    if (flips < flipThreshold)
        return false;

    reset();
    return true;
}

void HorizontalShakeDetector::reset()
{
    lastDir = 0;
    strokeDistance = 0;
    flips = 0;
    lastTsMs = 0;
}
