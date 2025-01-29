
#include "DistanceMapping.h"

namespace msdfgen {

DistanceMapping DistanceMapping::inverse(Range range) {
    double rangeWidth = range.upper-range.lower;
    return DistanceMapping(rangeWidth, range.lower/(rangeWidth ? rangeWidth : 1));
}

DistanceMapping::DistanceMapping() : scale(1), translate(0) { }

DistanceMapping::DistanceMapping(Range range) : scale(1/(range.upper-range.lower)), translate(-range.lower) { }

double DistanceMapping::operator()(double d) const {
    return scale*(d+translate);
}

double DistanceMapping::operator()(Delta d) const {
    return scale*d.value;
}

DistanceMapping DistanceMapping::inverse() const {
    return DistanceMapping(1/scale, -scale*translate);
}

}
