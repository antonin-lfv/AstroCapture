#pragma once
#include <string>
#include <cstdint>
namespace astro::processing {
// Auto uses Metal only for sufficiently large supported images. Both explicit
// requests and Auto fall back to CPU when Metal cannot complete safely.
enum class ComputePreference { Auto, Cpu, Metal };
enum class ComputeBackend { Cpu, Metal };
struct ComputeReport {
    ComputeBackend backend{ComputeBackend::Cpu};
    std::string fallbackReason;
    std::uint64_t submittedCommands{},finishedCommands{};
};
struct MetalAvailability {
    bool available{};
    std::string device;
    std::string reason;
};
MetalAvailability metalAvailability();
}
