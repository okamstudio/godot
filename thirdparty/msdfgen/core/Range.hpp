
#pragma once

#include "base.h"

namespace msdfgen {

/**
 * Represents the range between two real values.
 * For example, the range of representable signed distances.
 */
struct Range {

    double lower, upper;

    inline Range(double symmetricalWidth = 0) : lower(-.5*symmetricalWidth), upper(.5*symmetricalWidth) { }

    inline Range(double lowerBound, double upperBound) : lower(lowerBound), upper(upperBound) { }

    inline Range &operator*=(double factor) {
        lower *= factor;
        upper *= factor;
        return *this;
    }

    inline Range &operator/=(double divisor) {
        lower /= divisor;
        upper /= divisor;
        return *this;
    }

    inline Range operator*(double factor) const {
        return Range(lower*factor, upper*factor);
    }

    inline Range operator/(double divisor) const {
        return Range(lower/divisor, upper/divisor);
    }

};

inline Range operator*(double factor, const Range &range) {
    return Range(factor*range.lower, factor*range.upper);
}

}
