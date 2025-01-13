
#include "edge-coloring.h"

#include <cstdlib>
#include <cmath>
#include <cstring>
#include <cfloat>
#include <vector>
#include <queue>
#include "arithmetics.hpp"

namespace msdfgen {

/**
 * For each position < n, this function will return -1, 0, or 1,
 * depending on whether the position is closer to the beginning, middle, or end, respectively.
 * It is guaranteed that the output will be balanced in that the total for positions 0 through n-1 will be zero.
 */
static int symmetricalTrichotomy(int position, int n) {
    return int(3+2.875*position/(n-1)-1.4375+.5)-3;
}

static bool isCorner(const Vector2 &aDir, const Vector2 &bDir, double crossThreshold) {
    return dotProduct(aDir, bDir) <= 0 || fabs(crossProduct(aDir, bDir)) > crossThreshold;
}

static double estimateEdgeLength(const EdgeSegment *edge) {
    double len = 0;
    Point2 prev = edge->point(0);
    for (int i = 1; i <= MSDFGEN_EDGE_LENGTH_PRECISION; ++i) {
        Point2 cur = edge->point(1./MSDFGEN_EDGE_LENGTH_PRECISION*i);
        len += (cur-prev).length();
        prev = cur;
    }
    return len;
}

static int seedExtract2(unsigned long long &seed) {
    int v = int(seed)&1;
    seed >>= 1;
    return v;
}

static int seedExtract3(unsigned long long &seed) {
    int v = int(seed%3);
    seed /= 3;
    return v;
}

static EdgeColor initColor(unsigned long long &seed) {
    static const EdgeColor colors[3] = { CYAN, MAGENTA, YELLOW };
    return colors[seedExtract3(seed)];
}

static void switchColor(EdgeColor &color, unsigned long long &seed) {
    int shifted = color<<(1+seedExtract2(seed));
    color = EdgeColor((shifted|shifted>>3)&WHITE);
}

static void switchColor(EdgeColor &color, unsigned long long &seed, EdgeColor banned) {
    EdgeColor combined = EdgeColor(color&banned);
    if (combined == RED || combined == GREEN || combined == BLUE)
        color = EdgeColor(combined^WHITE);
    else
        switchColor(color, seed);
}

void edgeColoringSimple(Shape &shape, double angleThreshold, unsigned long long seed) {
    double crossThreshold = sin(angleThreshold);
    EdgeColor color = initColor(seed);
    std::vector<int> corners;
    for (std::vector<Contour>::iterator contour = shape.contours.begin(); contour != shape.contours.end(); ++contour) {
        if (contour->edges.empty())
            continue;
        { // Identify corners
            corners.clear();
            Vector2 prevDirection = contour->edges.back()->direction(1);
            int index = 0;
            for (std::vector<EdgeHolder>::const_iterator edge = contour->edges.begin(); edge != contour->edges.end(); ++edge, ++index) {
                if (isCorner(prevDirection.normalize(), (*edge)->direction(0).normalize(), crossThreshold))
                    corners.push_back(index);
                prevDirection = (*edge)->direction(1);
            }
        }

        // Smooth contour
        if (corners.empty()) {
            switchColor(color, seed);
            for (std::vector<EdgeHolder>::iterator edge = contour->edges.begin(); edge != contour->edges.end(); ++edge)
                (*edge)->color = color;
        }
        // "Teardrop" case
        else if (corners.size() == 1) {
            EdgeColor colors[3];
            switchColor(color, seed);
            colors[0] = color;
            colors[1] = WHITE;
            switchColor(color, seed);
            colors[2] = color;
            int corner = corners[0];
            if (contour->edges.size() >= 3) {
                int m = (int) contour->edges.size();
                for (int i = 0; i < m; ++i)
                    contour->edges[(corner+i)%m]->color = colors[1+symmetricalTrichotomy(i, m)];
            } else if (contour->edges.size() >= 1) {
                // Less than three edge segments for three colors => edges must be split
                EdgeSegment *parts[7] = { };
                contour->edges[0]->splitInThirds(parts[0+3*corner], parts[1+3*corner], parts[2+3*corner]);
                if (contour->edges.size() >= 2) {
                    contour->edges[1]->splitInThirds(parts[3-3*corner], parts[4-3*corner], parts[5-3*corner]);
                    parts[0]->color = parts[1]->color = colors[0];
                    parts[2]->color = parts[3]->color = colors[1];
                    parts[4]->color = parts[5]->color = colors[2];
                } else {
                    parts[0]->color = colors[0];
                    parts[1]->color = colors[1];
                    parts[2]->color = colors[2];
                }
                contour->edges.clear();
                for (int i = 0; parts[i]; ++i)
                    contour->edges.push_back(EdgeHolder(parts[i]));
            }
        }
        // Multiple corners
        else {
            int cornerCount = (int) corners.size();
            int spline = 0;
            int start = corners[0];
            int m = (int) contour->edges.size();
            switchColor(color, seed);
            EdgeColor initialColor = color;
            for (int i = 0; i < m; ++i) {
                int index = (start+i)%m;
                if (spline+1 < cornerCount && corners[spline+1] == index) {
                    ++spline;
                    switchColor(color, seed, EdgeColor((spline == cornerCount-1)*initialColor));
                }
                contour->edges[index]->color = color;
            }
        }
    }
}

struct EdgeColoringInkTrapCorner {
    int index;
    double prevEdgeLengthEstimate;
    bool minor;
    EdgeColor color;
};

void edgeColoringInkTrap(Shape &shape, double angleThreshold, unsigned long long seed) {
    typedef EdgeColoringInkTrapCorner Corner;
    double crossThreshold = sin(angleThreshold);
    EdgeColor color = initColor(seed);
    std::vector<Corner> corners;
    for (std::vector<Contour>::iterator contour = shape.contours.begin(); contour != shape.contours.end(); ++contour) {
        if (contour->edges.empty())
            continue;
        double splineLength = 0;
        { // Identify corners
            corners.clear();
            Vector2 prevDirection = contour->edges.back()->direction(1);
            int index = 0;
            for (std::vector<EdgeHolder>::const_iterator edge = contour->edges.begin(); edge != contour->edges.end(); ++edge, ++index) {
                if (isCorner(prevDirection.normalize(), (*edge)->direction(0).normalize(), crossThreshold)) {
                    Corner corner = { index, splineLength };
                    corners.push_back(corner);
                    splineLength = 0;
                }
                splineLength += estimateEdgeLength(*edge);
                prevDirection = (*edge)->direction(1);
            }
        }

        // Smooth contour
        if (corners.empty()) {
            switchColor(color, seed);
            for (std::vector<EdgeHolder>::iterator edge = contour->edges.begin(); edge != contour->edges.end(); ++edge)
                (*edge)->color = color;
        }
        // "Teardrop" case
        else if (corners.size() == 1) {
            EdgeColor colors[3];
            switchColor(color, seed);
            colors[0] = color;
            colors[1] = WHITE;
            switchColor(color, seed);
            colors[2] = color;
            int corner = corners[0].index;
            if (contour->edges.size() >= 3) {
                int m = (int) contour->edges.size();
                for (int i = 0; i < m; ++i)
                    contour->edges[(corner+i)%m]->color = colors[1+symmetricalTrichotomy(i, m)];
            } else if (contour->edges.size() >= 1) {
                // Less than three edge segments for three colors => edges must be split
                EdgeSegment *parts[7] = { };
                contour->edges[0]->splitInThirds(parts[0+3*corner], parts[1+3*corner], parts[2+3*corner]);
                if (contour->edges.size() >= 2) {
                    contour->edges[1]->splitInThirds(parts[3-3*corner], parts[4-3*corner], parts[5-3*corner]);
                    parts[0]->color = parts[1]->color = colors[0];
                    parts[2]->color = parts[3]->color = colors[1];
                    parts[4]->color = parts[5]->color = colors[2];
                } else {
                    parts[0]->color = colors[0];
                    parts[1]->color = colors[1];
                    parts[2]->color = colors[2];
                }
                contour->edges.clear();
                for (int i = 0; parts[i]; ++i)
                    contour->edges.push_back(EdgeHolder(parts[i]));
            }
        }
        // Multiple corners
        else {
            int cornerCount = (int) corners.size();
            int majorCornerCount = cornerCount;
            if (cornerCount > 3) {
                corners.begin()->prevEdgeLengthEstimate += splineLength;
                for (int i = 0; i < cornerCount; ++i) {
                    if (
                        corners[i].prevEdgeLengthEstimate > corners[(i+1)%cornerCount].prevEdgeLengthEstimate &&
                        corners[(i+1)%cornerCount].prevEdgeLengthEstimate < corners[(i+2)%cornerCount].prevEdgeLengthEstimate
                    ) {
                        corners[i].minor = true;
                        --majorCornerCount;
                    }
                }
            }
            EdgeColor initialColor = BLACK;
            for (int i = 0; i < cornerCount; ++i) {
                if (!corners[i].minor) {
                    --majorCornerCount;
                    switchColor(color, seed, EdgeColor(!majorCornerCount*initialColor));
                    corners[i].color = color;
                    if (!initialColor)
                        initialColor = color;
                }
            }
            for (int i = 0; i < cornerCount; ++i) {
                if (corners[i].minor) {
                    EdgeColor nextColor = corners[(i+1)%cornerCount].color;
                    corners[i].color = EdgeColor((color&nextColor)^WHITE);
                } else
                    color = corners[i].color;
            }
            int spline = 0;
            int start = corners[0].index;
            color = corners[0].color;
            int m = (int) contour->edges.size();
            for (int i = 0; i < m; ++i) {
                int index = (start+i)%m;
                if (spline+1 < cornerCount && corners[spline+1].index == index)
                    color = corners[++spline].color;
                contour->edges[index]->color = color;
            }
        }
    }
}

// EDGE COLORING BY DISTANCE - EXPERIMENTAL IMPLEMENTATION - WORK IN PROGRESS
#define MAX_RECOLOR_STEPS 16
#define EDGE_DISTANCE_PRECISION 16

static double edgeToEdgeDistance(const EdgeSegment &a, const EdgeSegment &b, int precision) {
    if (a.point(0) == b.point(0) || a.point(0) == b.point(1) || a.point(1) == b.point(0) || a.point(1) == b.point(1))
        return 0;
    double iFac = 1./precision;
    double minDistance = (b.point(0)-a.point(0)).length();
    for (int i = 0; i <= precision; ++i) {
        double t = iFac*i;
        double d = fabs(a.signedDistance(b.point(t), t).distance);
        minDistance = min(minDistance, d);
    }
    for (int i = 0; i <= precision; ++i) {
        double t = iFac*i;
        double d = fabs(b.signedDistance(a.point(t), t).distance);
        minDistance = min(minDistance, d);
    }
    return minDistance;
}

static double splineToSplineDistance(EdgeSegment *const *edgeSegments, int aStart, int aEnd, int bStart, int bEnd, int precision) {
    double minDistance = DBL_MAX;
    for (int ai = aStart; ai < aEnd; ++ai)
        for (int bi = bStart; bi < bEnd && minDistance; ++bi) {
            double d = edgeToEdgeDistance(*edgeSegments[ai], *edgeSegments[bi], precision);
            minDistance = min(minDistance, d);
        }
    return minDistance;
}

static void colorSecondDegreeGraph(int *coloring, const int *const *edgeMatrix, int vertexCount, unsigned long long seed) {
    for (int i = 0; i < vertexCount; ++i) {
        int possibleColors = 7;
        for (int j = 0; j < i; ++j) {
            if (edgeMatrix[i][j])
                possibleColors &= ~(1<<coloring[j]);
        }
        int color = 0;
        switch (possibleColors) {
            case 1:
                color = 0;
                break;
            case 2:
                color = 1;
                break;
            case 3:
                color = seedExtract2(seed); // 0 or 1
                break;
            case 4:
                color = 2;
                break;
            case 5:
                color = (int) !seedExtract2(seed)<<1; // 2 or 0
                break;
            case 6:
                color = seedExtract2(seed)+1; // 1 or 2
                break;
            case 7:
                color = (seedExtract3(seed)+i)%3; // 0 or 1 or 2
                break;
        }
        coloring[i] = color;
    }
}

static int vertexPossibleColors(const int *coloring, const int *edgeVector, int vertexCount) {
    int usedColors = 0;
    for (int i = 0; i < vertexCount; ++i)
        if (edgeVector[i])
            usedColors |= 1<<coloring[i];
    return 7&~usedColors;
}

static void uncolorSameNeighbors(std::queue<int> &uncolored, int *coloring, const int *const *edgeMatrix, int vertex, int vertexCount) {
    for (int i = vertex+1; i < vertexCount; ++i) {
        if (edgeMatrix[vertex][i] && coloring[i] == coloring[vertex]) {
            coloring[i] = -1;
            uncolored.push(i);
        }
    }
    for (int i = 0; i < vertex; ++i) {
        if (edgeMatrix[vertex][i] && coloring[i] == coloring[vertex]) {
            coloring[i] = -1;
            uncolored.push(i);
        }
    }
}

static bool tryAddEdge(int *coloring, int *const *edgeMatrix, int vertexCount, int vertexA, int vertexB, int *coloringBuffer) {
    static const int FIRST_POSSIBLE_COLOR[8] = { -1, 0, 1, 0, 2, 2, 1, 0 };
    edgeMatrix[vertexA][vertexB] = 1;
    edgeMatrix[vertexB][vertexA] = 1;
    if (coloring[vertexA] != coloring[vertexB])
        return true;
    int bPossibleColors = vertexPossibleColors(coloring, edgeMatrix[vertexB], vertexCount);
    if (bPossibleColors) {
        coloring[vertexB] = FIRST_POSSIBLE_COLOR[bPossibleColors];
        return true;
    }
    memcpy(coloringBuffer, coloring, sizeof(int)*vertexCount);
    std::queue<int> uncolored;
    {
        int *coloring = coloringBuffer;
        coloring[vertexB] = FIRST_POSSIBLE_COLOR[7&~(1<<coloring[vertexA])];
        uncolorSameNeighbors(uncolored, coloring, edgeMatrix, vertexB, vertexCount);
        int step = 0;
        while (!uncolored.empty() && step < MAX_RECOLOR_STEPS) {
            int i = uncolored.front();
            uncolored.pop();
            int possibleColors = vertexPossibleColors(coloring, edgeMatrix[i], vertexCount);
            if (possibleColors) {
                coloring[i] = FIRST_POSSIBLE_COLOR[possibleColors];
                continue;
            }
            do {
                coloring[i] = step++%3;
            } while (edgeMatrix[i][vertexA] && coloring[i] == coloring[vertexA]);
            uncolorSameNeighbors(uncolored, coloring, edgeMatrix, i, vertexCount);
        }
    }
    if (!uncolored.empty()) {
        edgeMatrix[vertexA][vertexB] = 0;
        edgeMatrix[vertexB][vertexA] = 0;
        return false;
    }
    memcpy(coloring, coloringBuffer, sizeof(int)*vertexCount);
    return true;
}

static int cmpDoublePtr(const void *a, const void *b) {
    return sign(**reinterpret_cast<const double *const *>(a)-**reinterpret_cast<const double *const *>(b));
}

void edgeColoringByDistance(Shape &shape, double angleThreshold, unsigned long long seed) {

    std::vector<EdgeSegment *> edgeSegments;
    std::vector<int> splineStarts;

    double crossThreshold = sin(angleThreshold);
    std::vector<int> corners;
    for (std::vector<Contour>::iterator contour = shape.contours.begin(); contour != shape.contours.end(); ++contour)
        if (!contour->edges.empty()) {
            // Identify corners
            corners.clear();
            Vector2 prevDirection = contour->edges.back()->direction(1);
            int index = 0;
            for (std::vector<EdgeHolder>::const_iterator edge = contour->edges.begin(); edge != contour->edges.end(); ++edge, ++index) {
                if (isCorner(prevDirection.normalize(), (*edge)->direction(0).normalize(), crossThreshold))
                    corners.push_back(index);
                prevDirection = (*edge)->direction(1);
            }

            splineStarts.push_back((int) edgeSegments.size());
            // Smooth contour
            if (corners.empty())
                for (std::vector<EdgeHolder>::iterator edge = contour->edges.begin(); edge != contour->edges.end(); ++edge)
                    edgeSegments.push_back(&**edge);
            // "Teardrop" case
            else if (corners.size() == 1) {
                int corner = corners[0];
                if (contour->edges.size() >= 3) {
                    int m = (int) contour->edges.size();
                    for (int i = 0; i < m; ++i) {
                        if (i == m/2)
                            splineStarts.push_back((int) edgeSegments.size());
                        if (symmetricalTrichotomy(i, m))
                            edgeSegments.push_back(&*contour->edges[(corner+i)%m]);
                        else
                            contour->edges[(corner+i)%m]->color = WHITE;
                    }
                } else if (contour->edges.size() >= 1) {
                    // Less than three edge segments for three colors => edges must be split
                    EdgeSegment *parts[7] = { };
                    contour->edges[0]->splitInThirds(parts[0+3*corner], parts[1+3*corner], parts[2+3*corner]);
                    if (contour->edges.size() >= 2) {
                        contour->edges[1]->splitInThirds(parts[3-3*corner], parts[4-3*corner], parts[5-3*corner]);
                        edgeSegments.push_back(parts[0]);
                        edgeSegments.push_back(parts[1]);
                        parts[2]->color = parts[3]->color = WHITE;
                        splineStarts.push_back((int) edgeSegments.size());
                        edgeSegments.push_back(parts[4]);
                        edgeSegments.push_back(parts[5]);
                    } else {
                        edgeSegments.push_back(parts[0]);
                        parts[1]->color = WHITE;
                        splineStarts.push_back((int) edgeSegments.size());
                        edgeSegments.push_back(parts[2]);
                    }
                    contour->edges.clear();
                    for (int i = 0; parts[i]; ++i)
                        contour->edges.push_back(EdgeHolder(parts[i]));
                }
            }
            // Multiple corners
            else {
                int cornerCount = (int) corners.size();
                int spline = 0;
                int start = corners[0];
                int m = (int) contour->edges.size();
                for (int i = 0; i < m; ++i) {
                    int index = (start+i)%m;
                    if (spline+1 < cornerCount && corners[spline+1] == index) {
                        splineStarts.push_back((int) edgeSegments.size());
                        ++spline;
                    }
                    edgeSegments.push_back(&*contour->edges[index]);
                }
            }
        }
    splineStarts.push_back((int) edgeSegments.size());

    int segmentCount = (int) edgeSegments.size();
    int splineCount = (int) splineStarts.size()-1;
    if (!splineCount)
        return;

    std::vector<double> distanceMatrixStorage(splineCount*splineCount);
    std::vector<double *> distanceMatrix(splineCount);
    for (int i = 0; i < splineCount; ++i)
        distanceMatrix[i] = &distanceMatrixStorage[i*splineCount];
    const double *distanceMatrixBase = &distanceMatrixStorage[0];

    for (int i = 0; i < splineCount; ++i) {
        distanceMatrix[i][i] = -1;
        for (int j = i+1; j < splineCount; ++j) {
            double dist = splineToSplineDistance(&edgeSegments[0], splineStarts[i], splineStarts[i+1], splineStarts[j], splineStarts[j+1], EDGE_DISTANCE_PRECISION);
            distanceMatrix[i][j] = dist;
            distanceMatrix[j][i] = dist;
        }
    }

    std::vector<const double *> graphEdgeDistances;
    graphEdgeDistances.reserve(splineCount*(splineCount-1)/2);
    for (int i = 0; i < splineCount; ++i)
        for (int j = i+1; j < splineCount; ++j)
            graphEdgeDistances.push_back(&distanceMatrix[i][j]);
    int graphEdgeCount = (int) graphEdgeDistances.size();
    if (!graphEdgeDistances.empty())
        qsort(&graphEdgeDistances[0], graphEdgeDistances.size(), sizeof(const double *), &cmpDoublePtr);

    std::vector<int> edgeMatrixStorage(splineCount*splineCount);
    std::vector<int *> edgeMatrix(splineCount);
    for (int i = 0; i < splineCount; ++i)
        edgeMatrix[i] = &edgeMatrixStorage[i*splineCount];
    int nextEdge = 0;
    for (; nextEdge < graphEdgeCount && !*graphEdgeDistances[nextEdge]; ++nextEdge) {
        int elem = (int) (graphEdgeDistances[nextEdge]-distanceMatrixBase);
        int row = elem/splineCount;
        int col = elem%splineCount;
        edgeMatrix[row][col] = 1;
        edgeMatrix[col][row] = 1;
    }

    std::vector<int> coloring(2*splineCount);
    colorSecondDegreeGraph(&coloring[0], &edgeMatrix[0], splineCount, seed);
    for (; nextEdge < graphEdgeCount; ++nextEdge) {
        int elem = (int) (graphEdgeDistances[nextEdge]-distanceMatrixBase);
        tryAddEdge(&coloring[0], &edgeMatrix[0], splineCount, elem/splineCount, elem%splineCount, &coloring[splineCount]);
    }

    const EdgeColor colors[3] = { YELLOW, CYAN, MAGENTA };
    int spline = -1;
    for (int i = 0; i < segmentCount; ++i) {
        if (splineStarts[spline+1] == i)
            ++spline;
        edgeSegments[i]->color = colors[coloring[spline]];
    }
}

}
