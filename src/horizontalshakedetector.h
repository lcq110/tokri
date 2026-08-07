#ifndef HORIZONTALSHAKEDETECTOR_H
#define HORIZONTALSHAKEDETECTOR_H

#include <cstdint>

class HorizontalShakeDetector
{
public:
    bool feed(int dx, uint64_t tsMs);
    void reset();

private:
    int lastDir = 0;
    int strokeDistance = 0;
    int flips = 0;
    uint64_t lastTsMs = 0;

    int minStrokeDistance = 18;
    uint64_t maxGapMs = 350;
    int flipThreshold = 2;
};

#endif // HORIZONTALSHAKEDETECTOR_H
