#include "astro/domain/Result.hpp"

#include "Check.hpp"

using astro::domain::Error;
using astro::domain::ErrorCode;
using astro::domain::Result;

int main() {
    auto ok = Result<int>::success(42);
    ASTRO_TEST_CHECK(ok.ok());
    ASTRO_TEST_CHECK(static_cast<bool>(ok));
    ASTRO_TEST_CHECK(ok.value() == 42);

    auto failed = Result<int>::failure({ErrorCode::DeviceNotFound, "no such device", ""});
    ASTRO_TEST_CHECK(!failed.ok());
    ASTRO_TEST_CHECK(!static_cast<bool>(failed));
    ASTRO_TEST_CHECK(failed.error().code == ErrorCode::DeviceNotFound);
    ASTRO_TEST_CHECK(failed.error().message == "no such device");

    auto voidOk = Result<void>::success();
    ASTRO_TEST_CHECK(voidOk.ok());

    auto voidFailed = Result<void>::failure({ErrorCode::DiskTooSlow, "disk cannot keep up", ""});
    ASTRO_TEST_CHECK(!voidFailed.ok());
    ASTRO_TEST_CHECK(voidFailed.error().code == ErrorCode::DiskTooSlow);

    ASTRO_TEST_CHECK(astro::domain::to_string(ErrorCode::DiskTooSlow) == "DiskTooSlow");

    std::cout << "OK\n";
    return 0;
}
