#include <array>
#include <astro/recording/SerReader.hpp>
#include <astro/recording/TiffWriter.hpp>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>

#include "Check.hpp"
using astro::recording::SerReader;
namespace {
void put(std::vector<unsigned char>& h, int p, std::uint32_t n) {
    for (int i = 0; i < 4; ++i) h[p + i] = static_cast<unsigned char>(n >> (i * 8));
}
std::vector<unsigned char> header(int color, int bits, int endian = 0) {
    std::vector<unsigned char> h(178, 0);
    const std::string magic = "LUCAM-RECORDER";
    std::copy(magic.begin(), magic.end(), h.begin());
    put(h, 18, color);
    put(h, 22, endian);
    put(h, 26, 2);
    put(h, 30, 2);
    put(h, 34, bits);
    put(h, 38, 2);
    return h;
}
void write(const std::filesystem::path& path, const std::vector<unsigned char>& bytes) {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}
void rejects(const std::function<void()>& operation) {
    bool rejected = false;
    try {
        operation();
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    ASTRO_TEST_CHECK(rejected);
}
}  // namespace
int main() {
    const auto path = std::filesystem::path("reader-validation.ser");
    for (int color : {0, 8, 9, 10, 11, 100, 101})
        for (int bits : {8, 12, 16})
            for (int endian : {0, 1}) {
                auto h = header(color, bits, endian);
                const int channels = color >= 100 ? 3 : 1;
                std::vector<std::uint16_t> expected;
                for (int i = 0; i < 8 * channels; ++i) {
                    const unsigned value = bits == 8 ? i * 3 : i * 127 + 13;
                    expected.push_back(value);
                    if (bits == 8)
                        h.push_back(value);
                    else if (endian) {
                        h.push_back(value >> 8);
                        h.push_back(value & 255);
                    } else {
                        h.push_back(value & 255);
                        h.push_back(value >> 8);
                    }
                }
                if (color == 101)
                    for (std::size_t i = 0; i < expected.size(); i += 3)
                        std::swap(expected[i], expected[i + 2]);
                write(path, h);
                SerReader reader(path);
                ASTRO_TEST_CHECK(reader.count == 2 && reader.width == 2 && reader.height == 2 &&
                                 reader.bits == bits);
                for (int frame : {1, 0, 1}) {
                    const auto values = reader.read(frame);
                    for (int i = 0; i < 4 * channels; ++i)
                        ASTRO_TEST_CHECK(values[i] == expected[frame * 4 * channels + i]);
                }
                rejects([&] { reader.read(2); });
            }
    auto valid = header(0, 8);
    valid.resize(186, 42);
    for (int field : {18, 22, 26, 30, 34, 38}) {
        auto h = valid;
        put(h, field, 0xffffffff);
        write(path, h);
        rejects([&] { SerReader r(path); });
    }
    for (int length : {0, 14, 177, 178, 185, 187, 201}) {
        auto h = valid;
        h.resize(length);
        write(path, h);
        rejects([&] { SerReader r(path); });
    }
    auto timed = valid;
    timed.resize(202);
    write(path, timed);
    {
        SerReader r(path);
        ASTRO_TEST_CHECK(r.read(0).size() == 4);
    }
    auto bad = valid;
    bad[0] = 'X';
    write(path, bad);
    rejects([&] { SerReader r(path); });
    std::filesystem::remove(path);
    const std::array<std::uint16_t, 3> pixel = {13, 12345, 65535};
    ASTRO_TEST_CHECK(astro::recording::writeTiff16Samples("true16.tif", 1, 1, pixel.data()));
    std::ifstream f("true16.tif", std::ios::binary);
    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(f)), {});
    for (int c = 0; c < 3; ++c)
        ASTRO_TEST_CHECK((bytes[bytes.size() - 6 + 2 * c] | (bytes[bytes.size() - 5 + 2 * c] << 8)) ==
                         pixel[c]);
    f.close();
    std::filesystem::remove("true16.tif");
}
