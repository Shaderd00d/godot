/**************************************************************************/
/*  editor_screenshot_plugin.cpp                                          */
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

#include "editor_screenshot_plugin.h"

#include "core/config/project_settings.h"
#include "core/error/error_list.h"
#include "core/error/error_macros.h"
#include "core/io/image.h"
#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include "core/os/time.h"
#include "core/string/node_path.h"
#include "core/string/ustring.h"
#include "core/variant/variant.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/plugins/node_3d_editor_plugin.h"
#include "scene/3d/camera_3d.h"
#include "servers/rendering_server.h"

ScreenshotPlugin *ScreenshotPlugin::singleton = nullptr;

ScreenshotPlugin::ScreenshotPlugin() {
	CRASH_COND_MSG(singleton != nullptr, "Instantiating a new ScreenshotSaver singleton is not supported.");
	singleton = this;

	// Canvas 2D
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "editor/screenshot/canvas/antialiasing/msaa", PROPERTY_HINT_ENUM, String::utf8("Disabled,2×,4×,8x")), 3);

	// Spatial 3D
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "editor/screenshot/spatial/size/viewport_width", PROPERTY_HINT_RANGE, "320,4096,1"), 1920);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "editor/screenshot/spatial/size/viewport_height", PROPERTY_HINT_RANGE, "320,4096,1"), 1080);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::FLOAT, "editor/screenshot/spatial/size/3d_scale", PROPERTY_HINT_RANGE, "1.0,2.0,0.01"), 1.0f);
	GLOBAL_DEF_BASIC("editor/screenshot/spatial/hide_gizmos/misc_tools", true);
	GLOBAL_DEF_BASIC("editor/screenshot/spatial/hide_gizmos/grid_layer", true);
	GLOBAL_DEF_BASIC("editor/screenshot/spatial/hide_gizmos/edit_layer", true);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "editor/screenshot/spatial/antialiasing/msaa", PROPERTY_HINT_ENUM, String::utf8("Disabled,2×,4×,8x")), 3);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "editor/screenshot/spatial/antialiasing/screen_space_aa", PROPERTY_HINT_ENUM, "Disabled,FXAA"), 0);
	GLOBAL_DEF("editor/screenshot/spatial/antialiasing/use_taa", false);
	GLOBAL_DEF("editor/screenshot/spatial/antialiasing/use_debanding", false);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "editor/screenshot/spatial/textures/anisotropic_filtering_level", PROPERTY_HINT_ENUM, String::utf8("Disabled,2×,4×,8×,16x")), 2);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "editor/screenshot/spatial/textures/texture_mipmap_bias", PROPERTY_HINT_RANGE, "-2,2,0.001"), 0.0f);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "editor/screenshot/spatial/lod_change/mesh_lod_threshold_pixels", PROPERTY_HINT_RANGE, "0,1024,0.1"), 1.0);
}

ScreenshotPlugin *ScreenshotPlugin::get_singleton() {
	if (singleton == nullptr) {
		singleton = memnew(ScreenshotPlugin);
	}
	return singleton;
}

void ScreenshotPlugin::set_3d_gizmos_visibility(int viewport_index, bool visible) {
	Camera3D *camera_3d = viewport->get_camera_3d();

	if (visible) {
		camera_3d->set_cull_mask(cull_mask);
	} else {
		uint32_t cm = cull_mask = camera_3d->get_cull_mask();
		if (GLOBAL_GET("editor/screenshot/spatial/hide_gizmos/misc_tools")) {
			cm &= ~(1 << Node3DEditorViewport::MISC_TOOL_LAYER);
		}
		if (GLOBAL_GET("editor/screenshot/spatial/hide_gizmos/grid_layer")) {
			cm &= ~(1 << Node3DEditorViewport::GIZMO_GRID_LAYER);
		}
		if (GLOBAL_GET("editor/screenshot/spatial/hide_gizmos/edit_layer")) {
			cm &= ~(1 << Node3DEditorViewport::GIZMO_EDIT_LAYER);
		}
		cm &= ~(1 << (Node3DEditorViewport::GIZMO_BASE_LAYER + viewport_index));
		camera_3d->set_cull_mask(cm);
	}
}

void ScreenshotPlugin::set_2d_quality() {
	msaa = viewport->get_msaa_2d();
	const int temp_msaa = GLOBAL_GET("editor/screenshot/canvas/antialiasing/msaa");
	if (temp_msaa != msaa) {
		viewport->set_msaa_2d((Viewport::MSAA)temp_msaa);
	}
}

