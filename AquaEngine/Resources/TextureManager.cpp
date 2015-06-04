#include "TextureManager.h"

#include "..\Renderer\RenderDevice\RenderDeviceDescs.h"

#include "..\Core\Allocators\Allocator.h"
#include "..\Utilities\Debug.h"

#include "DDS.h"

#include <algorithm>

//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Return the BPP for a particular format
//--------------------------------------------------------------------------------------
static size_t BitsPerPixel(_In_ DXGI_FORMAT fmt)
{
	switch(fmt)
	{
	case DXGI_FORMAT_R32G32B32A32_TYPELESS:
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32A32_SINT:
		return 128;

	case DXGI_FORMAT_R32G32B32_TYPELESS:
	case DXGI_FORMAT_R32G32B32_FLOAT:
	case DXGI_FORMAT_R32G32B32_UINT:
	case DXGI_FORMAT_R32G32B32_SINT:
		return 96;

	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R16G16B16A16_UNORM:
	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R16G16B16A16_SNORM:
	case DXGI_FORMAT_R16G16B16A16_SINT:
	case DXGI_FORMAT_R32G32_TYPELESS:
	case DXGI_FORMAT_R32G32_FLOAT:
	case DXGI_FORMAT_R32G32_UINT:
	case DXGI_FORMAT_R32G32_SINT:
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
	case DXGI_FORMAT_Y416:
	case DXGI_FORMAT_Y210:
	case DXGI_FORMAT_Y216:
		return 64;

	case DXGI_FORMAT_R10G10B10A2_TYPELESS:
	case DXGI_FORMAT_R10G10B10A2_UNORM:
	case DXGI_FORMAT_R10G10B10A2_UINT:
	case DXGI_FORMAT_R11G11B10_FLOAT:
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R8G8B8A8_SNORM:
	case DXGI_FORMAT_R8G8B8A8_SINT:
	case DXGI_FORMAT_R16G16_TYPELESS:
	case DXGI_FORMAT_R16G16_FLOAT:
	case DXGI_FORMAT_R16G16_UNORM:
	case DXGI_FORMAT_R16G16_UINT:
	case DXGI_FORMAT_R16G16_SNORM:
	case DXGI_FORMAT_R16G16_SINT:
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_SINT:
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
	case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
	case DXGI_FORMAT_R8G8_B8G8_UNORM:
	case DXGI_FORMAT_G8R8_G8B8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM:
	case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8X8_TYPELESS:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
	case DXGI_FORMAT_AYUV:
	case DXGI_FORMAT_Y410:
	case DXGI_FORMAT_YUY2:
		return 32;

	case DXGI_FORMAT_P010:
	case DXGI_FORMAT_P016:
		return 24;

	case DXGI_FORMAT_R8G8_TYPELESS:
	case DXGI_FORMAT_R8G8_UNORM:
	case DXGI_FORMAT_R8G8_UINT:
	case DXGI_FORMAT_R8G8_SNORM:
	case DXGI_FORMAT_R8G8_SINT:
	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_D16_UNORM:
	case DXGI_FORMAT_R16_UNORM:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R16_SNORM:
	case DXGI_FORMAT_R16_SINT:
	case DXGI_FORMAT_B5G6R5_UNORM:
	case DXGI_FORMAT_B5G5R5A1_UNORM:
	case DXGI_FORMAT_A8P8:
	case DXGI_FORMAT_B4G4R4A4_UNORM:
		return 16;

	case DXGI_FORMAT_NV12:
	case DXGI_FORMAT_420_OPAQUE:
	case DXGI_FORMAT_NV11:
		return 12;

	case DXGI_FORMAT_R8_TYPELESS:
	case DXGI_FORMAT_R8_UNORM:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_R8_SNORM:
	case DXGI_FORMAT_R8_SINT:
	case DXGI_FORMAT_A8_UNORM:
	case DXGI_FORMAT_AI44:
	case DXGI_FORMAT_IA44:
	case DXGI_FORMAT_P8:
		return 8;

	case DXGI_FORMAT_R1_UNORM:
		return 1;

	case DXGI_FORMAT_BC1_TYPELESS:
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
	case DXGI_FORMAT_BC4_TYPELESS:
	case DXGI_FORMAT_BC4_UNORM:
	case DXGI_FORMAT_BC4_SNORM:
		return 4;

	case DXGI_FORMAT_BC2_TYPELESS:
	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
	case DXGI_FORMAT_BC3_TYPELESS:
	case DXGI_FORMAT_BC3_UNORM:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
	case DXGI_FORMAT_BC5_TYPELESS:
	case DXGI_FORMAT_BC5_UNORM:
	case DXGI_FORMAT_BC5_SNORM:
	case DXGI_FORMAT_BC6H_TYPELESS:
	case DXGI_FORMAT_BC6H_UF16:
	case DXGI_FORMAT_BC6H_SF16:
	case DXGI_FORMAT_BC7_TYPELESS:
	case DXGI_FORMAT_BC7_UNORM:
	case DXGI_FORMAT_BC7_UNORM_SRGB:
		return 8;

	default:
		return 0;
	}
}


