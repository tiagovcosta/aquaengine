#include "Render.h"

#include "sjson.h"
#include "StringID.h"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>

const std::vector<std::string> blend_states =
{
	"default", "alpha_blend", "additive"
};

const std::vector<std::string> rasterizer_states =
{
	"default", "wireframe", "no_cull"
};

const std::vector<std::string> depth_stencil_states =
{
	"default", "no_depth_write", "no_depth_test"
};

bool getMeshDesc(const CommonDesc& common, const std::string& filename,
				 const Value& mesh_val, std::vector<Stream>& out_streams, u8& num_option_bits,
				 std::vector<OptionsGroup>& options)
{
	if(mesh_val.getType() != ValueType::OBJECT)
	{
		std::cout << "Error: " << filename << ":" << mesh_val.getLineNum() << ": 'mesh' must be an object!\n";
		return false;
	}

	auto& mesh = mesh_val.asObject();

	//Get mesh options
	if(!getOptions(filename, mesh, num_option_bits, options))
		return false;

	//Get mesh streams
	auto streams_val = mesh["streams"];

	if(streams_val == nullptr)
	{
		std::cout << "Error: " << filename << ":" << mesh_val.getLineNum() << ": 'streams' array not found!\n";
		return false;
	}

	if(streams_val->getType() != ValueType::ARRAY)
	{
		std::cout << "Error: " << filename << ":" << streams_val->getLineNum() << ": 'streams' must be an array!\n";
		return false;
	}

	auto& streams = streams_val->asArray();

	for(auto& stream_val : streams)
	{
		Stream stream;

		if(stream_val.getType() != ValueType::ARRAY)
		{
			std::cout << "Error: " << filename << ":" << stream_val.getLineNum() << ": Stream must be an array!\n";
			return false;
		}

		auto& stream_array = stream_val.asArray();

		for(auto& element_val : stream_array)
		{
			if(element_val.getType() != ValueType::OBJECT)
			{
				std::cout << "Error: " << filename << ":" <<
					element_val.getLineNum() << ": Stream Element must be an object!\n";

				return false;
			}

			auto& element = element_val.asObject();

			StreamElement stream_element;

			auto val = element["semantic"];

			if(val == nullptr)
			{
				printError(filename, element_val.getLineNum(), "Stream Element 'semantic' missing!");
				return false;
			}
			else if(val->getType() != ValueType::STRING)
			{
				printError(filename, val->getLineNum(), "'semantic' must be a string!");
				return false;
			}

			stream_element.semantic = val->asString();

			val = element["index"];

			if(val == nullptr)
			{
				stream_element.index = 0; //default index
			}
			else if(val->getType() != ValueType::NUMBER)
			{
				printError(filename, val->getLineNum(), "Stream Element 'index' must be a number!");
				return false;
			}
			else
			{
				stream_element.index = val->asNumber();
			}

			val = element["type"];

			if(val == nullptr)
			{
				printError(filename, element_val.getLineNum(), "Stream Element 'type' missing!");
				return false;
			}
			else if(val->getType() != ValueType::STRING)
			{
				printError(filename, val->getLineNum(), "'type' must be a string!");
				return false;
			}

			auto it = types_descs.find(val->asString());

			if(it == types_descs.end())
			{
				printError(filename, val->getLineNum(), "Invalid stream element 'type'!");
				return false;
			}

			stream_element.type = it->second;

			val = element["define"];

			stream_element.options_mask = 0;

			if(val != nullptr)
			{
				if(val->getType() != ValueType::STRING)
				{
					printError(filename, val->getLineNum(), "'define' must be a string!");
					return false;
				}

				for(const auto& group : options)
				{
					for(auto& option : group.options)
					{
						if(option.name == val->asString())
						{
							stream_element.options_mask = option.bits;
							break;
						}
					}
				}
			}

			stream.elements.push_back(std::move(stream_element));
		}

		out_streams.push_back(std::move(stream));
	}

	return true;
}

bool parseConditionalValue(const Value& val, const std::vector<OptionsGroup>& options, ConditionalValue& out)
{
	if(val.getType() == ValueType::STRING)
	{
		out.value     = val.asString();
		out.condition = nullptr;
	}
	else if(val.getType() == ValueType::OBJECT)
	{
		auto& func_obj = val.asObject();

		auto val = func_obj["condition"];

		if(val != nullptr)
		{
			if(val->getType() != ValueType::STRING)
			{
				// ERROR
				return false;
			}

			auto condition_str = val->asString().c_str();

			out.condition = parseCondition(condition_str, options);
		}
		else
		{
			out.condition = nullptr;
		}

		val = func_obj["value"];

		if(val != nullptr)
		{
			if(val->getType() != ValueType::STRING)
			{
				// ERROR
				return false;
			}

			out.value = val->asString();
		}
	}
	else
	{
		// ERROR
		return false;
	}

	return true;
}

