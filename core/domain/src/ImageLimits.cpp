#include <astro/domain/ImageLimits.hpp>
#include <algorithm>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif
#endif
namespace astro::domain {
std::uint64_t physicalMemoryBytes() {
#ifdef _WIN32
    MEMORYSTATUSEX state{};state.dwLength=sizeof(state);
    return GlobalMemoryStatusEx(&state)?state.ullTotalPhys:0;
#elif defined(__APPLE__)
    std::uint64_t bytes=0;std::size_t length=sizeof(bytes);
    return sysctlbyname("hw.memsize",&bytes,&length,nullptr,0)==0?bytes:0;
#else
    const auto pages=sysconf(_SC_PHYS_PAGES),size=sysconf(_SC_PAGESIZE);
    if(pages<=0 || size<=0)return 0;
    return std::uint64_t(pages)*std::uint64_t(size);
#endif
}
std::uint64_t imageMemoryBudget() {
    const auto physical=physicalMemoryBytes();
    return physical?std::min<std::uint64_t>(8ULL*1024*1024*1024,physical/2):2ULL*1024*1024*1024;
}
void requireImageMemory(std::uint64_t bytes) {
    if(bytes>imageMemoryBudget())throw std::runtime_error("This operation exceeds the image memory safety budget. Reduce image size or the active processing options; previous results are preserved.");
}
}
