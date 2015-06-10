
#include "MainViewGenerator.h"

#include <DevTools\TextRenderer.h>
#include <DevTools\Profiler.h>

#include <AquaGame.h>

#include <Generators\CSMUtilties.h>
#include <Generators\ShadowMapGenerator.h>
#include <Generators\ScreenSpaceReflections.h>

#include <Generators\PostProcess\DepthOfField.h>
#include <Generators\PostProcess\ToneMapper.h>

#include <DynamicSky.h>
#include <Terrain.h>

#include <Resources\TextureManager.h>
#include <Resources\FontManager.h>
#include <PrimitiveMeshManager.h>

#include <Components\VolumetricLightManager.h>

#include <Components\LightManager.h>
#include <Components\ModelManager.h>
#include <Components\PhysicsManager.h>
#include <Components\TransformManager.h>
#include <Components\EntityManager.h>

#include <Renderer\ParameterCache.h>
#include <Renderer\RendererStructs.h>
#include <Renderer\RenderDevice\ParameterGroup.h>

#include <Core\JobManager.h>
#include <Core\ThreadLocalArray.h>
#include <Core\Allocators\FreeListAllocator.h>
#include <Core\Allocators\DynamicLinearAllocator.h>
#include <Core\Allocators\ProxyAllocator.h>

#include <Utilities\StringID.h>
#include <Utilities\FileLoader.h>

#include <AquaMath.h>

#include <AntTweakBar.h>

#include <iostream>

#if AQUA_DEBUG || AQUA_DEVELOPMENT
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#define ENABLE_TERRAIN 0

//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------

using namespace aqua;

static const int NUM_TEST_COLORS = 14;

static const Vector3 TEST_COLORS[NUM_TEST_COLORS] =
{
	{ 0.0f, 0.9255f, 0.9255f },   // cyan
	{ 0.0f, 0.62745f, 0.9647f },  // light blue
	{ 0.0f, 0.0f, 0.9647f },      // blue
	{ 0.0f, 1.0f, 0.0f },         // bright green
	{ 0.0f, 0.7843f, 0.0f },      // green
	{ 0.0f, 0.5647f, 0.0f },      // dark green
	{ 1.0f, 1.0f, 0.0f },         // yellow
	{ 0.90588f, 0.75294f, 0.0f }, // yellow-orange
	{ 1.0f, 0.5647f, 0.0f },      // orange
	{ 1.0f, 0.0f, 0.0f },         // bright red
	{ 0.8392f, 0.0f, 0.0f },      // red
	{ 0.75294f, 0.0f, 0.0f },     // dark red
	{ 1.0f, 0.0f, 1.0f },         // magenta
	{ 0.6f, 0.3333f, 0.7882f },   // purple
};

