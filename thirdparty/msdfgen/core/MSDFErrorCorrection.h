
#pragma once

#include "SDFTransformation.h"
#include "Shape.h"
#include "BitmapRef.hpp"

namespace msdfgen {

/// Performs error correction on a computed MSDF to eliminate interpolation artifacts. This is a low-level class, you may want to use the API in msdf-error-correction.h instead.
class MSDFErrorCorrection {

public:
    /// Stencil flags.
    enum Flags {
        /// Texel marked as potentially causing interpolation errors.
        ERROR = 1,
        /// Texel marked as protected. Protected texels are only given the error flag if they cause inversion artifacts.
        PROTECTED = 2
    };

    MSDFErrorCorrection();
    explicit MSDFErrorCorrection(const BitmapRef<byte, 1> &stencil, const SDFTransformation &transformation);
    /// Sets the minimum ratio between the actual and maximum expected distance delta to be considered an error.
    void setMinDeviationRatio(double minDeviationRatio);
    /// Sets the minimum ratio between the pre-correction distance error and the post-correction distance error.
    void setMinImproveRatio(double minImproveRatio);
    /// Flags all texels that are interpolated at corners as protected.
    void protectCorners(const Shape &shape);
    /// Flags all texels that contribute to edges as protected.
    template <int N>
    void protectEdges(const BitmapConstRef<float, N> &sdf);
    /// Flags all texels as protected.
    void protectAll();
    /// Flags texels that are expected to cause interpolation artifacts based on analysis of the SDF only.
    template <int N>
    void findErrors(const BitmapConstRef<float, N> &sdf);
    /// Flags texels that are expected to cause interpolation artifacts based on analysis of the SDF and comparison with the exact shape distance.
    template <template <typename> class ContourCombiner, int N>
    void findErrors(const BitmapConstRef<float, N> &sdf, const Shape &shape);
    /// Modifies the MSDF so that all texels with the error flag are converted to single-channel.
    template <int N>
    void apply(const BitmapRef<float, N> &sdf) const;
    /// Returns the stencil in its current state (see Flags).
    BitmapConstRef<byte, 1> getStencil() const;

private:
    BitmapRef<byte, 1> stencil;
    SDFTransformation transformation;
    double minDeviationRatio;
    double minImproveRatio;

};

}