void ScreenshotPlugin::reset_2d_quality() {
	const int temp_msaa = GLOBAL_GET("editor/screenshot/canvas/antialiasing/msaa");
	if (temp_msaa != msaa) {
		viewport->set_msaa_2d((Viewport::MSAA)msaa);
	}
}

void ScreenshotPlugin::set_3d_quality() {
	Size2 viewport_size = viewport->get_visible_rect().size;
	viewport_width = viewport_size.x;
	viewport_height = viewport_size.y;
	const int temp_width = GLOBAL_GET("editor/screenshot/spatial/size/viewport_width");
	const int temp_height = GLOBAL_GET("editor/screenshot/spatial/size/viewport_height");
	if (temp_width != viewport_width || temp_height != viewport_height) {
		RenderingServer::get_singleton()->viewport_set_size(viewport->get_viewport_rid(), temp_width, temp_height);
	}
	scaling_3d_scale = viewport->get_scaling_3d_scale();
	const float temp_scaling_3d_scale = GLOBAL_GET("editor/screenshot/spatial/size/3d_scale");
	if (temp_scaling_3d_scale != scaling_3d_scale) {
		scaling_3d_mode = viewport->get_scaling_3d_mode();
		viewport->set_scaling_3d_mode(Viewport::SCALING_3D_MODE_BILINEAR);
		viewport->set_scaling_3d_scale(temp_scaling_3d_scale);
	}
	msaa = viewport->get_msaa_3d();
	const int temp_msaa = GLOBAL_GET("editor/screenshot/spatial/antialiasing/msaa");
	if (temp_msaa != msaa) {
		viewport->set_msaa_3d((Viewport::MSAA)temp_msaa);
	}
	const int temp_screen_space_aa = GLOBAL_GET("editor/screenshot/spatial/antialiasing/screen_space_aa");
	screen_space_aa = viewport->get_screen_space_aa();
	if (temp_screen_space_aa != screen_space_aa) {
		viewport->set_screen_space_aa((Viewport::ScreenSpaceAA)temp_screen_space_aa);
	}
	const bool temp_use_taa = GLOBAL_GET("editor/screenshot/spatial/antialiasing/use_taa");
	use_taa = viewport->is_using_taa();
	if (temp_use_taa != use_taa) {
		viewport->set_use_taa(temp_use_taa);
	}
	const bool temp_use_debanding = GLOBAL_GET("editor/screenshot/spatial/antialiasing/use_debanding");
	use_debanding = viewport->is_using_debanding();
	if (temp_use_debanding != use_debanding) {
		viewport->set_use_debanding(temp_use_debanding);
	}
	const int temp_anisotropic_filtering_level = GLOBAL_GET("editor/screenshot/spatial/textures/anisotropic_filtering_level");
	anisotropic_filtering_level = viewport->get_anisotropic_filtering_level();
	if (temp_anisotropic_filtering_level != anisotropic_filtering_level) {
		viewport->set_anisotropic_filtering_level((Viewport::AnisotropicFiltering)temp_anisotropic_filtering_level);
	}
	const float temp_texture_mipmap_bias = GLOBAL_GET("editor/screenshot/spatial/textures/texture_mipmap_bias");
	texture_mipmap_bias = viewport->get_texture_mipmap_bias();
	if (temp_texture_mipmap_bias != texture_mipmap_bias) {
		viewport->set_texture_mipmap_bias(temp_texture_mipmap_bias);
	}
	const float temp_mesh_lod_threshold = GLOBAL_GET("editor/screenshot/spatial/lod_change/mesh_lod_threshold_pixels");
	mesh_lod_threshold = viewport->get_mesh_lod_threshold();
	if (temp_mesh_lod_threshold != mesh_lod_threshold) {
		viewport->set_mesh_lod_threshold(temp_mesh_lod_threshold);
	}
}

