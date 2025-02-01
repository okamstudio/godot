/**************************************************************************/
/*  rendering_server_globals.h                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "servers/rendering/environment/renderer_fog.h"
#include "servers/rendering/environment/renderer_gi.h"
#include "servers/rendering/renderer_canvas_cull.h"
#include "servers/rendering/renderer_canvas_render.h"
#include "servers/rendering/rendering_method.h"
#include "servers/rendering/storage/camera_attributes_storage.h"
#include "servers/rendering/storage/light_storage.h"
#include "servers/rendering/storage/material_storage.h"
#include "servers/rendering/storage/mesh_storage.h"
#include "servers/rendering/storage/particles_storage.h"
#include "servers/rendering/storage/texture_storage.h"
#include "servers/rendering/storage/utilities.h"

class RendererCanvasCull;
class RendererViewport;
class RenderingMethod;

class RenderingServerGlobals {
public:
	static bool threaded;

	static RendererUtilities *utilities;
	static RendererLightStorage *light_storage;
	static RendererMaterialStorage *material_storage;
	static RendererMeshStorage *mesh_storage;
	static RendererParticlesStorage *particles_storage;
	static RendererTextureStorage *texture_storage;
	static RendererGI *gi;
	static RendererFog *fog;
	static RendererCameraAttributes *camera_attributes;
	static RendererCanvasRender *canvas_render;
	static RendererCompositor *rasterizer;

	static RendererCanvasCull *canvas;
	static RendererViewport *viewport;
	static RenderingMethod *scene;
};

#define RSG RenderingServerGlobals
