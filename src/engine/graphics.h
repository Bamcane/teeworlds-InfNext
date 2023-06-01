/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_GRAPHICS_H
#define ENGINE_GRAPHICS_H

#include "kernel.h"

#include <base/color.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#define GRAPHICS_TYPE_UNSIGNED_BYTE 0x1401
#define GRAPHICS_TYPE_UNSIGNED_SHORT 0x1403
#define GRAPHICS_TYPE_INT 0x1404
#define GRAPHICS_TYPE_UNSIGNED_INT 0x1405
#define GRAPHICS_TYPE_FLOAT 0x1406

struct SBufferContainerInfo
{
	int m_Stride;
	int m_VertBufferBindingIndex;

	// the attributes of the container
	struct SAttribute
	{
		int m_DataTypeCount;
		unsigned int m_Type;
		bool m_Normalized;
		void *m_pOffset;

		//0: float, 1:integer
		unsigned int m_FuncType;
	};
	std::vector<SAttribute> m_vAttributes;
};

struct SQuadRenderInfo
{
	ColorRGBA m_Color;
	vec2 m_Offsets;
	float m_Rotation;
	// allows easier upload for uniform buffers because of the alignment requirements
	float m_Padding;
};

struct SGraphicTile
{
	vec2 m_TopLeft;
	vec2 m_TopRight;
	vec2 m_BottomRight;
	vec2 m_BottomLeft;
};

struct SGraphicTileTexureCoords
{
	vec3 m_TexCoordTopLeft;
	vec3 m_TexCoordTopRight;
	vec3 m_TexCoordBottomRight;
	vec3 m_TexCoordBottomLeft;
};

class CImageInfo
{
public:
	enum
	{
		FORMAT_AUTO = -1,
		FORMAT_RGB = 0,
		FORMAT_RGBA = 1,
		FORMAT_SINGLE_COMPONENT = 2,
	};

	/* Variable: width
		Contains the width of the image */
	int m_Width;

	/* Variable: height
		Contains the height of the image */
	int m_Height;

	/* Variable: format
		Contains the format of the image. See <Image Formats> for more information. */
	int m_Format;

	/* Variable: data
		Pointer to the image data. */
	void *m_pData;
};

/*
	Structure: CVideoMode
*/
class CVideoMode
{
public:
	int m_CanvasWidth, m_CanvasHeight;
	int m_WindowWidth, m_WindowHeight;
	int m_RefreshRate;
	int m_Red, m_Green, m_Blue;
	uint32_t m_Format;
};

typedef vec2 GL_SPoint;
typedef vec2 GL_STexCoord;

struct GL_STexCoord3D
{
	GL_STexCoord3D &operator=(const GL_STexCoord &TexCoord)
	{
		u = TexCoord.u;
		v = TexCoord.v;
		return *this;
	}

	GL_STexCoord3D &operator=(const vec3 &TexCoord)
	{
		u = TexCoord.u;
		v = TexCoord.v;
		w = TexCoord.w;
		return *this;
	}

	float u, v, w;
};

typedef ColorRGBA GL_SColorf;
//use normalized color values
typedef vector4_base<unsigned char> GL_SColor;

struct GL_SVertex
{
	GL_SPoint m_Pos;
	GL_STexCoord m_Tex;
	GL_SColor m_Color;
};

struct GL_SVertexTex3D
{
	GL_SPoint m_Pos;
	GL_SColorf m_Color;
	GL_STexCoord3D m_Tex;
};

struct GL_SVertexTex3DStream
{
	GL_SPoint m_Pos;
	GL_SColor m_Color;
	GL_STexCoord3D m_Tex;
};

static constexpr size_t gs_GraphicsMaxQuadsRenderCount = 256;
static constexpr size_t gs_GraphicsMaxParticlesRenderCount = 512;

enum EGraphicsDriverAgeType
{
	GRAPHICS_DRIVER_AGE_TYPE_LEGACY = 0,
	GRAPHICS_DRIVER_AGE_TYPE_DEFAULT,
	GRAPHICS_DRIVER_AGE_TYPE_MODERN,

	GRAPHICS_DRIVER_AGE_TYPE_COUNT,
};

enum EBackendType
{
	BACKEND_TYPE_OPENGL = 0,
	BACKEND_TYPE_OPENGL_ES,
	BACKEND_TYPE_VULKAN,

	// special value to tell the backend to identify the current backend
	BACKEND_TYPE_AUTO,

	BACKEND_TYPE_COUNT,
};

struct STWGraphicGPU
{
	enum ETWGraphicsGPUType
	{
		GRAPHICS_GPU_TYPE_DISCRETE = 0,
		GRAPHICS_GPU_TYPE_INTEGRATED,
		GRAPHICS_GPU_TYPE_VIRTUAL,
		GRAPHICS_GPU_TYPE_CPU,

		// should stay at last position in this enum
		GRAPHICS_GPU_TYPE_INVALID,
	};

	struct STWGraphicGPUItem
	{
		char m_aName[256];
		ETWGraphicsGPUType m_GPUType;
	};
	std::vector<STWGraphicGPUItem> m_vGPUs;
	STWGraphicGPUItem m_AutoGPU;
};

typedef STWGraphicGPU TTWGraphicsGPUList;

typedef std::function<void()> WINDOW_RESIZE_FUNC;
typedef std::function<void()> WINDOW_PROPS_CHANGED_FUNC;

namespace client_data7 {
struct CDataSprite; // NOLINT(bugprone-forward-declaration-namespace)
}

typedef std::function<bool(uint32_t &Width, uint32_t &Height, uint32_t &Format, std::vector<uint8_t> &vDstData)> TGLBackendReadPresentedImageData;

#endif
