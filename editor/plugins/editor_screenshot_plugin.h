/**************************************************************************/
/*  editor_screenshot_plugin.h                                            */
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

#include "editor/plugins/editor_plugin.h"
#include "scene/main/viewport.h"

class ScreenshotPlugin : public EditorPlugin {
	GDCLASS(ScreenshotPlugin, EditorPlugin);

	static ScreenshotPlugin *singleton;

	Viewport *viewport = nullptr;
	int viewport_idx = -1;
	uint32_t cull_mask;
	int viewport_width;
	int viewport_height;
	int msaa;
	Viewport::Scaling3DMode scaling_3d_mode;
	float scaling_3d_scale;
	Viewport::AnisotropicFiltering anisotropic_filtering_level;
	Viewport::ScreenSpaceAA screen_space_aa;
	bool use_taa;
	float texture_mipmap_bias;
	bool use_debanding;
	float mesh_lod_threshold;
	void set_2d_quality();
	void reset_2d_quality();
	void set_3d_gizmos_visibility(int viewport_index, bool visible);
	void set_3d_quality();
	void reset_3d_quality();
	void save_screenshot();

public:
	ScreenshotPlugin();

	enum ScreenshotSource {
		EDITOR,
		CANVAS_EDITOR,
		SPATIAL_EDITOR,
	};

	static ScreenshotPlugin *get_singleton();

	void take_screenshot(ScreenshotSource source, Viewport *p_viewport, int viewport_index = -1);

private:
	ScreenshotSource screenshot_source;
};
