#pragma once

#include "Common.h"

struct StreamElement
{
	std::string semantic;
	u8          index;
	TypeDesc    type;
	Permutation options_mask;
	//Condition	condition;
	//bool        per_instance;
};

struct Stream
{
	std::vector<StreamElement> elements;
};

struct ConditionalValue
{
	std::string                value;
	std::shared_ptr<Condition> condition;
};

struct Pass
{
	std::string                   name;
	std::shared_ptr<Condition>	  condition;
	ConditionalValue              vertex_shader;
	ConditionalValue              hull_shader;
	ConditionalValue              domain_shader;
	ConditionalValue              geometry_shader;
	ConditionalValue              pixel_shader;
	std::vector<ConditionalValue> blend_state;
	std::vector<ConditionalValue> rasterizer_state;
	std::vector<ConditionalValue> depth_stencil_state;
};

struct RenderDesc
{
	std::vector<Pass>         passes;

	std::string               header_snippet_name;

	std::vector<OptionsGroup> options;

	u8                        start_mesh_options;
	u8                        num_mesh_options;

	u8                        start_material_options;
	u8                        num_material_options;

	u8                        start_instance_options;
	u8                        num_instance_options;

	Permutation               mesh_options_mask;

	std::vector<Stream>       streams;
	ParameterGroupDesc        material_params;
	ParameterGroupDesc        instance_params;
};

//-----------------------------------------------------------------------------------

struct RuntimeMeshDesc
{
	Permutation mesh_options_mask;
	u32         elements_names_offset;		//u32 array
	u32         elements_indices_offset;	//u8 array
	u32			elements_streams_offset;	//u8 array
	u32			elements_sizes_offset;		//u8 array
	u32         elements_bits_offset;		//Permutation array
	u8          num_elements;
};

struct RuntimeRenderShaderDesc
{
	RuntimeMeshDesc       mesh_desc;
	RuntimeParameterGroup material_params;
	RuntimeParameterGroup instance_params;

	u32                   passes_names_offset;				//u32 array
	u32                   parameters_binding_info_offset;	//ParameterBindingInfo array

	u32                   permutations_nums_offset;			//Permutation array
	u32                   num_permutations;

	u32                   num_functions;
	u32                   num_input_layouts;
	u32                   num_shaders;
	u32                   num_shader_permutations;

	u8                    num_passes;
};

//-----------------------------------------------------------------------------------

bool getRender(const CommonDesc& common, const std::string& filename, const Object& obj,
			   const std::map<std::string, Snippet>& snippets, RenderDesc& out);

bool compileRender(const CommonDesc& common, const std::string& filename, const std::string& shader_name,
				   const RenderDesc& render_desc, const std::map<std::string, Snippet>& snippets,
				   const std::string& output_filename, bool debug);