class BasicExample : public AquaGame
{
public:
	bool init() override
	{
		if(!AquaGame::init())
			return false;

		RenderDevice& render_device = *_renderer.getRenderDevice();

		//------------------------------------------
		// INIT COMPONENT MANAGERS
		//------------------------------------------

		_entity_manager_allocator    = allocator::allocateNew<ProxyAllocator>(*_main_allocator, *_main_allocator);
		_entity_manager              = allocator::allocateNew<EntityManager>(*_main_allocator, *_entity_manager_allocator);

		_transform_manager_allocator = allocator::allocateNew<ProxyAllocator>(*_main_allocator, *_main_allocator);
		_transform_manager           = allocator::allocateNew<TransformManager>(*_main_allocator, *_transform_manager_allocator, 1024);

		_physics_manager_allocator   = allocator::allocateNew<ProxyAllocator>(*_main_allocator, *_main_allocator);
		_physics_manager             = allocator::allocateNew<PhysicsManager>(*_main_allocator, *_physics_manager_allocator, 1024, 4);

		_model_manager_allocator     = allocator::allocateNew<ProxyAllocator>(*_main_allocator, *_main_allocator);
		_model_manager               = allocator::allocateNew<ModelManager>(*_main_allocator, *_model_manager_allocator,
																			*_scratchpad_allocator, _renderer,
																			*_transform_manager, 1024);

		_renderer.addRenderQueueGenerator(getStringID("model_queue_generator"), _model_manager);

		_light_manager_allocator = allocator::allocateNew<ProxyAllocator>(*_main_allocator, *_main_allocator);
		_light_manager           = allocator::allocateNew<LightManager>(*_main_allocator, *_light_manager_allocator,
																		*_transform_manager, _renderer, 1024);

		_renderer.addResourceGenerator(getStringID("light_generator"), _light_manager);

		//---------------------------------------------------------------------------------

		//------------------------------------------
		// INIT RESOURCE GENERATORS
		//------------------------------------------

		_shadow_map_generator_allocator = allocator::allocateNew<ProxyAllocator>(*_main_allocator, *_main_allocator);

		_shadow_map_generator.init(_renderer, _lua_state, *_shadow_map_generator_allocator, *_scratchpad_allocator);

		_renderer.addResourceGenerator(getStringID("shadow_map"), &_shadow_map_generator);

		//---------------------------------------------------------------------------------

		_screen_space_reflections.init(_renderer, _lua_state, *_main_allocator, *_scratchpad_allocator, _wnd_width, _wnd_height);

		_renderer.addResourceGenerator(getStringID("screen_space_reflections"), &_screen_space_reflections);

		

		//---------------------------------------------------------------------------------

		_volumetric_light_allocator = allocator::allocateNew<ProxyAllocator>(*_main_allocator, *_main_allocator);
		_volumetric_light_manager.init(_renderer, nullptr, *_volumetric_light_allocator, *_scratchpad_allocator, *_light_manager);

		_renderer.addResourceGenerator(getStringID("volumetric_lights"), &_volumetric_light_manager);

		//---------------------------------------------------------------------------------

		_main_view_allocator = allocator::allocateNew<ProxyAllocator>(*_main_allocator, *_main_allocator);

		_main_view_generator.init(_renderer, _lua_state, *_main_view_allocator, *_scratchpad_allocator,
								  *_light_manager, _wnd_width, _wnd_height);

		_renderer.addResourceGenerator(getStringID("main_view_generator"), &_main_view_generator);

		//---------------------------------------------------------------------------------
		// INIT RESOURCE MANAGERS
		//---------------------------------------------------------------------------------

		_texture_manager.init(*_main_allocator, *_scratchpad_allocator, _renderer);

		_font_manager = allocator::allocateNew<FontManager>(*_main_allocator, *_main_allocator, render_device);

		_primtive_mesh_manager = allocator::allocateNew<PrimitiveMeshManager>(*_main_allocator, *_main_allocator,
																			  *_scratchpad_allocator, render_device);

		//---------------------------------------------------------------------------------

		_terrain = allocator::allocateNew<Terrain>(*_main_allocator, *_main_allocator, *_scratchpad_allocator, _renderer);

		_renderer.addRenderQueueGenerator(getStringID("terrain"), _terrain);

		//---------------------------------------------------------------------------------

		_dynamic_sky = allocator::allocateNew<DynamicSky>(*_main_allocator, *_main_allocator,
			*_scratchpad_allocator, _renderer);

		_renderer.addRenderQueueGenerator(getStringID("dynamic_sky"), _dynamic_sky);

		//---------------------------------------------------------------------------------

		_depth_of_field.init(_renderer, _lua_state, *_main_allocator, *_scratchpad_allocator, _wnd_width, _wnd_height);

		_renderer.addResourceGenerator(getStringID("depth_of_field"), &_depth_of_field);

		//---------------------------------------------------------------------------------

		_tone_mapper.init(_renderer, _lua_state, *_main_allocator, *_scratchpad_allocator, _wnd_width, _wnd_height);

		_renderer.addResourceGenerator(getStringID("tone_mapper"), &_tone_mapper);

		//---------------------------------------------------------------------------------

		_text_renderer = allocator::allocateNew<TextRenderer>(*_main_allocator, *_main_allocator,
			*_scratchpad_allocator, _renderer, *_font_manager);

		_renderer.addResourceGenerator(getStringID("text_renderer"), _text_renderer);

		//----------------------------------------------------------------------------------
		// INIT SCENE
		//----------------------------------------------------------------------------------

		_camera.setPerspective(75 * DirectX::XM_PI / 180, (float)1280 / 720, 0.1f, 2000.0f);
		_camera.update();

		_camera_entity        = _entity_manager->create();
		auto camera_transform = _transform_manager->create(_camera_entity);
		_transform_manager->translate(camera_transform, Vector3(0, 1.8f, -5.0f));
		//_transform_manager->setLocalPosition(camera_transform, Vector3(0, 22, -50.0f));
		//_transform_manager->rotate(camera_transform, Quaternion::CreateFromYawPitchRoll(0.0f, 3.14 * 90 / 180, 0.0f), true);

		createMaterials();

		//Create basic physic material
		PhysicMaterialDesc physic_material_desc;
		physic_material_desc.static_friction  = 0.1f;
		physic_material_desc.dynamic_friction = 0.1f;
		physic_material_desc.restitution      = 0.8f;

		_physic_material = _physics_manager->createMaterial(physic_material_desc);

		//Get model shader parameters desc
		auto model_shader = _renderer.getShaderManager()->getRenderShader(getStringID("data/shaders/model.cshader"));

		auto model_material_params_desc_set = model_shader->getMaterialParameterGroupDescSet();
		auto model_material_params_desc     = getParameterGroupDesc(*model_material_params_desc_set, 0);

		//Ground
		{
			auto ground_plane     = _entity_manager->create();
			auto ground_transform = _transform_manager->create(ground_plane);
			auto groud_model      = _model_manager->create(ground_plane, _primtive_mesh_manager->getPlane(), 0);

			_transform_manager->translate(ground_transform, Vector3(-1.0f, 0.0f, 9.0f));
			_transform_manager->scale(ground_transform, Vector3(250.0f, 1.0f, 250.0f));

			_model_manager->addSubset(groud_model, 0, &_ground_material);

			//Ground rigid actor
			auto ground_shape = _physics_manager->createBoxShape(100.0f, 0.5f, 100.0f, _physic_material);

			auto ground_rigid_actor = _physics_manager->createStatic(ground_plane, ground_shape, Vector3(0.0f, -0.5f, 0.0f));
		}

		//--------------------
		// BOXES "WALL"
		//--------------------
		{
			const Vector3 WALL_ORIGIN(0.0f, 0.0f, -10.0f);

			const float SIDE_BLOCK_WIDTH  = 4.0f;
			const float SIDE_BLOCK_HEIGHT = 12.0f;

			//left
			auto box           = _entity_manager->create();
			auto box_transform = _transform_manager->create(box);
			auto box_model     = _model_manager->create(box, _primtive_mesh_manager->getBox(), 0);

			_transform_manager->scale(box_transform, Vector3(SIDE_BLOCK_WIDTH, SIDE_BLOCK_HEIGHT, 1.0f));
			_transform_manager->translate(box_transform, WALL_ORIGIN, true);

			_model_manager->addSubset(box_model, 0, &_red_material);

			auto physic_shape = _physics_manager->createBoxShape(SIDE_BLOCK_WIDTH / 2, SIDE_BLOCK_HEIGHT / 2, 0.5f, _physic_material);
			auto rigid_actor  = _physics_manager->createStatic(box, physic_shape, WALL_ORIGIN + Vector3(0.0f, SIDE_BLOCK_HEIGHT / 2, 0.0f));

			//right
			box           = _entity_manager->create();
			box_transform = _transform_manager->create(box);
			box_model     = _model_manager->create(box, _primtive_mesh_manager->getBox(), 0);

			_transform_manager->scale(box_transform, Vector3(SIDE_BLOCK_WIDTH, SIDE_BLOCK_HEIGHT, 1.0f));
			_transform_manager->translate(box_transform, WALL_ORIGIN + Vector3(2 * SIDE_BLOCK_WIDTH, 0.0f, 0.0f), true);

			_model_manager->addSubset(box_model, 0, &_red_material);

			rigid_actor = _physics_manager->createStatic(box, physic_shape, 
														 WALL_ORIGIN + Vector3(2 * SIDE_BLOCK_WIDTH, SIDE_BLOCK_HEIGHT / 2, 0.0f));

			//------------------------------------

			const float MIDDLE_BLOCK_WIDTH  = 4.0f;
			const float MIDDLE_BLOCK_HEIGHT = 4.0f;

			// middle physic shape
			physic_shape = _physics_manager->createBoxShape(MIDDLE_BLOCK_WIDTH / 2, MIDDLE_BLOCK_HEIGHT / 2, 0.5f, _physic_material);

			//middle top
			box           = _entity_manager->create();
			box_transform = _transform_manager->create(box);
			box_model     = _model_manager->create(box, _primtive_mesh_manager->getBox(), 0);

			_transform_manager->scale(box_transform, Vector3(MIDDLE_BLOCK_WIDTH, MIDDLE_BLOCK_HEIGHT, 1.0f));
			_transform_manager->translate(box_transform, WALL_ORIGIN + Vector3(MIDDLE_BLOCK_WIDTH, 2 * MIDDLE_BLOCK_HEIGHT, 0.0f), true);

			_model_manager->addSubset(box_model, 0, &_player_material);

			rigid_actor = _physics_manager->createStatic(box, physic_shape, 
														 WALL_ORIGIN + Vector3(MIDDLE_BLOCK_WIDTH, 5 * MIDDLE_BLOCK_HEIGHT / 2, 0.0f));

			//middle bottom
			box           = _entity_manager->create();
			box_transform = _transform_manager->create(box);
			box_model     = _model_manager->create(box, _primtive_mesh_manager->getBox(), 0);

			_transform_manager->scale(box_transform, Vector3(MIDDLE_BLOCK_WIDTH, MIDDLE_BLOCK_HEIGHT, 1.0f));
			_transform_manager->translate(box_transform, WALL_ORIGIN + Vector3(MIDDLE_BLOCK_WIDTH, 0.0f, 0.0f), true);

			_model_manager->addSubset(box_model, 0, &_player_material);

			rigid_actor = _physics_manager->createStatic(box, physic_shape, 
														 WALL_ORIGIN + Vector3(MIDDLE_BLOCK_WIDTH, MIDDLE_BLOCK_HEIGHT / 2, 0.0f));
		}
		/*
		for(int i = 0; i < BOXES_WALL_WIDTH; i++)
		{
			for(int j = 0; j < BOXES_WINDOW_HEIGHT; j++)
			{
				if(i > BOXES_WALL_WIDTH / 2 - 2 && i < BOXES_WALL_WIDTH / 2 + 2 &&
					j > 2 && j < BOXES_WINDOW_HEIGHT - 3)
					continue;

				auto box           = _entity_manager->create();
				auto box_transform = _transform_manager->create(box);
				auto box_model     = _model_manager->create(box, _primtive_mesh_manager->getBox(), 0);

				_transform_manager->translate(box_transform, Vector3(i - BOXES_WALL_WIDTH, j, -10.0f));
				//_transform_manager->rotate(box_transform, Quaternion::CreateFromYawPitchRoll(3.14 * 45 / 180, 0, 0));

				_model_manager->addSubset(box_model, 0, &_red_material);
			}
		}
		*/
		// FENCE
		{
			auto fence           = _entity_manager->create();
			auto fence_transform = _transform_manager->create(fence);
			auto fence_model     = _model_manager->create(fence, _primtive_mesh_manager->getPlane(), 0);

			_transform_manager->translate(fence_transform, Vector3(0.0f, 0.50f, -3.0f));
			_transform_manager->rotate(fence_transform, Quaternion::CreateFromYawPitchRoll(0.0f, -3.14f * 90 / 180, 0.0f), true);
			_transform_manager->scale(fence_transform, Vector3(10.0f, 1.0f, 2.0f));

			_model_manager->addSubset(fence_model, 0, &_fence_material);

			//Fence rigid actor
			auto fence_shape = _physics_manager->createBoxShape(5.0f, 1.0f, 0.01f, _physic_material);

			auto fence_rigid_actor = _physics_manager->createStatic(fence, fence_shape, Vector3(0.0f, 1.0f, -3.0f));
		}

		auto box_physic_shape = _physics_manager->createBoxShape(0.5f, 0.5f, 0.5f, _physic_material);
		_physics_manager->setShapeLocalPose(box_physic_shape, Vector3(0.0f, 0.5f, 0.0f));

		// Reflective planes
		{
			//auto fence_shape = _physics_manager->createBoxShape(1.0f, 0.01f, 3.0f, physic_material);

			auto plane           = _entity_manager->create();
			auto plane_transform = _transform_manager->create(plane);
			auto plane_model     = _model_manager->create(plane, _primtive_mesh_manager->getPlane(), 0);

			_transform_manager->translate(plane_transform, Vector3(-2.0f, 0.01f, 0.0f));
			_transform_manager->scale(plane_transform, Vector3(2.0f, 1.0f, 6.0f));

			_model_manager->addSubset(plane_model, 0, &_metal1);

			//auto fence_rigid_actor = _physics_manager->createStatic(plane, fence_shape, Vector3(-1.0f, 0.01f, 0.0f));

			//--------------------------------------------------------

			plane           = _entity_manager->create();
			plane_transform = _transform_manager->create(plane);
			plane_model     = _model_manager->create(plane, _primtive_mesh_manager->getPlane(), 0);

			_transform_manager->translate(plane_transform, Vector3(0.0f, 0.01f, 0.0f));
			_transform_manager->scale(plane_transform, Vector3(2.0f, 1.0f, 6.0f));

			_model_manager->addSubset(plane_model, 0, &_metal2);

			//--------------------------------------------------------

			plane           = _entity_manager->create();
			plane_transform = _transform_manager->create(plane);
			plane_model     = _model_manager->create(plane, _primtive_mesh_manager->getPlane(), 0);

			_transform_manager->translate(plane_transform, Vector3(2.0f, 0.01f, 0.0f));
			_transform_manager->scale(plane_transform, Vector3(2.0f, 1.0f, 6.0f));

			_model_manager->addSubset(plane_model, 0, &_metal3);
		}

		// Create player-controlled sphere (using kinematic physic actor)
		{
			_player_box = _entity_manager->create();
			auto box_transform = _transform_manager->create(_player_box);
			auto box_model = _model_manager->create(_player_box, _primtive_mesh_manager->getSphere(), 0);

			_model_manager->addSubset(box_model, 0, &_player_material);

			auto rigid_actor = _physics_manager->create(_player_box, PhysicActorType::DYNAMIC);

			_physics_manager->addShape(rigid_actor, box_physic_shape);
			_physics_manager->setKinematic(rigid_actor, true);
		}

		//-----------------------------------------------------------------------------------------------------
		//Setup lights
		//-----------------------------------------------------------------------------------------------------

		auto light_entity    = _entity_manager->create();
		auto light_transform = _transform_manager->create(light_entity);
		auto light           = _light_manager->create(light_entity, LightManager::LightType::POINT);
		_transform_manager->translate(light_transform, Vector3(0, 2, 0));
		_light_manager->setRadius(light, 10);
		_light_manager->setColor(light, 255, 255, 255);

		auto spotlight = _light_manager->create(_camera_entity, LightManager::LightType::SPOT);
		_light_manager->setRadius(spotlight, 20);
		_light_manager->setColor(spotlight, 255, 255, 255);
		_light_manager->setAngle(spotlight, 45 * 3.14f / 180);

		_sun               = _entity_manager->create();
		auto sun_transform = _transform_manager->create(_sun);
		auto sun           = _light_manager->create(_sun, LightManager::LightType::DIRECTIONAL);
		_light_manager->setColor(sun, 255, 255, 255, 255);

		//Vector3 initial_sun_dir = Vector3(1.0f, 1.0f, 0.0f);
		Vector3 initial_sun_dir = Vector3(0.0f, 0.15f, 1.0f);
		initial_sun_dir.Normalize();

		_transform_manager->rotate(sun_transform, quaternionLookRotation(initial_sun_dir));

		_dynamic_sky->setSunDirection(initial_sun_dir);

		updateSunSkyColor();

		//--------------------------------------------------------------
		//--------------------------------------------------------------
		//--------------------------------------------------------------

		_light_manager->update();
		_model_manager->update();

		//--------------------------------------------------------------
		//--------------------------------------------------------------
		//--------------------------------------------------------------

		auto shader_manager = _renderer.getShaderManager();

		auto test_compute = shader_manager->getComputeShader(getStringID("data/shaders/test_compute.cshader"));
		auto kernel       = test_compute->getKernel(getStringID("noise_one_liner"));

		auto kernel_params_desc_set = test_compute->getKernelParameterGroupDescSet(kernel);
		auto kernel_params_desc     = getParameterGroupDesc(*kernel_params_desc_set, 0);

		_kernel_permutation = test_compute->getPermutation(kernel, 0);

		_kernel_params = _renderer.getRenderDevice()->createParameterGroup(*_main_allocator, RenderDevice::ParameterGroupType::INSTANCE,
																		   *kernel_params_desc, UINT32_MAX, 0, nullptr);

		_kernel_params->setUAV(_renderer.getBackBuffer().uav, 0);

		_noise_shader = shader_manager->getRenderShader(getStringID("data/shaders/noise.cshader"))->getPermutation(0);

		_runtime = 0.0f;

		TwBar* my_bar;
		my_bar = TwNewBar("Example");

		float bar_size[2];

		TwGetParam(my_bar, NULL, "size", TwParamValueType::TW_PARAM_FLOAT, 2, bar_size);

		bar_size[0] = _wnd_width - bar_size[0] - 10;
		bar_size[1] = 10;

		TwSetParam(my_bar, NULL, "position", TwParamValueType::TW_PARAM_FLOAT, 2, bar_size);

		TwAddVarRW(my_bar, "Volumetric Light", TW_TYPE_BOOL8, &_enable_volumetric_light, "");

		TwAddVarRW(my_bar, "Shoulder Strength", TW_TYPE_FLOAT, &_shoulder_strength, "step = 0.01");
		TwAddVarRW(my_bar, "Linear Strength", TW_TYPE_FLOAT, &_linear_strength, "step = 0.01");
		TwAddVarRW(my_bar, "Linear Angle", TW_TYPE_FLOAT, &_linear_angle, "step = 0.01");
		TwAddVarRW(my_bar, "Toe Strength", TW_TYPE_FLOAT, &_toe_strength, "step = 0.01");
		TwAddVarRW(my_bar, "Toe Rise", TW_TYPE_FLOAT, &_toe_rise, "step = 0.01");
		TwAddVarRW(my_bar, "Toe Run", TW_TYPE_FLOAT, &_toe_run, "step = 0.01");
		TwAddVarRW(my_bar, "Linear White", TW_TYPE_FLOAT, &_linear_white, "step = 0.01");
		TwAddVarRW(my_bar, "Middle Grey", TW_TYPE_FLOAT, &_middle_grey, "step = 0.01");

		_shoulder_strength = 0.15f;
		_linear_strength   = 0.50f;
		_linear_angle      = 0.10f;
		_toe_strength      = 0.20f;
		_toe_rise          = 0.02f;
		_toe_run           = 0.30f;
		_linear_white      = 22.2f;
		_middle_grey       = 0.4f;
		
		//---------------------------------------------------

		TwAddVarRW(my_bar, "Screen Space Reflections", TW_TYPE_BOOL8, &_enable_ssr, "");
		TwAddVarRW(my_bar, "SSR Thickness", TW_TYPE_FLOAT, &_thickness, "step = 0.01");

		_enable_ssr = true;
		_thickness  = 1.0f;

		//---------------------------------------------------

		TwAddVarRW(my_bar, "Focus Plane Z", TW_TYPE_FLOAT, &_focus_plane_z, "step = 0.01");
		TwAddVarRW(my_bar, "DOF Size", TW_TYPE_FLOAT, &_dof_size, "step = 0.01");
		TwAddVarRW(my_bar, "Near Blur Transition Size", TW_TYPE_FLOAT, &_near_blur_transition_size, "step = 0.01");
		TwAddVarRW(my_bar, "Far Blur Transition Size", TW_TYPE_FLOAT, &_far_blur_transition_size, "step = 0.01");
		TwAddVarRW(my_bar, "Near Blur Radius Fraction", TW_TYPE_FLOAT, &_near_blur_radius_fraction, "step = 0.01");
		TwAddVarRW(my_bar, "Far Blur Radius Fraction", TW_TYPE_FLOAT, &_far_blur_radius_fraction, "step = 0.01");

		_focus_plane_z             = 22.0f;
		_dof_size                  = 20.0f;
		_near_blur_transition_size = 1;
		_far_blur_transition_size  = 10;
		_near_blur_radius_fraction = 1.5f;
		_far_blur_radius_fraction = 1.0f;

		//------------------------------------------------------------------------------------------
		// INIT TERRAIN
		//------------------------------------------------------------------------------------------

		const char* height_map_name = "data/textures/wm3.dds";

		size_t size = file2::getFileSize(height_map_name);

		char* buf = (char*)_scratchpad_allocator->allocate(size, 16);

		file2::readFile(height_map_name, buf);

		_texture_manager.create(getStringID("height_map"), buf, size);

		auto height_map = _texture_manager.getTexture(getStringID("height_map"));

		const char* color_map_name = "data/textures/wm3_color.dds";

		size = file2::getFileSize(color_map_name);

		buf = (char*)_scratchpad_allocator->allocate(size, 16);

		file2::readFile(color_map_name, buf);

		_texture_manager.create(getStringID("height_map_color"), buf, size);

		auto height_map_color = _texture_manager.getTexture(getStringID("height_map_color"));

#if ENABLE_TERRAIN
		_terrain->setHeightMap(height_map, height_map_color, 8192, 4096, 128, 10, 1.0f, 4480.0f, -2257.0f);
#endif

		//Load font
		size = file2::getFileSize("data/fonts/consolas14.spritefont");

		buf = (char*)_scratchpad_allocator->allocate(size, 16);

		file2::readFile("data/fonts/consolas14.spritefont", buf);

		_font_manager->loadFont(getStringID("data/fonts/consolas14.spritefont"), buf, size);

		_scratchpad_allocator->clear();

		_frame_num = 0;

		return true;
	}