//--------------------------------------------------------------------------------------
// Get surface information for a particular format
//--------------------------------------------------------------------------------------
static void GetSurfaceInfo(_In_ size_t width,
	_In_ size_t height,
	_In_ DXGI_FORMAT fmt,
	_Out_opt_ size_t* outNumBytes,
	_Out_opt_ size_t* outRowBytes,
	_Out_opt_ size_t* outNumRows)
{
	size_t numBytes = 0;
	size_t rowBytes = 0;
	size_t numRows = 0;

	bool bc = false;
	bool packed = false;
	bool planar = false;
	size_t bpe = 0;
	switch(fmt)
	{
	case DXGI_FORMAT_BC1_TYPELESS:
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
	case DXGI_FORMAT_BC4_TYPELESS:
	case DXGI_FORMAT_BC4_UNORM:
	case DXGI_FORMAT_BC4_SNORM:
		bc = true;
		bpe = 8;
		break;

	case DXGI_FORMAT_BC2_TYPELESS:
	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
	case DXGI_FORMAT_BC3_TYPELESS:
	case DXGI_FORMAT_BC3_UNORM:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
	case DXGI_FORMAT_BC5_TYPELESS:
	case DXGI_FORMAT_BC5_UNORM:
	case DXGI_FORMAT_BC5_SNORM:
	case DXGI_FORMAT_BC6H_TYPELESS:
	case DXGI_FORMAT_BC6H_UF16:
	case DXGI_FORMAT_BC6H_SF16:
	case DXGI_FORMAT_BC7_TYPELESS:
	case DXGI_FORMAT_BC7_UNORM:
	case DXGI_FORMAT_BC7_UNORM_SRGB:
		bc = true;
		bpe = 16;
		break;

	case DXGI_FORMAT_R8G8_B8G8_UNORM:
	case DXGI_FORMAT_G8R8_G8B8_UNORM:
	case DXGI_FORMAT_YUY2:
		packed = true;
		bpe = 4;
		break;

	case DXGI_FORMAT_Y210:
	case DXGI_FORMAT_Y216:
		packed = true;
		bpe = 8;
		break;

	case DXGI_FORMAT_NV12:
	case DXGI_FORMAT_420_OPAQUE:
		planar = true;
		bpe = 2;
		break;

	case DXGI_FORMAT_P010:
	case DXGI_FORMAT_P016:
		planar = true;
		bpe = 4;
		break;
	}

	if(bc)
	{
		size_t numBlocksWide = 0;
		if(width > 0)
		{
			numBlocksWide = std::max<size_t>(1, (width + 3) / 4);
		}
		size_t numBlocksHigh = 0;
		if(height > 0)
		{
			numBlocksHigh = std::max<size_t>(1, (height + 3) / 4);
		}
		rowBytes = numBlocksWide * bpe;
		numRows = numBlocksHigh;
		numBytes = rowBytes * numBlocksHigh;
	}
	else if(packed)
	{
		rowBytes = ((width + 1) >> 1) * bpe;
		numRows = height;
		numBytes = rowBytes * height;
	}
	else if(fmt == DXGI_FORMAT_NV11)
	{
		rowBytes = ((width + 3) >> 2) * 4;
		numRows = height * 2; // Direct3D makes this simplifying assumption, although it is larger than the 4:1:1 data
		numBytes = rowBytes * numRows;
	}
	else if(planar)
	{
		rowBytes = ((width + 1) >> 1) * bpe;
		numBytes = (rowBytes * height) + ((rowBytes * height + 1) >> 1);
		numRows = height + ((height + 1) >> 1);
	}
	else
	{
		size_t bpp = BitsPerPixel(fmt);
		rowBytes = (width * bpp + 7) / 8; // round up to nearest byte
		numRows = height;
		numBytes = rowBytes * height;
	}

	if(outNumBytes)
	{
		*outNumBytes = numBytes;
	}
	if(outRowBytes)
	{
		*outRowBytes = rowBytes;
	}
	if(outNumRows)
	{
		*outNumRows = numRows;
	}
}