bool parseConditionalValueArray(const Value& val, const std::vector<OptionsGroup>& options, 
								std::vector<ConditionalValue>& out)
{
	if(val.getType() == ValueType::ARRAY)
	{
		auto& array = val.asArray();

		for(auto& rast : array)
		{
			ConditionalValue cv;
			
			if(!parseConditionalValue(rast, options, cv))
				return false;

			out.push_back(std::move(cv));
		}
	}
	else
	{
		ConditionalValue cv;

		if(!parseConditionalValue(val, options, cv))
			return false;

		out.push_back(std::move(cv));
	}

	return true;
}

bool getRender(const CommonDesc& common, const std::string& filename, const Object& obj,
			   const std::map<std::string, Snippet>& snippets, RenderDesc& out)
{
	//Parse Header snippet name
	auto header_val = obj["header"];

	if(header_val != nullptr)
	{
		if(header_val->getType() != ValueType::STRING)
		{
			printError(filename, header_val->getLineNum(), "'header' must be a string");
			return false;
		}
		
		out.header_snippet_name = header_val->asString();
	}

	out.mesh_options_mask = 0;

	u8 num_option_bits = 0;

	auto mesh_val = obj["mesh"];

	out.start_mesh_options = out.options.size();

	if(mesh_val != nullptr)
		getMeshDesc(common, filename, *mesh_val, out.streams, num_option_bits, out.options);

	out.num_mesh_options = out.options.size() - out.start_mesh_options;

	//If mesh_val == nullptr DO NOTHING?????????????????????????????????

	out.mesh_options_mask = (1ULL << num_option_bits) - 1;

	//Parse Material options
	auto material_val = obj["material"];

	out.start_material_options = out.options.size();

	if(material_val == nullptr)
	{

	}
	else
	{
		auto& material = material_val->asObject();

		if(!getOptions(filename, material, num_option_bits, out.options))
			return false;
	}

	out.material_params.options_mask = ((1ULL << num_option_bits) - 1) & (~out.mesh_options_mask);

	out.num_material_options = out.options.size() - out.start_material_options;

	//Parse Instance options
	auto instance_val = obj["instance"];

	out.start_instance_options = out.options.size();

	if(instance_val == nullptr)
	{

	}
	else
	{
		auto& instance = instance_val->asObject();

		if(!getOptions(filename, instance, num_option_bits, out.options))
			return false;
	}

	out.instance_params.options_mask = ((1ULL << num_option_bits) - 1);
	out.instance_params.options_mask &= (~out.mesh_options_mask | ~out.material_params.options_mask);

	out.num_instance_options = out.options.size() - out.start_instance_options;

	auto passes_val = obj["passes"];

	if(passes_val == nullptr)
	{

	}
	else
	{
		if(passes_val->getType() != ValueType::OBJECT)
		{
			std::cout << "Error: " << filename << ":" << passes_val->getLineNum() << ": 'passes' must be an object!\n";
			return false;
		}

		auto& passes_obj = passes_val->asObject();

		for(size_t i = 0; i < passes_obj.size(); i++)
		{
			Object::Entry pass_desc = passes_obj[i];

			Pass pass;
			pass.name = pass_desc.key;

			if(pass_desc.value.getType() != ValueType::OBJECT)
			{
				printError(filename, pass_desc.key_line, "Pass must be an object!");
			}

			auto& pass_obj = pass_desc.value.asObject();

			auto val = pass_obj["condition"];

			if(val != nullptr)
			{
				if(val->getType() != ValueType::STRING)
				{
					//-*------------ERROR
				}

				auto condition_str = val->asString().c_str();

				pass.condition = parseCondition(condition_str, out.options);
			}
			else
				pass.condition = nullptr;

			val = pass_obj["vs"];

			if(val != nullptr)
			{
				if(!parseConditionalValue(*val, out.options, pass.vertex_shader))
					return false;
			}

			val = pass_obj["hs"];

			if(val != nullptr)
			{
				if(!parseConditionalValue(*val, out.options, pass.hull_shader))
					return false;
			}

			val = pass_obj["ds"];

			if(val != nullptr)
			{
				if(!parseConditionalValue(*val, out.options, pass.domain_shader))
					return false;
			}

			val = pass_obj["gs"];

			if(val != nullptr)
			{
				if(!parseConditionalValue(*val, out.options, pass.geometry_shader))
					return false;
			}

			val = pass_obj["ps"];

			if(val != nullptr)
			{
				if(!parseConditionalValue(*val, out.options, pass.pixel_shader))
					return false;
			}

			val = pass_obj["rasterizer_state"];

			if(val != nullptr)
			{
				if(!parseConditionalValueArray(*val, out.options, pass.rasterizer_state))
					return false;
			}

			val = pass_obj["blend_state"];

			if(val != nullptr)
			{
				if(!parseConditionalValueArray(*val, out.options, pass.blend_state))
					return false;
			}

			val = pass_obj["depth_stencil_state"];

			if(val != nullptr)
			{
				if(!parseConditionalValueArray(*val, out.options, pass.depth_stencil_state))
					return false;
			}

			out.passes.push_back(std::move(pass));
		}
	}

	//Calculate max num cbuffers, srvs and uavs used by a pass
	u8 max_cbuffers = 0;
	u8 max_srvs     = 0;
	u8 max_uavs     = 0;

	for(auto& pass : out.passes)
	{
		auto it = common.passes.find(pass.name);

		if(it != common.passes.end())
		{
			u8 num_cbuffers = it->second.cbuffers.size();
			u8 num_srvs     = it->second.srvs.size();
			u8 num_uavs     = it->second.uavs.size();

			if(num_cbuffers > max_cbuffers)
				max_cbuffers = num_cbuffers;

			if(num_srvs > max_srvs)
				max_srvs = num_srvs;

			if(num_uavs > max_uavs)
				max_uavs = num_uavs;
		}
	}

	u8 start_cbuffer = common.frame_params.cbuffers.size() + common.view_params.cbuffers.size() + max_cbuffers;
	u8 start_srv     = common.frame_params.srvs.size() + common.view_params.srvs.size() + max_srvs;
	u8 start_uav     = common.frame_params.uavs.size() + common.view_params.uavs.size() + max_uavs;

	//Parse Material params
	if(material_val != nullptr)
	{
		auto& material = material_val->asObject();

		//---------------------------------------------------------------------
		//MODIFY TO ONLY INCLUDE INSTANCE OPTIONS
		//---------------------------------------------------------------------

		parseParameterGroupDesc(filename, material, start_cbuffer, start_srv, start_uav, &out.options, out.material_params);

		start_cbuffer += out.material_params.cbuffers.size();
		start_srv     += out.material_params.srvs.size();
		start_uav     += out.material_params.uavs.size();
	}

	//Parse Instance params
	if(instance_val != nullptr)
	{
		auto& instance = instance_val->asObject();

		//---------------------------------------------------------------------
		//MODIFY TO ONLY INCLUDE INSTANCE OPTIONS
		//---------------------------------------------------------------------

		parseParameterGroupDesc(filename, instance, start_cbuffer, start_srv, start_uav, &out.options, out.instance_params);

		start_cbuffer += out.instance_params.cbuffers.size();
		start_srv += out.instance_params.srvs.size();
		start_uav += out.instance_params.uavs.size();
	}

	return true;
}