	int update(float dt) override
	{
		_runtime += dt;

		_frame_num++;

		if(_keys_pressed['K'])
		{
			spawnBoxes(10);
		}

		//Update player-controlled box
		Vector3 offset;

		auto camera_transform2   = _transform_manager->lookup(_camera_entity);
		Matrix4x4 camera_matrix2 = _transform_manager->getWorld(camera_transform2);

		Vector3 camera_forward = camera_matrix2.Backward();
		camera_forward.y       = 0.0f;
		camera_forward.Normalize();

		Vector3 camera_right = camera_matrix2.Right();
		camera_right.y       = 0.0f;
		camera_right.Normalize();

		if(_keys_down[VK_NUMPAD8])
			offset += camera_forward * 10 * dt;

		if(_keys_down[VK_NUMPAD2])
			offset -= camera_forward * 10 * dt;

		if(_keys_down[VK_NUMPAD4])
			offset -= camera_right * 10 * dt;

		if(_keys_down[VK_NUMPAD6])
			offset += camera_right * 10 * dt;

		if(offset != VECTOR3_ZERO)
		{
			auto t = _transform_manager->lookup(_player_box);

			Matrix4x4 m = _transform_manager->getWorld(t);

			offset.x += m._41;
			offset.y += m._42;
			offset.z += m._43;

			auto p = _physics_manager->lookup(_player_box);

			_physics_manager->setKinematicTarget(p, offset);
		}

		//Simulate physics
		_physics_manager->simulate(dt, *_scratchpad_allocator);

		//Update transform component of entities with a physic component
		u32 num_transforms = _physics_manager->getNumModifiedTransforms();
		auto transforms    = _physics_manager->getModifiedTransforms();

		for(u32 i = 0; i < num_transforms; i++)
		{
			auto t = _transform_manager->lookup(transforms[i].entity);

			_transform_manager->setLocalPosition(t, transforms[i].position);
			_transform_manager->setLocalRotation(t, transforms[i].rotation);
		}

		_model_manager->update();

		//--------------------------------------------------------------------------------------

		//Camera Update
		float walk_speed = 5.0f;

		if(_keys_down[VK_SHIFT])
			walk_speed = 10 * 25 * walk_speed;

		auto camera_transform = _transform_manager->lookup(_camera_entity);

		if(_keys_down['W'])
			_transform_manager->translate(camera_transform, Vector3(0.0f, 0.0f, walk_speed*dt));
		if(_keys_down['S'])
			_transform_manager->translate(camera_transform, Vector3(0.0f, 0.0f, -walk_speed*dt));
		if(_keys_down['A'])
			_transform_manager->translate(camera_transform, Vector3(-walk_speed*dt, 0.0f, 0.0f));
		if(_keys_down['D'])
			_transform_manager->translate(camera_transform, Vector3(walk_speed*dt, 0.0f, 0.0f));

		if(_keys_pressed[VK_RBUTTON])
		{
			GetCursorPos(&_start_mouse_position);
		}

		if(_keys_down[VK_RBUTTON])
		{
			POINT pos;

			if(GetCursorPos(&pos))
				ScreenToClient(_wnd, &pos);

			POINT start_position = _start_mouse_position;
			ScreenToClient(_wnd, &start_position);

			pos.x = pos.x - start_position.x;
			pos.y = start_position.y - pos.y;

			auto qy = Quaternion::CreateFromYawPitchRoll(0.0f, -pos.y * DirectX::XM_PI * 0.25f / 180, 0.0f);
			qy.Normalize();

			auto qx = Quaternion::CreateFromYawPitchRoll(pos.x * DirectX::XM_PI * 0.25f / 180, 0.0f, 0.0f);
			qx.Normalize();

			if(pos.y != 0)
				_transform_manager->rotate(camera_transform, qy, false);
			if(pos.x != 0)
				_transform_manager->rotate(camera_transform, qx, true);

			SetCursorPos(_start_mouse_position.x, _start_mouse_position.y);
		}

		Matrix4x4 camera_matrix = _transform_manager->getWorld(camera_transform);

		_camera.setPosition(camera_matrix.Translation());
		_camera.setDirection(camera_matrix.Backward(), camera_matrix.Up());
		_camera.update();

		//UPDATE SUN LIGHT

		bool update_sun_and_sky = false;

		float sun_rotation_angle = 30 * DirectX::XM_PI / 180 * dt;

		float sun_phi_rotation = 0.0f;
		float sun_theta_rotation = 0.0f;

		if(_keys_down[VK_UP])
		{
			sun_phi_rotation += sun_rotation_angle;
			update_sun_and_sky = true;
		}
		if(_keys_down[VK_DOWN])
		{
			sun_phi_rotation -= sun_rotation_angle;
			update_sun_and_sky = true;
		}

		if(_keys_down[VK_LEFT])
		{
			sun_theta_rotation -= sun_rotation_angle;
			update_sun_and_sky = true;
		}
		if(_keys_down[VK_RIGHT])
		{
			sun_theta_rotation += sun_rotation_angle;
			update_sun_and_sky = true;
		}

		if(update_sun_and_sky)
		{
			TransformManager::Instance sun_transform = _transform_manager->lookup(_sun);

			Quaternion phi_rotation = Quaternion::CreateFromAxisAngle(VECTOR3_LEFT, sun_phi_rotation);
			Quaternion theta_rotation = Quaternion::CreateFromAxisAngle(VECTOR3_UP, sun_theta_rotation);

			_transform_manager->rotate(sun_transform, phi_rotation);
			_transform_manager->rotate(sun_transform, theta_rotation, true);

			_dynamic_sky->setSunDirection(_transform_manager->getWorld(sun_transform).Backward());

			updateSunSkyColor();
		}

		_light_manager->update();

#if ENABLE_TERRAIN
		_terrain->updateLODs(_camera);
#endif

		//----------------------------------
		// Render
		//----------------------------------

		_renderer.getProfiler()->beginFrame();

		// Set some frame parameters
		_renderer.setFrameParameter(getStringID("delta_time"), &dt, sizeof(dt));
		//_renderer.setFrameParameter(getStringID("runtime"), &_runtime, sizeof(_runtime));

		_renderer.setFrameParameter(getStringID("A"), &_shoulder_strength, sizeof(_shoulder_strength));
		_renderer.setFrameParameter(getStringID("B"), &_linear_strength, sizeof(_linear_strength));
		_renderer.setFrameParameter(getStringID("C"), &_linear_angle, sizeof(_linear_angle));
		_renderer.setFrameParameter(getStringID("D"), &_toe_strength, sizeof(_toe_strength));
		_renderer.setFrameParameter(getStringID("E"), &_toe_rise, sizeof(_toe_rise));
		_renderer.setFrameParameter(getStringID("F"), &_toe_run, sizeof(_toe_run));
		_renderer.setFrameParameter(getStringID("W"), &_linear_white, sizeof(_linear_white));
		_renderer.setFrameParameter(getStringID("K"), &_middle_grey, sizeof(_middle_grey));

		//--------------------------------------------------------------------------

		Viewport vp;
		vp.width = 1.0;
		vp.height = 1.0f;
		vp.x = 0;
		vp.y = 0;

		RenderTexture rt = _renderer.getBackBuffer();

		u32 scope_id;

		//--------------------------------------------------------------------------

		scope_id = _renderer.getProfiler()->beginScope("fullscreen_noise");

		DrawCall dc = createDrawCall(false, 3, 0, 0);

		Mesh mesh;
		mesh.topology = PrimitiveTopology::TRIANGLE_LIST;

		RenderItem render_item;
		render_item.draw_call       = &dc;
		render_item.num_instances   = 1;
		render_item.instance_params = nullptr;
		render_item.material_params = nullptr;
		render_item.mesh            = &mesh;
		render_item.shader          = _noise_shader[0];

		_renderer.setViewport(vp, rt.width, rt.height);
		_renderer.setRenderTarget(1, &rt, nullptr);

		_renderer.render(render_item);

		_renderer.getProfiler()->endScope(scope_id);

		//--------------------------------------------------------------------------

		scope_id = _renderer.getProfiler()->beginScope("dynamic_sky_update");

		_dynamic_sky->update();

		_renderer.getProfiler()->endScope(scope_id);

		//--------------------------------------------------------------------------

		TransformManager::Instance sun_transform = _transform_manager->lookup(_sun);
		_main_view_generator.setSunLightDir(_transform_manager->getWorld(sun_transform).Backward());

		MainViewGenerator::Args args;
		args.target    = &rt;
		args.viewport  = &vp;
		args.camera    = &_camera;
		args.sun_color = _sun_color;

		if(_enable_volumetric_light)
		{
			_dynamic_sky->setScattering(_volumetric_light_manager.get());
			args.scattering = _volumetric_light_manager.get();

			_linear_white   = 100.0f;
			_middle_grey    = 5.0f;
		}
		else
		{
			_dynamic_sky->setScattering(nullptr);
			args.scattering = nullptr;

			_linear_white = 22.2f;
			_middle_grey = 0.4f;
		}

		args.enable_ssr                = _enable_ssr;
		args.thickness                 = _thickness;

		args.focus_plane_z             = _focus_plane_z;
		args.dof_size                  = _dof_size;
		args.near_blur_transition_size = _near_blur_transition_size;
		args.far_blur_transition_size  = _far_blur_transition_size;
		args.near_blur_radius_fraction = _near_blur_radius_fraction * 0.01f;
		args.far_blur_radius_fraction  = _far_blur_radius_fraction* 0.01f;

		_scratchpad_allocator->clear();

		scope_id = _renderer.getProfiler()->beginScope("main_view");

		_renderer.render(_camera, getStringID("main_view_generator"), &args);

		_renderer.getProfiler()->endScope(scope_id);

		/*
		RenderDevice* render_device = _renderer.getRenderDevice();

		float clear[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		render_device->clearRenderTarget(_renderer.getBackBuffer().render_target, clear);
		*/

		//--------------------------------------------------------------------------
		/*
		scope_id = _renderer.getProfiler()->beginScope("noise_corner_compute");

		_renderer.getRenderDevice()->unbindResources();

		_renderer.bindFrameParameters();

		_renderer.compute(8, 8, 1, *_kernel_permutation, *_kernel_params);

		_renderer.getRenderDevice()->unbindResources();

		_renderer.getProfiler()->endScope(scope_id);
		*/
		//--------------------------------------------------------------------------

		scope_id = _renderer.getProfiler()->beginScope("profiler_results");

		TextRenderer::Args tr_args;
		tr_args.target   = &rt;
		tr_args.viewport = &vp;
		tr_args.text     = "Test";
		tr_args.x        = 0.0f;
		tr_args.y        = 0.0f;

		auto results = _renderer.getProfiler()->getResults();

		if(results != nullptr)
			tr_args.text = results;
		else
			tr_args.text = "Performance Overview\nWait!";

		_text_renderer->generate(&tr_args, nullptr);

		_renderer.getProfiler()->endScope(scope_id);

		//--------------------------------------------------------------------------

		scope_id = _renderer.getProfiler()->beginScope("GUI");

		tr_args.target   = &rt;
		tr_args.viewport = &vp;
		tr_args.text     = "Test";
		tr_args.x        = 0.7f;
		tr_args.y        = 0.8f;
		tr_args.text     = "Controls:\n"
						   "WSAD - Camera Movement\n"
						   "Mouse + Right Click - Camera Rotation\n"
						   "Arrows - Rotate Sun\n"
						   "K - Spawn Boxes";

		_text_renderer->generate(&tr_args, nullptr);

		_renderer.setViewport(vp, rt.width, rt.height);
		_renderer.setRenderTarget(1, &rt, nullptr);
		_renderer.getRenderDevice()->applyStateChanges();

		TwDraw();

		_renderer.getProfiler()->endScope(scope_id);

		//--------------------------------------------------------------------------

		render();

		_renderer.getProfiler()->endFrame();

		return true;
	}

