
#pragma once

#include <vector>
#include "Contour.h"
#include "Scanline.h"

namespace msdfgen {

// Threshold of the dot product of adjacent edge directions to be considered convergent.
#define MSDFGEN_CORNER_DOT_EPSILON .000001

/// Vector shape representation.
class Shape {

public:
    struct Bounds {
        double l, b, r, t;
    };

    /// The list of contours the shape consists of.
    std::vector<Contour> contours;
    /// Specifies whether the shape uses bottom-to-top (false) or top-to-bottom (true) Y coordinates.
    bool inverseYAxis;

    Shape();
    /// Adds a contour.
    void addContour(const Contour &contour);
#ifdef MSDFGEN_USE_CPP11
    void addContour(Contour &&contour);
#endif
    /// Adds a blank contour and returns its reference.
    Contour &addContour();
    /// Normalizes the shape geometry for distance field generation.
    void normalize();
    /// Performs basic checks to determine if the object represents a valid shape.
    bool validate() const;
    /// Adjusts the bounding box to fit the shape.
    void bound(double &l, double &b, double &r, double &t) const;
    /// Adjusts the bounding box to fit the shape border's mitered corners.
    void boundMiters(double &l, double &b, double &r, double &t, double border, double miterLimit, int polarity) const;
    /// Computes the minimum bounding box that fits the shape, optionally with a (mitered) border.
    Bounds getBounds(double border = 0, double miterLimit = 0, int polarity = 0) const;
    /// Outputs the scanline that intersects the shape at y.
    void scanline(Scanline &line, double y) const;
    /// Returns the total number of edge segments
    int edgeCount() const;
    /// Assumes its contours are unoriented (even-odd fill rule). Attempts to orient them to conform to the non-zero winding rule.
    void orientContours();

};

}