//Calculates the function code and adds it to the functions vector
//Returns the index of the calculated function
inline u8 calculatePassFunction(const std::string& function_name, const std::map<std::string, Snippet>& snippets,
								const std::vector<OptionsGroup>& options, const std::string& shader_name, 
								const std::string& pass_name, u8 pass_num, ShaderType type,
								const std::string& common_code, std::vector<Function>& functions)
{
	if(!function_name.empty())
	{
		auto it = snippets.find(function_name);

		if(it == snippets.end())
		{
			printError("", 0, "Couldn't find snippet '" + function_name + "'!");
			//ERROR
		}

		Function func;
		func.name         = shader_name + '/' + pass_name + '/' + function_name;
		func.pass         = pass_num;
		func.type         = type;
		func.code         = common_code;

		if(!buildFullCode(it->second, snippets, func.code))
		{
			printError("", 0, "Error building '" + function_name + "' code!");
			//ERROR
		}

		func.options_mask = calculateSnippetOptionsMask(func.code, options);

		functions.push_back(std::move(func));

		return functions.size() - 1;
	}

	return UINT8_MAX;
}

//Adds the permutation to the required_functions map (if the permutation wasn't already added)
//Returns the permutation index
inline u64 addFunctionPermutation(u8 function_index, const std::vector<Function>& functions, Permutation permutation, 
								  std::map<u8, std::map<Permutation, std::vector<Permutation>>>& required_functions)
{
	if(function_index != UINT8_MAX)
	{
		auto& func = functions[function_index];

		Permutation func_permutation = permutation & func.options_mask;

		std::map<Permutation, std::vector<Permutation>>& req_permutations = required_functions[function_index];

		auto it = req_permutations.find(func_permutation);

		if(it == req_permutations.cend())
		{
			req_permutations.emplace(func_permutation, std::vector<Permutation>{permutation});

			return req_permutations.size() - 1;
		}
		else
		{
			it->second.push_back(permutation);
			return std::distance(req_permutations.begin(), it);
		}
	}

	return UINT64_MAX;
}

bool isValidPermutation(Permutation p, u8 start_option_group, u8 num_option_groups,
						const std::vector<OptionsGroup>& options)
{
	for(u8 j = 0; j < num_option_groups; j++)
	{
		const OptionsGroup& group = options[j + start_option_group];

		Permutation last_option_bits = group.options.back().bits;

		if((p & ~group.clear_mask) > last_option_bits)
			return false;

		for(auto& option : group.options)
		{
			if((p & ~group.clear_mask) == option.bits)
			{
				if(option.condition != nullptr && !option.condition->evaluate(p))
					return false;

				break;
			}
		}
	}

	return true;
}

