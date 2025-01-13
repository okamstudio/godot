
#pragma once

#include "edge-segments.h"

namespace msdfgen {

/// Container for a single edge of dynamic type.
class EdgeHolder {

public:
    /// Swaps the edges held by a and b.
    static void swap(EdgeHolder &a, EdgeHolder &b);

    inline EdgeHolder() : edgeSegment() { }
    inline EdgeHolder(EdgeSegment *segment) : edgeSegment(segment) { }
    inline EdgeHolder(Point2 p0, Point2 p1, EdgeColor edgeColor = WHITE) : edgeSegment(EdgeSegment::create(p0, p1, edgeColor)) { }
    inline EdgeHolder(Point2 p0, Point2 p1, Point2 p2, EdgeColor edgeColor = WHITE) : edgeSegment(EdgeSegment::create(p0, p1, p2, edgeColor)) { }
    inline EdgeHolder(Point2 p0, Point2 p1, Point2 p2, Point2 p3, EdgeColor edgeColor = WHITE) : edgeSegment(EdgeSegment::create(p0, p1, p2, p3, edgeColor)) { }
    EdgeHolder(const EdgeHolder &orig);
#ifdef MSDFGEN_USE_CPP11
    EdgeHolder(EdgeHolder &&orig);
#endif
    ~EdgeHolder();
    EdgeHolder &operator=(const EdgeHolder &orig);
#ifdef MSDFGEN_USE_CPP11
    EdgeHolder &operator=(EdgeHolder &&orig);
#endif
    EdgeSegment &operator*();
    const EdgeSegment &operator*() const;
    EdgeSegment *operator->();
    const EdgeSegment *operator->() const;
    operator EdgeSegment *();
    operator const EdgeSegment *() const;

private:
    EdgeSegment *edgeSegment;

};

}
