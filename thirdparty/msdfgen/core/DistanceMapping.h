
#pragma once

#include "Range.hpp"

namespace msdfgen {

/// Linear transformation of signed distance values.
class DistanceMapping {

public:
    /// Explicitly designates value as distance delta rather than an absolute distance.
    class Delta {
    public:
        double value;
        inline explicit Delta(double distanceDelta) : value(distanceDelta) { }
        inline operator double() const { return value; }
    };

    static DistanceMapping inverse(Range range);

    DistanceMapping();
    DistanceMapping(Range range);
    double operator()(double d) const;
    double operator()(Delta d) const;
    DistanceMapping inverse() const;

private:
    double scale;
    double translate;

    inline DistanceMapping(double scale, double translate) : scale(scale), translate(translate) { }

};

}
