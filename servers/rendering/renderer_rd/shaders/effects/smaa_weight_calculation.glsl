/**
 * Copyright (C) 2013 Jorge Jimenez (jorge@iryoku.com)
 * Copyright (C) 2013 Jose I. Echevarria (joseignacioechevarria@gmail.com)
 * Copyright (C) 2013 Belen Masia (bmasia@unizar.es)
 * Copyright (C) 2013 Fernando Navarro (fernandn@microsoft.com)
 * Copyright (C) 2013 Diego Gutierrez (diegog@unizar.es)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to
 * do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software. As clarification, there
 * is no requirement that the copyright notice and permission be included in
 * binary distributions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#[vertex]
#version 450

layout(location = 0) out vec2 tex_coord;
layout(location = 1) out vec2 pix_coord;
layout(location = 2) out vec4 offset[3];

layout(push_constant, std430) uniform Params {
	vec2 inv_size;
	ivec2 size;

	vec4 subsample_indices;
}
params;

#define SMAA_MAX_SEARCH_STEPS 32

void main() {
	vec2 vertex_base;
	if (gl_VertexIndex == 0) {
		vertex_base = vec2(-1.0, -1.0);
	} else if (gl_VertexIndex == 1) {
		vertex_base = vec2(-1.0, 3.0);
	} else {
		vertex_base = vec2(3.0, -1.0);
	}
	gl_Position = vec4(vertex_base, 0.0, 1.0);
	tex_coord = clamp(vertex_base, vec2(0.0, 0.0), vec2(1.0, 1.0)) * 2.0; // saturate(x) * 2.0
	pix_coord = tex_coord * params.size.xy;

	offset[0] = fma(params.inv_size.xyxy, vec4(-0.25, -0.125, 1.25, -0.125), tex_coord.xyxy);
	offset[1] = fma(params.inv_size.xyxy, vec4(-0.125, -0.25, -0.125, 1.25), tex_coord.xyxy);
	offset[2] = fma(params.inv_size.xxyy,
			vec4(-2.0, 2.0, -2.0, 2.0) * SMAA_MAX_SEARCH_STEPS,
			vec4(offset[0].xz, offset[1].yw));
}

#[fragment]
#version 450

layout(location = 0) in vec2 tex_coord;
layout(location = 1) in vec2 pix_coord;
layout(location = 2) in vec4 offset[3];
layout(set = 0, binding = 0) uniform sampler2D edges_tex;
layout(set = 1, binding = 0) uniform sampler2D area_tex;
layout(set = 1, binding = 1) uniform sampler2D search_tex;
layout(location = 0) out vec4 weights;

layout(push_constant, std430) uniform Params {
	vec2 inv_size;
	ivec2 size;

	vec4 subsample_indices;
}
params;

#define SMAA_MAX_SEARCH_STEPS 32
#define SMAA_MAX_SEARCH_STEPS_DIAG 16
#define SMAA_CORNER_ROUNDING 25

#ifndef SMAA_AREATEX_SELECT
#define SMAA_AREATEX_SELECT(sample) sample.rg
#endif

#ifndef SMAA_SEARCHTEX_SELECT
#define SMAA_SEARCHTEX_SELECT(sample) sample.r
#endif

#define SMAA_AREATEX_MAX_DISTANCE 16
#define SMAA_AREATEX_MAX_DISTANCE_DIAG 20
#define SMAA_AREATEX_PIXEL_SIZE (1.0 / vec2(160.0, 560.0))
#define SMAA_AREATEX_SUBTEX_SIZE (1.0 / 7.0)
#define SMAA_SEARCHTEX_SIZE vec2(66.0, 33.0)
#define SMAA_SEARCHTEX_PACKED_SIZE vec2(64.0, 16.0)
#define SMAA_CORNER_ROUNDING_NORM (float(SMAA_CORNER_ROUNDING) / 100.0)

void SMAAMovc(bvec2 cond, inout vec2 variable, vec2 value) {
	if (cond.x) {
		variable.x = value.x;
	}
	if (cond.y) {
		variable.y = value.y;
	}
}

vec2 SMAADecodeDiagBilinearAccess(vec2 e) {
	e.r = e.r * abs(5.0 * e.r - 5.0 * 0.75);
	return round(e);
}

vec4 SMAADecodeDiagBilinearAccess(vec4 e) {
	e.rb = e.rb * abs(5.0 * e.rb - 5.0 * 0.75);
	return round(e);
}

vec2 SMAASearchDiag1(vec2 tex_coord, vec2 dir, out vec2 e) {
	vec4 coord = vec4(tex_coord, -1.0, 1.0);
	vec3 t = vec3(params.inv_size.xy, 1.0);
	while (coord.z < float(SMAA_MAX_SEARCH_STEPS_DIAG - 1) &&
			coord.w > 0.9) {
		coord.xyz = fma(t, vec3(dir, 1.0), coord.xyz);
		e = textureLod(edges_tex, coord.xy, 0.0).rg;
		coord.w = dot(e, vec2(0.5, 0.5));
	}
	return coord.zw;
}

vec2 SMAASearchDiag2(vec2 tex_coord, vec2 dir, out vec2 e) {
	vec4 coord = vec4(tex_coord, -1.0, 1.0);
	coord.x += 0.25 * params.inv_size.x;
	vec3 t = vec3(params.inv_size.xy, 1.0);
	while (coord.z < float(SMAA_MAX_SEARCH_STEPS_DIAG - 1) &&
			coord.w > 0.9) {
		coord.xyz = fma(t, vec3(dir, 1.0), coord.xyz);

		e = textureLod(edges_tex, coord.xy, 0.0).rg;
		e = SMAADecodeDiagBilinearAccess(e);

		coord.w = dot(e, vec2(0.5, 0.5));
	}
	return coord.zw;
}

vec2 SMAAAreaDiag(vec2 dist, vec2 e, float offset) {
	vec2 coord = fma(vec2(SMAA_AREATEX_MAX_DISTANCE_DIAG, SMAA_AREATEX_MAX_DISTANCE_DIAG), e, dist);

	coord = fma(SMAA_AREATEX_PIXEL_SIZE, coord, 0.5 * SMAA_AREATEX_PIXEL_SIZE);

	coord.x += 0.5;

	coord.y += SMAA_AREATEX_SUBTEX_SIZE * offset;

	return SMAA_AREATEX_SELECT(textureLod(area_tex, coord, 0.0));
}

vec2 SMAACalculateDiagWeights(vec2 tex_coord, vec2 e, vec4 subsample_indices) {
	vec2 weights = vec2(0.0, 0.0);

	vec4 d;
	vec2 end;
	if (e.r > 0.0) {
		d.xz = SMAASearchDiag1(tex_coord, vec2(-1.0, 1.0), end);
		d.x += float(end.y > 0.9);
	} else {
		d.xz = vec2(0.0, 0.0);
	}
	d.yw = SMAASearchDiag1(tex_coord, vec2(1.0, -1.0), end);

	if (d.x + d.y > 2.0) {
		vec4 coords = fma(vec4(-d.x + 0.25, d.x, d.y, -d.y - 0.25), params.inv_size.xyxy, tex_coord.xyxy);
		vec4 c;
		c.xy = textureLodOffset(edges_tex, coords.xy, 0.0, ivec2(-1, 0)).rg;
		c.zw = textureLodOffset(edges_tex, coords.zw, 0.0, ivec2(1, 0)).rg;
		c.yxwz = SMAADecodeDiagBilinearAccess(c.xyzw);

		vec2 cc = fma(vec2(2.0, 2.0), c.xz, c.yw);

		SMAAMovc(bvec2(step(0.9, d.zw)), cc, vec2(0.0, 0.0));

		weights += SMAAAreaDiag(d.xy, cc, subsample_indices.z);
	}

	d.xz = SMAASearchDiag2(tex_coord, vec2(-1.0, -1.0), end);
	if (textureLodOffset(edges_tex, tex_coord, 0.0, ivec2(1, 0)).r > 0.0) {
		d.yw = SMAASearchDiag2(tex_coord, vec2(1.0, 1.0), end);
		d.y += float(end.y > 0.9);
	} else {
		d.yw = vec2(0.0, 0.0);
	}

	if (d.x + d.y > 2.0) {
		vec4 coords = fma(vec4(-d.x, -d.x, d.y, d.y), params.inv_size.xyxy, tex_coord.xyxy);
		vec4 c;
		c.x = textureLodOffset(edges_tex, coords.xy, 0.0, ivec2(-1, 0)).g;
		c.y = textureLodOffset(edges_tex, coords.xy, 0.0, ivec2(0, -1)).r;
		c.zw = textureLodOffset(edges_tex, coords.zw, 0.0, ivec2(1, 0)).gr;
		vec2 cc = fma(vec2(2.0, 2.0), c.xz, c.yw);

		SMAAMovc(bvec2(step(0.9, d.zw)), cc, vec2(0.0, 0.0));

		weights += SMAAAreaDiag(d.xy, cc, subsample_indices.w).gr;
	}

	return weights;
}

float SMAASearchLength(vec2 e, float offset) {
	vec2 scale = SMAA_SEARCHTEX_SIZE * vec2(0.5, -1.0);
	vec2 bias = SMAA_SEARCHTEX_SIZE * vec2(offset, 1.0);

	scale += vec2(-1.0, 1.0);
	bias += vec2(0.5, -0.5);

	scale *= 1.0 / SMAA_SEARCHTEX_PACKED_SIZE;
	bias *= 1.0 / SMAA_SEARCHTEX_PACKED_SIZE;

	return SMAA_SEARCHTEX_SELECT(textureLod(search_tex, fma(scale, e, bias), 0.0));
}

float SMAASearchXLeft(vec2 tex_coord, float end) {
	vec2 e = vec2(0.0, 1.0);
	while (tex_coord.x > end &&
			e.g > 0.8281 &&
			e.r == 0.0) {
		e = textureLod(edges_tex, tex_coord, 0.0).rg;
		tex_coord = fma(-vec2(2.0, 0.0), params.inv_size.xy, tex_coord);
	}

	float offset = fma(-(255.0 / 127.0), SMAASearchLength(e, 0.0), 3.25);
	return fma(params.inv_size.x, offset, tex_coord.x);
}

float SMAASearchXRight(vec2 tex_coord, float end) {
	vec2 e = vec2(0.0, 1.0);
	while (tex_coord.x < end &&
			e.g > 0.8281 &&
			e.r == 0.0) {
		e = textureLod(edges_tex, tex_coord, 0.0).rg;
		tex_coord = fma(vec2(2.0, 0.0), params.inv_size.xy, tex_coord);
	}

	float offset = fma(-(255.0 / 127.0), SMAASearchLength(e, 0.5), 3.25);
	return fma(-params.inv_size.x, offset, tex_coord.x);
}

float SMAASearchYUp(vec2 tex_coord, float end) {
	vec2 e = vec2(1.0, 0.0);
	while (tex_coord.y > end &&
			e.r > 0.8281 &&
			e.g == 0.0) {
		e = textureLod(edges_tex, tex_coord, 0.0).rg;
		tex_coord = fma(-vec2(0.0, 2.0), params.inv_size.xy, tex_coord);
	}

	float offset = fma(-(255.0 / 127.0), SMAASearchLength(e.gr, 0.0), 3.25);
	return fma(params.inv_size.y, offset, tex_coord.y);
}

float SMAASearchYDown(vec2 tex_coord, float end) {
	vec2 e = vec2(1.0, 0.0);
	while (tex_coord.y < end &&
			e.r > 0.8281 &&
			e.g == 0.0) {
		e = textureLod(edges_tex, tex_coord, 0.0).rg;
		tex_coord = fma(vec2(0.0, 2.0), params.inv_size.xy, tex_coord);
	}

	float offset = fma(-(255.0 / 127.0), SMAASearchLength(e.gr, 0.5), 3.25);
	return fma(-params.inv_size.y, offset, tex_coord.y);
}

vec2 SMAAArea(vec2 dist, float e1, float e2, float offset) {
	vec2 tex_coord = fma(vec2(SMAA_AREATEX_MAX_DISTANCE, SMAA_AREATEX_MAX_DISTANCE), round(4.0 * vec2(e1, e2)), dist);

	tex_coord = fma(SMAA_AREATEX_PIXEL_SIZE, tex_coord, 0.5 * SMAA_AREATEX_PIXEL_SIZE);

	tex_coord.y = fma(SMAA_AREATEX_SUBTEX_SIZE, offset, tex_coord.y);

	return SMAA_AREATEX_SELECT(textureLod(area_tex, tex_coord, 0.0));
}

void SMAADetectHorizontalCornerPattern(inout vec2 weights, vec4 coord, vec2 d) {
	vec2 left_right = step(d.xy, d.yx);
	vec2 rounding = (1.0 - SMAA_CORNER_ROUNDING_NORM) * left_right;

	rounding /= left_right.x + left_right.y;

	vec2 factor = vec2(1.0, 1.0);
	factor.x -= rounding.x * textureLodOffset(edges_tex, coord.xy, 0.0, ivec2(0, 1)).r;
	factor.x -= rounding.y * textureLodOffset(edges_tex, coord.zw, 0.0, ivec2(1, 1)).r;
	factor.y -= rounding.x * textureLodOffset(edges_tex, coord.xy, 0.0, ivec2(0, -2)).r;
	factor.y -= rounding.y * textureLodOffset(edges_tex, coord.zw, 0.0, ivec2(1, -2)).r;

	weights *= clamp(factor, 0.0, 1.0);
}

void SMAADetectVerticalCornerPattern(inout vec2 weights, vec4 coord, vec2 d) {
	vec2 left_right = step(d.xy, d.yx);
	vec2 rounding = (1.0 - SMAA_CORNER_ROUNDING_NORM) * left_right;

	rounding /= left_right.x + left_right.y;

	vec2 factor = vec2(1.0, 1.0);
	factor.x -= rounding.x * textureLodOffset(edges_tex, coord.xy, 0.0, ivec2(1, 0)).g;
	factor.x -= rounding.y * textureLodOffset(edges_tex, coord.zw, 0.0, ivec2(1, 1)).g;
	factor.y -= rounding.x * textureLodOffset(edges_tex, coord.xy, 0.0, ivec2(-2, 0)).g;
	factor.y -= rounding.y * textureLodOffset(edges_tex, coord.zw, 0.0, ivec2(-2, 1)).g;

	weights *= clamp(factor, 0.0, 1.0);
}

void main() {
	weights = vec4(0.0, 0.0, 0.0, 0.0);
	vec2 e = textureLod(edges_tex, tex_coord, 0.0).rg;

	if (e.g > 0.0) { // Edge at north
		weights.rg = SMAACalculateDiagWeights(tex_coord, e, params.subsample_indices);
		if (weights.r == -weights.g) {
			vec2 d;
			vec3 coords;
			coords.x = SMAASearchXLeft(offset[0].xy, offset[2].x);
			coords.y = offset[1].y;
			d.x = coords.x;

			float e1 = textureLod(edges_tex, coords.xy, 0.0).r;

			coords.z = SMAASearchXRight(offset[0].zw, offset[2].y);
			d.y = coords.z;

			d = abs(round(fma(params.size.xx, d, -pix_coord.xx)));

			vec2 sqrt_d = sqrt(d);

			float e2 = textureLodOffset(edges_tex, coords.zy, 0.0, ivec2(1, 0)).r;

			weights.rg = SMAAArea(sqrt_d, e1, e2, params.subsample_indices.y);

			coords.y = tex_coord.y;
			SMAADetectHorizontalCornerPattern(weights.rg, coords.xyzy, d);
		} else {
			e.r = 0.0;
		}
	}

	if (e.r > 0.0) { // Edge at west
		vec2 d;
		vec3 coords;
		coords.y = SMAASearchYUp(offset[1].xy, offset[2].z);
		coords.x = offset[0].x;
		d.x = coords.y;

		float e1 = textureLod(edges_tex, coords.xy, 0.0).g;

		coords.z = SMAASearchYDown(offset[1].zw, offset[2].w);
		d.y = coords.z;

		d = abs(round(fma(params.size.yy, d, -pix_coord.yy)));

		vec2 sqrt_d = sqrt(d);

		float e2 = textureLodOffset(edges_tex, coords.xz, 0.0, ivec2(0, 1)).g;

		weights.ba = SMAAArea(sqrt_d, e1, e2, params.subsample_indices.x);

		coords.x = tex_coord.x;
		SMAADetectVerticalCornerPattern(weights.ba, coords.xyxz, d);
	}
}