	int shutdown() override
	{
		_font_manager->unloadFont(getStringID("data/fonts/consolas14.spritefont"));

		_texture_manager.destroy(getStringID("height_map_color"));
		_texture_manager.destroy(getStringID("height_map"));

		_texture_manager.destroy(getStringID("fence"));

		RenderDevice& render_device = *_renderer.getRenderDevice();

		render_device.deleteCachedParameterGroup(*_red_material.params);
		render_device.deleteCachedParameterGroup(*_ground_material.params);
		render_device.deleteCachedParameterGroup(*_fence_material.params);
		render_device.deleteCachedParameterGroup(*_player_material.params);

		for(int i = 0; i < NUM_TEST_COLORS; i++)
		{
			render_device.deleteCachedParameterGroup(*_test_materials[i].params);
		}

		render_device.deleteCachedParameterGroup(*_metal1.params);
		render_device.deleteCachedParameterGroup(*_metal2.params);
		render_device.deleteCachedParameterGroup(*_metal3.params);

		render_device.deleteParameterGroup(*_main_allocator, *_kernel_params);

		//-----------------------------------------------------------

		if(_text_renderer != nullptr)
			allocator::deallocateDelete(*_main_allocator, _text_renderer);

		_tone_mapper.shutdown();
		_depth_of_field.shutdown();

		if(_dynamic_sky != nullptr)
			allocator::deallocateDelete(*_main_allocator, _dynamic_sky);

		if(_terrain != nullptr)
			allocator::deallocateDelete(*_main_allocator, _terrain);

		if(_primtive_mesh_manager != nullptr)
			allocator::deallocateDelete(*_main_allocator, _primtive_mesh_manager);

		_texture_manager.shutdown();

		allocator::deallocateDelete(*_main_allocator, _font_manager);

		//-----------------------------------------------------------

		_volumetric_light_manager.shutdown();

		if(_volumetric_light_allocator != nullptr)
			allocator::deallocateDelete(*_main_allocator, _volumetric_light_allocator);

		if(_light_manager != nullptr)
			allocator::deallocateDelete(*_main_allocator, _light_manager);

		if(_light_manager_allocator != nullptr)
			allocator::deallocateDelete(*_main_allocator, _light_manager_allocator);

		if(_model_manager != nullptr)
			allocator::deallocateDelete(*_main_allocator, _model_manager);

		if(_model_manager_allocator != nullptr)
			allocator::deallocateDelete(*_main_allocator, _model_manager_allocator);

		if(_physics_manager != nullptr)
			allocator::deallocateDelete(*_main_allocator, _physics_manager);

		if(_physics_manager_allocator != nullptr)
			allocator::deallocateDelete(*_main_allocator, _physics_manager_allocator);

		if(_transform_manager != nullptr)
			allocator::deallocateDelete(*_main_allocator, _transform_manager);

		if(_transform_manager_allocator != nullptr)
			allocator::deallocateDelete(*_main_allocator, _transform_manager_allocator);

		if(_entity_manager != nullptr)
			allocator::deallocateDelete(*_main_allocator, _entity_manager);

		if(_entity_manager_allocator != nullptr)
			allocator::deallocateDelete(*_main_allocator, _entity_manager_allocator);

		//-----------------------------------------------

		_screen_space_reflections.shutdown();

		_shadow_map_generator.shutdown();

		if(_shadow_map_generator_allocator != nullptr)
			allocator::deallocateDelete(*_main_allocator, _shadow_map_generator_allocator);

		_main_view_generator.shutdown();

		if(_main_view_allocator != nullptr)
			allocator::deallocateDelete(*_main_allocator, _main_view_allocator);

		//----------------------------------

		AquaGame::shutdown();

		return true;
	}

private:

	void createMaterials()
	{
		RenderDevice& render_device = *_renderer.getRenderDevice();

		//Get model shader parameters desc
		auto model_shader = _renderer.getShaderManager()->getRenderShader(getStringID("data/shaders/model.cshader"));

		auto model_material_params_desc_set = model_shader->getMaterialParameterGroupDescSet();

		//Red material
		{
			Permutation permutation;

			auto params_desc = getParameterGroupDesc(*model_material_params_desc_set, permutation);

			auto params = render_device.createParameterGroup(*_main_allocator, RenderDevice::ParameterGroupType::MATERIAL,
															 *params_desc, UINT32_MAX, 0, nullptr);

			char* cbuffer_data = (char*)params->getCBuffersData();

			*(Vector3*)(cbuffer_data) = Vector3(1.0f, 0.0f, 0.0f); //diffuse color
			*(Vector2*)(cbuffer_data + 12) = Vector2(0.1f, 0.5f);  //f0 + roughness

			_red_material.params      = render_device.cacheParameterGroup(*params);
			_red_material.permutation = permutation;

			render_device.deleteParameterGroup(*_main_allocator, *params);
		}

		//Ground material
		{
			Permutation permutation;
			permutation      = enableOption(*model_material_params_desc_set, getStringID("CHECKBOARD"), permutation);
			auto params_desc = getParameterGroupDesc(*model_material_params_desc_set, permutation);

			auto params = render_device.createParameterGroup(*_main_allocator, RenderDevice::ParameterGroupType::MATERIAL,
															 *params_desc, UINT32_MAX, 0, nullptr);

			char* cbuffer_data = (char*)params->getCBuffersData();

			*(Vector3*)(cbuffer_data) = Vector3(0.2f, 0.2f, 0.2f); //diffuse color
			*(Vector2*)(cbuffer_data + 12) = Vector2(0.1f, 0.5f);  //f0 + roughness

			_ground_material.params = render_device.cacheParameterGroup(*params);

			render_device.deleteParameterGroup(*_main_allocator, *params);

			_ground_material.permutation = permutation;
		}

		// Fence material
		{
			Permutation permutation;
			permutation = enableOption(*model_material_params_desc_set, getStringID("TWO_SIDED"), permutation);
			permutation = enableOption(*model_material_params_desc_set, getStringID("DIFFUSE_MAP"), permutation);
			permutation = enableOption(*model_material_params_desc_set, getStringID("ALPHA_MASKED"), permutation);
			permutation = enableOption(*model_material_params_desc_set, getStringID("TILING"), permutation);

			auto params_desc = getParameterGroupDesc(*model_material_params_desc_set, permutation);

			auto params = render_device.createParameterGroup(*_main_allocator, RenderDevice::ParameterGroupType::MATERIAL,
															 *params_desc, UINT32_MAX, 0, nullptr);

			char* cbuffer_data = (char*)params->getCBuffersData();

			*(Vector2*)(cbuffer_data) = Vector2(0.1f, 0.5f); //f0 + roughness

			u32 offset = params_desc->getConstantOffset(getStringID("tiling_scale"));

			*(Vector2*)(cbuffer_data + offset) = Vector2(4.0f, 20.0f);

			auto fence_data_size = file2::getFileSize("data/textures/fence.dds");

			char* fence_texture_data = (char*)_scratchpad_allocator->allocate(fence_data_size);

			file2::readFile("data/textures/fence.dds", fence_texture_data);

			_texture_manager.create(getStringID("fence"), fence_texture_data, fence_data_size);
			auto fence_texture = _texture_manager.getTexture(getStringID("fence"));

			params->setSRV(fence_texture, 0);

			_fence_material.params = render_device.cacheParameterGroup(*params);

			render_device.deleteParameterGroup(*_main_allocator, *params);
;
			_fence_material.permutation = permutation;
		}

		//Player material
		{
			Permutation permutation;

			auto params_desc = getParameterGroupDesc(*model_material_params_desc_set, permutation);

			auto params = render_device.createParameterGroup(*_main_allocator, RenderDevice::ParameterGroupType::MATERIAL,
															 *params_desc, UINT32_MAX, 0, nullptr);

			char* cbuffer_data = (char*)params->getCBuffersData();

			*(Vector3*)(cbuffer_data) = Vector3(1.0f, 1.0f, 1.0f); //diffuse color
			*(Vector2*)(cbuffer_data + 12) = Vector2(0.1f, 0.1f);  //f0 + roughness

			_player_material.params = render_device.cacheParameterGroup(*params);

			render_device.deleteParameterGroup(*_main_allocator, *params);

			_player_material.permutation = permutation;
		}

		// Create colorful test materials
		{
			Permutation permutation;

			auto params_desc = getParameterGroupDesc(*model_material_params_desc_set, permutation);

			auto params = render_device.createParameterGroup(*_main_allocator, RenderDevice::ParameterGroupType::MATERIAL,
															 *params_desc, UINT32_MAX, 0, nullptr);

			for(int i = 0; i < NUM_TEST_COLORS; i++)
			{
				Vector3* color = (Vector3*)params->getCBuffersData();
				*color         = TEST_COLORS[i%NUM_TEST_COLORS];

				if(i % 2)
					*(Vector2*)(color + 1) = Vector2(0.1f, 0.3f);
				else
					*(Vector2*)(color + 1) = Vector2(0.1f, 0.8f);

				_test_materials[i].params      = render_device.cacheParameterGroup(*params);
				_test_materials[i].permutation = permutation;
			}

			render_device.deleteParameterGroup(*_main_allocator, *params);
		}

		//Metal materials
		{
			Permutation permutation;

			auto params_desc = getParameterGroupDesc(*model_material_params_desc_set, permutation);

			auto params = render_device.createParameterGroup(*_main_allocator, RenderDevice::ParameterGroupType::MATERIAL,
															 *params_desc, UINT32_MAX, 0, nullptr);

			char* cbuffer_data = (char*)params->getCBuffersData();

			*(Vector3*)(cbuffer_data) = Vector3(1.0f, 1.0f, 1.0f); //diffuse color
			*(Vector2*)(cbuffer_data + 12) = Vector2(1.0f, 0.1f);  //f0 + roughness

			_metal1.params      = render_device.cacheParameterGroup(*params);
			_metal1.permutation = permutation;

			*(Vector2*)(cbuffer_data + 12) = Vector2(1.0f, 0.3f);  //f0 + roughness

			_metal2.params      = render_device.cacheParameterGroup(*params);
			_metal2.permutation = permutation;

			*(Vector2*)(cbuffer_data + 12) = Vector2(1.0f, 0.6f);  //f0 + roughness

			_metal3.params      = render_device.cacheParameterGroup(*params);
			_metal3.permutation = permutation;

			render_device.deleteParameterGroup(*_main_allocator, *params);			
		}
	}