//--------------------------------------------------------------------------------------
#define ISBITMASK( r,g,b,a ) ( ddpf.RBitMask == r && ddpf.GBitMask == g && ddpf.BBitMask == b && ddpf.ABitMask == a )

static DXGI_FORMAT GetDXGIFormat(const DirectX::DDS_PIXELFORMAT& ddpf)
{
	if(ddpf.flags & DDS_RGB)
	{
		// Note that sRGB formats are written using the "DX10" extended header

		switch(ddpf.RGBBitCount)
		{
		case 32:
			if(ISBITMASK(0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000))
			{
				return DXGI_FORMAT_R8G8B8A8_UNORM;
			}

			if(ISBITMASK(0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000))
			{
				return DXGI_FORMAT_B8G8R8A8_UNORM;
			}

			if(ISBITMASK(0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000))
			{
				return DXGI_FORMAT_B8G8R8X8_UNORM;
			}

			// No DXGI format maps to ISBITMASK(0x000000ff,0x0000ff00,0x00ff0000,0x00000000) aka D3DFMT_X8B8G8R8

			// Note that many common DDS reader/writers (including D3DX) swap the
			// the RED/BLUE masks for 10:10:10:2 formats. We assumme
			// below that the 'backwards' header mask is being used since it is most
			// likely written by D3DX. The more robust solution is to use the 'DX10'
			// header extension and specify the DXGI_FORMAT_R10G10B10A2_UNORM format directly

			// For 'correct' writers, this should be 0x000003ff,0x000ffc00,0x3ff00000 for RGB data
			if(ISBITMASK(0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000))
			{
				return DXGI_FORMAT_R10G10B10A2_UNORM;
			}

			// No DXGI format maps to ISBITMASK(0x000003ff,0x000ffc00,0x3ff00000,0xc0000000) aka D3DFMT_A2R10G10B10

			if(ISBITMASK(0x0000ffff, 0xffff0000, 0x00000000, 0x00000000))
			{
				return DXGI_FORMAT_R16G16_UNORM;
			}

			if(ISBITMASK(0xffffffff, 0x00000000, 0x00000000, 0x00000000))
			{
				// Only 32-bit color channel format in D3D9 was R32F
				return DXGI_FORMAT_R32_FLOAT; // D3DX writes this out as a FourCC of 114
			}
			break;

		case 24:
			// No 24bpp DXGI formats aka D3DFMT_R8G8B8
			break;

		case 16:
			if(ISBITMASK(0x7c00, 0x03e0, 0x001f, 0x8000))
			{
				return DXGI_FORMAT_B5G5R5A1_UNORM;
			}
			if(ISBITMASK(0xf800, 0x07e0, 0x001f, 0x0000))
			{
				return DXGI_FORMAT_B5G6R5_UNORM;
			}

			// No DXGI format maps to ISBITMASK(0x7c00,0x03e0,0x001f,0x0000) aka D3DFMT_X1R5G5B5

			if(ISBITMASK(0x0f00, 0x00f0, 0x000f, 0xf000))
			{
				return DXGI_FORMAT_B4G4R4A4_UNORM;
			}

			// No DXGI format maps to ISBITMASK(0x0f00,0x00f0,0x000f,0x0000) aka D3DFMT_X4R4G4B4

			// No 3:3:2, 3:3:2:8, or paletted DXGI formats aka D3DFMT_A8R3G3B2, D3DFMT_R3G3B2, D3DFMT_P8, D3DFMT_A8P8, etc.
			break;
		}
	}
	else if(ddpf.flags & DDS_LUMINANCE)
	{
		if(8 == ddpf.RGBBitCount)
		{
			if(ISBITMASK(0x000000ff, 0x00000000, 0x00000000, 0x00000000))
			{
				return DXGI_FORMAT_R8_UNORM; // D3DX10/11 writes this out as DX10 extension
			}

			// No DXGI format maps to ISBITMASK(0x0f,0x00,0x00,0xf0) aka D3DFMT_A4L4
		}

		if(16 == ddpf.RGBBitCount)
		{
			if(ISBITMASK(0x0000ffff, 0x00000000, 0x00000000, 0x00000000))
			{
				return DXGI_FORMAT_R16_UNORM; // D3DX10/11 writes this out as DX10 extension
			}
			if(ISBITMASK(0x000000ff, 0x00000000, 0x00000000, 0x0000ff00))
			{
				return DXGI_FORMAT_R8G8_UNORM; // D3DX10/11 writes this out as DX10 extension
			}
		}
	}
	else if(ddpf.flags & DDS_ALPHA)
	{
		if(8 == ddpf.RGBBitCount)
		{
			return DXGI_FORMAT_A8_UNORM;
		}
	}
	else if(ddpf.flags & DDS_FOURCC)
	{
		if(MAKEFOURCC('D', 'X', 'T', '1') == ddpf.fourCC)
		{
			return DXGI_FORMAT_BC1_UNORM;
		}
		if(MAKEFOURCC('D', 'X', 'T', '3') == ddpf.fourCC)
		{
			return DXGI_FORMAT_BC2_UNORM;
		}
		if(MAKEFOURCC('D', 'X', 'T', '5') == ddpf.fourCC)
		{
			return DXGI_FORMAT_BC3_UNORM;
		}

		// While pre-mulitplied alpha isn't directly supported by the DXGI formats,
		// they are basically the same as these BC formats so they can be mapped
		if(MAKEFOURCC('D', 'X', 'T', '2') == ddpf.fourCC)
		{
			return DXGI_FORMAT_BC2_UNORM;
		}
		if(MAKEFOURCC('D', 'X', 'T', '4') == ddpf.fourCC)
		{
			return DXGI_FORMAT_BC3_UNORM;
		}

		if(MAKEFOURCC('A', 'T', 'I', '1') == ddpf.fourCC)
		{
			return DXGI_FORMAT_BC4_UNORM;
		}
		if(MAKEFOURCC('B', 'C', '4', 'U') == ddpf.fourCC)
		{
			return DXGI_FORMAT_BC4_UNORM;
		}
		if(MAKEFOURCC('B', 'C', '4', 'S') == ddpf.fourCC)
		{
			return DXGI_FORMAT_BC4_SNORM;
		}

		if(MAKEFOURCC('A', 'T', 'I', '2') == ddpf.fourCC)
		{
			return DXGI_FORMAT_BC5_UNORM;
		}
		if(MAKEFOURCC('B', 'C', '5', 'U') == ddpf.fourCC)
		{
			return DXGI_FORMAT_BC5_UNORM;
		}
		if(MAKEFOURCC('B', 'C', '5', 'S') == ddpf.fourCC)
		{
			return DXGI_FORMAT_BC5_SNORM;
		}

		// BC6H and BC7 are written using the "DX10" extended header

		if(MAKEFOURCC('R', 'G', 'B', 'G') == ddpf.fourCC)
		{
			return DXGI_FORMAT_R8G8_B8G8_UNORM;
		}
		if(MAKEFOURCC('G', 'R', 'G', 'B') == ddpf.fourCC)
		{
			return DXGI_FORMAT_G8R8_G8B8_UNORM;
		}

		if(MAKEFOURCC('Y', 'U', 'Y', '2') == ddpf.fourCC)
		{
			return DXGI_FORMAT_YUY2;
		}

		// Check for D3DFORMAT enums being set here
		switch(ddpf.fourCC)
		{
		case 36: // D3DFMT_A16B16G16R16
			return DXGI_FORMAT_R16G16B16A16_UNORM;

		case 110: // D3DFMT_Q16W16V16U16
			return DXGI_FORMAT_R16G16B16A16_SNORM;

		case 111: // D3DFMT_R16F
			return DXGI_FORMAT_R16_FLOAT;

		case 112: // D3DFMT_G16R16F
			return DXGI_FORMAT_R16G16_FLOAT;

		case 113: // D3DFMT_A16B16G16R16F
			return DXGI_FORMAT_R16G16B16A16_FLOAT;

		case 114: // D3DFMT_R32F
			return DXGI_FORMAT_R32_FLOAT;

		case 115: // D3DFMT_G32R32F
			return DXGI_FORMAT_R32G32_FLOAT;

		case 116: // D3DFMT_A32B32G32R32F
			return DXGI_FORMAT_R32G32B32A32_FLOAT;
		}
	}

	return DXGI_FORMAT_UNKNOWN;
}


