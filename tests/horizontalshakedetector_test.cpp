#include "horizontalshakedetector.h"

#include <cstdlib>
#include <iostream>

static void expect(bool condition, const char *message)
{
    if (condition)
        return;

    std::cerr << message << '\n';
    std::exit(1);
}

int main()
{
    HorizontalShakeDetector detector;
    uint64_t now = 0;

    for (int i = 0; i < 6; ++i)
        expect(!detector.feed(3, now += 10), "triggered during first stroke");
    for (int i = 0; i < 6; ++i)
        expect(!detector.feed(-3, now += 10), "triggered after one reversal");
    expect(detector.feed(3, now += 10), "small deltas did not accumulate");

    detector.reset();
    for (int i = 0; i < 6; ++i)
        expect(!detector.feed(3, now += 10), "triggered during reset test");
    expect(!detector.feed(-3, now += 500), "slow reversal did not reset");
    for (int i = 0; i < 5; ++i)
        expect(!detector.feed(-3, now += 10), "triggered after slow reversal");
    expect(!detector.feed(3, now += 10), "slow sequence counted an old stroke");

    detector.reset();
    for (int i = 0; i < 4; ++i)
        expect(!detector.feed(3, now += 10), "triggered during jitter test");
    expect(!detector.feed(-2, now += 10), "short jitter counted as a stroke");
    for (int i = 0; i < 2; ++i)
        expect(!detector.feed(3, now += 10), "triggered while completing stroke");
    expect(!detector.feed(-3, now += 10), "first valid reversal triggered");

    return 0;
}
