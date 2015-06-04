#include "Common.h"

#include "Condition.h"

#include "StringID.h"

#include <algorithm>
#include <iostream>
#include <fstream>

const std::vector<std::string> srvs_types =
{
	"Buffer", "Texture1D", "Texture2D", "Texture3D",
	"Texture1DArray", "Texture2DArray", "Texture2DMS", "Texture2DMSArray", "StructuredBuffer"
};

const std::vector<std::string> uav_types =
{
	"RWBuffer", "RWTexture1D", "RWTexture2D", "RWTexture3D",
	"RWTexture1DArray", "RWTexture2DArray", "RWTexture3D",
	"RWStructuredBuffer", "AppendStructuredBuffer", "ConsumeStructuredBuffer"
};

const std::map<std::string, TypeDesc> types_descs =
{
	{ "float", { Type::FLOAT, 4 } }, { "float2", { Type::FLOAT2, 8 } }, 
	{ "float3", { Type::FLOAT3, 12 } }, { "float4", { Type::FLOAT4, 16 } },
	{ "int", { Type::INT, 4 } }, { "int2", { Type::INT2, 8 } }, 
	{ "int3", { Type::INT3, 12 } }, { "int4", { Type::INT4, 16 } },
	{ "uint", { Type::UINT, 4 } }, { "uint2", { Type::UINT2, 8 } }, 
	{ "uint3", { Type::UINT3, 12 } }, { "uint4", { Type::UINT4, 16 } },
	{ "float3x3", { Type::FLOAT3X3, 36 } }, { "float4x4", { Type::FLOAT4X4, 64 } },
};

const std::map<std::string, ResourceType> resource_types =
{
	{ "AppendStructuredBuffer", ResourceType::APPEND_STRUCTURED_BUFFER },
	{ "Buffer", ResourceType::BUFFER },
	{ "ByteAddressBuffer", ResourceType::BYTE_ADDRESS_BUFFER },
	{ "ConsumeStructuredBuffer", ResourceType::CONSUME_STRUCTURED_BUFFER },
	{ "RWBuffer", ResourceType::RW_BUFFER },
	{ "RWByteAddressBuffer", ResourceType::RW_BYTE_ADDRESS_BUFFER },
	{ "RWStructuredBuffer", ResourceType::RW_STRUCTURED_BUFFER },
	{ "RWTexture1D", ResourceType::RW_TEXTURE1D },
	{ "RWTexture1DArray", ResourceType::RW_TEXTURE1D_ARRAY },
	{ "RWTexture2D", ResourceType::RW_TEXTURE2D },
	{ "RWTexture2DArray", ResourceType::RW_TEXTURE2D_ARRAY },
	{ "RWTexture3D", ResourceType::RW_TEXTURE3D },
	{ "StructuredBuffer", ResourceType::STRUCTURED_BUFFER },
	{ "Texture1D", ResourceType::TEXTURE1D },
	{ "Texture1DArray", ResourceType::TEXTURE1D_ARRAY },
	{ "Texture2D", ResourceType::TEXTURE2D },
	{ "Texture2DArray", ResourceType::TEXTURE2D_ARRAY },
	{ "Texture2DMS", ResourceType::TEXTURE2DMS },
	{ "Texture2DMSArray", ResourceType::TEXTURE2DMS_ARRAY },
	{ "Texture3D", ResourceType::TEXTURE3D }
};

//-----------------------------------------------------------------------------------------------------------

ParameterGroupReflection::ParameterGroupReflection(const ParameterGroupDesc& group_desc, u8 num_passes)
	: _group_desc(group_desc), _num_passes(num_passes)
{

}

void ParameterGroupReflection::reflectPermutation(const std::vector<Permutation>& permutation, u8 pass_num,
												  ShaderType shader_type, ID3D11ShaderReflection* shader_reflection)
{
	std::vector<ParameterGroupPermutationReflection*> group_permutations;

	for(auto p : permutation)
	{
		p &= _group_desc.options_mask;

		auto it = _permutations.find(p);

		auto& group_permutation = _permutations[p];

		group_permutations.push_back(&group_permutation);

		if(it == _permutations.end())
		{
			group_permutation.cbuffers.resize(_group_desc.cbuffers.size());

			for(auto& x : group_permutation.cbuffers)
				x.resize(_num_passes, 0);

			group_permutation.srvs.resize(_group_desc.srvs.size());

			for(auto& x : group_permutation.srvs)
				x.resize(_num_passes, 0);

			group_permutation.uavs.resize(_group_desc.uavs.size());

			for(auto& x : group_permutation.uavs)
				x.resize(_num_passes, 0);
		}
	}

	for(size_t i = 0; i < _group_desc.cbuffers.size(); i++)
	{
		D3D11_SHADER_INPUT_BIND_DESC desc;

		if(FAILED(shader_reflection->GetResourceBindingDescByName(_group_desc.cbuffers[i].name.c_str(), &desc)))
			continue;

		for(auto& group_permutation : group_permutations)
			group_permutation->cbuffers[i][pass_num] |= (u8)shader_type;
	}

	for(size_t i = 0; i < _group_desc.srvs.size(); i++)
	{
		D3D11_SHADER_INPUT_BIND_DESC desc;

		if(FAILED(shader_reflection->GetResourceBindingDescByName(_group_desc.srvs[i].name.c_str(), &desc)))
			continue;

		for(auto& group_permutation : group_permutations)
			group_permutation->srvs[i][pass_num] |= (u8)shader_type;
	}

	for(size_t i = 0; i < _group_desc.uavs.size(); i++)
	{
		D3D11_SHADER_INPUT_BIND_DESC desc;

		if(FAILED(shader_reflection->GetResourceBindingDescByName(_group_desc.uavs[i].name.c_str(), &desc)))
			continue;

		for(auto& group_permutation : group_permutations)
			group_permutation->uavs[i][pass_num] |= (u8)shader_type;
	}
}

