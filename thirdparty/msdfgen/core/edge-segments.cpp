
#include "edge-segments.h"

#include "arithmetics.hpp"
#include "equation-solver.h"

namespace msdfgen {

EdgeSegment *EdgeSegment::create(Point2 p0, Point2 p1, EdgeColor edgeColor) {
    return new LinearSegment(p0, p1, edgeColor);
}

EdgeSegment *EdgeSegment::create(Point2 p0, Point2 p1, Point2 p2, EdgeColor edgeColor) {
    if (!crossProduct(p1-p0, p2-p1))
        return new LinearSegment(p0, p2, edgeColor);
    return new QuadraticSegment(p0, p1, p2, edgeColor);
}

EdgeSegment *EdgeSegment::create(Point2 p0, Point2 p1, Point2 p2, Point2 p3, EdgeColor edgeColor) {
    Vector2 p12 = p2-p1;
    if (!crossProduct(p1-p0, p12) && !crossProduct(p12, p3-p2))
        return new LinearSegment(p0, p3, edgeColor);
    if ((p12 = 1.5*p1-.5*p0) == 1.5*p2-.5*p3)
        return new QuadraticSegment(p0, p12, p3, edgeColor);
    return new CubicSegment(p0, p1, p2, p3, edgeColor);
}

void EdgeSegment::distanceToPerpendicularDistance(SignedDistance &distance, Point2 origin, double param) const {
    if (param < 0) {
        Vector2 dir = direction(0).normalize();
        Vector2 aq = origin-point(0);
        double ts = dotProduct(aq, dir);
        if (ts < 0) {
            double perpendicularDistance = crossProduct(aq, dir);
            if (fabs(perpendicularDistance) <= fabs(distance.distance)) {
                distance.distance = perpendicularDistance;
                distance.dot = 0;
            }
        }
    } else if (param > 1) {
        Vector2 dir = direction(1).normalize();
        Vector2 bq = origin-point(1);
        double ts = dotProduct(bq, dir);
        if (ts > 0) {
            double perpendicularDistance = crossProduct(bq, dir);
            if (fabs(perpendicularDistance) <= fabs(distance.distance)) {
                distance.distance = perpendicularDistance;
                distance.dot = 0;
            }
        }
    }
}

LinearSegment::LinearSegment(Point2 p0, Point2 p1, EdgeColor edgeColor) : EdgeSegment(edgeColor) {
    p[0] = p0;
    p[1] = p1;
}

QuadraticSegment::QuadraticSegment(Point2 p0, Point2 p1, Point2 p2, EdgeColor edgeColor) : EdgeSegment(edgeColor) {
    p[0] = p0;
    p[1] = p1;
    p[2] = p2;
}

CubicSegment::CubicSegment(Point2 p0, Point2 p1, Point2 p2, Point2 p3, EdgeColor edgeColor) : EdgeSegment(edgeColor) {
    p[0] = p0;
    p[1] = p1;
    p[2] = p2;
    p[3] = p3;
}

LinearSegment *LinearSegment::clone() const {
    return new LinearSegment(p[0], p[1], color);
}

QuadraticSegment *QuadraticSegment::clone() const {
    return new QuadraticSegment(p[0], p[1], p[2], color);
}

CubicSegment *CubicSegment::clone() const {
    return new CubicSegment(p[0], p[1], p[2], p[3], color);
}

int LinearSegment::type() const {
    return (int) EDGE_TYPE;
}

int QuadraticSegment::type() const {
    return (int) EDGE_TYPE;
}

int CubicSegment::type() const {
    return (int) EDGE_TYPE;
}

const Point2 *LinearSegment::controlPoints() const {
    return p;
}

const Point2 *QuadraticSegment::controlPoints() const {
    return p;
}

const Point2 *CubicSegment::controlPoints() const {
    return p;
}

Point2 LinearSegment::point(double param) const {
    return mix(p[0], p[1], param);
}

Point2 QuadraticSegment::point(double param) const {
    return mix(mix(p[0], p[1], param), mix(p[1], p[2], param), param);
}

Point2 CubicSegment::point(double param) const {
    Vector2 p12 = mix(p[1], p[2], param);
    return mix(mix(mix(p[0], p[1], param), p12, param), mix(p12, mix(p[2], p[3], param), param), param);
}

Vector2 LinearSegment::direction(double param) const {
    return p[1]-p[0];
}

Vector2 QuadraticSegment::direction(double param) const {
    Vector2 tangent = mix(p[1]-p[0], p[2]-p[1], param);
    if (!tangent)
        return p[2]-p[0];
    return tangent;
}

Vector2 CubicSegment::direction(double param) const {
    Vector2 tangent = mix(mix(p[1]-p[0], p[2]-p[1], param), mix(p[2]-p[1], p[3]-p[2], param), param);
    if (!tangent) {
        if (param == 0) return p[2]-p[0];
        if (param == 1) return p[3]-p[1];
    }
    return tangent;
}

Vector2 LinearSegment::directionChange(double param) const {
    return Vector2();
}

Vector2 QuadraticSegment::directionChange(double param) const {
    return (p[2]-p[1])-(p[1]-p[0]);
}

Vector2 CubicSegment::directionChange(double param) const {
    return mix((p[2]-p[1])-(p[1]-p[0]), (p[3]-p[2])-(p[2]-p[1]), param);
}

double LinearSegment::length() const {
    return (p[1]-p[0]).length();
}

double QuadraticSegment::length() const {
    Vector2 ab = p[1]-p[0];
    Vector2 br = p[2]-p[1]-ab;
    double abab = dotProduct(ab, ab);
    double abbr = dotProduct(ab, br);
    double brbr = dotProduct(br, br);
    double abLen = sqrt(abab);
    double brLen = sqrt(brbr);
    double crs = crossProduct(ab, br);
    double h = sqrt(abab+abbr+abbr+brbr);
    return (
        brLen*((abbr+brbr)*h-abbr*abLen)+
        crs*crs*log((brLen*h+abbr+brbr)/(brLen*abLen+abbr))
    )/(brbr*brLen);
}

SignedDistance LinearSegment::signedDistance(Point2 origin, double &param) const {
    Vector2 aq = origin-p[0];
    Vector2 ab = p[1]-p[0];
    param = dotProduct(aq, ab)/dotProduct(ab, ab);
    Vector2 eq = p[param > .5]-origin;
    double endpointDistance = eq.length();
    if (param > 0 && param < 1) {
        double orthoDistance = dotProduct(ab.getOrthonormal(false), aq);
        if (fabs(orthoDistance) < endpointDistance)
            return SignedDistance(orthoDistance, 0);
    }
    return SignedDistance(nonZeroSign(crossProduct(aq, ab))*endpointDistance, fabs(dotProduct(ab.normalize(), eq.normalize())));
}

SignedDistance QuadraticSegment::signedDistance(Point2 origin, double &param) const {
    Vector2 qa = p[0]-origin;
    Vector2 ab = p[1]-p[0];
    Vector2 br = p[2]-p[1]-ab;
    double a = dotProduct(br, br);
    double b = 3*dotProduct(ab, br);
    double c = 2*dotProduct(ab, ab)+dotProduct(qa, br);
    double d = dotProduct(qa, ab);
    double t[3];
    int solutions = solveCubic(t, a, b, c, d);

    Vector2 epDir = direction(0);
    double minDistance = nonZeroSign(crossProduct(epDir, qa))*qa.length(); // distance from A
    param = -dotProduct(qa, epDir)/dotProduct(epDir, epDir);
    {
        epDir = direction(1);
        double distance = (p[2]-origin).length(); // distance from B
        if (distance < fabs(minDistance)) {
            minDistance = nonZeroSign(crossProduct(epDir, p[2]-origin))*distance;
            param = dotProduct(origin-p[1], epDir)/dotProduct(epDir, epDir);
        }
    }
    for (int i = 0; i < solutions; ++i) {
        if (t[i] > 0 && t[i] < 1) {
            Point2 qe = qa+2*t[i]*ab+t[i]*t[i]*br;
            double distance = qe.length();
            if (distance <= fabs(minDistance)) {
                minDistance = nonZeroSign(crossProduct(ab+t[i]*br, qe))*distance;
                param = t[i];
            }
        }
    }

    if (param >= 0 && param <= 1)
        return SignedDistance(minDistance, 0);
    if (param < .5)
        return SignedDistance(minDistance, fabs(dotProduct(direction(0).normalize(), qa.normalize())));
    else
        return SignedDistance(minDistance, fabs(dotProduct(direction(1).normalize(), (p[2]-origin).normalize())));
}

SignedDistance CubicSegment::signedDistance(Point2 origin, double &param) const {
    Vector2 qa = p[0]-origin;
    Vector2 ab = p[1]-p[0];
    Vector2 br = p[2]-p[1]-ab;
    Vector2 as = (p[3]-p[2])-(p[2]-p[1])-br;

    Vector2 epDir = direction(0);
    double minDistance = nonZeroSign(crossProduct(epDir, qa))*qa.length(); // distance from A
    param = -dotProduct(qa, epDir)/dotProduct(epDir, epDir);
    {
        epDir = direction(1);
        double distance = (p[3]-origin).length(); // distance from B
        if (distance < fabs(minDistance)) {
            minDistance = nonZeroSign(crossProduct(epDir, p[3]-origin))*distance;
            param = dotProduct(epDir-(p[3]-origin), epDir)/dotProduct(epDir, epDir);
        }
    }
    // Iterative minimum distance search
    for (int i = 0; i <= MSDFGEN_CUBIC_SEARCH_STARTS; ++i) {
        double t = (double) i/MSDFGEN_CUBIC_SEARCH_STARTS;
        Vector2 qe = qa+3*t*ab+3*t*t*br+t*t*t*as;
        for (int step = 0; step < MSDFGEN_CUBIC_SEARCH_STEPS; ++step) {
            // Improve t
            Vector2 d1 = 3*ab+6*t*br+3*t*t*as;
            Vector2 d2 = 6*br+6*t*as;
            t -= dotProduct(qe, d1)/(dotProduct(d1, d1)+dotProduct(qe, d2));
            if (t <= 0 || t >= 1)
                break;
            qe = qa+3*t*ab+3*t*t*br+t*t*t*as;
            double distance = qe.length();
            if (distance < fabs(minDistance)) {
                minDistance = nonZeroSign(crossProduct(d1, qe))*distance;
                param = t;
            }
        }
    }

    if (param >= 0 && param <= 1)
        return SignedDistance(minDistance, 0);
    if (param < .5)
        return SignedDistance(minDistance, fabs(dotProduct(direction(0).normalize(), qa.normalize())));
    else
        return SignedDistance(minDistance, fabs(dotProduct(direction(1).normalize(), (p[3]-origin).normalize())));
}

int LinearSegment::scanlineIntersections(double x[3], int dy[3], double y) const {
    if ((y >= p[0].y && y < p[1].y) || (y >= p[1].y && y < p[0].y)) {
        double param = (y-p[0].y)/(p[1].y-p[0].y);
        x[0] = mix(p[0].x, p[1].x, param);
        dy[0] = sign(p[1].y-p[0].y);
        return 1;
    }
    return 0;
}

int QuadraticSegment::scanlineIntersections(double x[3], int dy[3], double y) const {
    int total = 0;
    int nextDY = y > p[0].y ? 1 : -1;
    x[total] = p[0].x;
    if (p[0].y == y) {
        if (p[0].y < p[1].y || (p[0].y == p[1].y && p[0].y < p[2].y))
            dy[total++] = 1;
        else
            nextDY = 1;
    }
    {
        Vector2 ab = p[1]-p[0];
        Vector2 br = p[2]-p[1]-ab;
        double t[2];
        int solutions = solveQuadratic(t, br.y, 2*ab.y, p[0].y-y);
        // Sort solutions
        double tmp;
        if (solutions >= 2 && t[0] > t[1])
            tmp = t[0], t[0] = t[1], t[1] = tmp;
        for (int i = 0; i < solutions && total < 2; ++i) {
            if (t[i] >= 0 && t[i] <= 1) {
                x[total] = p[0].x+2*t[i]*ab.x+t[i]*t[i]*br.x;
                if (nextDY*(ab.y+t[i]*br.y) >= 0) {
                    dy[total++] = nextDY;
                    nextDY = -nextDY;
                }
            }
        }
    }
    if (p[2].y == y) {
        if (nextDY > 0 && total > 0) {
            --total;
            nextDY = -1;
        }
        if ((p[2].y < p[1].y || (p[2].y == p[1].y && p[2].y < p[0].y)) && total < 2) {
            x[total] = p[2].x;
            if (nextDY < 0) {
                dy[total++] = -1;
                nextDY = 1;
            }
        }
    }
    if (nextDY != (y >= p[2].y ? 1 : -1)) {
        if (total > 0)
            --total;
        else {
            if (fabs(p[2].y-y) < fabs(p[0].y-y))
                x[total] = p[2].x;
            dy[total++] = nextDY;
        }
    }
    return total;
}

int CubicSegment::scanlineIntersections(double x[3], int dy[3], double y) const {
    int total = 0;
    int nextDY = y > p[0].y ? 1 : -1;
    x[total] = p[0].x;
    if (p[0].y == y) {
        if (p[0].y < p[1].y || (p[0].y == p[1].y && (p[0].y < p[2].y || (p[0].y == p[2].y && p[0].y < p[3].y))))
            dy[total++] = 1;
        else
            nextDY = 1;
    }
    {
        Vector2 ab = p[1]-p[0];
        Vector2 br = p[2]-p[1]-ab;
        Vector2 as = (p[3]-p[2])-(p[2]-p[1])-br;
        double t[3];
        int solutions = solveCubic(t, as.y, 3*br.y, 3*ab.y, p[0].y-y);
        // Sort solutions
        double tmp;
        if (solutions >= 2) {
            if (t[0] > t[1])
                tmp = t[0], t[0] = t[1], t[1] = tmp;
            if (solutions >= 3 && t[1] > t[2]) {
                tmp = t[1], t[1] = t[2], t[2] = tmp;
                if (t[0] > t[1])
                    tmp = t[0], t[0] = t[1], t[1] = tmp;
            }
        }
        for (int i = 0; i < solutions && total < 3; ++i) {
            if (t[i] >= 0 && t[i] <= 1) {
                x[total] = p[0].x+3*t[i]*ab.x+3*t[i]*t[i]*br.x+t[i]*t[i]*t[i]*as.x;
                if (nextDY*(ab.y+2*t[i]*br.y+t[i]*t[i]*as.y) >= 0) {
                    dy[total++] = nextDY;
                    nextDY = -nextDY;
                }
            }
        }
    }
    if (p[3].y == y) {
        if (nextDY > 0 && total > 0) {
            --total;
            nextDY = -1;
        }
        if ((p[3].y < p[2].y || (p[3].y == p[2].y && (p[3].y < p[1].y || (p[3].y == p[1].y && p[3].y < p[0].y)))) && total < 3) {
            x[total] = p[3].x;
            if (nextDY < 0) {
                dy[total++] = -1;
                nextDY = 1;
            }
        }
    }
    if (nextDY != (y >= p[3].y ? 1 : -1)) {
        if (total > 0)
            --total;
        else {
            if (fabs(p[3].y-y) < fabs(p[0].y-y))
                x[total] = p[3].x;
            dy[total++] = nextDY;
        }
    }
    return total;
}

static void pointBounds(Point2 p, double &l, double &b, double &r, double &t) {
    if (p.x < l) l = p.x;
    if (p.y < b) b = p.y;
    if (p.x > r) r = p.x;
    if (p.y > t) t = p.y;
}

void LinearSegment::bound(double &l, double &b, double &r, double &t) const {
    pointBounds(p[0], l, b, r, t);
    pointBounds(p[1], l, b, r, t);
}

void QuadraticSegment::bound(double &l, double &b, double &r, double &t) const {
    pointBounds(p[0], l, b, r, t);
    pointBounds(p[2], l, b, r, t);
    Vector2 bot = (p[1]-p[0])-(p[2]-p[1]);
    if (bot.x) {
        double param = (p[1].x-p[0].x)/bot.x;
        if (param > 0 && param < 1)
            pointBounds(point(param), l, b, r, t);
    }
    if (bot.y) {
        double param = (p[1].y-p[0].y)/bot.y;
        if (param > 0 && param < 1)
            pointBounds(point(param), l, b, r, t);
    }
}

void CubicSegment::bound(double &l, double &b, double &r, double &t) const {
    pointBounds(p[0], l, b, r, t);
    pointBounds(p[3], l, b, r, t);
    Vector2 a0 = p[1]-p[0];
    Vector2 a1 = 2*(p[2]-p[1]-a0);
    Vector2 a2 = p[3]-3*p[2]+3*p[1]-p[0];
    double params[2];
    int solutions;
    solutions = solveQuadratic(params, a2.x, a1.x, a0.x);
    for (int i = 0; i < solutions; ++i)
        if (params[i] > 0 && params[i] < 1)
            pointBounds(point(params[i]), l, b, r, t);
    solutions = solveQuadratic(params, a2.y, a1.y, a0.y);
    for (int i = 0; i < solutions; ++i)
        if (params[i] > 0 && params[i] < 1)
            pointBounds(point(params[i]), l, b, r, t);
}

void LinearSegment::reverse() {
    Point2 tmp = p[0];
    p[0] = p[1];
    p[1] = tmp;
}

void QuadraticSegment::reverse() {
    Point2 tmp = p[0];
    p[0] = p[2];
    p[2] = tmp;
}

void CubicSegment::reverse() {
    Point2 tmp = p[0];
    p[0] = p[3];
    p[3] = tmp;
    tmp = p[1];
    p[1] = p[2];
    p[2] = tmp;
}

void LinearSegment::moveStartPoint(Point2 to) {
    p[0] = to;
}

void QuadraticSegment::moveStartPoint(Point2 to) {
    Vector2 origSDir = p[0]-p[1];
    Point2 origP1 = p[1];
    p[1] += crossProduct(p[0]-p[1], to-p[0])/crossProduct(p[0]-p[1], p[2]-p[1])*(p[2]-p[1]);
    p[0] = to;
    if (dotProduct(origSDir, p[0]-p[1]) < 0)
        p[1] = origP1;
}

void CubicSegment::moveStartPoint(Point2 to) {
    p[1] += to-p[0];
    p[0] = to;
}

void LinearSegment::moveEndPoint(Point2 to) {
    p[1] = to;
}

void QuadraticSegment::moveEndPoint(Point2 to) {
    Vector2 origEDir = p[2]-p[1];
    Point2 origP1 = p[1];
    p[1] += crossProduct(p[2]-p[1], to-p[2])/crossProduct(p[2]-p[1], p[0]-p[1])*(p[0]-p[1]);
    p[2] = to;
    if (dotProduct(origEDir, p[2]-p[1]) < 0)
        p[1] = origP1;
}

void CubicSegment::moveEndPoint(Point2 to) {
    p[2] += to-p[3];
    p[3] = to;
}

void LinearSegment::splitInThirds(EdgeSegment *&part0, EdgeSegment *&part1, EdgeSegment *&part2) const {
    part0 = new LinearSegment(p[0], point(1/3.), color);
    part1 = new LinearSegment(point(1/3.), point(2/3.), color);
    part2 = new LinearSegment(point(2/3.), p[1], color);
}

void QuadraticSegment::splitInThirds(EdgeSegment *&part0, EdgeSegment *&part1, EdgeSegment *&part2) const {
    part0 = new QuadraticSegment(p[0], mix(p[0], p[1], 1/3.), point(1/3.), color);
    part1 = new QuadraticSegment(point(1/3.), mix(mix(p[0], p[1], 5/9.), mix(p[1], p[2], 4/9.), .5), point(2/3.), color);
    part2 = new QuadraticSegment(point(2/3.), mix(p[1], p[2], 2/3.), p[2], color);
}

void CubicSegment::splitInThirds(EdgeSegment *&part0, EdgeSegment *&part1, EdgeSegment *&part2) const {
    part0 = new CubicSegment(p[0], p[0] == p[1] ? p[0] : mix(p[0], p[1], 1/3.), mix(mix(p[0], p[1], 1/3.), mix(p[1], p[2], 1/3.), 1/3.), point(1/3.), color);
    part1 = new CubicSegment(point(1/3.),
        mix(mix(mix(p[0], p[1], 1/3.), mix(p[1], p[2], 1/3.), 1/3.), mix(mix(p[1], p[2], 1/3.), mix(p[2], p[3], 1/3.), 1/3.), 2/3.),
        mix(mix(mix(p[0], p[1], 2/3.), mix(p[1], p[2], 2/3.), 2/3.), mix(mix(p[1], p[2], 2/3.), mix(p[2], p[3], 2/3.), 2/3.), 1/3.),
        point(2/3.), color);
    part2 = new CubicSegment(point(2/3.), mix(mix(p[1], p[2], 2/3.), mix(p[2], p[3], 2/3.), 2/3.), p[2] == p[3] ? p[3] : mix(p[2], p[3], 2/3.), p[3], color);
}

EdgeSegment *QuadraticSegment::convertToCubic() const {
    return new CubicSegment(p[0], mix(p[0], p[1], 2/3.), mix(p[1], p[2], 1/3.), p[2], color);
}

}
