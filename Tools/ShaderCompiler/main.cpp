#include "Common.h"
#include "Render.h"
#include "Compute.h"

#include "sjson.h"
#include "StringID.h"

//#include <d3dcompiler.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif //WIN32_LEAN_AND_MEAN

#include <Windows.h>

#include <vector>
#include <map>
#include <string>
#include <algorithm>

#include <fstream>
#include <iostream>

//#include <limits>

#define PAUSE_BEFORE_CLOSE 0

std::string getFileContents(const char *filename)
{
	std::ifstream in(filename);

	if(!in)
		return "";

	std::string contents;
	in.seekg(0, std::ios::end);
	contents.resize(in.tellg());
	in.seekg(0, std::ios::beg);
	in.read(&contents[0], contents.size());
	in.close();

	return contents;
}

bool getFileTimestamp(const std::string& filename, FILETIME& out_time)
{
	HANDLE h = CreateFile(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, NULL);

	if(h == INVALID_HANDLE_VALUE || !GetFileTime(h, NULL, NULL, &out_time))
	{
		CloseHandle(h);
		return false;
	}

	CloseHandle(h);

	return true;
}

void getPermutations(const ParameterGroupDesc& desc, std::vector<u8>& command_groups,
					 RuntimeCBuffers& cbuffers, RuntimeConstants& constants, RuntimeResources& srvs, RuntimeResources& uavs)
{
	ParameterGroupPermutation permutation;

	std::vector<u8> vec;

	u32 current_constant_offset = 0;

	for(u8 i = 0; i < desc.cbuffers.size(); i++)
	{
		vec.push_back(i);						//index
		vec.push_back(desc.cbuffers[i].slot);	//slot
		vec.push_back(UINT8_MAX);				//stages

		u32 cbuffer_size = 0;

		for(auto& constant : desc.cbuffers[i].constants)
		{
			u32 prev_vector_index = cbuffer_size / 16;
			u32 vector_index      = (cbuffer_size + constant.type.size) / 16;
			u8 x                  = (cbuffer_size + constant.type.size) % 16;

			//If constant crosses 16-byte boundary
			if(vector_index != prev_vector_index && x != 0)
			{
				//add padding
				current_constant_offset += constant.type.size - x;
				cbuffer_size			+= constant.type.size - x;
			}

			constants.names.push_back(aqua::getStringID(constant.name.c_str()));
			constants.offsets.push_back(current_constant_offset);

			current_constant_offset += constant.type.size;
			cbuffer_size			+= constant.type.size;
		}

		//cbuffer size must be a multiple of 16
		if(cbuffer_size % 16 != 0)
		{
			current_constant_offset += 16 - cbuffer_size % 16;
			cbuffer_size += 16 - cbuffer_size % 16;
		}

		cbuffers.names.push_back(aqua::getStringID(desc.cbuffers[i].name.c_str()));
		cbuffers.sizes.push_back(cbuffer_size);

	}

	vec.push_back(UINT8_MAX); //cbuffers terminator


	for(u8 i = 0; i < desc.srvs.size(); i++)
	{
		vec.push_back(i);					//index
		vec.push_back(desc.srvs[i].slot);	//slot
		vec.push_back(UINT8_MAX);			//stages

		srvs.names.push_back(aqua::getStringID(desc.srvs[i].name.c_str()));
	}

	vec.push_back(UINT8_MAX ); //srvs terminator

	for(u8 i = 0; i < desc.uavs.size(); i++)
	{
		vec.push_back(i);					//index
		vec.push_back(desc.uavs[i].slot);	//slot
		vec.push_back(UINT8_MAX);			//stages

		uavs.names.push_back(aqua::getStringID(desc.uavs[i].name.c_str()));
	}

	vec.push_back(UINT8_MAX); //uavs terminator

	//Add cmd_groups to global vector
	auto it = std::search(command_groups.cbegin(), command_groups.cend(), vec.cbegin(), vec.cend());

	if(it != command_groups.cend())
	{
		//If equal sequence was found, reuse it
		permutation.cmd_groups_indices.push_back(std::distance(command_groups.cbegin(), it));
	}
	else
	{
		//Else add the new sequence
		permutation.cmd_groups_indices.push_back(command_groups.size());

		command_groups.insert(command_groups.cend(), vec.cbegin(), vec.cend());
	}
}