ParameterGroupPermutations ParameterGroupReflection::getPermutations(std::vector<u8>& command_groups) const
{
	ParameterGroupPermutations out;

	RuntimeCBuffers cbuffers;
	RuntimeConstants constants;
	RuntimeResources srvs;
	RuntimeResources uavs;

	for(auto& permutation_reflection : _permutations)
	{
		cbuffers.names.clear();
		cbuffers.sizes.clear();
		constants.names.clear();
		constants.offsets.clear();
		srvs.names.clear();
		uavs.names.clear();

		ParameterGroupPermutation permutation;

		std::vector<std::vector<u8>> vec(_num_passes);

		u8 num_cbuffers             = 0;
		u32 current_constant_offset = 0;
		
		for(u8 i = 0; i < _group_desc.cbuffers.size(); i++)
		{
			bool used = false;

			for(u8 j = 0; j < _num_passes; j++)
			{
				auto stages = permutation_reflection.second.cbuffers[i][j];

				if(stages != 0)
				{
					vec[j].push_back(num_cbuffers);					//index
					vec[j].push_back(_group_desc.cbuffers[i].slot); //slot
					vec[j].push_back(stages);						//stages
					used = true;
				}
			}

			if(used)
			{
				u32 cbuffer_size = 0;

				for(auto& constant : _group_desc.cbuffers[i].constants)
				{
					//check constant condition!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
					if(constant.condition != nullptr && !constant.condition->evaluate(permutation_reflection.first))
						continue;

					u32 constant_size       = constant.type.size;
					u32 total_constant_size = constant_size;

					if(constant.array_size > 0)
					{
						u8 x = constant_size % 16;

						if(x > 0)
							constant_size = constant_size + 16 - x;

						total_constant_size = constant_size * constant.array_size;
					}

					u32 prev_vector_index = cbuffer_size / 16;
					u32 vector_index      = (cbuffer_size + constant_size) / 16;
					u8 x                  = (cbuffer_size + constant_size) % 16;

					//If constant crosses 16-byte boundary
					if(vector_index != prev_vector_index && x != 0)
					{
						//add padding
						u8 padding = 16 - (cbuffer_size % 16);

						current_constant_offset += padding;
						cbuffer_size			+= padding;
					}

					constants.names.push_back(aqua::getStringID(constant.name.c_str()));
					constants.offsets.push_back(current_constant_offset);

					current_constant_offset += total_constant_size;
					cbuffer_size			+= total_constant_size;
				}

				//cbuffer size must be a multiple of 16
				if(cbuffer_size % 16 != 0)
				{
					current_constant_offset += 16 - cbuffer_size % 16;
					cbuffer_size			+= 16 - cbuffer_size % 16;
				}

				cbuffers.names.push_back(aqua::getStringID(_group_desc.cbuffers[i].name.c_str()));
				cbuffers.sizes.push_back(cbuffer_size);

				num_cbuffers++;
			}
		}

		for(u8 i = 0; i < _num_passes; i++)
			vec[i].push_back(UINT8_MAX); //cbuffers terminator

		u8 num_srvs = 0;

		for(u8 i = 0; i < _group_desc.srvs.size(); i++)
		{
			bool used = false;

			for(u8 j = 0; j < _num_passes; j++)
			{
				auto stages = permutation_reflection.second.srvs[i][j];

				if(stages != 0)
				{
					vec[j].push_back(num_srvs);					//index
					vec[j].push_back(_group_desc.srvs[i].slot); //slot
					vec[j].push_back(stages);					//stages
					used = true;
				}
					
			}

			if(used)
			{
				srvs.names.push_back(aqua::getStringID(_group_desc.srvs[i].name.c_str()));
				num_srvs++;
			}
		}

		for(u8 i = 0; i < _num_passes; i++)
			vec[i].push_back(UINT8_MAX); //srvs terminator

		u8 num_uavs = 0;

		for(u8 i = 0; i < _group_desc.uavs.size(); i++)
		{
			bool used = false;

			for(u8 j = 0; j < _num_passes; j++)
			{
				auto stages = permutation_reflection.second.uavs[i][j];

				if(stages != 0)
				{
					vec[j].push_back(num_uavs);					//index
					vec[j].push_back(_group_desc.uavs[i].slot); //slot
					vec[j].push_back(stages);					//stages
					used = true;
				}

			}

			if(used)
			{
				uavs.names.push_back(aqua::getStringID(_group_desc.uavs[i].name.c_str()));
				num_uavs++;
			}
		}

		for(u8 i = 0; i < _num_passes; i++)
			vec[i].push_back(UINT8_MAX); //uavs terminator

		//Add cmd_groups to global vector
		for(u8 i = 0; i < _num_passes; i++)
		{
			//Search for a equal sequence
			auto it = std::search(command_groups.cbegin(), command_groups.cend(), vec[i].cbegin(), vec[i].cend());

			if(it != command_groups.cend())
			{
				//If equal sequence was found, reuse it
				permutation.cmd_groups_indices.push_back(std::distance(command_groups.cbegin(), it));
			}
			else
			{
				//Else add the new sequence
				permutation.cmd_groups_indices.push_back(command_groups.size());

				command_groups.insert(command_groups.cend(), vec[i].cbegin(), vec[i].cend());
			}
		}

		//CBuffers
		if(cbuffers.names.size() > 0)
		{
			auto it = std::find(out.cbuffers_vector.cbegin(), out.cbuffers_vector.cend(), cbuffers);

			if(it == out.cbuffers_vector.cend())
			{
				out.cbuffers_vector.push_back(cbuffers);
				permutation.cbuffers_index = out.cbuffers_vector.size() - 1;
			}
			else
			{
				permutation.cbuffers_index = std::distance(out.cbuffers_vector.cbegin(), it);
			}
		}
		else
			permutation.cbuffers_index = UINT32_MAX;

		//Constants
		if(constants.names.size() > 0)
		{
			auto it = std::find(out.constants_vector.cbegin(), out.constants_vector.cend(), constants);

			if(it == out.constants_vector.cend())
			{
				out.constants_vector.push_back(constants);
				permutation.constants_index = out.constants_vector.size() - 1;
			}
			else
			{
				permutation.constants_index = std::distance(out.constants_vector.cbegin(), it);
			}
		}
		else
			permutation.constants_index = UINT32_MAX;

		//SRVS
		if(srvs.names.size() > 0)
		{
			auto it = std::find(out.srvs_vector.cbegin(), out.srvs_vector.cend(), srvs);

			if(it == out.srvs_vector.cend())
			{
				out.srvs_vector.push_back(srvs);
				permutation.srvs_index = out.srvs_vector.size() - 1;
			}
			else
			{
				permutation.srvs_index = std::distance(out.srvs_vector.cbegin(), it);
			}
		}
		else
			permutation.srvs_index = UINT32_MAX;

		//UAVS
		if(uavs.names.size() > 0)
		{
			auto it = std::find(out.uavs_vector.cbegin(), out.uavs_vector.cend(), uavs);

			if(it == out.uavs_vector.cend())
			{
				out.uavs_vector.push_back(uavs);
				permutation.uavs_index = out.uavs_vector.size() - 1;
			}
			else
			{
				permutation.uavs_index = std::distance(out.uavs_vector.cbegin(), it);
			}
		}
		else
			permutation.uavs_index = UINT32_MAX;

		out.permutations.emplace(permutation_reflection.first, std::move(permutation));
	}

	return out;
}

