
#include "export-svg.h"

#include <cstdio>
#include "edge-segments.h"

namespace msdfgen {

static void writeSvgCoord(FILE *f, Point2 coord) {
    fprintf(f, "%.17g %.17g", coord.x, coord.y);
}

static void writeSvgPathDef(FILE *f, const Shape &shape) {
    bool beginning = true;
    for (const Contour &c : shape.contours) {
        if (c.edges.empty())
            continue;
        if (beginning)
            beginning = false;
        else
            fputc(' ', f);
        fputs("M ", f);
        writeSvgCoord(f, c.edges[0]->controlPoints()[0]);
        for (const EdgeHolder &e : c.edges) {
            const Point2 *cp = e->controlPoints();
            switch (e->type()) {
                case (int) LinearSegment::EDGE_TYPE:
                    fputs(" L ", f);
                    writeSvgCoord(f, cp[1]);
                    break;
                case (int) QuadraticSegment::EDGE_TYPE:
                    fputs(" Q ", f);
                    writeSvgCoord(f, cp[1]);
                    fputc(' ', f);
                    writeSvgCoord(f, cp[2]);
                    break;
                case (int) CubicSegment::EDGE_TYPE:
                    fputs(" C ", f);
                    writeSvgCoord(f, cp[1]);
                    fputc(' ', f);
                    writeSvgCoord(f, cp[2]);
                    fputc(' ', f);
                    writeSvgCoord(f, cp[3]);
                    break;
            }
        }
        fputs(" Z", f);
    }
}

bool saveSvgShape(const Shape &shape, const char *filename) {
    if (FILE *f = fopen(filename, "w")) {
        fputs("<svg xmlns=\"http://www.w3.org/2000/svg\"><path", f);
        if (!shape.inverseYAxis)
            fputs(" transform=\"scale(1 -1)\"", f);
        fputs(" d=\"", f);
        writeSvgPathDef(f, shape);
        fputs("\"/></svg>\n", f);
        fclose(f);
        return true;
    }
    return false;
}

bool saveSvgShape(const Shape &shape, const Shape::Bounds &bounds, const char *filename) {
    if (FILE *f = fopen(filename, "w")) {
        fprintf(f, "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"%.17g %.17g %.17g %.17g\"><path", bounds.l, bounds.b, bounds.r-bounds.l, bounds.t-bounds.b);
        if (!shape.inverseYAxis)
            fprintf(f, " transform=\"translate(0 %.17g) scale(1 -1)\"", bounds.b+bounds.t);
        fputs(" d=\"", f);
        writeSvgPathDef(f, shape);
        fputs("\"/></svg>\n", f);
        fclose(f);
        return true;
    }
    return false;
}

}
