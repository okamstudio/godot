
#pragma once

#include "Projection.h"
#include "DistanceMapping.h"

namespace msdfgen {

/**
 * Full signed distance field transformation specifies both spatial transformation (Projection)
 * as well as distance value transformation (DistanceMapping).
 */
class SDFTransformation : public Projection {

public:
    DistanceMapping distanceMapping;

    inline SDFTransformation() { }

    inline SDFTransformation(const Projection &projection, const DistanceMapping &distanceMapping) : Projection(projection), distanceMapping(distanceMapping) { }

};

}