//--------------------------------------------------------------------------------------
static DXGI_FORMAT MakeSRGB(_In_ DXGI_FORMAT format)
{
	switch(format)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	case DXGI_FORMAT_BC1_UNORM:
		return DXGI_FORMAT_BC1_UNORM_SRGB;

	case DXGI_FORMAT_BC2_UNORM:
		return DXGI_FORMAT_BC2_UNORM_SRGB;

	case DXGI_FORMAT_BC3_UNORM:
		return DXGI_FORMAT_BC3_UNORM_SRGB;

	case DXGI_FORMAT_B8G8R8A8_UNORM:
		return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

	case DXGI_FORMAT_B8G8R8X8_UNORM:
		return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;

	case DXGI_FORMAT_BC7_UNORM:
		return DXGI_FORMAT_BC7_UNORM_SRGB;

	default:
		return format;
	}
}

//--------------------------------------------------------------------------------------

static HRESULT fillInitData(size_t width, size_t height, size_t depth, size_t mip_count, size_t array_size,
	DXGI_FORMAT format, size_t bitSize, const uint8_t* bitData,
	aqua::SubTextureData* out_init_data)
{
	if(!bitData || !out_init_data)
	{
		return E_POINTER;
	}

	size_t NumBytes = 0;
	size_t RowBytes = 0;
	const uint8_t* pSrcBits = bitData;
	const uint8_t* pEndBits = bitData + bitSize;

	size_t index = 0;
	for(size_t j = 0; j < array_size; j++)
	{
		size_t w = width;
		size_t h = height;
		size_t d = depth;
		for(size_t i = 0; i < mip_count; i++)
		{
			GetSurfaceInfo(w, h, format, &NumBytes, &RowBytes, nullptr);

			ASSERT(index < mip_count * array_size);

			out_init_data[index].data = (const void*)pSrcBits;
			out_init_data[index].line_offset = static_cast<UINT>(RowBytes);
			out_init_data[index].depth_offset = static_cast<UINT>(NumBytes);
			++index;

			if(pSrcBits + (NumBytes*d) > pEndBits)
			{
				return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
			}

			pSrcBits += NumBytes * d;

			w = w >> 1;
			h = h >> 1;
			d = d >> 1;

			if(w == 0)
			{
				w = 1;
			}

			if(h == 0)
			{
				h = 1;
			}

			if(d == 0)
			{
				d = 1;
			}
		}
	}

	return (index > 0) ? S_OK : E_FAIL;
}