	void spawnBoxes(int row_count)
	{
		auto box_physic_shape = _physics_manager->createBoxShape(0.5f, 0.5f, 0.5f, _physic_material);
		_physics_manager->setShapeLocalPose(box_physic_shape, Vector3(0.0f, 0.5f, 0.0f));

		for(int i = 0; i < row_count; i++)
		{
			for(int j = 0; j < row_count; j++)
			{
				auto box = _entity_manager->create();
				auto box_transform = _transform_manager->create(box);
				auto box_model = _model_manager->create(box, _primtive_mesh_manager->getBox(), 0);

				_model_manager->addSubset(box_model, 0, &_test_materials[(i + j) % NUM_TEST_COLORS]);

				float height = 50.0f + (rand() % 40) / 10.0f;

				auto rigid_actor = _physics_manager->create(box, PhysicActorType::DYNAMIC, Vector3(i * 0.85f - row_count, height, j * 0.85f));

				_physics_manager->addShape(rigid_actor, box_physic_shape);
			}
		}
	}

	void updateSunSkyColor()
	{
		Vector3 sun_color = _dynamic_sky->getSunColor();

		//Limit sun light intensity
		float sun_max = max(sun_color.x, sun_color.y);
		sun_max = max(sun_max, sun_color.z);

		if(sun_max > 0)
		{
			sun_color /= sun_max;
			sun_color *= 255;
		}
		else
		{
			sun_color = VECTOR3_ZERO;
		}

		auto sun = _light_manager->lookup(_sun);
		_light_manager->setColor(sun, sun_color.x, sun_color.y, sun_color.z, 20);

		_sun_color = sun_color / 255;
		_sun_color *= 20;
	}

