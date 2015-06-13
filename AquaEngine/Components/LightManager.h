#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////
///////////////// Tiago Costa, 2015              
/////////////////////////////////////////////////////////////////////////////////////////////

//#include <World\TransformManager.h>
#include "EntityManager.h"

#include "..\Renderer\RendererStructs.h"
#include "..\Renderer\RendererInterfaces.h"
#include "..\Renderer\RenderDevice\RenderDeviceTypes.h"

#include "..\Core\Containers\HashMap.h"

#include "..\AquaMath.h"
#include "..\AquaTypes.h"

namespace aqua
{
	class TransformManager;
	class Renderer;
	class ShaderManager;
	class ComputeShader;
	class ParameterGroupDesc;

	struct RenderTexture;
	struct Permutation;
	struct ComputeKernelPermutation;

	class LightManager : public ResourceGenerator
	{
	public:
		struct Args
		{
			const Camera*        camera;
			const Viewport*      viewport;
			const RenderTexture* target;

			ShaderResourceH color_buffer;
			ShaderResourceH normal_buffer;
			ShaderResourceH depth_buffer;
			ShaderResourceH ssao_buffer;
			ShaderResourceH scattering_buffer;
			/*
			ShaderResourceH shadow_map;
			Matrix4x4*      cascades_matrices;
			float*          cascades_ends;
			*/
		};

		enum class LightType : u8
		{
			DIRECTIONAL,
			POINT,
			SPOT
		};

		struct Instance
		{
		public:
			operator u32()
			{
				return i;
			};

			bool valid() const
			{
				return (i & INDEX_MASK) != INVALID_INDEX;
			}

		private:

			static const u32 INDEX_MASK = 0x3FFFFFFF;

			LightType getType() const
			{
				return (LightType)(i >> 30);
			}

			u32 getIndex() const
			{
				return i & INDEX_MASK;
			}

			//Instance() {};
			Instance(LightType type, u32 index) : i(((u8)type << 30) | index) { ASSERT(index >> 30 == 0); }

			u32 i;

			friend class LightManager;
		};

		static const u32 INVALID_INDEX = UINT32_MAX >> 2;

		static const Instance INVALID_INSTANCE;

		LightManager(Allocator& allocator, TransformManager& transform, Renderer& renderer, u32 inital_capacity);
		~LightManager();

		//ResourceGenerator interface
		u32 getSecondaryViews(const Camera& camera, RenderView* out_views) override final
		{
			return 0;
		}

		void generate(const void* args_, const VisibilityData* visibility) override final;
		void generate(lua_State* lua_state) override final;

		Instance create(Entity e, LightType type);
		Instance lookup(Entity e);
		void	 destroy(Instance i);

		void update();

		void setColor(Instance i, u8 red, u8 green, u8 blue, u8 intensity = 1);
		void setIntensity(Instance i, float intensity);

		bool setRadius(Instance i, float radius);
		bool setAngle(Instance i, float angle);

		void setShadows(Instance i, bool enabled);

		void setCapacity(u32 new_capacity);
		void setDirectionalLightsCapacity(u32 new_capacity);
		void setPointLightsCapacity(u32 new_capacity);
		void setSpotLightsCapacity(u32 new_capacity);

		void setShadowsParams(ShaderResourceH shadow_map, Matrix4x4* cascades_matrices, float* cascades_ends);

	private:
		/*
		struct InstanceData
		{
		u32         size;
		u32         capacity;

		u32         num_directional_lights;
		u32         num_point_lights;
		u32         num_spot_lights;

		void*       buffer;

		Entity*     entity;
		LightType*  type;
		Vector3*    point_light_color_radius;

		Vector4* positions_radius;
		};
		*/
		struct DirectionalLightsData
		{
			u32      count;
			u32      capacity;

			void*    buffer;

			Entity*  entity;
			Vector3* direction;
			u32*     color;
		};

		struct PointLightsData
		{
			u32      count;
			u32      capacity;

			void*    buffer;

			Entity*  entity;
			Vector4* position_radius;
			u32*     color;
		};

#define SPOT_PARAMS_HALF 1
#define SPHEREMAP_ENCODE 1

		struct SpotParams
		{
#if SPOT_PARAMS_HALF
			u16 light_dir_x;
			u16 light_dir_y;
			u16 cosine_of_cone_angle_light_dir_zsign;
			u16 falloff_radius;
#else
			float light_dir_x;
			float light_dir_y;
			float cosine_of_cone_angle_light_dir_zsign;
			float falloff_radius;
#endif
		};

		struct SpotLightsData
		{
			u32      count;
			u32      capacity;

			void*    buffer;

			Entity*     entity;
			Vector4*    position_radius;
			u32*        color;
			SpotParams* params;
			float*      angle;
			float*      radius;
		};

		Allocator& _allocator;

		HashMap<u32, Instance> _map;

		//InstanceData _data;
		DirectionalLightsData _directional_lights_data;
		PointLightsData       _point_lights_data;
		SpotLightsData        _spot_lights_data;

		TransformManager& _transform_manager;
		Renderer*         _renderer;

		//--------------------
		BufferH			_directional_light_direction_buffer;
		ShaderResourceH _directional_light_direction_buffer_srv;

		BufferH			_directional_light_color_buffer;
		ShaderResourceH _directional_light_color_buffer_srv;
		//--------------------
		BufferH			_point_light_position_radius_buffer;
		ShaderResourceH _point_light_position_radius_buffer_srv;

		BufferH			_point_light_color_buffer;
		ShaderResourceH _point_light_color_buffer_srv;
		//--------------------
		BufferH			_spot_light_position_radius_buffer;
		ShaderResourceH _spot_light_position_radius_buffer_srv;

		BufferH			_spot_light_color_buffer;
		ShaderResourceH _spot_light_color_buffer_srv;

		BufferH			_spot_light_params_buffer;
		ShaderResourceH _spot_light_params_buffer_srv;

		//--------------------

		const ComputeShader*	  _tiled_deferred_shader;
		u8						  _tiled_deferred_kernel;

		const ComputeKernelPermutation* _tiled_deferred_kernel_permutation;

		const ParameterGroupDesc* _tiled_deferred_params_desc;
		ParameterGroup*			  _tiled_deferred_params;
		u8						  _output_texture_index;

		//BufferH					  _tiled_deferred_cbuffer;
	};
};