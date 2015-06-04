#include "Compute.h"

#include "sjson.h"

#include "StringID.h"

#include <iostream>
#include <fstream>
#include <sstream>

bool getComputeKernels(const CommonDesc& common, const std::string& filename, const Object& obj,
					   const std::map<std::string, Snippet>& snippets, std::vector<ComputeKernel>& out)
{
	auto compute_val = obj["compute_kernels"];

	if(compute_val == nullptr)
		return true;

	if(compute_val->getType() != ValueType::OBJECT)
	{
		printError(filename, compute_val->getLineNum(), "'compute_kernels' must be an object!");
		return false;
	}

	auto& compute_kernels = compute_val->asObject();

	for(size_t i = 0; i < compute_kernels.size(); i++)
	{
		auto entry = compute_kernels[i];

		if(entry.value.getType() != ValueType::OBJECT)
		{
			printError(filename, entry.value.getLineNum(), "Compute kernel must be an object!");
			return false;
		}

		const Object& compute = entry.value.asObject();

		ComputeKernel kernel;
		kernel.name = entry.key;

		//Parse Header snippet name
		auto header_val = compute["header"];

		if(header_val != nullptr)
		{
			if(header_val->getType() != ValueType::STRING)
			{
				printError(filename, header_val->getLineNum(), "'header' must be a string");
				return false;
			}

			kernel.header_snippet_name = header_val->asString();
		}

		u8 k = 0;

		if(!getOptions(filename, compute, k, kernel.options))
			return false;

		kernel.params.options_mask = (1ULL << k) - 1;

		u8 num_cbuffers = common.frame_params.cbuffers.size() + common.view_params.cbuffers.size();
		u8 num_srvs     = common.frame_params.srvs.size() + common.view_params.srvs.size();
		u8 num_uavs     = common.frame_params.uavs.size() + common.view_params.uavs.size();

		if(!parseParameterGroupDesc(filename, compute, num_cbuffers, num_srvs, num_uavs, &kernel.options, kernel.params))
			return false;

		out.push_back(std::move(kernel));
	}

	return true;
}

struct ComputeShaderDesc
{
	u32 kernels_names_offset;				//u32 array
	u32 kernels_offset;						//ParameterGroup array
	u32 kernels_permutations_start_offset;	//u32 array
	u32 parameters_binding_info_offset;		//ParameterBindingInfo array
	u32 num_kernel_permutations;
	u8  num_kernels;
};