	ProxyAllocator*	   _main_view_allocator;
	MainViewGenerator  _main_view_generator;

	ProxyAllocator*	    _shadow_map_generator_allocator;
	ShadowMapGenerator  _shadow_map_generator;

	ScreenSpaceReflections _screen_space_reflections;

	ProxyAllocator* _entity_manager_allocator;
	ProxyAllocator* _transform_manager_allocator;
	ProxyAllocator* _physics_manager_allocator;
	ProxyAllocator* _model_manager_allocator;
	ProxyAllocator* _light_manager_allocator;

	EntityManager*        _entity_manager;
	TransformManager*     _transform_manager;
	PhysicsManager*       _physics_manager;
	ModelManager*         _model_manager;
	LightManager*         _light_manager;

	ProxyAllocator*			_volumetric_light_allocator;
	VolumetricLightManager  _volumetric_light_manager;

	TextureManager        _texture_manager;
	FontManager*		  _font_manager;
	PrimitiveMeshManager* _primtive_mesh_manager;

	Terrain*	 _terrain;
	DynamicSky*  _dynamic_sky;
	DepthOfField _depth_of_field;
	ToneMapper   _tone_mapper;

	TextRenderer* _text_renderer;

	Camera _camera;
	Entity _camera_entity;
	POINT  _start_mouse_position;

