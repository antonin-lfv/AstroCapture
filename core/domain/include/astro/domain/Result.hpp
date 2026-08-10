#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace astro::domain {

// Machine-readable error categories. Vendor-specific codes are attached as
// diagnostic context on Error, never mapped 1:1 into this enum.
enum class ErrorCode {
    DeviceNotFound,
    DeviceBusy,
    PermissionDenied,
    UnsupportedFormat,
    UnsupportedControl,
    StreamStartFailed,
    DeviceDisconnected,
    InvalidFrame,
    RecordingOpenFailed,
    DiskWriteFailed,
    DiskTooSlow,
    ModelLoadFailed,
    InternalInvariantViolation,
};

std::string_view to_string(ErrorCode code);

struct Error {
    ErrorCode code;
    std::string message;
    // Optional vendor/SDK-specific detail (e.g. a raw SDK error code), kept
    // separate from `code` so cross-project logic never depends on it.
    std::string diagnosticContext;
};

// A lightweight expected<T, Error> substitute so the domain layer does not
// need to pull in a third-party dependency to express fallible operations.
template <typename T>
class Result {
public:
    static Result success(T value) { return Result(std::in_place_index<0>, std::move(value)); }
    static Result failure(Error error) { return Result(std::in_place_index<1>, std::move(error)); }

    bool ok() const { return data_.index() == 0; }
    explicit operator bool() const { return ok(); }

    const T& value() const& { return std::get<0>(data_); }
    T& value() & { return std::get<0>(data_); }
    T&& value() && { return std::get<0>(std::move(data_)); }

    const Error& error() const& { return std::get<1>(data_); }

private:
    Result(std::in_place_index_t<0>, T value) : data_(std::in_place_index<0>, std::move(value)) {}
    Result(std::in_place_index_t<1>, Error error) : data_(std::in_place_index<1>, std::move(error)) {}

    std::variant<T, Error> data_;
};

template <>
class Result<void> {
public:
    static Result success() { return Result(std::nullopt); }
    static Result failure(Error error) { return Result(std::move(error)); }

    bool ok() const { return !error_.has_value(); }
    explicit operator bool() const { return ok(); }

    const Error& error() const& { return *error_; }

private:
    explicit Result(std::optional<Error> error) : error_(std::move(error)) {}

    std::optional<Error> error_;
};

}  // namespace astro::domain