void ScreenshotPlugin::reset_3d_quality() {
	const int temp_width = GLOBAL_GET("editor/screenshot/spatial/size/viewport_width");
	const int temp_height = GLOBAL_GET("editor/screenshot/spatial/size/viewport_height");
	if (temp_width != viewport_width || temp_height != viewport_height) {
		RenderingServer::get_singleton()->viewport_set_size(viewport->get_viewport_rid(), viewport_width, viewport_height);
	}
	const float temp_scaling_3d_scale = GLOBAL_GET("editor/screenshot/spatial/size/3d_scale");
	if (temp_scaling_3d_scale != scaling_3d_scale) {
		viewport->set_scaling_3d_scale(scaling_3d_scale);
		viewport->set_scaling_3d_mode(scaling_3d_mode);
	}
	const int temp_msaa = GLOBAL_GET("editor/screenshot/spatial/antialiasing/msaa");
	if (temp_msaa != msaa) {
		viewport->set_msaa_3d((Viewport::MSAA)msaa);
	}
	const int temp_screen_space_aa = GLOBAL_GET("editor/screenshot/spatial/antialiasing/screen_space_aa");
	if (temp_screen_space_aa != screen_space_aa) {
		viewport->set_screen_space_aa((Viewport::ScreenSpaceAA)screen_space_aa);
	}
	const bool temp_use_taa = GLOBAL_GET("editor/screenshot/spatial/antialiasing/use_taa");
	if (temp_use_taa != use_taa) {
		viewport->set_use_taa(use_taa);
	}
	const bool temp_use_debanding = GLOBAL_GET("editor/screenshot/spatial/antialiasing/use_debanding");
	if (temp_use_debanding != use_debanding) {
		viewport->set_use_debanding(use_debanding);
	}
	const int temp_anisotropic_filtering_level = GLOBAL_GET("editor/screenshot/spatial/textures/anisotropic_filtering_level");
	if (temp_anisotropic_filtering_level != anisotropic_filtering_level) {
		viewport->set_anisotropic_filtering_level(anisotropic_filtering_level);
	}
	const float temp_texture_mipmap_bias = GLOBAL_GET("editor/screenshot/spatial/textures/texture_mipmap_bias");
	if (temp_texture_mipmap_bias != texture_mipmap_bias) {
		viewport->set_texture_mipmap_bias(texture_mipmap_bias);
	}
	const float temp_mesh_lod_threshold = GLOBAL_GET("editor/screenshot/spatial/lod_change/mesh_lod_threshold_pixels");
	if (temp_mesh_lod_threshold != mesh_lod_threshold) {
		viewport->set_mesh_lod_threshold(mesh_lod_threshold);
	}
}

void ScreenshotPlugin::take_screenshot(ScreenshotSource source, Viewport *p_viewport, int viewport_index) {
	ERR_FAIL_COND_MSG(p_viewport == nullptr, "A valid viewport is required to take a screenshot.");
	screenshot_source = source;
	viewport = p_viewport;
	viewport_idx = viewport_index;
	if (screenshot_source == ScreenshotSource::CANVAS_EDITOR) {
		set_2d_quality();
	}
	if (screenshot_source == ScreenshotSource::SPATIAL_EDITOR) {
		set_3d_gizmos_visibility(viewport_index, false);
		set_3d_quality();
	}
	RenderingServer::get_singleton()->draw();
	RenderingServer::get_singleton()->sync();
	save_screenshot();
}

void ScreenshotPlugin::save_screenshot() {
	Ref<ViewportTexture> texture = viewport->get_texture();
	ERR_FAIL_COND_MSG(texture.is_null(), "Cannot get a viewportTexture from the Viewport.");
	Ref<Image> img = texture->get_image();
	ERR_FAIL_COND_MSG(img.is_null(), "Cannot get an Image from the ViewportTexture.");
	String file_prefix = "editor";
	if (screenshot_source == ScreenshotSource::CANVAS_EDITOR) {
		file_prefix = "canvas";
		reset_2d_quality();
	}
	if (screenshot_source == ScreenshotSource::SPATIAL_EDITOR) {
		file_prefix = "spatial";
		reset_3d_quality();
		set_3d_gizmos_visibility(viewport_idx, true);
	}
	String filename = file_prefix + "_screenshot_" + Time::get_singleton()->get_datetime_string_from_system().remove_char(':') + ".png";
	NodePath path = String("user://") + filename;
	Error error = img->save_png(path);
	ERR_FAIL_COND_MSG(error != OK, "Cannot save screenshot to file '" + path + "'.");
	if (EDITOR_GET("interface/editor/automatically_open_screenshots")) {
		OS::get_singleton()->shell_show_in_file_manager(ProjectSettings::get_singleton()->globalize_path(path), true);
	}
}