//-----------------------------------------------------------------------------------------------------------

bool operator==(const RuntimeCBuffers& x, const RuntimeCBuffers& y)
{
	return x.names == y.names && x.sizes == y.sizes;
}

bool operator==(const RuntimeConstants& x, const RuntimeConstants& y)
{
	return x.names == y.names && x.offsets == y.offsets;
}

bool operator==(const RuntimeResources& x, const RuntimeResources& y)
{
	return x.names == y.names;
}

bool operator==(const InputLayoutElement& x, const InputLayoutElement& y)
{
	return	x.semantic_name     == y.semantic_name		&&
			x.semantic_index    == y.semantic_index		&&
			x.register_num      == y.register_num		&&
			//x.system_value_type == y.system_value_type	&&
			x.component_type    == y.component_type		&&
			x.mask              == y.mask				&&
			x.read_write_mask   == y.read_write_mask	&&
			x.stream            == y.stream				&&
			x.min_precision     == y.min_precision;
};

void skipWhitespaces(const char*& str)
{
	while(*str == ' ' || *str == '\t' || *str == '\n')
		str++;
}

bool validOptionName(const std::string& name)
{
	if(name.empty())
		return false;

	if(name[0] < 'A' || name[0] > 'Z')
		return false;

	for(char c : name)
	{
		if((c < 'A' || c > 'Z') && (c < '0' || c > '9') && c != '_')
			return false;
	}

	return true;
}

std::string parseOptionName(const char*& str)
{
	const char* start = str;

	if(*start < 'A' || *start > 'Z')
		return std::string();

	str++;

	while((*str >= 'A' && *str <= 'Z') || (*str >= '0' && *str <= '9') || *str == '_')
		str++;

	return std::string(start, str - start);
}

std::shared_ptr<OptionValue> parseOptionValue(const char*& condition, const std::vector<OptionsGroup>& options)
{
	std::string option_name = parseOptionName(condition);

	for(auto& group : options)
	{
		for(auto& option : group.options)
		{
			if(option.name == option_name)
			{
				return std::make_shared<OptionValue>(option.bits, ~group.clear_mask);
			}
		}
	}

	return nullptr;
}

std::shared_ptr<Condition> parseCondition(const char*& condition, const std::vector<OptionsGroup>& options);

std::shared_ptr<Condition> parseTerm(const char*& condition, const std::vector<OptionsGroup>& options)
{
	skipWhitespaces(condition);

	if(*condition == '(')
	{
		condition++;

		auto cond = parseCondition(condition, options);

		if(*condition != ')')
			return nullptr;

		condition++;

		return cond;
	}
	else if(*condition == '!')
	{
		condition++;

		return std::make_shared<Operation>(OperationType::NOT, parseTerm(condition, options), nullptr);
	}
	else if(condition[0] >= 'A' && condition[0] <= 'Z')
		return parseOptionValue(condition, options);
	else
		return nullptr; //ERROR------------------------------------------
}

