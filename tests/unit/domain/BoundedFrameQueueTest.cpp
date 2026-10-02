#include "astro/domain/BoundedFrameQueue.hpp"

#include <atomic>
#include <chrono>
#include <thread>

#include "Check.hpp"

using astro::domain::BoundedFrameQueue;
using astro::domain::FrameBuffer;

namespace {

FrameBuffer makeFrame(std::uint64_t sequence) {
    FrameBuffer frame;
    frame.metadata.sequence = sequence;
    return frame;
}

}  // namespace

int main() {
    // Stopping seals the queue even while a producer is still running.
    // Only frames accepted before stop may be drained by the recorder.
    {
        BoundedFrameQueue queue(4);
        ASTRO_TEST_CHECK(queue.tryPush(makeFrame(1)));
        queue.stop();
        ASTRO_TEST_CHECK(!queue.tryPush(makeFrame(2)));
        ASTRO_TEST_CHECK(queue.waitPop()->metadata.sequence == 1);
        ASTRO_TEST_CHECK(!queue.waitPop());
        ASTRO_TEST_CHECK(!queue.tryPush(makeFrame(3)));
        ASTRO_TEST_CHECK(!queue.waitPop());
    }

    // Capacity is enforced: the (capacity+1)th push is dropped, not queued.
    {
        BoundedFrameQueue queue(2);
        ASTRO_TEST_CHECK(queue.tryPush(makeFrame(1)));
        ASTRO_TEST_CHECK(queue.tryPush(makeFrame(2)));
        ASTRO_TEST_CHECK(!queue.tryPush(makeFrame(3)));
    }

    // FIFO order.
    {
        BoundedFrameQueue queue(4);
        queue.tryPush(makeFrame(10));
        queue.tryPush(makeFrame(20));
        queue.tryPush(makeFrame(30));

        const auto first = queue.waitPop();
        const auto second = queue.waitPop();
        const auto third = queue.waitPop();
        ASTRO_TEST_CHECK(first && first->metadata.sequence == 10);
        ASTRO_TEST_CHECK(second && second->metadata.sequence == 20);
        ASTRO_TEST_CHECK(third && third->metadata.sequence == 30);
    }

    // stop() unblocks a waiting waitPop() with nullopt once drained.
    {
        BoundedFrameQueue queue(4);
        queue.tryPush(makeFrame(1));

        std::thread stopper([&queue] {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            queue.stop();
        });

        const auto first = queue.waitPop();
        ASTRO_TEST_CHECK(first && first->metadata.sequence == 1);
        const auto afterStop = queue.waitPop();
        ASTRO_TEST_CHECK(!afterStop.has_value());
        stopper.join();
    }

    // Concurrent producer/consumer must not crash, and every accepted push
    // must be observed exactly once by the consumer.
    {
        BoundedFrameQueue queue(8);
        constexpr std::uint64_t kFrameCount = 5'000;
        std::atomic<std::uint64_t> received{0};

        std::thread consumer([&queue, &received] {
            while (auto frame = queue.waitPop()) {
                (void)frame;
                received.fetch_add(1, std::memory_order_relaxed);
            }
        });

        for (std::uint64_t i = 0; i < kFrameCount; ++i) {
            while (!queue.tryPush(makeFrame(i))) {
                std::this_thread::yield();
            }
        }
        queue.stop();
        consumer.join();

        ASTRO_TEST_CHECK(received.load() == kFrameCount);
    }

    std::cout << "OK\n";
    return 0;
}
