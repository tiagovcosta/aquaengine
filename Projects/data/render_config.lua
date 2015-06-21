
render_shaders = {
	"data/shaders/model.cshader",
	--"data/shaders/debug.cshader",
	"data/shaders/aa_lines.cshader",
	"data/shaders/lines.cshader",
	"data/shaders/text.cshader",
	"data/shaders/update_rayleigh.cshader",
	"data/shaders/update_mie.cshader",
	"data/shaders/dynamic_sky.cshader",
	"data/shaders/terrain.cshader",
	"data/shaders/apply_gamma.cshader",
	"data/shaders/particles_render.cshader",
	"data/shaders/noise.cshader",
	"data/shaders/ssao.cshader",
	"data/shaders/ssao_blur.cshader",

	"data/shaders/downscale.cshader",
	"data/shaders/accumulation.cshader",
	"data/shaders/bilateral_blur.cshader",

	"data/shaders/screen_space_reflections.cshader",
	"data/shaders/screen_space_reflections_blur.cshader",
	
	"data/shaders/reflections_composite.cshader",

	"data/shaders/camera_velocity.cshader",

	--post process
	"data/shaders/color_coc.cshader",
	"data/shaders/dof_blur.cshader",
	"data/shaders/dof_composite.cshader",

	"data/shaders/tile_max.cshader",
	"data/shaders/neighbor_max.cshader",
	"data/shaders/motion_blur.cshader",

	"data/shaders/luminance.cshader",
	"data/shaders/eye_adapt.cshader",
	"data/shaders/tonemap.cshader",
	"data/shaders/bright_pass.cshader",
	"data/shaders/bloom_blur.cshader",
}

compute_shaders = {
	"data/shaders/test_compute.cshader",
	"data/shaders/tiled_deferred.cshader",
	"data/shaders/count_to_buffer.cshader",
	"data/shaders/particles_compute.cshader"
}

render_settings = {
	back_buffer_width         = window_width,
	back_buffer_height        = window_height,
	--back_buffer_width         = Settings.window_width,
	--back_buffer_height        = Settings.window_height,
	--back_buffer_width         = 1920,
	--back_buffer_height        = 1080,
	back_buffer_render_target = true,
	back_buffer_uav           = true
}

function getRenderTargetsDescs()

	return {
		{ name = "diffuse", format = Renderer.Format.RGBA8, width = render_settings.back_buffer_width, height = render_settings.back_buffer_height },
		{ name = "normals", format = Renderer.Format.RGBA16, width = render_settings.back_buffer_width, height = render_settings.back_buffer_height },
		{ name = "depth", format = Renderer.Format.R32, width = render_settings.back_buffer_width, height = render_settings.back_buffer_height }
	}
end

function getDepthStencilTargetsDescs()

	return {
		{ name = "main_dst", shader_resource = true, width = render_settings.back_buffer_width, height = render_settings.back_buffer_height }
	}
end

--[[
resource_generators = {
	"shadow_map" = { 
		params = {shadow_render_queue = "pointer"},
		func = function(renderer, args)
			render_queue = StructuredBlob.get(args, "shadow_render_queue");
			Renderer.render(render_queue, viewport);
		end
	}
}
]]--