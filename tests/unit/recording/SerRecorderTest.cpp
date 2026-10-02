#include "astro/recording/SerRecorder.hpp"

#include <array>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

#include "Check.hpp"

using namespace astro::domain;
using astro::recording::SerRecorder;

namespace {

FrameBuffer makeFrame(const FrameDescriptor& descriptor, std::uint64_t sequence, std::uint8_t fillValue) {
    FrameBuffer frame;
    frame.descriptor = descriptor;
    frame.metadata.sequence = sequence;
    // 1ms apart, so the trailer's converted ticks (100ns units) come out
    // to a clean, checkable 10000-tick step between frames.
    frame.metadata.hostArrival =
        std::chrono::steady_clock::time_point(std::chrono::nanoseconds(static_cast<std::int64_t>(sequence) * 1'000'000));

    std::vector<std::byte> bytes(descriptor.expectedPayloadBytes(), std::byte{fillValue});
    frame.storage = std::make_shared<const BufferStorage>(std::move(bytes));
    return frame;
}

std::int32_t readI32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(bytes[offset]) |
                                      (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
                                      (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
                                      (static_cast<std::uint32_t>(bytes[offset + 3]) << 24));
}

std::int64_t readI64(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    std::uint64_t value = 0;
    for (int i = 7; i >= 0; --i) {
        value = (value << 8) | bytes[offset + static_cast<std::size_t>(i)];
    }
    return static_cast<std::int64_t>(value);
}

}  // namespace