struct PassDesc
{
	u8 vs_function_index;
	u8 hs_function_index;
	u8 ds_function_index;
	u8 gs_function_index;
	u8 ps_function_index;
};

struct PassPermutation
{
	u8  pass_num;
	//u64 input_layout_permutation_index;
	u64 vs_permutation_index;
	u64 hs_permutation_index;
	u64 ds_permutation_index;
	u64 gs_permutation_index;
	u64 ps_permutation_index;

	u8 blend_state_index;
	u8 rasterizer_state_index;
	u8 depth_stencil_state_index;
};

struct RenderShaderPermutation
{
	std::vector<u64> passes_permutations_indices;
};

bool operator==(const PassPermutation& x, const PassPermutation& y)
{
	return	x.pass_num					== y.pass_num			 &&
			x.vs_permutation_index		== y.vs_permutation_index &&
			x.hs_permutation_index		== y.hs_permutation_index &&
			x.ds_permutation_index		== y.ds_permutation_index &&
			x.gs_permutation_index		== y.gs_permutation_index &&
			x.ps_permutation_index		== y.ps_permutation_index &&
			x.blend_state_index			== y.blend_state_index &&
			x.rasterizer_state_index	== y.rasterizer_state_index &&
			x.depth_stencil_state_index == y.depth_stencil_state_index;
};

struct RuntimeShaderPermutation
{
	u64 shader_index;
	u32 vertex_buffers_bind_info_index;
	u32 material_params_bind_info_index;
	u32 instance_params_bind_info_index;
};

bool operator==(const RuntimeShaderPermutation& x, const RuntimeShaderPermutation& y)
{
	return	x.shader_index                    == y.shader_index					   &&
			x.vertex_buffers_bind_info_index  == y.vertex_buffers_bind_info_index  &&
			x.material_params_bind_info_index == y.material_params_bind_info_index &&
			x.instance_params_bind_info_index == y.instance_params_bind_info_index;
};

