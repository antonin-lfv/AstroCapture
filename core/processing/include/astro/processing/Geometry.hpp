#pragma once
#include <astro/processing/Planetary.hpp>
namespace astro::processing {
struct Geometry {
    bool crop{};
    int x{},y{},width{1},height{1};
    double angle{},scale{1}; // Positive angles rotate clockwise.
    int interpolation{4}; // Nearest, bilinear, bicubic, area, Lanczos-4.
    bool clamp{true},keepSize{};
};
FloatImage transformGeometry(const FloatImage&,const Geometry&,const CheckCancel& = {});
}