std::shared_ptr<Condition> parseCondition(const char*& condition, const std::vector<OptionsGroup>& options)
{
	std::shared_ptr<Condition> left;
	std::shared_ptr<Condition> right;

	left = parseTerm(condition, options);

	while(true)
	{
		OperationType op_type = OperationType::INVALID;

		skipWhitespaces(condition);

		if(*condition == '&' && *(condition + 1) == '&')
			op_type = OperationType::AND;
		else if(*condition == '|' && *(condition + 1) == '|')
			op_type = OperationType::OR;

		if(op_type == OperationType::INVALID)
			break;

		condition += 2;

		left = std::make_shared<Operation>(op_type, left, parseTerm(condition, options));
	}

	return left;
}

//Tries to consume <value> from <source>
//If fails, restores file to original state
bool consume(const char*& str, const char* value)
{
	const char* temp = str;

	while(*str == *value && *value != '\0')
	{
		str++;
		value++;
	}

	if(*value == '\0')
		return true;

	//Didnt match so restore state
	str = temp;

	return false;
}

Permutation calculateSnippetOptionsMask(const std::string& snippet, const std::vector<OptionsGroup>& options)
{
	Permutation options_mask = 0;

	const char* source = snippet.c_str();

	skipWhitespaces(source);

	while(*source != '\0')
	{
		//skip multiline comments
		if(consume(source, "/*"))
		{
			while(!consume(source, "*/") && *source != '\0')
				source++;

			skipWhitespaces(source);
			continue;
		}

		//skip line comments
		if(consume(source, "//"))
		{
			while(*source != '\0' && *source != '\n')
				source++;

			skipWhitespaces(source);
			continue;
		}
		
		std::string option_name = parseOptionName(source);

		if(!option_name.empty())
		{
			bool found = false;

			for(auto& group : options)
			{
				for(auto& option : group.options)
				{
					if(option.name == option_name)
					{
						options_mask |= ~group.clear_mask;
						found = true;
						break;
					}
				}

				if(found)
					break;
			}
		}
		else
			source++;

		skipWhitespaces(source);
	}

	return options_mask;
}

bool getOptions(const std::string& filename, const Object& obj, u8& group_start, std::vector<OptionsGroup>& out)
{
	auto options_value = obj["options"];

	if(options_value == nullptr)
		return true;

	if(options_value->getType() != ValueType::ARRAY)
	{
		printError(filename, options_value->getLineNum(), "Options must be an array!");
		return false;
	}

	const Array& options = options_value->asArray();

	for(auto& group_desc : options)
	{
		OptionsGroup group;
		u8           num_bits;

		if(group_desc.getType() == ValueType::OBJECT)
		{
			const Object& option_obj = group_desc.asObject();

			auto val = option_obj["define"];

			if(val->getType() != ValueType::STRING)
			{

			}

			auto& option_name = val->asString();

			if(!validOptionName(option_name))
			{
				printError(filename, group_desc.getLineNum(), "Invalid option name!");
				return false;
			}

			val = option_obj["condition"];

			std::shared_ptr<Condition> condition = nullptr;

			if(val != nullptr)
			{
				if(val->getType() != ValueType::STRING)
				{

				}

				auto condition_str = val->asString().c_str();

				condition = parseCondition(condition_str, out);
			}

			num_bits = 1;

			group.clear_mask = ~(1ULL << group_start);

			group.options.push_back({ option_name, ~group.clear_mask, condition });
		}
		else if(group_desc.getType() == ValueType::ARRAY)
		{
			const Array& array = group_desc.asArray();

			size_t array_size = array.size();

			if(array_size == 0)
				continue;
			else if(array_size == 1)
			{
				auto& option = array[0];

				if(option.getType() != ValueType::STRING)
				{
					printError(filename, option.getLineNum(), "Option must be a string!");
					return false;
				}

				const std::string& option_name = option.asString();

				if(!validOptionName(option_name))
				{
					printError(filename, option.getLineNum(), "Invalid option name!");
					return false;
				}

				num_bits = 0;

				group.clear_mask = ~(0ULL);

				group.options.push_back({ option_name, ~group.clear_mask });
			}
			else
			{
				num_bits = (u8)std::floor(std::log2(array.size() - 1) + 1);

				group.clear_mask = ~(((1ULL << num_bits) - 1) << group_start);

				for(size_t i = 0; i < array.size(); i++)
				{
					auto& option = array[i];

					if(option.getType() != ValueType::STRING)
					{
						printError(filename, option.getLineNum(), "Option must be a string!");
						return false;
					}

					const std::string& option_name = option.asString();

					if(!validOptionName(option_name))
					{
						printError(filename, option.getLineNum(), "Invalid option name!");
						return false;
					}

					group.options.push_back({ option_name, i << group_start });
				}
			}
		}
		else
		{
			printError(filename, group_desc.getLineNum(), "Group must be an array or an object!");
			return false;
		}

		out.push_back(std::move(group));

		group_start += num_bits;
	}

	return true;
}