bool compileRender(const CommonDesc& common, const std::string& filename, const std::string& shader_name, 
				   const RenderDesc& render_desc, const std::map<std::string, Snippet>& snippets, 
				   const std::string& output_filename, bool debug)
{
	std::ostringstream temp_stream(std::ios::out | std::ios::binary);

	//////////////////////////////////////////////////////////////////////////////////////////////
	
	std::vector<Function> functions;
	std::vector<PassDesc> passes;
	passes.reserve(render_desc.passes.size());

	for(u8 i = 0; i < render_desc.passes.size(); i++)
	{
		auto& pass              = render_desc.passes[i];

		std::string common_code = common.frame_params.code + common.view_params.code;

		auto pass_it            = common.passes.find(pass.name);

		if(pass_it != common.passes.end())
			common_code += pass_it->second.code;

		if(!render_desc.header_snippet_name.empty())
		{
			auto it = snippets.find(render_desc.header_snippet_name);

			if(it == snippets.end())
			{
				printError(filename, 0, "Cannot find 'header' snippet '" + render_desc.header_snippet_name + "'");
				return false;
			}
			else if(it->second.includes.size() > 0)
			{
				printError(filename, 0, "'header' snippet '" + render_desc.header_snippet_name + "' cant have 'includes'");
				return false;
			}

			common_code += it->second.code;
		}
		
		common_code += render_desc.material_params.code;
		common_code += render_desc.instance_params.code;

		PassDesc desc;

		desc.vs_function_index = calculatePassFunction(pass.vertex_shader.value, snippets, render_desc.options, 
													   shader_name, pass.name, i, 
													   ShaderType::VERTEX_SHADER, common_code, functions);

		desc.hs_function_index = calculatePassFunction(pass.hull_shader.value, snippets, render_desc.options,
													   shader_name, pass.name, i,
													   ShaderType::HULL_SHADER, common_code, functions);

		desc.ds_function_index = calculatePassFunction(pass.domain_shader.value, snippets, render_desc.options,
													   shader_name, pass.name, i,
													   ShaderType::DOMAIN_SHADER, common_code, functions);

		desc.gs_function_index = calculatePassFunction(pass.geometry_shader.value, snippets, render_desc.options,
													   shader_name, pass.name, i,
													   ShaderType::GEOMETRY_SHADER, common_code, functions);

		desc.ps_function_index = calculatePassFunction(pass.pixel_shader.value, snippets, render_desc.options,
													   shader_name, pass.name, i,
													   ShaderType::PIXEL_SHADER, common_code, functions);

		passes.push_back(std::move(desc));
	}

	std::map<u8, std::map<Permutation, std::vector<Permutation>>>	required_functions;
	std::vector<PassPermutation>									passes_permutations;
	std::map<Permutation, RenderShaderPermutation>					shader_permutations;

	//------------------------------------------------------------------------------------
	//Calculate required permutations
	//------------------------------------------------------------------------------------

	Permutation x = render_desc.mesh_options_mask |
					render_desc.material_params.options_mask |
					render_desc.instance_params.options_mask;
	
	for(Permutation i = 0; i <= x; i++)
	{
		//Check if this is a valid permutation
		
		if(!isValidPermutation(i, render_desc.start_mesh_options, render_desc.num_mesh_options, render_desc.options))
			continue;

		if(!isValidPermutation(i, render_desc.start_material_options, 
							   render_desc.num_material_options, render_desc.options))
			continue;

		if(!isValidPermutation(i, render_desc.start_instance_options, 
							   render_desc.num_instance_options, render_desc.options))
			continue;

		auto& passes_permutations_indices = shader_permutations[i].passes_permutations_indices;
		passes_permutations_indices.reserve(passes.size());

		for(u8 j = 0; j < passes.size(); j++)
		{
			const auto& pass_desc = passes[j];
			const auto& pass      = render_desc.passes[j];

			//Check if this permutation uses this pass
			if(pass.condition != nullptr && !pass.condition->evaluate(i))
			{
				passes_permutations_indices.push_back(UINT64_MAX);
				continue;
			}

			PassPermutation perm;
			perm.pass_num = j;

			//Calculate which function permutations are needed
			if(pass.vertex_shader.condition == nullptr || 
			   pass.vertex_shader.condition->evaluate(i))
			{
				perm.vs_permutation_index = addFunctionPermutation(pass_desc.vs_function_index, functions,
																   i, required_functions);
			}
			else
				perm.vs_permutation_index = UINT64_MAX;

			if(pass.hull_shader.condition == nullptr ||
				pass.hull_shader.condition->evaluate(i))
			{
				perm.hs_permutation_index = addFunctionPermutation(pass_desc.hs_function_index, functions,
																	i, required_functions);
			}
			else
				perm.hs_permutation_index = UINT64_MAX;

			if(pass.domain_shader.condition == nullptr ||
				pass.domain_shader.condition->evaluate(i))
			{
				perm.ds_permutation_index = addFunctionPermutation(pass_desc.ds_function_index, functions,
																	i, required_functions);
			}
			else
				perm.ds_permutation_index = UINT64_MAX;

			if(pass.geometry_shader.condition == nullptr ||
				pass.geometry_shader.condition->evaluate(i))
			{
				perm.gs_permutation_index = addFunctionPermutation(pass_desc.gs_function_index, functions,
																	i, required_functions);
			}
			else
				perm.gs_permutation_index = UINT64_MAX;

			if(pass.pixel_shader.condition == nullptr ||
				pass.pixel_shader.condition->evaluate(i))
			{
				perm.ps_permutation_index = addFunctionPermutation(pass_desc.ps_function_index, functions,
																	i, required_functions);
			}
			else
				perm.ps_permutation_index = UINT64_MAX;

			//Choose which states are to be used by this pass permutation
			perm.blend_state_index         = 0;
			perm.rasterizer_state_index    = 0;
			perm.depth_stencil_state_index = 0;

			for(auto& k : pass.blend_state)
			{
				if(k.condition == nullptr || k.condition->evaluate(i))
				{
					auto it = std::find(blend_states.cbegin(), blend_states.cend(), k.value);

					if(it == blend_states.cend())
					{
						//---------------ERROR------------------
					}

					perm.blend_state_index = std::distance(blend_states.cbegin(), it);
					break;
				}
			}

			for(auto& k : pass.rasterizer_state)
			{
				if(k.condition == nullptr || k.condition->evaluate(i))
				{
					auto it = std::find(rasterizer_states.cbegin(), rasterizer_states.cend(), k.value);

					if(it == rasterizer_states.cend())
					{
						//---------------ERROR------------------
					}

					perm.rasterizer_state_index = std::distance(rasterizer_states.cbegin(), it);
					break;
				}
			}

			for(auto& k : pass.depth_stencil_state)
			{
				if(k.condition == nullptr || k.condition->evaluate(i))
				{
					auto it = std::find(depth_stencil_states.cbegin(), depth_stencil_states.cend(), k.value);

					if(it == depth_stencil_states.cend())
					{
						//---------------ERROR------------------
					}

					perm.depth_stencil_state_index = std::distance(depth_stencil_states.cbegin(), it);
					break;
				}
			}

			//Check if pass permutation already exists
			auto it = std::find(passes_permutations.cbegin(), passes_permutations.cend(), perm);

			if(it == passes_permutations.cend())
			{
				passes_permutations.push_back(std::move(perm));
				passes_permutations_indices.push_back(passes_permutations.size() - 1);
			}
			else
			{
				passes_permutations_indices.push_back(std::distance(passes_permutations.cbegin(), it));
			}
		}
	}

	//------------------------------------------------------------------------------------
	//Compile and write functions
	//------------------------------------------------------------------------------------

	u64 num_functions = functions.size();

	temp_stream.write((char*)&num_functions, sizeof(num_functions));

	std::vector<InputLayout>       input_layouts;
	std::map<u8, std::vector<u32>> input_layouts_indices;

	ParameterGroupReflection material_reflection(render_desc.material_params, render_desc.passes.size());
	ParameterGroupReflection instance_reflection(render_desc.instance_params, render_desc.passes.size());

	std::vector<ParameterGroupReflection*> param_groups_reflection = {&material_reflection, &instance_reflection};

	for(u8 i = 0; i < functions.size(); i++)
	{
		if(functions[i].type == ShaderType::VERTEX_SHADER)
		{
			compileFunctionPermutations(filename, functions[i], debug, required_functions[i], render_desc.options, 
										render_desc.mesh_options_mask, param_groups_reflection,
										&input_layouts, &input_layouts_indices[i], temp_stream);
		} else
			compileFunctionPermutations(filename, functions[i], debug, required_functions[i], render_desc.options, 
										0, param_groups_reflection, nullptr, nullptr, temp_stream);
	}

	//------------------------------------------------------------------------------------
	//Write input layouts
	//------------------------------------------------------------------------------------

	//Write mesh elements names and types
	for(const auto& stream : render_desc.streams)
	{
		for(const auto& element : stream.elements)
		{
			temp_stream.write(element.semantic.c_str(), sizeof(char)*(element.semantic.size()+1));
			temp_stream.write((const char*)&element.type.type, sizeof(element.type.type));
		}
	}

	for(const auto& input_layout : input_layouts)
	{
		temp_stream.write((char*)&input_layout.mesh_permutation, sizeof(input_layout.mesh_permutation));
		temp_stream.write((char*)&input_layout.bytecode_offset, sizeof(input_layout.bytecode_offset));
	}

	//------------------------------------------------------------------------------------
	//Write shaders permutations
	//------------------------------------------------------------------------------------

	//Calculate the index of the first permutation of the function

	std::vector<u64> func_permutations_starts;
	func_permutations_starts.reserve(functions.size());

	u64 current_start = 0;

	for(u8 i = 0; i < functions.size(); i++)
	{
		func_permutations_starts.push_back(current_start);
		current_start += required_functions[i].size();
	}

	struct RuntimeShader
	{
		u32 input_layout_index;
		u32 vs_function_index;
		u32 hs_function_index;
		u32 ds_function_index;
		u32 gs_function_index;
		u32 ps_function_index;
		u8  rasterizer_state_index;
		u8  blend_state_index;
		u8  depth_stencil_state_index;
	};

	for(const auto& permutation : passes_permutations)
	{
		const auto& pass = passes[permutation.pass_num];

		RuntimeShader k;

		if(pass.vs_function_index != UINT8_MAX && permutation.vs_permutation_index != UINT64_MAX)
		{
			k.input_layout_index = input_layouts_indices[pass.vs_function_index][permutation.vs_permutation_index];
			k.vs_function_index  = func_permutations_starts[pass.vs_function_index] + permutation.vs_permutation_index;
		}
		else
		{
			k.input_layout_index = UINT32_MAX;
			k.vs_function_index  = UINT32_MAX;
		}
		
		if(pass.hs_function_index != UINT8_MAX && permutation.hs_permutation_index != UINT64_MAX)
			k.hs_function_index = func_permutations_starts[pass.hs_function_index] + permutation.hs_permutation_index;
		else
			k.hs_function_index = UINT32_MAX;

		if(pass.ds_function_index != UINT8_MAX && permutation.ds_permutation_index != UINT64_MAX)
			k.ds_function_index = func_permutations_starts[pass.ds_function_index] + permutation.ds_permutation_index;
		else
			k.ds_function_index = UINT32_MAX;

		if(pass.gs_function_index != UINT8_MAX && permutation.gs_permutation_index != UINT64_MAX)
			k.gs_function_index = func_permutations_starts[pass.gs_function_index] + permutation.gs_permutation_index;
		else
			k.gs_function_index = UINT32_MAX;

		if(pass.ps_function_index != UINT8_MAX && permutation.ps_permutation_index != UINT64_MAX)
			k.ps_function_index = func_permutations_starts[pass.ps_function_index] + permutation.ps_permutation_index;
		else
			k.ps_function_index = UINT32_MAX;

		k.rasterizer_state_index    = permutation.rasterizer_state_index;
		k.blend_state_index         = permutation.blend_state_index;
		k.depth_stencil_state_index = permutation.depth_stencil_state_index;

		temp_stream.write((const char*)&k, sizeof(k));
	}

	std::vector<u8> command_groups;

	//------------------------------------------------------------------------------------
	//Add input layout cmd groups (vertex buffer bind info)
	//------------------------------------------------------------------------------------

	std::vector<u32> input_layouts_cmd_groups_indices;

	for(const auto& input_layout : input_layouts)
	{
		std::vector<u8> cmd_group;

		u8 current_stream = 0;
		for(const auto& stream : render_desc.streams)
		{
			for(const auto& element : stream.elements)
			{
				auto k = std::find_if(input_layout.elements.cbegin(), input_layout.elements.cend(),
							 [&element](const InputLayoutElement& x)
				{
					return x.semantic_name == element.semantic;
				});

				if(k != input_layout.elements.cend())
				{
					cmd_group.push_back(current_stream);
					break;
				}
			}

			current_stream++;
		}

		cmd_group.push_back(UINT8_MAX);

		//Add cmd_group to global vector
		
		//Search for a equal sequence
		auto it = std::search(command_groups.cbegin(), command_groups.cend(), cmd_group.cbegin(), cmd_group.cend());

		if(it != command_groups.cend())
		{
			//If equal sequence was found, reuse it
			input_layouts_cmd_groups_indices.push_back(std::distance(command_groups.cbegin(), it));
		}
		else
		{
			//Else add the new sequence
			input_layouts_cmd_groups_indices.push_back(command_groups.size());

			command_groups.insert(command_groups.cend(), cmd_group.cbegin(), cmd_group.cend());
		}
	}

	//------------------------------------------------------------------------------------
	//Write pass permutations
	//------------------------------------------------------------------------------------

	ParameterGroupPermutations material_permutations = material_reflection.getPermutations(command_groups);

	ParameterGroupPermutations instance_permutations = instance_reflection.getPermutations(command_groups);

	std::vector<RuntimeShaderPermutation> runtime_shader_permutations;
	std::vector<u64>					  runtime_shader_permutations_indices;

	u64 num_saved = 0;

	for(const auto& shader_permutation : shader_permutations)
	{
		//Mesh
		Permutation mesh_permutation_num = shader_permutation.first & render_desc.instance_params.options_mask;
		//const std::vector<u32>& mesh_commands_indices =
		//	instance_permutations.permutations[mesh_permutation_num].cmd_groups_indices;
		
		//Material
		Permutation material_permutation_num = shader_permutation.first & render_desc.material_params.options_mask;
		const std::vector<u32>& material_commands_indices = 
			material_permutations.permutations[material_permutation_num].cmd_groups_indices;

		//Instance
		Permutation instance_permutation_num = shader_permutation.first & render_desc.instance_params.options_mask;
		const std::vector<u32>& instance_commands_indices = 
			instance_permutations.permutations[instance_permutation_num].cmd_groups_indices;

		auto& passes_permutations_indices = shader_permutation.second.passes_permutations_indices;

		u8 valid_pass_index = 0;

		//for(u8 i = 0; i < passes_permutations_indices.size(); i++)
		for(const auto& pass_permutation_index : passes_permutations_indices)
		{
			//const auto& pass_permutation_index = passes_permutations_indices[i]; //shader index

			static_assert(sizeof(pass_permutation_index) == sizeof(u64), "Pass index must be u64");

			if(pass_permutation_index == UINT64_MAX)
			{
				runtime_shader_permutations_indices.push_back(UINT64_MAX);
				//---------------------------------------------------------------------------------------------------- MODIFIED
				valid_pass_index++;//----------------------------------------------------------------------------------------------------
				//----------------------------------------------------------------------------------------------------
				continue;
			}

			RuntimeShaderPermutation k;
			k.shader_index = pass_permutation_index;

			u32 il_index = input_layouts_indices[passes[valid_pass_index].vs_function_index]
							[passes_permutations[pass_permutation_index].vs_permutation_index];

			//Set mesh vertex buffers binding info index
			if(il_index == UINT32_MAX)
				k.vertex_buffers_bind_info_index = UINT32_MAX;
			else
				k.vertex_buffers_bind_info_index = input_layouts_cmd_groups_indices[il_index];

			k.material_params_bind_info_index = material_commands_indices[valid_pass_index];
			k.instance_params_bind_info_index = instance_commands_indices[valid_pass_index];

			auto it = std::find(runtime_shader_permutations.cbegin(), runtime_shader_permutations.cend(), k);

			if(it == runtime_shader_permutations.cend())
			{
				//Write RuntimeShaderPermutation
				temp_stream.write((const char*)&k, sizeof(k));

				runtime_shader_permutations.push_back(k);
				runtime_shader_permutations_indices.push_back(runtime_shader_permutations.size() - 1);
			}
			else
			{
				runtime_shader_permutations_indices.push_back(std::distance(runtime_shader_permutations.cbegin(), it));
				num_saved++;
			}

			valid_pass_index++;
		}
	}

	printf("\n Salvos: %d\n", num_saved);

	temp_stream.write((const char*)&runtime_shader_permutations_indices.at(0), 
					  sizeof(u64) * runtime_shader_permutations_indices.size());

	//////////////////////////////////////////////////////////////////////////////////////////////
	// WRITE RENDER SHADER DESC
	//////////////////////////////////////////////////////////////////////////////////////////////

	std::ofstream ofs(output_filename, std::ios::trunc | std::ios::out | std::ios::binary);

	ofs.write((const char*)&RENDER_MAGIC_NUMBER, sizeof(RENDER_MAGIC_NUMBER));

	RuntimeRenderShaderDesc desc;

	//reserve space for size
	u32 render_desc_size_offset = (u32)ofs.tellp();
	u64 render_desc_size        = 0;
	ofs.write((const char*)&render_desc_size, sizeof(render_desc_size));

	//reserve space for desc
	u32 render_desc_offset = (u32)ofs.tellp();
	ofs.write((const char*)&desc, sizeof(desc)); 
	
	// WRITE MESH DESC ////////////////////////////////////
	desc.mesh_desc.num_elements = 0;

	for(size_t i = 0; i < render_desc.streams.size(); i++)
	{
		desc.mesh_desc.num_elements += render_desc.streams[i].elements.size();
	}

	//write elements names
	alignStream(ofs, __alignof(u32));
	desc.mesh_desc.elements_names_offset = (u32)ofs.tellp() - render_desc_offset;

	for(size_t i = 0; i < render_desc.streams.size(); i++)
	{
		for(auto& element : render_desc.streams[i].elements)
		{
			u32 hash = aqua::getStringID(element.semantic.c_str());
			ofs.write((const char*)&hash, sizeof(hash));
		}
	}

	//write elements indices
	alignStream(ofs, __alignof(u8));
	desc.mesh_desc.elements_indices_offset = (u32)ofs.tellp() - render_desc_offset;

	for(size_t i = 0; i < render_desc.streams.size(); i++)
	{
		for(auto& element : render_desc.streams[i].elements)
		{
			ofs.write((const char*)&element.index, sizeof(element.index));
		}
	}

	//write elements streams
	alignStream(ofs, __alignof(u8));
	desc.mesh_desc.elements_streams_offset = (u32)ofs.tellp() - render_desc_offset;

	for(size_t i = 0; i < render_desc.streams.size(); i++)
	{
		for(auto& element : render_desc.streams[i].elements)
		{
			u8 stream = (u8)i;

			ofs.write((const char*)&stream, sizeof(stream));
		}
	}

	//write elements sizes
	alignStream(ofs, __alignof(u8));
	desc.mesh_desc.elements_sizes_offset = (u32)ofs.tellp() - render_desc_offset;

	for(size_t i = 0; i < render_desc.streams.size(); i++)
	{
		for(auto& element : render_desc.streams[i].elements)
		{
			ofs.write((const char*)&element.type.size, sizeof(element.type.size));
		}
	}

	//write elements bits
	alignStream(ofs, __alignof(Permutation));
	desc.mesh_desc.elements_bits_offset = (u32)ofs.tellp() - render_desc_offset;

	for(size_t i = 0; i < render_desc.streams.size(); i++)
	{
		for(auto& element : render_desc.streams[i].elements)
		{
			ofs.write((const char*)&element.options_mask, sizeof(element.options_mask));
		}
	}

	// WRITE MATERIAL PARAMS ////////////////////////////////////
	writeParameterGroup(render_desc_offset + offsetof(RuntimeRenderShaderDesc, material_params),
						render_desc.material_params.options_mask, render_desc.options, material_permutations,
						desc.material_params, ofs);

	// WRITE INSTANCE PARAMS ////////////////////////////////////
	writeParameterGroup(render_desc_offset + offsetof(RuntimeRenderShaderDesc, instance_params),
						render_desc.instance_params.options_mask, render_desc.options, instance_permutations, 
						desc.instance_params, ofs);

	desc.num_permutations = shader_permutations.size() * render_desc.passes.size();

	desc.num_functions = 0;

	for(u8 i = 0; i < functions.size(); i++)
		desc.num_functions += required_functions[i].size();

	desc.num_input_layouts       = input_layouts.size();
	desc.num_shaders             = passes_permutations.size();
	desc.num_shader_permutations = runtime_shader_permutations.size();

	desc.num_passes = render_desc.passes.size();

	//Write passes names
	alignStream(ofs, __alignof(u32));
	desc.passes_names_offset = (u32)ofs.tellp() - render_desc_offset;

	for(auto& pass : render_desc.passes)
	{
		u32 hash = aqua::getStringID(pass.name.c_str());
		ofs.write((const char*)&hash, sizeof(hash));
	}

	//Write params bind info

	desc.parameters_binding_info_offset = (u32)ofs.tellp() - render_desc_offset;

	ofs.write((const char*)&command_groups.at(0), sizeof(u8)*command_groups.size());

	//Write permutations nums

	alignStream(ofs, __alignof(Permutation));
	desc.permutations_nums_offset = (u32)ofs.tellp() - render_desc_offset;

	for(const auto& shader_permutation : shader_permutations)
		ofs.write((const char*)&shader_permutation.first, sizeof(shader_permutation.first));

	////////////////////////////////////////////////////

	//Write render desc size
	render_desc_size = (u32)ofs.tellp() - render_desc_offset;

	ofs.seekp(render_desc_size_offset);
	ofs.write((const char*)&render_desc_size, sizeof(render_desc_size));

	//Write desc
	ofs.seekp(render_desc_offset);
	ofs.write((const char*)&desc, sizeof(desc));

	ofs.seekp(0, std::ios_base::end);

	std::string temp = temp_stream.str();

	ofs.write(temp.c_str(), temp.size() + 1);

	return true;
}