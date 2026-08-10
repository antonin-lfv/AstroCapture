#pragma once

#include <cstddef>
#include <utility>
#include <vector>

namespace astro::domain {

// Immutable owner of raw frame bytes. Consumers hold a
// shared_ptr<const BufferStorage> so a frame can be handed to preview,
// recording and analysis simultaneously without copying, while the pool/SDK
// origin of the bytes stays an implementation detail of the producer.
class BufferStorage {
public:
    explicit BufferStorage(std::vector<std::byte> bytes) : bytes_(std::move(bytes)) {}

    const std::byte* data() const noexcept { return bytes_.data(); }
    std::size_t size() const noexcept { return bytes_.size(); }

private:
    std::vector<std::byte> bytes_;
};

}  // namespace astro::domain