bool parseCBuffer(const std::string& filename, const std::string& name, const Value& cbuffer, u32 reg,
				  const std::vector<OptionsGroup>* options, std::string& out_code, std::vector<Constant>& out_constants)
{
	out_code += "cbuffer " + name + " : register(b" + std::to_string(reg) + ")\n{\n";

	if(cbuffer.getType() != ValueType::OBJECT)
	{
		printError(filename, cbuffer.getLineNum(), "'" + name + "' must be an object!");
		return false;
	}

	for(size_t i = 0; i < cbuffer.asObject().size(); i++)
	{
		Object::Entry constant_entry = cbuffer.asObject()[i];

		Constant constant;
		constant.name = constant_entry.key;

		std::string type;
		std::string condition;
		std::string array_size;

		if(constant_entry.value.getType() == ValueType::STRING)
		{
			type                = constant_entry.value.asString();
			constant.array_size = 0;
		}
		else if(constant_entry.value.getType() == ValueType::OBJECT)
		{
			auto& con = constant_entry.value.asObject();

			auto val = con["type"];

			if(val != nullptr)
			{
				if(val->getType() == ValueType::STRING)
				{
					type = val->asString();
				}
				else
				{

				}
			}
			else
			{

			}

			val = con["condition"];

			if(val != nullptr)
			{
				if(val->getType() == ValueType::STRING)
				{
					if(options == nullptr)
					{
						printError(filename, val->getLineNum(), "Options list not provided!");
						return false;
					}

					condition = val->asString();

					const char* cond = condition.c_str();
					constant.condition = parseCondition(cond, *options);
				}
				else
				{
					printError(filename, val->getLineNum(), "'condition' must be a string!");
					return false;
				}
			}

			val = con["array_size"];

			if(val != nullptr)
			{
				if(val->getType() == ValueType::NUMBER)
				{
					constant.array_size = (u32)val->asNumber();

					array_size = '[' + std::to_string(constant.array_size) + ']';
				}
				else
				{
					printError(filename, val->getLineNum(), "'array_size' must be a number!");
					return false;
				}
			}
			else
				constant.array_size = 0;
		}
		else
		{
			printError(filename, constant_entry.value.getLineNum(), 
						"'" + constant_entry.key + "' must be a string or an object!");
			return false;
		}

		auto type_desc_it = types_descs.find(type);

		if(type_desc_it == types_descs.end())
		{
			printError(filename, constant_entry.value.getLineNum(), "Invalid constant type!");
			return false;
		}

		constant.type = type_desc_it->second;

		out_constants.push_back(std::move(constant));

		if(!condition.empty())
		{
			out_code += "\t#if " + condition + "\n";
			out_code += "\t\t#line " + std::to_string(constant_entry.key_line) + "\n";
			out_code += "\t\t" + type + " " + constant.name + array_size + ";\n";
			out_code += "\t#endif\n";
		}
		else
		{
			out_code += "\t#line " + std::to_string(constant_entry.key_line) + "\n";
			out_code += "\t" + type + " " + constant.name + array_size + ";\n";
		}
	}

	out_code += "};\n\n";

	return true;
}

bool parseParameterGroupDesc(const std::string& filename, const Object& obj, u8 start_cbuffer, u8 start_srv, 
							 u8 start_uav, const std::vector<OptionsGroup>* options, ParameterGroupDesc& out)
{
	const Value* cbuffers = obj["constant_buffers"];

	if(cbuffers != nullptr)
	{
		if(cbuffers->getType() != ValueType::OBJECT)
		{
			printError(filename, cbuffers->getLineNum(), "'constant_buffers' must be an object!");
			return false;
		}

		for(size_t i = 0; i < cbuffers->asObject().size(); i++)
		{
			Object::Entry cbuffer_desc = cbuffers->asObject()[i];

			std::vector<Constant> constants;

			out.code += "#line " + std::to_string(cbuffer_desc.key_line) + " \"" + filename + "\"\n";
			
			if(!parseCBuffer(filename, cbuffer_desc.key, cbuffer_desc.value, start_cbuffer, options, out.code, constants))
				return false;

			out.cbuffers.push_back({ cbuffer_desc.key, constants, start_cbuffer });

			start_cbuffer++;
		}
	}

	const Value* resources = obj["resources"];

	if(resources != nullptr)
	{
		if(resources->getType() != ValueType::OBJECT)
		{
			printError(filename, resources->getLineNum(), "'resources' must be an object!");
			return false;
		}

		for(size_t i = 0; i < resources->asObject().size(); i++)
		{
			Object::Entry resource = resources->asObject()[i];

			if(resource.value.getType() != ValueType::OBJECT)
			{
				printError(filename, resource.value.getLineNum(), "'" + resource.key + "' must be an object!");
				return false;
			}

			const Object& resource_desc = resource.value.asObject();

			const Value* resource_type = resource_desc["type"];

			if(resource_type == nullptr)
			{
				std::cout << "Error: " << filename << ":" << resource.value.getLineNum() <<
					": '" << resource.key << "'-'type' missing!\n";
				return false;
			}

			if(resource_type->getType() != ValueType::STRING)
			{
				std::cout << "Error: " << filename << ":" << resource_type->getLineNum() <<
					": 'type' must be a string!\n";
				return false;
			}

			std::string reg;

			auto& type = resource_type->asString();

			if(std::find(srvs_types.begin(), srvs_types.end(), type) != srvs_types.end())
			{
				reg = "t" + std::to_string(start_srv);

				out.srvs.push_back({ resource.key, "", resource_types.find(type)->second, start_srv });

				start_srv++;
			}
			else if(std::find(uav_types.begin(), uav_types.end(), type) != uav_types.end())
			{
				reg = "u" + std::to_string(start_uav);

				out.uavs.push_back({ resource.key, "", resource_types.find(type)->second, start_uav });

				start_uav++;
			}
			else
			{
				std::cout << "Error: " << filename << ":" << resource_type->getLineNum() <<
					": Resource type '" << type << "' is invalid!\n";
				return false;
			}

			const Value* resource_element_type = resource_desc["element_type"];

			out.code += "#line " + std::to_string(resource.key_line) + " \"" + filename + "\"\n";

			if(resource_element_type != nullptr)
			{
				if(resource_element_type->getType() != ValueType::STRING)
				{
					printError(filename, resource_element_type->getLineNum(), "'element_type' must be a string!");
					return false;
				}

				out.code += resource_type->asString() + "<" + resource_element_type->asString() + "> " +
							resource.key + " : register(" + reg + ");\n";
			}
			else
			{
				out.code += resource_type->asString() + " " + resource.key + " : register(" + reg + ");\n";
			}
		}

		out.code += "\n";
	}

	return true;
}