int main() {
    const std::string path = "astro_ser_recorder_test.ser";

    FrameDescriptor descriptor;
    descriptor.width = 4;
    descriptor.height = 4;
    descriptor.storage = PixelStorage::Bayer8;
    descriptor.bayer = BayerPattern::RGGB;
    descriptor.validBits = 8;

    {
        SerRecorder recorder;
        ASTRO_TEST_CHECK(recorder.open(path, descriptor, 30.0).ok());
        ASTRO_TEST_CHECK(recorder.writeFrame(makeFrame(descriptor, 0, 10)).ok());
        ASTRO_TEST_CHECK(recorder.writeFrame(makeFrame(descriptor, 1, 20)).ok());
        ASTRO_TEST_CHECK(recorder.writeFrame(makeFrame(descriptor, 2, 30)).ok());

        const auto summary = recorder.close();
        ASTRO_TEST_CHECK(summary.ok());
        ASTRO_TEST_CHECK(summary.value().framesWritten == 3);
    }

    // A frame with the wrong payload size is rejected, not silently
    // truncated/padded — same discipline as RawSequenceRecorder.
    {
        SerRecorder recorder;
        ASTRO_TEST_CHECK(recorder.open("astro_ser_recorder_test_reject.ser", descriptor, 30.0).ok());
        FrameBuffer wrongSize = makeFrame(descriptor, 0, 1);
        wrongSize.storage = std::make_shared<const BufferStorage>(std::vector<std::byte>(4));
        ASTRO_TEST_CHECK(!recorder.writeFrame(wrongSize).ok());
        recorder.close();
        std::remove("astro_ser_recorder_test_reject.ser");
    }

    // A format SER has no color ID for must be rejected at open(), before
    // any frame is written.
    {
        FrameDescriptor unsupported;
        unsupported.width = 4;
        unsupported.height = 4;
        unsupported.storage = PixelStorage::BGRA32;
        SerRecorder recorder;
        ASTRO_TEST_CHECK(!recorder.open("astro_ser_recorder_test_unsupported.ser", unsupported, 30.0).ok());
    }

    // Independently re-parse the written file against the SER
    // specification's documented byte layout, rather than calling back
    // into the writer's own logic.
    std::ifstream file(path, std::ios::binary);
    ASTRO_TEST_CHECK(file.is_open());
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    const std::string fileId(reinterpret_cast<const char*>(bytes.data()), 14);
    ASTRO_TEST_CHECK(fileId == "LUCAM-RECORDER");
    ASTRO_TEST_CHECK(readI32(bytes, 14) == 0);  // LuID
    ASTRO_TEST_CHECK(readI32(bytes, 18) == 8);  // ColorID: BAYER_RGGB
    ASTRO_TEST_CHECK(readI32(bytes, 22) == 0);  // Siril/SER Player little-endian convention
    ASTRO_TEST_CHECK(readI32(bytes, 26) == 4);  // ImageWidth
    ASTRO_TEST_CHECK(readI32(bytes, 30) == 4);  // ImageHeight
    ASTRO_TEST_CHECK(readI32(bytes, 34) == 8);  // PixelDepthPerPlane
    ASTRO_TEST_CHECK(readI32(bytes, 38) == 3);  // FrameCount

    // Observer/Instrument/Telescope: 40 bytes each at 42/82/122.
    // DateTime/DateTime_UTC: int64 at 162/170.
    ASTRO_TEST_CHECK(readI64(bytes, 162) == readI64(bytes, 170));
    ASTRO_TEST_CHECK(readI64(bytes, 162) > 0);

    constexpr std::size_t kHeaderSize = 178;
    constexpr std::size_t kFrameSize = 4 * 4;  // Bayer8: 1 byte/pixel
    const std::array<std::uint8_t, 3> expectedFill = {10, 20, 30};
    for (std::size_t i = 0; i < 3; ++i) {
        const std::size_t frameOffset = kHeaderSize + i * kFrameSize;
        for (std::size_t b = 0; b < kFrameSize; ++b) {
            ASTRO_TEST_CHECK(bytes[frameOffset + b] == expectedFill[i]);
        }
    }

    // Trailer: 3 x int64 UTC .NET ticks. Frames were stamped 1ms apart,
    // i.e. exactly 10000 ticks (100ns units) apart.
    const std::size_t trailerOffset = kHeaderSize + 3 * kFrameSize;
    ASTRO_TEST_CHECK(bytes.size() == trailerOffset + 3 * 8);
    const auto t0 = readI64(bytes, trailerOffset);
    const auto t1 = readI64(bytes, trailerOffset + 8);
    const auto t2 = readI64(bytes, trailerOffset + 16);
    ASTRO_TEST_CHECK(t1 - t0 == 10000);
    ASTRO_TEST_CHECK(t2 - t1 == 10000);

    std::remove(path.c_str());

    // Non-symmetric 16-bit values and padded rows expose byte-order and
    // frame-offset bugs that a uniform Bayer8 fixture cannot detect.
    {
        FrameDescriptor wide;
        wide.width = 2;
        wide.height = 2;
        wide.strideBytes = 6;
        wide.storage = PixelStorage::Mono16;
        const std::string widePath = "astro_ser_padded16_test.ser";
        SerRecorder recorder;
        ASTRO_TEST_CHECK(recorder.open(widePath, wide, 30).ok());
        auto frame = makeFrame(wide, 0, 0);
        frame.storage = std::make_shared<const BufferStorage>(std::vector<std::byte>{
            std::byte{0x34}, std::byte{0x12}, std::byte{0xcd}, std::byte{0xab}, std::byte{0xee}, std::byte{0xee},
            std::byte{0x02}, std::byte{0x01}, std::byte{0xff}, std::byte{0xfe}, std::byte{0xee}, std::byte{0xee}});
        ASTRO_TEST_CHECK(recorder.writeFrame(frame).ok());
        auto changed = frame;
        changed.descriptor.width = 1;  // Same buffer length, different geometry.
        ASTRO_TEST_CHECK(!recorder.writeFrame(changed).ok());
        changed = frame;
        changed.descriptor.storage = PixelStorage::Bayer16;
        changed.descriptor.bayer = BayerPattern::RGGB;
        ASTRO_TEST_CHECK(!recorder.writeFrame(changed).ok());
        ASTRO_TEST_CHECK(recorder.close().ok());
        std::ifstream input(widePath, std::ios::binary);
        const std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(input)), {});
        ASTRO_TEST_CHECK(data.size() == 178 + 8 + 8);
        ASTRO_TEST_CHECK(readI32(data, 22) == 0);
        ASTRO_TEST_CHECK(readI32(data, 34) == 16);
        ASTRO_TEST_CHECK(readI32(data, 38) == 1);
        const std::array<std::uint16_t, 4> expected{0x1234, 0xabcd, 0x0102, 0xfeff};
        for (std::size_t i = 0; i < expected.size(); ++i) {
            const auto sample = data[178 + i * 2] | (data[179 + i * 2] << 8);
            ASTRO_TEST_CHECK(sample == expected[i]);
        }
        input.close();
        std::remove(widePath.c_str());
        wide.validBits = 8;  // Would make readers expect 1 byte/sample.
        ASTRO_TEST_CHECK(!recorder.open(widePath, wide, 30).ok());
        wide.validBits = 16;
        wide.strideBytes = 1;
        ASTRO_TEST_CHECK(!recorder.open(widePath, wide, 30).ok());
        wide.strideBytes = 0;
        wide.height = 0;
        ASTRO_TEST_CHECK(!recorder.open(widePath, wide, 30).ok());
    }
    std::cout << "OK\n";
    return 0;
}