//--------------------------------------------------------------------------------------

enum class TextureDimension : aqua::u8
{
	TEXTURE1D,
	TEXTURE2D,
	TEXTURE3D
};

struct TextureDESCC
{
	TextureDimension      dimension;
	aqua::u32             width;
	aqua::u32             height;
	aqua::u32             depth;
	aqua::u32             num_mips;
	aqua::u32             array_size;
	aqua::RenderResourceFormat  format;
	bool                  is_cubemap;
	aqua::SubTextureData* initial_data;
};

static HRESULT getTextureDescFromDDS(const DirectX::DDS_HEADER* header, const uint8_t* bitData, size_t bitSize,
	aqua::Allocator& allocator, TextureDESCC& out)
{
	HRESULT hr = S_OK;

	out.width = header->width;
	out.height = header->height;
	out.depth = header->depth;
	out.array_size = 1;
	out.is_cubemap = false;
	out.format = aqua::RenderResourceFormat::UNKNOWN;

	DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

	out.num_mips = header->mipMapCount;

	if(out.num_mips == 0)
	{
		out.num_mips = 1;
	}

	if((header->ddspf.flags & DDS_FOURCC) && (MAKEFOURCC('D', 'X', '1', '0') == header->ddspf.fourCC))
	{
		auto d3d10ext = reinterpret_cast<const DirectX::DDS_HEADER_DXT10*>((const char*)header + sizeof(DirectX::DDS_HEADER));

		out.array_size = d3d10ext->arraySize;

		if(out.array_size == 0)
		{
			return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
		}

		switch(d3d10ext->dxgiFormat)
		{
		case DXGI_FORMAT_AI44:
		case DXGI_FORMAT_IA44:
		case DXGI_FORMAT_P8:
		case DXGI_FORMAT_A8P8:
			return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

		default:
			if(BitsPerPixel(d3d10ext->dxgiFormat) == 0)
			{
				return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
			}
		}

		format = d3d10ext->dxgiFormat;

		switch(d3d10ext->resourceDimension)
		{
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
			// D3DX writes 1D textures with a fixed Height of 1
			if((header->flags & DDS_HEIGHT) && out.height != 1)
			{
				return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
			}

			out.dimension = TextureDimension::TEXTURE1D;
			out.height = out.depth = 1;
			break;

		case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
			if(d3d10ext->miscFlag & D3D11_RESOURCE_MISC_TEXTURECUBE)
			{
				out.array_size *= 6;
				out.is_cubemap = true;
			}

			out.dimension = TextureDimension::TEXTURE2D;
			out.depth = 1;
			break;

		case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
			if(!(header->flags & DDS_HEADER_FLAGS_VOLUME))
			{
				return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
			}

			if(out.array_size > 1)
			{
				return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
			}

			out.dimension = TextureDimension::TEXTURE3D;

			break;

		default:
			return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
		}
	}
	else
	{
		format = GetDXGIFormat(header->ddspf);

		if(format == DXGI_FORMAT_UNKNOWN)
		{
			return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
		}

		if(header->flags & DDS_HEADER_FLAGS_VOLUME)
		{
			out.dimension = TextureDimension::TEXTURE3D;
		}
		else
		{
			if(header->caps2 & DDS_CUBEMAP)
			{
				// We require all six faces to be defined
				if((header->caps2 & DDS_CUBEMAP_ALLFACES) != DDS_CUBEMAP_ALLFACES)
				{
					return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
				}

				out.array_size = 6;
				out.is_cubemap = true;
			}

			out.depth = 1;
			out.dimension = TextureDimension::TEXTURE2D;

			// Note there's no way for a legacy Direct3D 9 DDS to express a '1D' texture
		}

		ASSERT(BitsPerPixel(format) != 0);
	}

	// Bound sizes (for security purposes we don't trust DDS file metadata larger than the D3D 11.x hardware requirements)
	if(out.num_mips > D3D11_REQ_MIP_LEVELS)
	{
		return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
	}

	switch(out.dimension)
	{
	case TextureDimension::TEXTURE1D:
		if((out.array_size > D3D11_REQ_TEXTURE1D_ARRAY_AXIS_DIMENSION) ||
			(out.width > D3D11_REQ_TEXTURE1D_U_DIMENSION))
		{
			return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
		}
		break;

	case TextureDimension::TEXTURE2D:
		if(out.is_cubemap)
		{
			// This is the right bound because we set arraySize to (NumCubes*6) above
			if((out.array_size > D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION) ||
				(out.width > D3D11_REQ_TEXTURECUBE_DIMENSION) ||
				(out.height > D3D11_REQ_TEXTURECUBE_DIMENSION))
			{
				return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
			}
		}
		else if((out.array_size > D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION) ||
			(out.width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION) ||
			(out.height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION))
		{
			return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
		}
		break;

	case TextureDimension::TEXTURE3D:
		if((out.array_size > 1) ||
			(out.width > D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION) ||
			(out.height > D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION) ||
			(out.depth > D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION))
		{
			return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
		}
		break;

	default:
		return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
	}

	//Get init data

	out.initial_data = aqua::allocator::allocateArray<aqua::SubTextureData>(allocator, out.num_mips * out.array_size);

	if(out.initial_data == nullptr)
	{
		return E_OUTOFMEMORY;
	}

	hr = fillInitData(out.width, out.height, out.depth, out.num_mips, out.array_size,
		format, bitSize, bitData, out.initial_data);

	out.format = (aqua::RenderResourceFormat)format;

	return hr;
}