bool getSnippets(const std::string& filename, const Object& snippets, std::map<std::string, Snippet>& out)
{
	for(size_t i = 0; i < snippets.size(); i++)
	{
		Object::Entry entry = snippets[i];

		const std::string& snippet_name = entry.key;

		if(out.find(snippet_name) != out.end())
		{
			printError(filename, entry.key_line, "Snippet "  + snippet_name + "already defined!");
			return false;
		}

		Snippet snippet;

		if(entry.value.getType() != ValueType::OBJECT)
		{
			printError(filename, entry.value.getLineNum(), "Snippet must be an object!");

			return false;
		}

		const Object& snippet_desc = entry.value.asObject();

		auto includes_val = snippet_desc["include"];

		if(includes_val != nullptr)
		{
			if(includes_val->getType() != ValueType::ARRAY)
			{
				printError(filename, includes_val->getLineNum(), "'include' must be an array!");
				return false;
			}

			const Array& includes = includes_val->asArray();

			for(auto& include : includes)
			{
				if(include.getType() != ValueType::STRING)
				{
					printError(filename, include.getLineNum(), "'include' must be an array of strings!");
					return false;
				}

				snippet.includes.push_back(include.asString());
			}
		}

		auto hlsl_val = snippet_desc["hlsl"];

		if(hlsl_val == nullptr)
		{
			printError(filename, entry.value.getLineNum(), "Snippet is missing code!");
			return false;
		}

		if(hlsl_val->getType() != ValueType::STRING)
		{
			printError(filename, hlsl_val->getLineNum(), "'hlsl' must be a string!");
			return false;
		}

		snippet.code = "//Snippet '" + snippet_name + "':\n";
		snippet.code += "#line " + std::to_string(hlsl_val->getLineNum()) + " \"" + filename + "\"\n";

		snippet.code += hlsl_val->asString();

		//snippet.options_mask = calculateSnippetOptionsMask(snippet.code, options);

		out[snippet_name] = std::move(snippet);
	}

	return true;
}

bool buildFullCodeEx(const Snippet& snippet, const std::map<std::string, Snippet>& snippets,
					 std::string& out_code, std::vector<std::string>& included_snippets)
{
	//Add dependencies
	for(auto& include : snippet.includes)
	{
		auto it = snippets.find(include);

		if(it == snippets.end())
		{
			printError("", 0, "Couldn't find snippet '" + include + "'!");
			return false;
		}

		{
			auto it = std::find(included_snippets.cbegin(), included_snippets.cend(), include);

			if(it != included_snippets.cend())
				continue;
		}

		const Snippet& x = it->second;

		buildFullCodeEx(x, snippets, out_code, included_snippets);
	}

	out_code += snippet.code + '\n';

	return true;
}

bool buildFullCode(const Snippet& snippet, const std::map<std::string, Snippet>& snippets,
				   std::string& out_code)
{
	std::vector<std::string> included_snippets;

	return buildFullCodeEx(snippet, snippets, out_code, included_snippets);
}

bool reflectInputLayout(ID3D11ShaderReflection* shader_reflection, std::vector<InputLayoutElement>& il)
{
	// Get shader info
	D3D11_SHADER_DESC shader_desc;

	if(FAILED(shader_reflection->GetDesc(&shader_desc)))
		return false;

	for(UINT i = 0; i < shader_desc.InputParameters; i++)
	{
		D3D11_SIGNATURE_PARAMETER_DESC input_desc;
		
		if(FAILED(shader_reflection->GetInputParameterDesc(i, &input_desc)))
			return false;

		if(input_desc.SystemValueType != D3D10_NAME_UNDEFINED)
			continue;
			
		InputLayoutElement element;
		element.semantic_name     = input_desc.SemanticName;
		element.semantic_index    = input_desc.SemanticIndex;
		element.register_num      = input_desc.Register;
		//element.system_value_type = input_desc.SystemValueType;
		element.component_type    = input_desc.ComponentType;
		element.mask              = input_desc.Mask;
		element.read_write_mask   = input_desc.ReadWriteMask;
		element.stream            = input_desc.Stream;
		element.min_precision     = input_desc.MinPrecision;

		il.push_back(std::move(element));
	}

	return true;
}