	const ComputeKernelPermutation* _kernel_permutation;
	ParameterGroup*					_kernel_params;

	ShaderPermutation _noise_shader;

	float _runtime;
	u32 _frame_num;

	Material _red_material;
	Material _ground_material;
	Material _fence_material;
	Material _player_material;
	Material _test_materials[NUM_TEST_COLORS];

	Material _metal1;
	Material _metal2;
	Material _metal3;

	Entity _sun;
	Entity _player_box;

	Vector3 _sun_color;

	PhysicMaterial _physic_material;

	bool _enable_volumetric_light;

	//tone map parameters
	float _shoulder_strength;
	float _linear_strength;
	float _linear_angle;
	float _toe_strength;
	float _toe_rise;
	float _toe_run;
	float _linear_white;
	float _middle_grey;

	// SSR args
	bool  _enable_ssr;
	float _thickness;

	// DOF args
	float _focus_plane_z;
	float _dof_size;
	float _near_blur_transition_size;
	float _far_blur_transition_size;
	float _near_blur_radius_fraction;
	float _far_blur_radius_fraction;
};

#ifdef _WIN32

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
#if _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	//_CrtSetBreakAlloc(230); //Uncomment and change number to enable breakpoint at memory leak
#endif

	//Get directory from cmd line args
	if(lpCmdLine[0] != 0)
	{
#if _WIN32
		SetCurrentDirectory(lpCmdLine);
#endif
	}

	BasicExample game;

	if(!game.init())
	{
		game.shutdown();
		return 0;
	}

	return game.run();
}

#endif