//--------------------------------------------------------------------------------------

using namespace aqua;

TextureManager::TextureManager()
	: _allocator(nullptr), _temp_allocator(nullptr), _renderer(nullptr), _num_textures(0), _capacity(0)
{
}

bool TextureManager::init(Allocator& allocator, Allocator& temp_allocator, Renderer& renderer)
{
	_allocator = &allocator;
	_temp_allocator = &temp_allocator;
	_renderer = &renderer;

	setCapacity(16);

	return true;
}

bool TextureManager::shutdown()
{
	ASSERT(_num_textures == 0);

	if(_capacity > 0)
	{
		allocator::deallocateArray(*_allocator, _textures_names);
		allocator::deallocateArray(*_allocator, _textures);
		allocator::deallocateArray(*_allocator, _ref_count);
	}

	_capacity = 0;

	return true;
}

bool TextureManager::create(u32 name, const char* data, size_t data_size)
{
	// Validate DDS file in memory
	if(data_size < (sizeof(uint32_t) + sizeof(DirectX::DDS_HEADER)))
		return false;

	uint32_t dwMagicNumber = *(const uint32_t*)(data);

	if(dwMagicNumber != DirectX::DDS_MAGIC)
		return false;

	auto header = reinterpret_cast<const DirectX::DDS_HEADER*>(data + sizeof(uint32_t));

	// Verify header to validate DDS file
	if(header->size != sizeof(DirectX::DDS_HEADER) ||
		header->ddspf.size != sizeof(DirectX::DDS_PIXELFORMAT))
	{
		return false;
	}

	// Check for DX10 extension
	bool bDXT10Header = false;
	if((header->ddspf.flags & DDS_FOURCC) &&
		(MAKEFOURCC('D', 'X', '1', '0') == header->ddspf.fourCC))
	{
		// Must be long enough for both headers and magic value
		if(data_size < (sizeof(DirectX::DDS_HEADER) + sizeof(uint32_t) + sizeof(DirectX::DDS_HEADER_DXT10)))
		{
			return false;
		}

		bDXT10Header = true;
	}

	ptrdiff_t offset = sizeof(uint32_t) + sizeof(DirectX::DDS_HEADER) + (bDXT10Header ? sizeof(DirectX::DDS_HEADER_DXT10) : 0);

	TextureDESCC desc;

	getTextureDescFromDDS(header, (const uint8_t*)data + offset, data_size - offset, *_temp_allocator, desc);

	if(desc.dimension == TextureDimension::TEXTURE1D)
	{
		Texture1DDesc tex_desc;
		tex_desc.width         = desc.width;
		tex_desc.mip_levels    = desc.num_mips;
		tex_desc.array_size    = desc.array_size;
		tex_desc.format        = desc.format;
		tex_desc.update_mode   = UpdateMode::NONE;
		tex_desc.generate_mips = false;
	}
	else if(desc.dimension == TextureDimension::TEXTURE2D)
	{
		if(desc.is_cubemap)
		{

		}

		Texture2DDesc tex_desc;
		tex_desc.width          = desc.width;
		tex_desc.height         = desc.height;
		tex_desc.mip_levels     = desc.num_mips;
		tex_desc.array_size     = desc.array_size;
		tex_desc.format         = desc.format;
		tex_desc.sample_count   = 1;
		tex_desc.sample_quality = 0;
		tex_desc.update_mode    = UpdateMode::NONE;
		tex_desc.generate_mips  = false;

		TextureViewDesc view_desc;
		view_desc.most_detailed_mip = 0;
		view_desc.mip_levels        = -1;
		view_desc.format            = desc.format;

		ShaderResourceH sr;

		if(!_renderer->getRenderDevice()->createTexture2D(tex_desc, desc.initial_data,
			1, &view_desc, 0, nullptr, 0, nullptr, 0, nullptr,
			&sr, nullptr, nullptr, nullptr))
		{
			return false;
		}

		_textures_names[_num_textures] = name;
		_textures[_num_textures] = sr;

		_num_textures++;
	}
	else if(desc.dimension == TextureDimension::TEXTURE3D)
	{

	}
	else
		ASSERT("Invalid dimension" && false);


	return true;
}