void writeParameterGroup(u32 desc_offset, u32 cmd_group_offset_offset,
						 const ParameterGroupDesc& group, std::ostream& out_stream)
{
	std::vector<u8>  command_group;
	RuntimeCBuffers  cbuffers;
	RuntimeConstants constants;
	RuntimeResources srvs;
	RuntimeResources uavs;

	getPermutations(group, command_group, cbuffers, constants, srvs, uavs);

	RuntimeParameterGroupPermutation desc;
	desc.num_cbuffers  = group.cbuffers.size();
	desc.num_srvs      = group.srvs.size();
	desc.num_uavs      = group.uavs.size();
	desc.num_constants = 0;

	for(auto& cbuffer : group.cbuffers)
	{
		for(auto& constant : cbuffer.constants)
			desc.num_constants++;
	}

	//Write parameters vectors

	alignStream(out_stream, __alignof(u32));

	if(desc.num_cbuffers > 0)
	{
		desc.cbuffers_offset = (u32)out_stream.tellp() - desc_offset;

		out_stream.write((const char*)&cbuffers.names.at(0), sizeof(u32)*cbuffers.names.size());
		out_stream.write((const char*)&cbuffers.sizes.at(0), sizeof(u32)*cbuffers.sizes.size());
	}

	if(desc.num_srvs > 0)
	{
		desc.srvs_names_offset = (u32)out_stream.tellp() - desc_offset;

		out_stream.write((const char*)&srvs.names.at(0), sizeof(u32)*srvs.names.size());
	}

	if(desc.num_uavs > 0)
	{
		desc.uavs_names_offset = (u32)out_stream.tellp() - desc_offset;

		out_stream.write((const char*)&uavs.names.at(0), sizeof(u32)*uavs.names.size());
	}

	if(desc.num_constants > 0)
	{
		desc.constants_offset = (u32)out_stream.tellp() - desc_offset;

		out_stream.write((const char*)&constants.names.at(0), sizeof(u32)*constants.names.size());
		out_stream.write((const char*)&constants.offsets.at(0), sizeof(u32)*constants.offsets.size());
	}

	u32 cmd_group_offset = UINT32_MAX;

	if(command_group.size() > 0)
	{
		cmd_group_offset = (u32)out_stream.tellp();
		out_stream.write((const char*)&command_group.at(0), sizeof(u8)*command_group.size());
	}

	//Write desc
	out_stream.seekp(desc_offset);

	out_stream.write((const char*)&desc, sizeof(desc));

	//Write cmd group offset
	out_stream.seekp(cmd_group_offset_offset);

	out_stream.write((const char*)&cmd_group_offset, sizeof(cmd_group_offset));

	out_stream.seekp(0, std::ios_base::end);
}

static const std::map<std::string, BlendOption> blend_options =
{
	{ "zero", BlendOption::ZERO },
	{ "one", BlendOption::ONE },
	{ "src_color", BlendOption::SRC_COLOR },
	{ "inv_src_color", BlendOption::INV_SRC_COLOR },
	{ "src_alpha", BlendOption::SRC_ALPHA },
	{ "inv_src_alpha", BlendOption::INV_SRC_ALPHA },
	{ "dest_alpha", BlendOption::DEST_ALPHA },
	{ "inv_dest_alpha", BlendOption::INV_DEST_ALPHA },
	{ "dest_color", BlendOption::DEST_COLOR },
	{ "inv_dest_color", BlendOption::INV_DEST_COLOR },
	{ "src_alpha_sat", BlendOption::SRC_ALPHA_SAT },
	{ "blend_factor", BlendOption::BLEND_FACTOR },
	{ "inv_blend_factor", BlendOption::INV_BLEND_FACTOR },
	{ "src1_color", BlendOption::SRC1_COLOR },
	{ "inv_src1_color", BlendOption::INV_SRC1_COLOR },
	{ "src1_alpha", BlendOption::SRC1_ALPHA },
	{ "inv_src1_alpha", BlendOption::INV_SRC1_ALPHA }
};

