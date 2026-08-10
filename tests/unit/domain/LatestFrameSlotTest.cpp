#include "astro/domain/LatestFrameSlot.hpp"

#include <thread>

#include "Check.hpp"

using astro::domain::FrameBuffer;
using astro::domain::LatestFrameSlot;

namespace {

FrameBuffer makeFrame(std::uint64_t sequence) {
    FrameBuffer frame;
    frame.metadata.sequence = sequence;
    return frame;
}

}  // namespace

int main() {
    LatestFrameSlot slot;
    ASTRO_TEST_CHECK(!slot.latest().has_value());

    slot.store(makeFrame(1));
    ASTRO_TEST_CHECK(slot.latest()->metadata.sequence == 1);

    // A later store must fully replace the earlier one, not queue it.
    slot.store(makeFrame(2));
    ASTRO_TEST_CHECK(slot.latest()->metadata.sequence == 2);

    // Concurrent producer/consumer must not crash or tear a read.
    LatestFrameSlot concurrentSlot;
    std::thread producer([&] {
        for (std::uint64_t i = 0; i < 10'000; ++i) {
            concurrentSlot.store(makeFrame(i));
        }
    });
    for (int i = 0; i < 10'000; ++i) {
        (void)concurrentSlot.latest();
    }
    producer.join();
    ASTRO_TEST_CHECK(concurrentSlot.latest().has_value());

    std::cout << "OK\n";
    return 0;
}
