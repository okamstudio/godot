
#include "Shape.h"

#include <cstdlib>
#include "arithmetics.hpp"

#define DECONVERGE_OVERSHOOT 1.11111111111111111 // moves control points slightly more than necessary to account for floating-point errors

namespace msdfgen {

Shape::Shape() : inverseYAxis(false) { }

void Shape::addContour(const Contour &contour) {
    contours.push_back(contour);
}

#ifdef MSDFGEN_USE_CPP11
void Shape::addContour(Contour &&contour) {
    contours.push_back((Contour &&) contour);
}
#endif

Contour &Shape::addContour() {
    contours.resize(contours.size()+1);
    return contours.back();
}

bool Shape::validate() const {
    for (std::vector<Contour>::const_iterator contour = contours.begin(); contour != contours.end(); ++contour) {
        if (!contour->edges.empty()) {
            Point2 corner = contour->edges.back()->point(1);
            for (std::vector<EdgeHolder>::const_iterator edge = contour->edges.begin(); edge != contour->edges.end(); ++edge) {
                if (!*edge)
                    return false;
                if ((*edge)->point(0) != corner)
                    return false;
                corner = (*edge)->point(1);
            }
        }
    }
    return true;
}

static void deconvergeEdge(EdgeHolder &edgeHolder, int param, Vector2 vector) {
    switch (edgeHolder->type()) {
        case (int) QuadraticSegment::EDGE_TYPE:
            edgeHolder = static_cast<const QuadraticSegment *>(&*edgeHolder)->convertToCubic();
            // fallthrough
        case (int) CubicSegment::EDGE_TYPE:
            {
                Point2 *p = static_cast<CubicSegment *>(&*edgeHolder)->p;
                switch (param) {
                    case 0:
                        p[1] += (p[1]-p[0]).length()*vector;
                        break;
                    case 1:
                        p[2] += (p[2]-p[3]).length()*vector;
                        break;
                }
            }
    }
}

void Shape::normalize() {
    for (std::vector<Contour>::iterator contour = contours.begin(); contour != contours.end(); ++contour) {
        if (contour->edges.size() == 1) {
            EdgeSegment *parts[3] = { };
            contour->edges[0]->splitInThirds(parts[0], parts[1], parts[2]);
            contour->edges.clear();
            contour->edges.push_back(EdgeHolder(parts[0]));
            contour->edges.push_back(EdgeHolder(parts[1]));
            contour->edges.push_back(EdgeHolder(parts[2]));
        } else {
            // Push apart convergent edge segments
            EdgeHolder *prevEdge = &contour->edges.back();
            for (std::vector<EdgeHolder>::iterator edge = contour->edges.begin(); edge != contour->edges.end(); ++edge) {
                Vector2 prevDir = (*prevEdge)->direction(1).normalize();
                Vector2 curDir = (*edge)->direction(0).normalize();
                if (dotProduct(prevDir, curDir) < MSDFGEN_CORNER_DOT_EPSILON-1) {
                    double factor = DECONVERGE_OVERSHOOT*sqrt(1-(MSDFGEN_CORNER_DOT_EPSILON-1)*(MSDFGEN_CORNER_DOT_EPSILON-1))/(MSDFGEN_CORNER_DOT_EPSILON-1);
                    Vector2 axis = factor*(curDir-prevDir).normalize();
                    // Determine curve ordering using third-order derivative (t = 0) of crossProduct((*prevEdge)->point(1-t)-p0, (*edge)->point(t)-p0) where p0 is the corner (*edge)->point(0)
                    if (crossProduct((*prevEdge)->directionChange(1), (*edge)->direction(0))+crossProduct((*edge)->directionChange(0), (*prevEdge)->direction(1)) < 0)
                        axis = -axis;
                    deconvergeEdge(*prevEdge, 1, axis.getOrthogonal(true));
                    deconvergeEdge(*edge, 0, axis.getOrthogonal(false));
                }
                prevEdge = &*edge;
            }
        }
    }
}

void Shape::bound(double &l, double &b, double &r, double &t) const {
    for (std::vector<Contour>::const_iterator contour = contours.begin(); contour != contours.end(); ++contour)
        contour->bound(l, b, r, t);
}

void Shape::boundMiters(double &l, double &b, double &r, double &t, double border, double miterLimit, int polarity) const {
    for (std::vector<Contour>::const_iterator contour = contours.begin(); contour != contours.end(); ++contour)
        contour->boundMiters(l, b, r, t, border, miterLimit, polarity);
}

Shape::Bounds Shape::getBounds(double border, double miterLimit, int polarity) const {
    static const double LARGE_VALUE = 1e240;
    Shape::Bounds bounds = { +LARGE_VALUE, +LARGE_VALUE, -LARGE_VALUE, -LARGE_VALUE };
    bound(bounds.l, bounds.b, bounds.r, bounds.t);
    if (border > 0) {
        bounds.l -= border, bounds.b -= border;
        bounds.r += border, bounds.t += border;
        if (miterLimit > 0)
            boundMiters(bounds.l, bounds.b, bounds.r, bounds.t, border, miterLimit, polarity);
    }
    return bounds;
}

void Shape::scanline(Scanline &line, double y) const {
    std::vector<Scanline::Intersection> intersections;
    double x[3];
    int dy[3];
    for (std::vector<Contour>::const_iterator contour = contours.begin(); contour != contours.end(); ++contour) {
        for (std::vector<EdgeHolder>::const_iterator edge = contour->edges.begin(); edge != contour->edges.end(); ++edge) {
            int n = (*edge)->scanlineIntersections(x, dy, y);
            for (int i = 0; i < n; ++i) {
                Scanline::Intersection intersection = { x[i], dy[i] };
                intersections.push_back(intersection);
            }
        }
    }
#ifdef MSDFGEN_USE_CPP11
    line.setIntersections((std::vector<Scanline::Intersection> &&) intersections);
#else
    line.setIntersections(intersections);
#endif
}

int Shape::edgeCount() const {
    int total = 0;
    for (std::vector<Contour>::const_iterator contour = contours.begin(); contour != contours.end(); ++contour)
        total += (int) contour->edges.size();
    return total;
}

void Shape::orientContours() {
    struct Intersection {
        double x;
        int direction;
        int contourIndex;

        static int compare(const void *a, const void *b) {
            return sign(reinterpret_cast<const Intersection *>(a)->x-reinterpret_cast<const Intersection *>(b)->x);
        }
    };

    const double ratio = .5*(sqrt(5)-1); // an irrational number to minimize chance of intersecting a corner or other point of interest
    std::vector<int> orientations(contours.size());
    std::vector<Intersection> intersections;
    for (int i = 0; i < (int) contours.size(); ++i) {
        if (!orientations[i] && !contours[i].edges.empty()) {
            // Find an Y that crosses the contour
            double y0 = contours[i].edges.front()->point(0).y;
            double y1 = y0;
            for (std::vector<EdgeHolder>::const_iterator edge = contours[i].edges.begin(); edge != contours[i].edges.end() && y0 == y1; ++edge)
                y1 = (*edge)->point(1).y;
            for (std::vector<EdgeHolder>::const_iterator edge = contours[i].edges.begin(); edge != contours[i].edges.end() && y0 == y1; ++edge)
                y1 = (*edge)->point(ratio).y; // in case all endpoints are in a horizontal line
            double y = mix(y0, y1, ratio);
            // Scanline through whole shape at Y
            double x[3];
            int dy[3];
            for (int j = 0; j < (int) contours.size(); ++j) {
                for (std::vector<EdgeHolder>::const_iterator edge = contours[j].edges.begin(); edge != contours[j].edges.end(); ++edge) {
                    int n = (*edge)->scanlineIntersections(x, dy, y);
                    for (int k = 0; k < n; ++k) {
                        Intersection intersection = { x[k], dy[k], j };
                        intersections.push_back(intersection);
                    }
                }
            }
            if (!intersections.empty()) {
                qsort(&intersections[0], intersections.size(), sizeof(Intersection), &Intersection::compare);
                // Disqualify multiple intersections
                for (int j = 1; j < (int) intersections.size(); ++j)
                    if (intersections[j].x == intersections[j-1].x)
                        intersections[j].direction = intersections[j-1].direction = 0;
                // Inspect scanline and deduce orientations of intersected contours
                for (int j = 0; j < (int) intersections.size(); ++j)
                    if (intersections[j].direction)
                        orientations[intersections[j].contourIndex] += 2*((j&1)^(intersections[j].direction > 0))-1;
                intersections.clear();
            }
        }
    }
    // Reverse contours that have the opposite orientation
    for (int i = 0; i < (int) contours.size(); ++i)
        if (orientations[i] < 0)
            contours[i].reverse();
}

}
