#pragma once

#include "sjson.h"

#define INITGUID

#include <d3dcompiler.h>

#include <functional>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <iostream>

//------------------------------------------------------------------------
// TODO:
// - Improve options mask calculation
//		(eg: a function doesnt use any variable from a CBuffer but
//			 the CBuffer desc is still included in the function code
//			 so OPTIONS inside the CBufferDesc are included 
//			 in the function options mask but have no effect)
//------------------------------------------------------------------------

static const uint64_t COMMON_MAGIC_NUMBER  = 0xC072C072C072C072;
static const uint64_t RENDER_MAGIC_NUMBER  = COMMON_MAGIC_NUMBER + 1;
static const uint64_t COMPUTE_MAGIC_NUMBER = RENDER_MAGIC_NUMBER + 1;

using u8          = uint8_t;
using u32         = uint32_t;
using u64         = uint64_t;
using Permutation = uint64_t;

enum class ShaderType : u8
{
	VERTEX_SHADER   = 1 << 0,
	HULL_SHADER     = 1 << 1,
	DOMAIN_SHADER   = 1 << 2,
	GEOMETRY_SHADER = 1 << 3,
	PIXEL_SHADER    = 1 << 4,
	COMPUTE_SHADER  = 1 << 5
};

enum class Type : u8
{
	FLOAT,
	FLOAT2,
	FLOAT3,
	FLOAT4,
	UINT,
	UINT2,
	UINT3,
	UINT4,
	INT,
	INT2,
	INT3,
	INT4,
	FLOAT3X3,
	FLOAT4X4
};

enum class ResourceType : u8
{
	APPEND_STRUCTURED_BUFFER,
	BUFFER,
	BYTE_ADDRESS_BUFFER,
	CONSUME_STRUCTURED_BUFFER,
	RW_BUFFER,
	RW_BYTE_ADDRESS_BUFFER,
	RW_STRUCTURED_BUFFER,
	RW_TEXTURE1D,
	RW_TEXTURE1D_ARRAY,
	RW_TEXTURE2D,
	RW_TEXTURE2D_ARRAY,
	RW_TEXTURE3D,
	STRUCTURED_BUFFER,
	TEXTURE1D,
	TEXTURE1D_ARRAY,
	TEXTURE2D,
	TEXTURE2D_ARRAY,
	TEXTURE2DMS,
	TEXTURE2DMS_ARRAY,
	TEXTURE3D
};

struct TypeDesc
{
	Type type;
	u8   size;
};

struct Snippet
{
	std::string              code;
	std::vector<std::string> includes;
	//Permutation              options_mask;
};

class Condition
{
public:
	virtual bool evaluate(Permutation permutation) = 0;
};

struct Option
{
	std::string				   name;
	Permutation				   bits;
	std::shared_ptr<Condition> condition;
	//u8					     parent; //parent option index

	//static const u8 NO_PARENT = 255;
};

struct OptionsGroup
{
	std::vector<Option> options;
	Permutation         clear_mask;
};

struct Constant
{
	std::string                name;
	std::string                display_name;
	TypeDesc                   type;
	std::shared_ptr<Condition> condition;
	u32                        array_size;
};

struct CBuffer
{
	std::string           name;
	std::vector<Constant> constants;
	//Permutation           options_mask;
	u8                    slot;
};
/*
struct CBufferPermutation
{
	std::vector<int> constants_offset;
	u32              num_constants;
	u32              size;
};
*/
struct Resource
{
	std::string  name;
	std::string  display_name;
	ResourceType type;
	//std::string element_type;
	u8           slot;
};

struct RuntimeCBuffers
{
	std::vector<u32> names;
	std::vector<u32> sizes;
};

bool operator==(const RuntimeCBuffers& x, const RuntimeCBuffers& y);

struct RuntimeConstants
{
	std::vector<u32> names;
	std::vector<u32> offsets;
};

bool operator==(const RuntimeConstants& x, const RuntimeConstants& y);

struct RuntimeResources
{
	std::vector<u32> names;
};

bool operator==(const RuntimeResources& x, const RuntimeResources& y);

struct ParameterGroupPermutation
{
	std::vector<u32> cmd_groups_indices;
	u32				 cbuffers_index;
	u32				 constants_index;
	u32				 srvs_index;
	u32				 uavs_index;
};

struct ParameterGroupDesc
{
	std::vector<CBuffer>  cbuffers;
	std::vector<Resource> srvs;
	std::vector<Resource> uavs;
	std::string           code;
	Permutation           options_mask;
};

struct ParameterGroupPermutations
{
	std::map<Permutation, ParameterGroupPermutation> permutations;
	std::vector<RuntimeCBuffers>					 cbuffers_vector;
	std::vector<RuntimeConstants>                    constants_vector;
	std::vector<RuntimeResources>                    srvs_vector;
	std::vector<RuntimeResources>                    uavs_vector;
};

class ParameterGroupReflection
{
public:
	ParameterGroupReflection(const ParameterGroupDesc& group_desc, u8 num_passes);

	ParameterGroupPermutations getPermutations(std::vector<u8>& command_groups) const;

	void reflectPermutation(const std::vector<Permutation>& permutation, u8 pass_num, 
							ShaderType shader_type, ID3D11ShaderReflection* shader_reflection);

private:

	struct ParameterGroupPermutationReflection
	{
		//Example: cbuffers[i][j] = bitfield of which shader stages use cbuffer 'i' in pass 'j'
		std::vector<std::vector<u8>> cbuffers;
		std::vector<std::vector<u8>> srvs;
		std::vector<std::vector<u8>> uavs;
	};

