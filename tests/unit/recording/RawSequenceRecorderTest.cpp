#include "astro/recording/RawSequenceRecorder.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <vector>

#include "astro/recording/RawSequenceReader.hpp"

#include "Check.hpp"

using astro::domain::BayerPattern;
using astro::domain::BufferStorage;
using astro::domain::FrameBuffer;
using astro::domain::FrameDescriptor;
using astro::domain::PixelStorage;
using astro::recording::frameWallClockUnixNanos;
using astro::recording::RawSequenceReader;
using astro::recording::RawSequenceRecorder;

namespace {

FrameBuffer makeFrame(const FrameDescriptor& descriptor, std::uint64_t sequence, std::uint8_t fillValue) {
    FrameBuffer frame;
    frame.descriptor = descriptor;
    frame.metadata.sequence = sequence;
    frame.metadata.hostArrival =
        std::chrono::steady_clock::time_point(std::chrono::nanoseconds(static_cast<std::int64_t>(sequence) * 1000));

    std::vector<std::byte> bytes(descriptor.expectedPayloadBytes(), std::byte{fillValue});
    frame.storage = std::make_shared<const BufferStorage>(std::move(bytes));
    return frame;
}

}  // namespace

int main() {
    const std::string path = "astro_raw_sequence_recorder_test.acraw";

    FrameDescriptor descriptor;
    descriptor.width = 8;
    descriptor.height = 4;
    descriptor.storage = PixelStorage::Bayer8;
    descriptor.bayer = BayerPattern::RGGB;
    descriptor.validBits = 8;

    // Write three frames, closing patches the header's frame count.
    {
        RawSequenceRecorder recorder;
        ASTRO_TEST_CHECK(recorder.open(path, descriptor, 30.0).ok());
        ASTRO_TEST_CHECK(recorder.writeFrame(makeFrame(descriptor, 0, 10)).ok());
        ASTRO_TEST_CHECK(recorder.writeFrame(makeFrame(descriptor, 1, 20)).ok());
        ASTRO_TEST_CHECK(recorder.writeFrame(makeFrame(descriptor, 2, 30)).ok());

        const auto summary = recorder.close();
        ASTRO_TEST_CHECK(summary.ok());
        ASTRO_TEST_CHECK(summary.value().framesWritten == 3);
    }

    // A frame with the wrong payload size is rejected, not silently
    // truncated/padded.
    {
        RawSequenceRecorder recorder;
        ASTRO_TEST_CHECK(recorder.open("astro_raw_sequence_recorder_test_reject.acraw", descriptor, 30.0).ok());
        FrameBuffer wrongSize = makeFrame(descriptor, 0, 1);
        wrongSize.storage = std::make_shared<const BufferStorage>(std::vector<std::byte>(4));
        ASTRO_TEST_CHECK(!recorder.writeFrame(wrongSize).ok());
        recorder.close();
        std::remove("astro_raw_sequence_recorder_test_reject.acraw");
    }

    // Round-trip: the reader must reconstruct the same descriptor, frame
    // count, sequence numbers and payload bytes that were written.
    {
        RawSequenceReader reader;
        const auto header = reader.open(path);
        ASTRO_TEST_CHECK(header.ok());
        ASTRO_TEST_CHECK(header.value().frameCount == 3);
        ASTRO_TEST_CHECK(header.value().descriptor.width == descriptor.width);
        ASTRO_TEST_CHECK(header.value().descriptor.height == descriptor.height);
        ASTRO_TEST_CHECK(header.value().descriptor.storage == descriptor.storage);
        ASTRO_TEST_CHECK(header.value().descriptor.bayer == descriptor.bayer);
        // Two independent clocks read microseconds apart in open() — allow
        // slack, but they must be unmistakably "now", not zero/garbage.
        ASTRO_TEST_CHECK(header.value().anchorSteadyNanos != 0);
        ASTRO_TEST_CHECK(header.value().anchorSystemUnixNanos > 0);

        const std::array<std::uint8_t, 3> expectedFill = {10, 20, 30};
        std::optional<std::int64_t> previousWallClockNanos;
        for (std::uint64_t i = 0; i < 3; ++i) {
            const auto frame = reader.readNextFrame();
            ASTRO_TEST_CHECK(frame.ok());
            ASTRO_TEST_CHECK(frame.value().metadata.sequence == i);
            ASTRO_TEST_CHECK(frame.value().storage->size() == descriptor.expectedPayloadBytes());
            const auto* bytes = frame.value().storage->data();
            for (std::size_t b = 0; b < frame.value().storage->size(); ++b) {
                ASTRO_TEST_CHECK(static_cast<std::uint8_t>(bytes[b]) == expectedFill[i]);
            }

            // frameWallClockUnixNanos() must recover each frame's actual
            // Unix time from its steady_clock hostArrival plus the
            // header's anchors. makeFrame() stamped frame i exactly
            // `i * 1000` steady-clock nanoseconds apart, and that spacing
            // is clock-independent (unlike the absolute value, which this
            // synthetic test's fabricated timestamps don't make
            // meaningful) — so consecutive frames' recovered wall-clock
            // times must be exactly 1000ns apart too.
            const auto wallClockNanos = frameWallClockUnixNanos(header.value(), frame.value().metadata.hostArrival);
            if (previousWallClockNanos) {
                ASTRO_TEST_CHECK(wallClockNanos - *previousWallClockNanos == 1000);
            }
            previousWallClockNanos = wallClockNanos;
        }

        // Past the last frame: a clear failure, not garbage data.
        ASTRO_TEST_CHECK(!reader.readNextFrame().ok());
    }

    std::remove(path.c_str());

    std::cout << "OK\n";
    return 0;
}
