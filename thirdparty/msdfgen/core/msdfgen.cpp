
#include "../msdfgen.h"

#include <vector>
#include "edge-selectors.h"
#include "contour-combiners.h"
#include "ShapeDistanceFinder.h"

namespace msdfgen {

template <typename DistanceType>
class DistancePixelConversion;

template <>
class DistancePixelConversion<double> {
    DistanceMapping mapping;
public:
    typedef BitmapRef<float, 1> BitmapRefType;
    inline explicit DistancePixelConversion(DistanceMapping mapping) : mapping(mapping) { }
    inline void operator()(float *pixels, double distance) const {
        *pixels = float(mapping(distance));
    }
};

template <>
class DistancePixelConversion<MultiDistance> {
    DistanceMapping mapping;
public:
    typedef BitmapRef<float, 3> BitmapRefType;
    inline explicit DistancePixelConversion(DistanceMapping mapping) : mapping(mapping) { }
    inline void operator()(float *pixels, const MultiDistance &distance) const {
        pixels[0] = float(mapping(distance.r));
        pixels[1] = float(mapping(distance.g));
        pixels[2] = float(mapping(distance.b));
    }
};

template <>
class DistancePixelConversion<MultiAndTrueDistance> {
    DistanceMapping mapping;
public:
    typedef BitmapRef<float, 4> BitmapRefType;
    inline explicit DistancePixelConversion(DistanceMapping mapping) : mapping(mapping) { }
    inline void operator()(float *pixels, const MultiAndTrueDistance &distance) const {
        pixels[0] = float(mapping(distance.r));
        pixels[1] = float(mapping(distance.g));
        pixels[2] = float(mapping(distance.b));
        pixels[3] = float(mapping(distance.a));
    }
};

template <class ContourCombiner>
void generateDistanceField(const typename DistancePixelConversion<typename ContourCombiner::DistanceType>::BitmapRefType &output, const Shape &shape, const SDFTransformation &transformation) {
    DistancePixelConversion<typename ContourCombiner::DistanceType> distancePixelConversion(transformation.distanceMapping);
#ifdef MSDFGEN_USE_OPENMP
    #pragma omp parallel
#endif
    {
        ShapeDistanceFinder<ContourCombiner> distanceFinder(shape);
        bool rightToLeft = false;
#ifdef MSDFGEN_USE_OPENMP
        #pragma omp for
#endif
        for (int y = 0; y < output.height; ++y) {
            int row = shape.inverseYAxis ? output.height-y-1 : y;
            for (int col = 0; col < output.width; ++col) {
                int x = rightToLeft ? output.width-col-1 : col;
                Point2 p = transformation.unproject(Point2(x+.5, y+.5));
                typename ContourCombiner::DistanceType distance = distanceFinder.distance(p);
                distancePixelConversion(output(x, row), distance);
            }
            rightToLeft = !rightToLeft;
        }
    }
}

void generateSDF(const BitmapRef<float, 1> &output, const Shape &shape, const SDFTransformation &transformation, const GeneratorConfig &config) {
    if (config.overlapSupport)
        generateDistanceField<OverlappingContourCombiner<TrueDistanceSelector> >(output, shape, transformation);
    else
        generateDistanceField<SimpleContourCombiner<TrueDistanceSelector> >(output, shape, transformation);
}

void generatePSDF(const BitmapRef<float, 1> &output, const Shape &shape, const SDFTransformation &transformation, const GeneratorConfig &config) {
    if (config.overlapSupport)
        generateDistanceField<OverlappingContourCombiner<PerpendicularDistanceSelector> >(output, shape, transformation);
    else
        generateDistanceField<SimpleContourCombiner<PerpendicularDistanceSelector> >(output, shape, transformation);
}

void generateMSDF(const BitmapRef<float, 3> &output, const Shape &shape, const SDFTransformation &transformation, const MSDFGeneratorConfig &config) {
    if (config.overlapSupport)
        generateDistanceField<OverlappingContourCombiner<MultiDistanceSelector> >(output, shape, transformation);
    else
        generateDistanceField<SimpleContourCombiner<MultiDistanceSelector> >(output, shape, transformation);
    msdfErrorCorrection(output, shape, transformation, config);
}

void generateMTSDF(const BitmapRef<float, 4> &output, const Shape &shape, const SDFTransformation &transformation, const MSDFGeneratorConfig &config) {
    if (config.overlapSupport)
        generateDistanceField<OverlappingContourCombiner<MultiAndTrueDistanceSelector> >(output, shape, transformation);
    else
        generateDistanceField<SimpleContourCombiner<MultiAndTrueDistanceSelector> >(output, shape, transformation);
    msdfErrorCorrection(output, shape, transformation, config);
}

void generateSDF(const BitmapRef<float, 1> &output, const Shape &shape, const Projection &projection, Range range, const GeneratorConfig &config) {
    if (config.overlapSupport)
        generateDistanceField<OverlappingContourCombiner<TrueDistanceSelector> >(output, shape, SDFTransformation(projection, range));
    else
        generateDistanceField<SimpleContourCombiner<TrueDistanceSelector> >(output, shape, SDFTransformation(projection, range));
}

void generatePSDF(const BitmapRef<float, 1> &output, const Shape &shape, const Projection &projection, Range range, const GeneratorConfig &config) {
    if (config.overlapSupport)
        generateDistanceField<OverlappingContourCombiner<PerpendicularDistanceSelector> >(output, shape, SDFTransformation(projection, range));
    else
        generateDistanceField<SimpleContourCombiner<PerpendicularDistanceSelector> >(output, shape, SDFTransformation(projection, range));
}

void generateMSDF(const BitmapRef<float, 3> &output, const Shape &shape, const Projection &projection, Range range, const MSDFGeneratorConfig &config) {
    if (config.overlapSupport)
        generateDistanceField<OverlappingContourCombiner<MultiDistanceSelector> >(output, shape, SDFTransformation(projection, range));
    else
        generateDistanceField<SimpleContourCombiner<MultiDistanceSelector> >(output, shape, SDFTransformation(projection, range));
    msdfErrorCorrection(output, shape, SDFTransformation(projection, range), config);
}

void generateMTSDF(const BitmapRef<float, 4> &output, const Shape &shape, const Projection &projection, Range range, const MSDFGeneratorConfig &config) {
    if (config.overlapSupport)
        generateDistanceField<OverlappingContourCombiner<MultiAndTrueDistanceSelector> >(output, shape, SDFTransformation(projection, range));
    else
        generateDistanceField<SimpleContourCombiner<MultiAndTrueDistanceSelector> >(output, shape, SDFTransformation(projection, range));
    msdfErrorCorrection(output, shape, SDFTransformation(projection, range), config);
}

// Legacy API

void generatePseudoSDF(const BitmapRef<float, 1> &output, const Shape &shape, const Projection &projection, Range range, const GeneratorConfig &config) {
    generatePSDF(output, shape, SDFTransformation(projection, range), config);
}

void generateSDF(const BitmapRef<float, 1> &output, const Shape &shape, Range range, const Vector2 &scale, const Vector2 &translate, bool overlapSupport) {
    generateSDF(output, shape, Projection(scale, translate), range, GeneratorConfig(overlapSupport));
}

void generatePSDF(const BitmapRef<float, 1> &output, const Shape &shape, Range range, const Vector2 &scale, const Vector2 &translate, bool overlapSupport) {
    generatePSDF(output, shape, Projection(scale, translate), range, GeneratorConfig(overlapSupport));
}

void generatePseudoSDF(const BitmapRef<float, 1> &output, const Shape &shape, Range range, const Vector2 &scale, const Vector2 &translate, bool overlapSupport) {
    generatePSDF(output, shape, Projection(scale, translate), range, GeneratorConfig(overlapSupport));
}

void generateMSDF(const BitmapRef<float, 3> &output, const Shape &shape, Range range, const Vector2 &scale, const Vector2 &translate, const ErrorCorrectionConfig &errorCorrectionConfig, bool overlapSupport) {
    generateMSDF(output, shape, Projection(scale, translate), range, MSDFGeneratorConfig(overlapSupport, errorCorrectionConfig));
}

void generateMTSDF(const BitmapRef<float, 4> &output, const Shape &shape, Range range, const Vector2 &scale, const Vector2 &translate, const ErrorCorrectionConfig &errorCorrectionConfig, bool overlapSupport) {
    generateMTSDF(output, shape, Projection(scale, translate), range, MSDFGeneratorConfig(overlapSupport, errorCorrectionConfig));
}

// Legacy version

void generateSDF_legacy(const BitmapRef<float, 1> &output, const Shape &shape, Range range, const Vector2 &scale, const Vector2 &translate) {
    DistanceMapping distanceMapping(range);
#ifdef MSDFGEN_USE_OPENMP
    #pragma omp parallel for
#endif
    for (int y = 0; y < output.height; ++y) {
        int row = shape.inverseYAxis ? output.height-y-1 : y;
        for (int x = 0; x < output.width; ++x) {
            double dummy;
            Point2 p = Vector2(x+.5, y+.5)/scale-translate;
            SignedDistance minDistance;
            for (std::vector<Contour>::const_iterator contour = shape.contours.begin(); contour != shape.contours.end(); ++contour)
                for (std::vector<EdgeHolder>::const_iterator edge = contour->edges.begin(); edge != contour->edges.end(); ++edge) {
                    SignedDistance distance = (*edge)->signedDistance(p, dummy);
                    if (distance < minDistance)
                        minDistance = distance;
                }
            *output(x, row) = float(distanceMapping(minDistance.distance));
        }
    }
}

void generatePSDF_legacy(const BitmapRef<float, 1> &output, const Shape &shape, Range range, const Vector2 &scale, const Vector2 &translate) {
    DistanceMapping distanceMapping(range);
#ifdef MSDFGEN_USE_OPENMP
    #pragma omp parallel for
#endif
    for (int y = 0; y < output.height; ++y) {
        int row = shape.inverseYAxis ? output.height-y-1 : y;
        for (int x = 0; x < output.width; ++x) {
            Point2 p = Vector2(x+.5, y+.5)/scale-translate;
            SignedDistance minDistance;
            const EdgeHolder *nearEdge = NULL;
            double nearParam = 0;
            for (std::vector<Contour>::const_iterator contour = shape.contours.begin(); contour != shape.contours.end(); ++contour)
                for (std::vector<EdgeHolder>::const_iterator edge = contour->edges.begin(); edge != contour->edges.end(); ++edge) {
                    double param;
                    SignedDistance distance = (*edge)->signedDistance(p, param);
                    if (distance < minDistance) {
                        minDistance = distance;
                        nearEdge = &*edge;
                        nearParam = param;
                    }
                }
            if (nearEdge)
                (*nearEdge)->distanceToPerpendicularDistance(minDistance, p, nearParam);
            *output(x, row) = float(distanceMapping(minDistance.distance));
        }
    }
}

void generatePseudoSDF_legacy(const BitmapRef<float, 1> &output, const Shape &shape, Range range, const Vector2 &scale, const Vector2 &translate) {
    generatePSDF_legacy(output, shape, range, scale, translate);
}

void generateMSDF_legacy(const BitmapRef<float, 3> &output, const Shape &shape, Range range, const Vector2 &scale, const Vector2 &translate, ErrorCorrectionConfig errorCorrectionConfig) {
    DistanceMapping distanceMapping(range);
#ifdef MSDFGEN_USE_OPENMP
    #pragma omp parallel for
#endif
    for (int y = 0; y < output.height; ++y) {
        int row = shape.inverseYAxis ? output.height-y-1 : y;
        for (int x = 0; x < output.width; ++x) {
            Point2 p = Vector2(x+.5, y+.5)/scale-translate;

            struct {
                SignedDistance minDistance;
                const EdgeHolder *nearEdge;
                double nearParam;
            } r, g, b;
            r.nearEdge = g.nearEdge = b.nearEdge = NULL;
            r.nearParam = g.nearParam = b.nearParam = 0;

            for (std::vector<Contour>::const_iterator contour = shape.contours.begin(); contour != shape.contours.end(); ++contour)
                for (std::vector<EdgeHolder>::const_iterator edge = contour->edges.begin(); edge != contour->edges.end(); ++edge) {
                    double param;
                    SignedDistance distance = (*edge)->signedDistance(p, param);
                    if ((*edge)->color&RED && distance < r.minDistance) {
                        r.minDistance = distance;
                        r.nearEdge = &*edge;
                        r.nearParam = param;
                    }
                    if ((*edge)->color&GREEN && distance < g.minDistance) {
                        g.minDistance = distance;
                        g.nearEdge = &*edge;
                        g.nearParam = param;
                    }
                    if ((*edge)->color&BLUE && distance < b.minDistance) {
                        b.minDistance = distance;
                        b.nearEdge = &*edge;
                        b.nearParam = param;
                    }
                }

            if (r.nearEdge)
                (*r.nearEdge)->distanceToPerpendicularDistance(r.minDistance, p, r.nearParam);
            if (g.nearEdge)
                (*g.nearEdge)->distanceToPerpendicularDistance(g.minDistance, p, g.nearParam);
            if (b.nearEdge)
                (*b.nearEdge)->distanceToPerpendicularDistance(b.minDistance, p, b.nearParam);
            output(x, row)[0] = float(distanceMapping(r.minDistance.distance));
            output(x, row)[1] = float(distanceMapping(g.minDistance.distance));
            output(x, row)[2] = float(distanceMapping(b.minDistance.distance));
        }
    }

    errorCorrectionConfig.distanceCheckMode = ErrorCorrectionConfig::DO_NOT_CHECK_DISTANCE;
    msdfErrorCorrection(output, shape, Projection(scale, translate), range, MSDFGeneratorConfig(false, errorCorrectionConfig));
}

void generateMTSDF_legacy(const BitmapRef<float, 4> &output, const Shape &shape, Range range, const Vector2 &scale, const Vector2 &translate, ErrorCorrectionConfig errorCorrectionConfig) {
    DistanceMapping distanceMapping(range);
#ifdef MSDFGEN_USE_OPENMP
    #pragma omp parallel for
#endif
    for (int y = 0; y < output.height; ++y) {
        int row = shape.inverseYAxis ? output.height-y-1 : y;
        for (int x = 0; x < output.width; ++x) {
            Point2 p = Vector2(x+.5, y+.5)/scale-translate;

            SignedDistance minDistance;
            struct {
                SignedDistance minDistance;
                const EdgeHolder *nearEdge;
                double nearParam;
            } r, g, b;
            r.nearEdge = g.nearEdge = b.nearEdge = NULL;
            r.nearParam = g.nearParam = b.nearParam = 0;

            for (std::vector<Contour>::const_iterator contour = shape.contours.begin(); contour != shape.contours.end(); ++contour)
                for (std::vector<EdgeHolder>::const_iterator edge = contour->edges.begin(); edge != contour->edges.end(); ++edge) {
                    double param;
                    SignedDistance distance = (*edge)->signedDistance(p, param);
                    if (distance < minDistance)
                        minDistance = distance;
                    if ((*edge)->color&RED && distance < r.minDistance) {
                        r.minDistance = distance;
                        r.nearEdge = &*edge;
                        r.nearParam = param;
                    }
                    if ((*edge)->color&GREEN && distance < g.minDistance) {
                        g.minDistance = distance;
                        g.nearEdge = &*edge;
                        g.nearParam = param;
                    }
                    if ((*edge)->color&BLUE && distance < b.minDistance) {
                        b.minDistance = distance;
                        b.nearEdge = &*edge;
                        b.nearParam = param;
                    }
                }

            if (r.nearEdge)
                (*r.nearEdge)->distanceToPerpendicularDistance(r.minDistance, p, r.nearParam);
            if (g.nearEdge)
                (*g.nearEdge)->distanceToPerpendicularDistance(g.minDistance, p, g.nearParam);
            if (b.nearEdge)
                (*b.nearEdge)->distanceToPerpendicularDistance(b.minDistance, p, b.nearParam);
            output(x, row)[0] = float(distanceMapping(r.minDistance.distance));
            output(x, row)[1] = float(distanceMapping(g.minDistance.distance));
            output(x, row)[2] = float(distanceMapping(b.minDistance.distance));
            output(x, row)[3] = float(distanceMapping(minDistance.distance));
        }
    }

    errorCorrectionConfig.distanceCheckMode = ErrorCorrectionConfig::DO_NOT_CHECK_DISTANCE;
    msdfErrorCorrection(output, shape, Projection(scale, translate), range, MSDFGeneratorConfig(false, errorCorrectionConfig));
}

}
