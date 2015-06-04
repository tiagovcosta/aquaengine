#pragma once

#include "Common.h"

struct ComputeKernel
{
	std::string               name;
	std::string               header_snippet_name;
	std::vector<OptionsGroup> options;
	ParameterGroupDesc        params;
};

bool getComputeKernels(const CommonDesc& common, const std::string& filename, const Object& obj,
					   const std::map<std::string, Snippet>& snippets, std::vector<ComputeKernel>& out);

bool compileComputeKernels(const CommonDesc& common, const std::string& filename, 
						   const std::vector<ComputeKernel>& kernels,
						   const std::map<std::string, Snippet>& snippets, 
						   const std::string& output_filename, bool debug);