bool TextureManager::destroy(u32 name)
{
	for(u32 i = 0; i < _num_textures; i++)
	{
		if(_textures_names[i] == name)
		{
			RenderDevice::release(_textures[i]);

			_num_textures--;

			//no need to check if empty (it might cause an unecessary copy but no problem)
			_textures_names[i] = _textures_names[_num_textures];
			_textures[i] = _textures[_num_textures];

			return true;
		}
	}

	return false;
}

ShaderResourceH TextureManager::getTexture(u32 name)
{
	for(u32 i = 0; i < _num_textures; i++)
	{
		if(_textures_names[i] == name)
			return _textures[i];
	}

	return nullptr;
}

bool TextureManager::setCapacity(u32 new_capacity)
{
	u32* new_names = allocator::allocateArray<u32>(*_allocator, new_capacity);

	if(new_names == nullptr)
		return false;

	ShaderResourceH* new_textures = allocator::allocateArray<ShaderResourceH>(*_allocator, new_capacity);

	if(new_textures == nullptr)
	{
		allocator::deallocateArray(*_allocator, new_names);

		return false;
	}

	u32* new_ref_count = allocator::allocateArray<u32>(*_allocator, new_capacity);

	if(new_ref_count == nullptr)
	{
		allocator::deallocateArray(*_allocator, new_textures);
		allocator::deallocateArray(*_allocator, new_names);

		return false;
	}

	memcpy(new_names, _textures_names, sizeof(*_textures_names)*_num_textures);
	memcpy(new_textures, _textures, sizeof(*_textures)*_num_textures);
	memcpy(new_ref_count, _ref_count, sizeof(*_ref_count)*_num_textures);

	if(_capacity > 0)
	{
		allocator::deallocateArray(*_allocator, _ref_count);
		allocator::deallocateArray(*_allocator, _textures);
		allocator::deallocateArray(*_allocator, _textures_names);
	}

	_capacity = new_capacity;

	_textures_names = new_names;
	_textures       = new_textures;
	_ref_count      = new_ref_count;

	return true;
}