bool compileComputeKernels(const CommonDesc& common, const std::string& filename,
						   const std::vector<ComputeKernel>& kernels,
						   const std::map<std::string, Snippet>& snippets, 
						   const std::string& output_filename, bool debug)
{
	std::ostringstream temp_stream(std::ios::out | std::ios::binary);

	std::ofstream ofs(output_filename, std::ios::trunc | std::ios::out | std::ios::binary);

	ofs.write((const char*)&COMPUTE_MAGIC_NUMBER, sizeof(COMPUTE_MAGIC_NUMBER));

	//reserve space for size
	u32 desc_size_offset = (u32)ofs.tellp();
	u64 desc_size = 0;
	ofs.write((const char*)&desc_size, sizeof(desc_size));

	ComputeShaderDesc desc;
	desc.num_kernel_permutations = 0;
	desc.num_kernels             = kernels.size();

	u32 desc_offset = (u32)ofs.tellp();

	ofs.write((const char*)&desc, sizeof(desc));

	alignStream(ofs, __alignof(u32));

	desc.kernels_names_offset = (u32)ofs.tellp() - desc_offset;

	for(const ComputeKernel& kernel : kernels)
	{
		u32 name = aqua::getStringID(kernel.name.c_str());
		ofs.write((const char*)&name, sizeof(name));
	}

	alignStream(ofs, __alignof(RuntimeParameterGroup));

	u32 kernels_offset = (u32)ofs.tellp();

	desc.kernels_offset = kernels_offset - desc_offset; //this is kernels offset from start of desc

	//reserve space for kernels params
	{
		u32 space_size = sizeof(RuntimeParameterGroup) * kernels.size();
		char* space = (char*)malloc(space_size);

		ofs.write(space, space_size);

		free(space);
	}

	std::vector<u32> kernels_permutations_starts;

	std::vector<u8> command_groups;

	u8 kernel_index = 0;

	for(const ComputeKernel& kernel : kernels)
	{
		auto it = snippets.find(kernel.name);

		if(it == snippets.end())
		{
			printError(filename, 0, "Cannot find '" + kernel.name + "' snippet!");
			return false;
		}

		std::string full_code;

		full_code += common.frame_params.code + common.view_params.code;

		if(!kernel.header_snippet_name.empty())
		{
			auto it = snippets.find(kernel.header_snippet_name);

			if(it == snippets.end())
			{
				printError(filename, 0, "Cannot find 'header' snippet '" + kernel.header_snippet_name + "'");
				return false;
			}
			else if(it->second.includes.size() > 0)
			{
				printError(filename, 0, "'header' snippet '" + kernel.header_snippet_name + "' cant have 'includes'");
				return false;
			}

			full_code += it->second.code + '\n';
		}

		full_code += kernel.params.code;

		buildFullCode(it->second, snippets, full_code);

		Function func;
		func.name         = kernel.name;
		func.code         = full_code;
		//func.options_mask = options_mask;
		func.options_mask = calculateSnippetOptionsMask(full_code, kernel.options);
		func.type         = ShaderType::COMPUTE_SHADER;
		func.pass         = 0;

		kernels_permutations_starts.push_back(desc.num_kernel_permutations);

		//std::map<Permutation, std::vector<Permutation>> required_permutations = { { 0, { 0 } } };
		std::map<Permutation, std::vector<Permutation>> required_permutations;

		for(Permutation i = 0; i <= func.options_mask; i++)
		{
			bool valid = true;

			for(auto& group : kernel.options)
			{
				if(kernel.options.size() == 0)
					continue;

				if((i & ~group.clear_mask) > group.options.back().bits)
				{
					valid = false;
					break;
				}
			}

			if(valid)
				required_permutations[i] = { i };
		}

		desc.num_kernel_permutations += required_permutations.size();

		ParameterGroupReflection reflection(kernel.params, 1);
		std::vector<ParameterGroupReflection*> param_groups_reflection = { &reflection };

		if(!compileFunctionPermutations(filename, func, debug, required_permutations, kernel.options, 0,
										param_groups_reflection, nullptr, nullptr, temp_stream))
		{
			return false;
		}

		ParameterGroupPermutations params_permutations = reflection.getPermutations(command_groups);

		RuntimeParameterGroup runtime_group;

		writeParameterGroup(kernels_offset + sizeof(RuntimeParameterGroup) * kernel_index,
							kernel.params.options_mask, kernel.options, params_permutations, runtime_group, ofs);

		//Write GROUP
		ofs.seekp(kernels_offset + sizeof(RuntimeParameterGroup) * kernel_index);

		ofs.write((const char*)&runtime_group, sizeof(runtime_group));

		ofs.seekp(0, std::ios_base::end);

		for(auto& permutation : params_permutations.permutations)
		{
			temp_stream.write((const char*)&permutation.second.cmd_groups_indices.at(0), sizeof(u32));
		}

		kernel_index++;
	}

	alignStream(ofs, __alignof(u32));

	desc.kernels_permutations_start_offset = (u32)ofs.tellp() - desc_offset;

	ofs.write((const char*)&kernels_permutations_starts.at(0), sizeof(u32)*kernels_permutations_starts.size());

	alignStream(ofs, __alignof(u8));

	desc.parameters_binding_info_offset = (u32)ofs.tellp() - desc_offset;
	
	ofs.write((const char*)&command_groups.at(0), sizeof(u8)*command_groups.size());

	//Write desc size
	desc_size = (u32)ofs.tellp() - desc_offset;

	ofs.seekp(desc_size_offset);
	ofs.write((const char*)&desc_size, sizeof(desc_size));

	//Write desc
	ofs.seekp(desc_offset);
	ofs.write((const char*)&desc, sizeof(desc));

	ofs.seekp(0, std::ios_base::end);

	std::string temp = temp_stream.str();

	ofs.write(temp.c_str(), temp.size() + 1);

	return true;
}