bool compileFunctionPermutations(const std::string& shader_filename, const Function& func, bool debug,
								 const std::map<Permutation, std::vector<Permutation>>& permutations,
								 const std::vector<OptionsGroup>& options, u64 mesh_options_mask,
								 std::vector<ParameterGroupReflection*>& param_groups,
								 std::vector<InputLayout>* input_layouts,
								 std::vector<u32>* input_layout_indices, std::ostream& out_stream)
{
	out_stream.write(func.name.c_str(), (func.name.size() + 1)*sizeof(char));

	std::cout << std::endl << "Compiling " << func.name << " permutations..." << std::endl << std::endl;

	UINT flags = D3DCOMPILE_ENABLE_STRICTNESS |
				 //D3DCOMPILE_IEEE_STRICTNESS |
				 D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;
				 //D3DCOMPILE_WARNINGS_ARE_ERRORS;


	if(debug)
		flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_WARNINGS_ARE_ERRORS;
	else
		flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;

	const char* target;
	const char* entry_point;

	switch(func.type)
	{
	case ShaderType::VERTEX_SHADER:
		target      = "vs_5_0";
		entry_point = "vs_main";
		break;
	case ShaderType::HULL_SHADER:
		target      = "hs_5_0";
		entry_point = "hs_main";
		break;
	case ShaderType::DOMAIN_SHADER:
		target      = "ds_5_0";
		entry_point = "ds_main";
		break;
	case ShaderType::GEOMETRY_SHADER:
		target      = "gs_5_0";
		entry_point = "gs_main";
		break;
	case ShaderType::PIXEL_SHADER:
		target      = "ps_5_0";
		entry_point = "ps_main";
		break;
	case ShaderType::COMPUTE_SHADER:
		target      = "cs_5_0";
		entry_point = "cs_main";
		break;
	default:
		target      = nullptr;
		entry_point = nullptr;
	}

	static_assert(sizeof(ShaderType) == 1, "sizeof(ShaderType) should be 1 byte");

	//Write shader type
	out_stream.write((const char*)&func.type, sizeof(ShaderType));

	{
		u32 num_permutations = permutations.size();

		out_stream.write((char*)&num_permutations, sizeof(num_permutations));
	}

	size_t num_failed_permutations = 0;

	std::vector<D3D_SHADER_MACRO> defines;
	defines.reserve(options.size() + 2);

	for(const auto& permutation : permutations)
	{
		defines.clear();

		for(auto& group : options)
		{
			//Find best fit option in this group
			for(auto& option : group.options)
			{
				if((permutation.first & ~group.clear_mask) == option.bits)
				{
					defines.push_back({ option.name.c_str(), "1" });
					break;
				}
			}
		}

		//Terminator
		defines.push_back({ nullptr, nullptr });

		ID3DBlob* code = nullptr;
		ID3DBlob* errors = nullptr;

		HRESULT result = D3DCompile(func.code.c_str(), func.code.size(), shader_filename.c_str(),
								    &defines.at(0), D3D_COMPILE_STANDARD_FILE_INCLUDE,
								    entry_point, target, flags, 0, &code, &errors);

		if(errors != nullptr)
		{
			std::cout << "Error: compiling permutation " << permutation.first << std::endl;
			std::cout << (char*)errors->GetBufferPointer();
			errors->Release();

			if(result != S_OK)
			{
				num_failed_permutations++;

				u32 code_size = 0;
				out_stream.write((char*)&code_size, sizeof(code_size));

				continue;
			}
		}

		if(code->GetBufferSize() > UINT32_MAX)
		{
			//error----------------------------------------------------------------------
		}

		//Save bytecode offset to create input layouts later
		u32 code_offset = (u32)out_stream.tellp();

		u32 code_size = static_cast<u32>(code->GetBufferSize());

		out_stream.write((char*)&code_size, sizeof(code_size));
		out_stream.write((char*)code->GetBufferPointer(), code_size);

		//Reflect shader
		ID3D11ShaderReflection* shader_reflection = nullptr;
		if(FAILED(D3DReflect(code->GetBufferPointer(), code->GetBufferSize(), IID_ID3D11ShaderReflection,
							(void**)&shader_reflection)))
		{
			return false;
		}

		for(auto& param_group : param_groups)
			param_group->reflectPermutation(permutation.second, func.pass, func.type, shader_reflection);

		if(func.type == ShaderType::VERTEX_SHADER)
		{
			InputLayout il;
			il.bytecode_offset  = code_offset;
			il.mesh_permutation = permutation.first & mesh_options_mask;

			if(!reflectInputLayout(shader_reflection, il.elements))
				return false;

			if(il.elements.empty())
				input_layout_indices->push_back(UINT32_MAX); //Permutation doesn't use mesh vertex data
			else
			{
				//Store offset of bytecode so the engine can create the input layout

				auto it = std::find_if(input_layouts->cbegin(), input_layouts->cend(), [&il](const InputLayout& x)
				{
					return x.elements == il.elements && x.mesh_permutation == il.mesh_permutation;
				});

				if(it == input_layouts->cend())
				{
					input_layouts->push_back(il);
					input_layout_indices->push_back(input_layouts->size() - 1);
				}
				else
				{
					input_layout_indices->push_back(std::distance(input_layouts->cbegin(), it));
				}
			}
		}

		code->Release();
	}

	std::cout << std::endl << " - " << permutations.size() - num_failed_permutations 
			  << " permutations of " << func.name << " compiled!" << std::endl << std::endl;

	return true;
}

struct RuntimeOption
{
	Permutation bits;
	Permutation group_clear_mask;;
};

