
#pragma once

#include "Shape.h"

namespace msdfgen {

bool saveSvgShape(const Shape &shape, const char *filename);
bool saveSvgShape(const Shape &shape, const Shape::Bounds &bounds, const char *filename);

}