static const std::map<std::string, BlendOperation> blend_operations =
{
	{ "add", BlendOperation::ADD },
	{ "subtract", BlendOperation::SUBTRACT },
	{ "rev_subtract", BlendOperation::REV_SUBTRACT },
	{ "min", BlendOperation::MIN },
	{ "max", BlendOperation::MAX }
};

void getBlendStates()
{
}

//Loads common.temp or compiles common.shader into common.temp and common.cshader
int loadCommon(const std::string& data_dir, bool check_time, FILETIME shader_timestamp, CommonDesc& out_common,
				std::map<std::string, Snippet>& snippets)
{
	std::string source_filename   = data_dir + "source/shaders/common.shader";
	//std::string temp_filename     = data_dir + "source/shaders/temp/common.hlsl";
	std::string compiled_filename = data_dir + "shaders/common.cshader";

	bool modified         = true;
	bool recompile_shader = true;

	if(check_time)
	{
		FILETIME source_time;
		//FILETIME temp_time;
		FILETIME compiled_time;

		if(!getFileTimestamp(source_filename, source_time))
		{
			std::cout << "Error: common.shader not found!" << std::endl;
			return false;
		}

		//Try to get common.temp and common.cshader timestamps
		if(getFileTimestamp(compiled_filename, compiled_time))
		{
			//Check if files are up-to-date
			if(CompareFileTime(&source_time, &compiled_time) < 1)
			{
				/*
				//No need to recompile them
				//Return common.temp

				out_common.code = getFileContents(temp_filename.c_str());
				//Extract num_cbuffers, srvs and uavs


				//Get snippets

				return true;
				*/

				modified = false;

				if(CompareFileTime(&compiled_time, &shader_timestamp) < 1)
				{
					recompile_shader = false;
				}
			}
		}
	}
	//Recompile common.shader
	std::string source_file = getFileContents(source_filename.c_str());

	//Is this check required since getFileTimestamp was successful???
	if(source_file.empty())
	{
		std::cout << "Error: common.shader not found!" << std::endl;
		return false;
	}

	Object obj;

	if(!parseSJSON(source_filename, source_file.c_str(), obj))
	{
		std::cout << "Error parsing common.shader!" << std::endl;
		return false;
	}
	/*
	out_common.frame_params.code.clear();
	out_common.frame_params.cbuffers.clear();
	out_common.frame_params.srvs.clear();
	out_common.frame_params.uavs.clear();
	
	out_common.view_params.code.clear();
	out_common.view_params.cbuffers.clear();
	out_common.view_params.srvs.clear();
	out_common.view_params.uavs.clear();
	*/
	out_common.passes.clear();

	const Value* frame = obj["frame"];

	if(frame != nullptr)
	{
		if(frame->getType() != ValueType::OBJECT)
		{
			std::cout << "Error: " << source_filename << ":" << 
						 frame->getLineNum() << ": 'frame' must be an object!\n";
			return false;
		}

		parseParameterGroupDesc(source_filename, frame->asObject(), 0, 0, 0, nullptr, out_common.frame_params);
	}

	const Value* view = obj["view"];

	if(view != nullptr)
	{
		if(view->getType() != ValueType::OBJECT)
		{
			std::cout << "Error: " << source_filename << ":" << 
						 view->getLineNum() << ": 'view' must be an object!\n";
			return false;
		}

		parseParameterGroupDesc(source_filename, view->asObject(), out_common.frame_params.cbuffers.size(),
								out_common.frame_params.srvs.size(), out_common.frame_params.uavs.size(), 
								nullptr, out_common.view_params);
	}

	const Value* passes = obj["passes"];

	if(passes != nullptr)
	{
		if(passes->getType() != ValueType::OBJECT)
		{
			std::cout << "Error: " << source_filename << ":" <<
				passes->getLineNum() << ": 'passes' must be an object!\n";
			return false;
		}

		const Object& passes_obj = passes->asObject();

		for(size_t i = 0; i < passes_obj.size(); i++)
		{
			Object::Entry entry = passes_obj[i];

			if(entry.value.getType() != ValueType::OBJECT)
			{
				std::cout << "Error: " << source_filename << ":" <<
					entry.value.getLineNum() << ": '"<< entry.key << "' must be an object!\n";
				return false;
			}

			ParameterGroupDesc params_desc;

			parseParameterGroupDesc(source_filename, entry.value.asObject(), 
									out_common.frame_params.cbuffers.size() + out_common.view_params.cbuffers.size(),
									out_common.frame_params.srvs.size() + out_common.view_params.srvs.size(),
									out_common.frame_params.uavs.size() + out_common.view_params.uavs.size(),
									nullptr, params_desc);

			out_common.passes[entry.key] = std::move(params_desc);
		}
	}

	//------------------------------------------------------------------------------------
	//Get blend states
	//------------------------------------------------------------------------------------

	const Value* blend_states = obj["blend_states"];

	if(blend_states != nullptr)
	{
		if(blend_states->getType() != ValueType::OBJECT)
		{
			std::cout << "Error: " << source_filename << ":" <<
				passes->getLineNum() << ": 'blend_states' must be an object!\n";
			return false;
		}

		const Object& blend_states_obj = blend_states->asObject();

		for(size_t i = 0; i < blend_states_obj.size(); i++)
		{
			Object::Entry entry = blend_states_obj[i];

			if(entry.value.getType() != ValueType::OBJECT)
			{
				std::cout << "Error: " << source_filename << ":" <<
					entry.value.getLineNum() << ": '" << entry.key << "' must be an object!\n";
				return false;
			}

			BlendStateDesc desc;

			const Object& blend_states_obj = entry.value.asObject();

			const Value* val = blend_states_obj["src_blend"];

			if(val != nullptr)
			{
				if(val->getType() != ValueType::STRING)
				{
					std::cout << "Error: " << source_filename << ":" <<
						val->getLineNum() << ": 'src_blend' must be a string!\n";
					return false;
				}

				auto it = blend_options.find(val->asString());

				if(it == blend_options.end())
				{
					std::cout << "Error: " << source_filename << ":" <<
						val->getLineNum() << ": Invalid blend option!\n";
					return false;
				}

				desc.src_blend = it->second;
			}
			else
			{
				std::cout << "Error: " << source_filename << ":" <<
					val->getLineNum() << ": Missing 'src_blend' option!\n";
				return false;
			}

			val = blend_states_obj["dest_blend"];

			if(val != nullptr)
			{
				if(val->getType() != ValueType::STRING)
				{
					std::cout << "Error: " << source_filename << ":" <<
						val->getLineNum() << ": 'dest_blend' must be a string!\n";
					return false;
				}

				auto it = blend_options.find(val->asString());

				if(it == blend_options.end())
				{
					std::cout << "Error: " << source_filename << ":" <<
						val->getLineNum() << ": Invalid blend option!\n";
					return false;
				}

				desc.dest_blend = it->second;
			}
			else
			{
				std::cout << "Error: " << source_filename << ":" <<
					val->getLineNum() << ": Missing 'dest_blend' option!\n";
				return false;
			}

			val = blend_states_obj["blend_op"];

			if(val != nullptr)
			{
				if(val->getType() != ValueType::STRING)
				{
					std::cout << "Error: " << source_filename << ":" <<
						val->getLineNum() << ": 'blend_op' must be a string!\n";
					return false;
				}

				auto it = blend_operations.find(val->asString());

				if(it == blend_operations.end())
				{
					std::cout << "Error: " << source_filename << ":" <<
						val->getLineNum() << ": Invalid blend operation!\n";
					return false;
				}

				desc.blend_op = it->second;
			}

			val = blend_states_obj["src_blend_alpha"];

			if(val != nullptr)
			{
				if(val->getType() != ValueType::STRING)
				{
					std::cout << "Error: " << source_filename << ":" <<
						val->getLineNum() << ": 'src_blend_alpha' must be a string!\n";
					return false;
				}

				auto it = blend_options.find(val->asString());

				if(it == blend_options.end())
				{
					std::cout << "Error: " << source_filename << ":" <<
						val->getLineNum() << ": Invalid blend option!\n";
					return false;
				}

				desc.src_blend_alpha = it->second;
			}
			else
			{
				std::cout << "Error: " << source_filename << ":" <<
					val->getLineNum() << ": Missing 'src_blend_alpha' option!\n";
				return false;
			}

			val = blend_states_obj["dest_blend_alpha"];

			if(val != nullptr)
			{
				if(val->getType() != ValueType::STRING)
				{
					std::cout << "Error: " << source_filename << ":" <<
						val->getLineNum() << ": 'dest_blend_alpha' must be a string!\n";
					return false;
				}

				auto it = blend_options.find(val->asString());

				if(it == blend_options.end())
				{
					std::cout << "Error: " << source_filename << ":" <<
						val->getLineNum() << ": Invalid blend option!\n";
					return false;
				}

				desc.dest_blend_alpha = it->second;
			}
			else
			{
				std::cout << "Error: " << source_filename << ":" <<
					val->getLineNum() << ": Missing 'dest_blend_alpha' option!\n";
				return false;
			}

			val = blend_states_obj["blend_op_alpha"];

			if(val != nullptr)
			{
				if(val->getType() != ValueType::STRING)
				{
					std::cout << "Error: " << source_filename << ":" <<
						val->getLineNum() << ": 'blend_op_alpha' must be a string!\n";
					return false;
				}

				auto it = blend_operations.find(val->asString());

				if(it == blend_operations.end())
				{
					std::cout << "Error: " << source_filename << ":" <<
						val->getLineNum() << ": Invalid blend operation!\n";
					return false;
				}

				desc.blend_op_alpha = it->second;
			}

			out_common.blend_states[entry.key] = std::move(desc);
		}
	}

	//------------------------------------------------------------------------------------
	//Get snippets
	//------------------------------------------------------------------------------------

	auto snippets_val = obj["snippets"];

	if(snippets_val != nullptr)
	{
		if(snippets_val->getType() != ValueType::OBJECT)
		{
			std::cout << "Error: " << source_filename << ":" <<
				snippets_val->getLineNum() << ": 'snippets' must be an object!\n";

			//ERROR ---------------------------------------------------------------------
		}

		getSnippets(source_filename, snippets_val->asObject(), snippets);
	}

	/*
	//add header
	out_common.code = "//" + std::to_string(out_common.num_cbuffers) + " " +
		              std::to_string(out_common.num_srvs) + " " +
					  std::to_string(out_common.num_uavs) + "\n" +
					  "// AUTO GENERATED - DO NOT MODIFY\n\n" + out_common.code;
*/

	if(modified)
	{
		//WRITE FRAME, VIEW AND PASSES PARAMS DESCS TO FILE
		std::ofstream ofs(compiled_filename, std::ios::trunc | std::ios::out | std::ios::binary);

		if(!ofs.good())
		{
			std::cout << "Error opening output file '" + compiled_filename  + "'!\n";
			return false;
		}

		struct RuntimeCommon
		{
			u32 passes_names_offset;
			u32 param_groups_offset;
			u32 cmd_groups_offsets_offset;
			u8  num_passes;
		};

		RuntimeCommon runtime_common;
		runtime_common.num_passes = out_common.passes.size();

		ofs.write((const char*)&runtime_common, sizeof(runtime_common));

		runtime_common.passes_names_offset = (u32)ofs.tellp();

		for(auto& pass : out_common.passes)
		{
			u32 hash = aqua::getStringID(pass.first.c_str());
			
			ofs.write((const char*)&hash, sizeof(hash));
		}

		runtime_common.param_groups_offset = (u32)ofs.tellp();

		//Reserve space for permutations
		{
			u32 space_size = sizeof(RuntimeParameterGroupPermutation) * (runtime_common.num_passes + 2);
			char* space = (char*)malloc(space_size);

			ofs.write(space, space_size);

			free(space);
		}

		runtime_common.cmd_groups_offsets_offset = (u32)ofs.tellp();

		//Reserve space for cmd groups offsets
		{
			u32 space_size = sizeof(u32) * (runtime_common.num_passes + 2);
			char* space = (char*)malloc(space_size);

			ofs.write(space, space_size);

			free(space);
		}

		writeParameterGroup(runtime_common.param_groups_offset, runtime_common.cmd_groups_offsets_offset,
							out_common.frame_params, ofs);

		writeParameterGroup(runtime_common.param_groups_offset + sizeof(RuntimeParameterGroupPermutation),
							runtime_common.cmd_groups_offsets_offset + sizeof(u32),
							out_common.view_params, ofs);

		u8 index = 0;
		for(auto& pass : out_common.passes)
		{
			writeParameterGroup(runtime_common.param_groups_offset + sizeof(RuntimeParameterGroupPermutation) * (index + 2),
								runtime_common.cmd_groups_offsets_offset + sizeof(u32) * (index + 2),
								pass.second, ofs);

			index++;
		}

		ofs.seekp(0, std::ios_base::beg);

		ofs.write((const char*)&runtime_common, sizeof(runtime_common));
	}

	if(recompile_shader)
		return 2;
	else
		return 1;
	//return true;
}

