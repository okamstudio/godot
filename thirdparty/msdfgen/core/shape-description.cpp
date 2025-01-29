
#define _CRT_SECURE_NO_WARNINGS
#include "shape-description.h"

#include <cstdlib>

namespace msdfgen {

int readCharF(FILE *input) {
    int c = '\0';
    do {
        c = fgetc(input);
    } while (c == ' ' || c == '\t' || c == '\r' || c == '\n');
    return c;
}

int readCharS(const char **input) {
    int c = '\0';
    do {
        c = *(*input)++;
    } while (c == ' ' || c == '\t' || c == '\r' || c == '\n');
    if (!c) {
        --c;
        return EOF;
    }
    return c;
}

int readCoordF(FILE *input, Point2 &coord) {
    return fscanf(input, "%lf , %lf", &coord.x, &coord.y);
}

int readCoordS(const char **input, Point2 &coord) {
    char *end = NULL;
    coord.x = strtod(*input, &end);
    if (end <= *input)
        return 0;
    *input = end;
    while (**input == ' ' || **input == '\t' || **input == '\n' || **input == '\r')
        ++*input;
    if (**input != ',')
        return 1;
    ++*input;
    coord.y = strtod(*input, &end);
    if (end <= *input)
        return 1;
    *input = end;
    return 2;
}

static bool writeCoord(FILE *output, Point2 coord) {
    fprintf(output, "%.12g, %.12g", coord.x, coord.y);
    return true;
}

template <typename T, int (*readChar)(T *), int (*readCoord)(T *, Point2 &)>
static int readControlPoints(T *input, Point2 *output) {
    int result = readCoord(input, output[0]);
    if (result == 2) {
        switch (readChar(input)) {
            case ')':
                return 1;
            case ';':
                break;
            default:
                return -1;
        }
        result = readCoord(input, output[1]);
        if (result == 2 && readChar(input) == ')')
            return 2;
    } else if (result != 1 && readChar(input) == ')')
        return 0;
    return -1;
}

template <typename T, int (*readChar)(T *), int (*readCoord)(T *, Point2 &)>
static bool readContour(T *input, Contour &output, const Point2 *first, int terminator, bool &colorsSpecified) {
    Point2 p[4], start;
    if (first)
        p[0] = *first;
    else {
        int result = readCoord(input, p[0]);
        if (result != 2)
            return result != 1 && readChar(input) == terminator;
    }
    start = p[0];
    int c = '\0';
    while ((c = readChar(input)) != terminator) {
        if (c != ';')
            return false;
        EdgeColor color = WHITE;
        int result = readCoord(input, p[1]);
        if (result == 2) {
            output.addEdge(EdgeHolder(p[0], p[1], color));
            p[0] = p[1];
            continue;
        } else if (result == 1)
            return false;
        else {
            int controlPoints = 0;
            switch ((c = readChar(input))) {
                case '#':
                    output.addEdge(EdgeHolder(p[0], start, color));
                    p[0] = start;
                    continue;
                case ';':
                    goto FINISH_EDGE;
                case '(':
                    goto READ_CONTROL_POINTS;
                case 'C': case 'c':
                    color = CYAN;
                    colorsSpecified = true;
                    break;
                case 'M': case 'm':
                    color = MAGENTA;
                    colorsSpecified = true;
                    break;
                case 'Y': case 'y':
                    color = YELLOW;
                    colorsSpecified = true;
                    break;
                case 'W': case 'w':
                    color = WHITE;
                    colorsSpecified = true;
                    break;
                default:
                    return c == terminator;
            }
            switch (readChar(input)) {
                case ';':
                    goto FINISH_EDGE;
                case '(':
                READ_CONTROL_POINTS:
                    if ((controlPoints = readControlPoints<T, readChar, readCoord>(input, p+1)) < 0)
                        return false;
                    break;
                default:
                    return false;
            }
            if (readChar(input) != ';')
                return false;
        FINISH_EDGE:
            result = readCoord(input, p[1+controlPoints]);
            if (result != 2) {
                if (result == 1)
                    return false;
                else {
                    if (readChar(input) == '#')
                        p[1+controlPoints] = start;
                    else
                        return false;
                }
            }
            switch (controlPoints) {
                case 0:
                    output.addEdge(EdgeHolder(p[0], p[1], color));
                    p[0] = p[1];
                    continue;
                case 1:
                    output.addEdge(EdgeHolder(p[0], p[1], p[2], color));
                    p[0] = p[2];
                    continue;
                case 2:
                    output.addEdge(EdgeHolder(p[0], p[1], p[2], p[3], color));
                    p[0] = p[3];
                    continue;
            }
        }
    }
    return true;
}

bool readShapeDescription(FILE *input, Shape &output, bool *colorsSpecified) {
    bool locColorsSpec = false;
    output.contours.clear();
    output.inverseYAxis = false;
    Point2 p;
    int result = readCoordF(input, p);
    if (result == 2) {
        return readContour<FILE, readCharF, readCoordF>(input, output.addContour(), &p, EOF, locColorsSpec);
    } else if (result == 1)
        return false;
    else {
        int c = readCharF(input);
        if (c == '@') {
            char after = '\0';
            if (fscanf(input, "invert-y%c", &after) != 1)
                return feof(input) != 0;
            output.inverseYAxis = true;
            c = after;
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
                c = readCharF(input);
        }
        for (; c == '{'; c = readCharF(input))
            if (!readContour<FILE, readCharF, readCoordF>(input, output.addContour(), NULL, '}', locColorsSpec))
                return false;
        if (colorsSpecified)
            *colorsSpecified = locColorsSpec;
        return c == EOF && feof(input);
    }
}

bool readShapeDescription(const char *input, Shape &output, bool *colorsSpecified) {
    bool locColorsSpec = false;
    output.contours.clear();
    output.inverseYAxis = false;
    Point2 p;
    int result = readCoordS(&input, p);
    if (result == 2) {
        return readContour<const char *, readCharS, readCoordS>(&input, output.addContour(), &p, EOF, locColorsSpec);
    } else if (result == 1)
        return false;
    else {
        int c = readCharS(&input);
        if (c == '@') {
            for (int i = 0; i < (int) sizeof("invert-y")-1; ++i)
                if (input[i] != "invert-y"[i])
                    return false;
            output.inverseYAxis = true;
            input += sizeof("invert-y")-1;
            c = readCharS(&input);
        }
        for (; c == '{'; c = readCharS(&input))
            if (!readContour<const char *, readCharS, readCoordS>(&input, output.addContour(), NULL, '}', locColorsSpec))
                return false;
        if (colorsSpecified)
            *colorsSpecified = locColorsSpec;
        return c == EOF;
    }
}

static bool isColored(const Shape &shape) {
    for (std::vector<Contour>::const_iterator contour = shape.contours.begin(); contour != shape.contours.end(); ++contour)
        for (std::vector<EdgeHolder>::const_iterator edge = contour->edges.begin(); edge != contour->edges.end(); ++edge)
            if ((*edge)->color != WHITE)
                return true;
    return false;
}

bool writeShapeDescription(FILE *output, const Shape &shape) {
    if (!shape.validate())
        return false;
    bool writeColors = isColored(shape);
    if (shape.inverseYAxis)
        fprintf(output, "@invert-y\n");
    for (std::vector<Contour>::const_iterator contour = shape.contours.begin(); contour != shape.contours.end(); ++contour) {
        fprintf(output, "{\n");
        if (!contour->edges.empty()) {
            for (std::vector<EdgeHolder>::const_iterator edge = contour->edges.begin(); edge != contour->edges.end(); ++edge) {
                char colorCode = '\0';
                if (writeColors) {
                    switch ((*edge)->color) {
                        case YELLOW: colorCode = 'y'; break;
                        case MAGENTA: colorCode = 'm'; break;
                        case CYAN: colorCode = 'c'; break;
                        case WHITE: colorCode = 'w'; break;
                        default:;
                    }
                }
                const Point2 *p = (*edge)->controlPoints();
                switch ((*edge)->type()) {
                    case (int) LinearSegment::EDGE_TYPE:
                        fprintf(output, "\t");
                        writeCoord(output, p[0]);
                        fprintf(output, ";\n");
                        if (colorCode)
                            fprintf(output, "\t\t%c;\n", colorCode);
                        break;
                    case (int) QuadraticSegment::EDGE_TYPE:
                        fprintf(output, "\t");
                        writeCoord(output, p[0]);
                        fprintf(output, ";\n\t\t");
                        if (colorCode)
                            fprintf(output, "%c", colorCode);
                        fprintf(output, "(");
                        writeCoord(output, p[1]);
                        fprintf(output, ");\n");
                        break;
                    case (int) CubicSegment::EDGE_TYPE:
                        fprintf(output, "\t");
                        writeCoord(output, p[0]);
                        fprintf(output, ";\n\t\t");
                        if (colorCode)
                            fprintf(output, "%c", colorCode);
                        fprintf(output, "(");
                        writeCoord(output, p[1]);
                        fprintf(output, "; ");
                        writeCoord(output, p[2]);
                        fprintf(output, ");\n");
                        break;
                }
            }
            fprintf(output, "\t#\n");
        }
        fprintf(output, "}\n");
    }
    return true;
}

}