void writeParameterGroup(u32 offset, Permutation options_mask, const std::vector<OptionsGroup>& options,
						 const ParameterGroupPermutations& permutations,
						 RuntimeParameterGroup& out, std::ostream& out_stream)
{
	out.num_permutations = permutations.permutations.size();

	alignStream(out_stream, __alignof(Permutation));

	out.permutations_nums_offset = (u32)out_stream.tellp() - offset;

	for(auto& permutation : permutations.permutations)
	{
		out_stream.write((const char*)&permutation.first, sizeof(Permutation));
	}

	alignStream(out_stream, __alignof(RuntimeParameterGroupPermutation));

	out.permutations_offset = (u32)out_stream.tellp() - offset;

	//Reserve space for permutations
	{
		u32 space_size = sizeof(RuntimeParameterGroupPermutation) * permutations.permutations.size();
		char* space = (char*)malloc(space_size);

		out_stream.write(space, space_size);

		free(space);
	}

	//Write options
	out.options_mask = options_mask;
	out.num_options = 0;

	alignStream(out_stream, __alignof(u32));

	out.options_names_offset = (u32)out_stream.tellp() - offset;

	for(auto& option_group : options)
	{
		for(auto& option : option_group.options)
		{
			if(option.bits != 0 && (options_mask & option.bits) == 0)
				continue;

			u32 name_hash = aqua::getStringID(option.name.c_str());

			out_stream.write((const char*)&name_hash, sizeof(name_hash));

			out.num_options++;
		}
	}

	alignStream(out_stream, __alignof(RuntimeOption));

	out.options_offset = (u32)out_stream.tellp() - offset;

	for(auto& option_group : options)
	{
		RuntimeOption x;
		x.group_clear_mask = option_group.clear_mask;

		for(auto& option : option_group.options)
		{
			if(option.bits != 0 && (options_mask & option.bits) == 0)
				continue;

			x.bits = option.bits;

			out_stream.write((const char*)&x, sizeof(x));
		}
	}

	//Write parameters vectors

	alignStream(out_stream, __alignof(u32));

	std::vector<u32> cbuffers_offsets;

	for(auto& cbuffers : permutations.cbuffers_vector)
	{
		cbuffers_offsets.push_back((u32)out_stream.tellp());
		out_stream.write((const char*)&cbuffers.names.at(0), sizeof(u32)*cbuffers.names.size());
		out_stream.write((const char*)&cbuffers.sizes.at(0), sizeof(u32)*cbuffers.sizes.size());
	}

	std::vector<u32> srvs_offsets;

	for(auto& srvs : permutations.srvs_vector)
	{
		srvs_offsets.push_back((u32)out_stream.tellp());
		out_stream.write((const char*)&srvs.names.at(0), sizeof(u32)*srvs.names.size());
	}

	std::vector<u32> uavs_offsets;

	for(auto& uavs : permutations.uavs_vector)
	{
		uavs_offsets.push_back((u32)out_stream.tellp());
		out_stream.write((const char*)&uavs.names.at(0), sizeof(u32)*uavs.names.size());
	}

	std::vector<u32> constants_offsets;

	for(auto& constants : permutations.constants_vector)
	{
		constants_offsets.push_back((u32)out_stream.tellp());
		out_stream.write((const char*)&constants.names.at(0), sizeof(u32)*constants.names.size());
		out_stream.write((const char*)&constants.offsets.at(0), sizeof(u32)*constants.offsets.size());
	}

	//Write permutations
	out_stream.seekp(offset + out.permutations_offset);

	for(auto& permutation : permutations.permutations)
	{
		u32 current_permutation_offset = (u32)out_stream.tellp();
		RuntimeParameterGroupPermutation perm;

		if(permutation.second.cbuffers_index != UINT32_MAX)
		{
			perm.cbuffers_offset = cbuffers_offsets[permutation.second.cbuffers_index] - current_permutation_offset;
			perm.num_cbuffers    = permutations.cbuffers_vector[permutation.second.cbuffers_index].names.size();
		}
		else
		{
			perm.cbuffers_offset = UINT32_MAX;
			perm.num_cbuffers    = 0;
		}

		if(permutation.second.srvs_index != UINT32_MAX)
		{
			perm.srvs_names_offset = srvs_offsets[permutation.second.srvs_index] - current_permutation_offset;
			perm.num_srvs          = permutations.srvs_vector[permutation.second.srvs_index].names.size();
		}
		else
		{
			perm.srvs_names_offset = UINT32_MAX;
			perm.num_srvs          = 0;
		}

		if(permutation.second.uavs_index != UINT32_MAX)
		{
			perm.uavs_names_offset = uavs_offsets[permutation.second.uavs_index] - current_permutation_offset;
			perm.num_uavs          = permutations.uavs_vector[permutation.second.uavs_index].names.size();
		}
		else
		{
			perm.uavs_names_offset = UINT32_MAX;
			perm.num_uavs          = 0;
		}

		if(permutation.second.constants_index != UINT32_MAX)
		{
			perm.constants_offset = constants_offsets[permutation.second.constants_index] - current_permutation_offset;
			perm.num_constants    = permutations.constants_vector[permutation.second.constants_index].names.size();
		}
		else
		{
			perm.constants_offset = UINT32_MAX;
			perm.num_constants    = 0;
		}

		out_stream.write((const char*)&perm, sizeof(perm));
	}

	out_stream.seekp(0, std::ios_base::end);
}