	const ParameterGroupDesc&								  _group_desc;
	u8                                                        _num_passes;
	std::map<Permutation,ParameterGroupPermutationReflection> _permutations;
};

//-------------------------------------------------------------------------------------

std::shared_ptr<Condition> parseCondition(const char*& condition, const std::vector<OptionsGroup>& options);

//-------------------------------------------------------------------------------------

enum class BlendOption : u8
{
	ZERO,
	ONE,
	SRC_COLOR,
	INV_SRC_COLOR,
	SRC_ALPHA,
	INV_SRC_ALPHA,
	DEST_ALPHA,
	INV_DEST_ALPHA,
	DEST_COLOR,
	INV_DEST_COLOR,
	SRC_ALPHA_SAT,
	BLEND_FACTOR,
	INV_BLEND_FACTOR,
	SRC1_COLOR,
	INV_SRC1_COLOR,
	SRC1_ALPHA,
	INV_SRC1_ALPHA,
};

enum class BlendOperation : u8
{
	ADD,
	SUBTRACT,
	REV_SUBTRACT,
	MIN,
	MAX 

};

struct BlendStateDesc
{
	BlendOption    src_blend;
	BlendOption    dest_blend;
	BlendOperation blend_op;
	BlendOption    src_blend_alpha;
	BlendOption    dest_blend_alpha;
	BlendOperation blend_op_alpha;
	u8             render_target_write_mask;
};

struct CommonDesc
{
	ParameterGroupDesc                        frame_params;
	ParameterGroupDesc                        view_params;
	std::map<std::string, ParameterGroupDesc> passes;
	std::map<std::string, BlendStateDesc>     blend_states;
};

struct Function
{
	std::string name;
	std::string code;
	Permutation options_mask;
	ShaderType  type;
	u8          pass;
};

//-------------------------------------------------------------------------------------

struct InputLayoutElement
{
	std::string                 semantic_name;
	UINT                        semantic_index;
	UINT                        register_num;
	//D3D_NAME                    system_value_type;
	D3D_REGISTER_COMPONENT_TYPE component_type;
	BYTE                        mask;
	BYTE                        read_write_mask;
	UINT                        stream;
	D3D_MIN_PRECISION           min_precision;
};

bool operator==(const InputLayoutElement& x, const InputLayoutElement& y);

struct InputLayout
{
	std::vector<InputLayoutElement> elements;
	Permutation                     mesh_permutation;
	u64                             bytecode_offset;
};

//-------------------------------------------------------------------------------------

extern const std::vector<std::string> srvs_types;
extern const std::vector<std::string> uav_types;
extern const std::map<std::string, TypeDesc> types_descs;
extern const std::map<std::string, ResourceType> resource_types;

//-------------------------------------------------------------------------------------

inline void alignStream(std::ostream& stream, u8 alignment)
{
	u32 pos = (u32)stream.tellp();

	u32 diff = ((pos + (alignment - 1)) & ~(alignment - 1)) - pos;

	char x = 0x55;

	std::string k(diff, x);

	stream.write(k.c_str(), diff);
}

inline void printError(const std::string& filename, u32 line_num, const std::string& message)
{
	std::cout << "Error: " << filename << "(" << line_num << "): " << message << "\n";
}

Permutation calculateSnippetOptionsMask(const std::string& snippet, const std::vector<OptionsGroup>& options);

bool getOptions(const std::string& filename, const Object& obj, u8& group_start, std::vector<OptionsGroup>& out);

bool getSnippets(const std::string& filename, const Object& snippets, std::map<std::string, Snippet>& out);

bool parseParameterGroupDesc(const std::string& filename, const Object& obj, u8 start_cbuffer, u8 start_srv, 
							 u8 start_uav, const std::vector<OptionsGroup>* options, ParameterGroupDesc& out);

bool buildFullCode(const Snippet& snippet, const std::map<std::string, Snippet>& snippets,
				   std::string& out_code);

bool compileFunctionPermutations(const std::string& shader_filename, const Function& func, bool debug,
								 const std::map<Permutation,std::vector<Permutation>>& permutations, 
								 const std::vector<OptionsGroup>& options, u64 mesh_options_mask,
								 std::vector<ParameterGroupReflection*>& param_groups, 
								 std::vector<InputLayout>* input_layouts, 
								 std::vector<u32>* input_layout_indices, std::ostream& out_stream);

struct RuntimeParameterGroupPermutation
{
	u32 cbuffers_offset;	//u32 array (names) + u32 array (sizes)
	u32 srvs_names_offset;	//u32 array
	u32 uavs_names_offset;	//u32 array
	u32 constants_offset;	//u32 array (names) + u32 array (offsets)

	u8  num_cbuffers;
	u8  num_srvs;
	u8  num_uavs;
	u8  num_constants;
};

struct RuntimeParameterGroup
{
	Permutation options_mask;

	u32         num_permutations;
	u32         permutations_nums_offset;	//Permutation array
	u32         permutations_offset;		//ParameterGroupPermutation array

	u32         options_names_offset;		//u32 array
	u32		    options_offset;				//Option array
	u8          num_options;
};

void writeParameterGroup(u32 offset, Permutation options_mask, const std::vector<OptionsGroup>& options,
						 const ParameterGroupPermutations& permutations, 
						 RuntimeParameterGroup& out, std::ostream& out_stream);