int main(int argc, char* argv[])
{
	std::string data_dir;

	//------------------------------------------------------------------------------------
	// Parse cmd line args
	//------------------------------------------------------------------------------------

	std::string shader_filename;
	bool debug           = false;
	bool needs_recompile = false;

	{
		std::string data_dir_prefix("--data-dir=");

		bool got_dir      = false;
		bool got_filename = false;

		for(int i = 1; i < argc; i++)
		{
			std::string arg = argv[i];

			if(!got_dir && arg.compare(0, data_dir_prefix.length(), data_dir_prefix) == 0)
			{
				data_dir = arg.substr(data_dir_prefix.length());
				got_dir = true;
			}
			else if(arg == "--force-recompile")
			{
				needs_recompile = true;
			}
			else if(arg == "--debug" || arg == "-d")
			{
				debug = true;
			}
			else if(!got_filename)
			{
				shader_filename = arg;
				got_filename = true;
			}
		}

		/*
		if(!got_filename)
		{
		std::cout << "Shader filename not found!" << std::endl;

		#if PAUSE_BEFORE_CLOSE
		system("PAUSE");
		#endif

		return 0;
		}
		*/
	}

	// Ask for shader filename if it wasnt in the cmd line args
	if(shader_filename.empty())
	{
		std::cout << "Insert shader filename: ";
		std::cin >> shader_filename;

		std::cout << "Debug? ";

		std::string k;

		std::getline(std::cin, k); // read \n from filename
		std::getline(std::cin, k);

		if(!k.empty() && k[0] == 'y')
			debug = true;

		std::cout << "Force recompile? ";

		std::getline(std::cin, k);

		if(!k.empty() && k[0] == 'y')
			needs_recompile = true;
	}

	std::string full_shader_filename = data_dir + "source/shaders/" + shader_filename + ".shader";
	std::string output_filename = data_dir + "shaders/" + shader_filename + ".cshader";

	FILETIME source_time;
	FILETIME compiled_time;

	if(!getFileTimestamp(full_shader_filename, source_time))
	{
		std::cout << "Error: File '" + shader_filename + "' not found!" << std::endl;

#if PAUSE_BEFORE_CLOSE
		system("PAUSE");
#endif

		return 0;
	}

	if(!needs_recompile && !getFileTimestamp(output_filename, compiled_time))
	{
		//Compile file doesn't exist
		needs_recompile = true;

		compiled_time = source_time;
	}
	else if(CompareFileTime(&source_time, &compiled_time) == 1) //Check if files are up-to-date
	{
		needs_recompile = true;
	}

	//------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------

	std::map<std::string, Snippet> snippets;

	//------------------------------------------------------------------------------------
	//Load 'common.shader'
	//------------------------------------------------------------------------------------

	CommonDesc common;

	int cr = loadCommon(data_dir, !needs_recompile, compiled_time, common, snippets);

	if(cr == 0) //error
	{
#if PAUSE_BEFORE_CLOSE
		system("PAUSE");
#endif

		return 0;
	}
	else if(cr == 2)
		needs_recompile = true;

	if(!needs_recompile)
	{
		std::cout << std::endl << "No need to recompile '" + shader_filename + "' - Compiled shader is up-to-date!" << std::endl;

#if PAUSE_BEFORE_CLOSE
		system("PAUSE");
#endif

		return 0;
	}

	std::string source_file = getFileContents(full_shader_filename.c_str());

	if(source_file.empty())
	{
		std::cout << "Error: File not found!" << std::endl;

#if PAUSE_BEFORE_CLOSE
		system("PAUSE");
#endif

		return 0;
	}

	Object obj;

	if(!parseSJSON(shader_filename, source_file.c_str(), obj))
	{
		std::cout << "Error parsing shader file!" << std::endl;

#if PAUSE_BEFORE_CLOSE
		system("PAUSE");
#endif

		return 0;
	}

	//Only common.shader should have frame and view descs
	//if(obj["frame"] != nullptr) ERROR
	//if(obj["view"] != nullptr) ERROR

	//------------------------------------------------------------------------------------
	//Load includes
	//------------------------------------------------------------------------------------

	auto include_val = obj["include"];

	if(include_val != nullptr)
	{
		if(include_val->getType() != ValueType::ARRAY)
		{
			printError(full_shader_filename, include_val->getLineNum(), "'include' must be an array!");

#if PAUSE_BEFORE_CLOSE
			system("PAUSE");
#endif
			return 0;
		}

		const Array& include_array = include_val->asArray();

		for(const auto& include : include_array)
		{
			if(include.getType() != ValueType::STRING)
			{
				printError(full_shader_filename, include.getLineNum(), "'include' must be an array of strings!");

#if PAUSE_BEFORE_CLOSE
				system("PAUSE");
#endif
				return 0;
			}

			//Get included file snippets
			const std::string& include_name   = include.asString();
			std::string full_include_filename = data_dir + "source/shaders/" + include_name + ".shader";

			std::string source_file = getFileContents(full_include_filename.c_str());

			if(source_file.empty())
			{
				std::cout << "Error: Included file '" << include_name << "' not found!" << std::endl;
				continue;
			}

			Object obj;

			if(!parseSJSON(include_name, source_file.c_str(), obj))
			{
				std::cout << "Error parsing included file '" << include_name << "'!" << std::endl;
				continue;
			}

			auto snippets_val = obj["snippets"];

			if(snippets_val != nullptr)
			{
				if(snippets_val->getType() != ValueType::OBJECT)
				{
					printError(full_include_filename, snippets_val->getLineNum(), "'snippets' must be an object!");

					continue;
				}

				if(!getSnippets(full_include_filename, snippets_val->asObject(), snippets))
				{
#if PAUSE_BEFORE_CLOSE
					system("PAUSE");
#endif
					return 0;
				}
			}
		}
	}

	//------------------------------------------------------------------------------------
	//Get snippets
	//------------------------------------------------------------------------------------

	auto snippets_val = obj["snippets"];

	if(snippets_val != nullptr)
	{
		if(snippets_val->getType() != ValueType::OBJECT)
		{
			printError(full_shader_filename, snippets_val->getLineNum(), "'snippets' must be an object!");

#if PAUSE_BEFORE_CLOSE
			system("PAUSE");
#endif
			return 0;
		}

		getSnippets(full_shader_filename, snippets_val->asObject(), snippets);
	}

	auto type_val = obj["type"];

	if(type_val == nullptr)
	{
		std::cout << "Error: " << full_shader_filename << ": Missing 'type'!\n";

	}
	else if(type_val->getType() != ValueType::STRING)
	{
		printError(full_shader_filename, type_val->getLineNum(), "'type' must be a string!");

#if PAUSE_BEFORE_CLOSE
		system("PAUSE");
#endif
		return 0;
	}
	else
	{
		auto& type = type_val->asString();

		if(type == "render")
		{
			RenderDesc desc;

			getRender(common, full_shader_filename, obj, snippets, desc);

			compileRender(common, full_shader_filename, shader_filename, desc, snippets, output_filename, debug);
		}
		else if(type == "compute")
		{
			std::vector<ComputeKernel> kernels;

			getComputeKernels(common, full_shader_filename, obj, snippets, kernels);

			compileComputeKernels(common, full_shader_filename, kernels, snippets, output_filename, debug);
		}
		else
		{
			printError(full_shader_filename, type_val->getLineNum(), 
					   "Invalid shader type! Valid types are 'render' and 'compute'.");
		}
	}

#if PAUSE_BEFORE_CLOSE
	system("PAUSE");
#endif

	return 0;
}