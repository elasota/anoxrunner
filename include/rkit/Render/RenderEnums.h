#pragma once

namespace rkit { namespace render
{
	enum class Filter
	{
		Nearest,
		Linear,

		Count,
	};

	enum class MipMapMode
	{
		Nearest,
		Linear,

		Count,
	};

	enum class AddressMode
	{
		Repeat,
		MirrorRepeat,
		ClampEdge,
		ClampBorder,

		Count,
	};

	enum class AnisotropicFiltering
	{
		Disabled,

		Anisotropic1,
		Anisotropic2,
		Anisotropic4,
		Anisotropic8,
		Anisotropic16,

		Count,
	};

	enum class ComparisonFunction
	{
		Disabled,

		Never,
		Always,
		Equal,
		NotEqual,
		Less,
		LessOrEqual,
		Greater,
		GreaterOrEqual,

		Count,
	};

	enum class BorderColor
	{
		TransparentBlack,
		OpaqueBlack,
		OpaqueWhite,

		Count,
	};

	enum class VectorOrScalarDimension
	{
		Scalar,
		Dimension2,
		Dimension3,
		Dimension4,

		Count,
	};

	enum class VectorDimension
	{
		Dimension2,
		Dimension3,
		Dimension4,

		Count,
	};

	enum class NumericType
	{
		Float16,
		Float32,
		Float64,

		SInt8,
		SInt16,
		SInt32,
		SInt64,

		UInt8,
		UInt16,
		UInt32,
		UInt64,

		Bool,

		SNorm8,
		SNorm16,

		UNorm8,
		UNorm16,

		Count,
	};

	enum class InputLayoutVertexInputStepping
	{
		Vertex,
		Instance,

		Count,
	};

	enum class ReadWriteAccess
	{
		Read,
		Write,
		ReadWrite,

		Count,
	};

	enum class OptionalReadWriteAccess
	{
		None,

		Read,
		Write,
		ReadWrite,

		Count,
	};

	enum class StencilOp
	{
		Keep,
		Zero,
		Replace,
		IncrementSaturate,
		DecrementSaturate,
		Invert,
		Increment,
		Decrement,

		Count,
	};

	enum class FillMode
	{
		Wireframe,
		Solid,

		Count,
	};

	enum class CullMode
	{
		None,
		Front,
		Back,

		Count,
	};

	enum class StageVisibility
	{
		All,

		Vertex,
		Pixel,

		Count,
	};

	enum class DescriptorType
	{
		Sampler,

		StaticConstantBuffer,
		DynamicConstantBuffer,

		Buffer,
		RWBuffer,

		ByteAddressBuffer,
		RWByteAddressBuffer,

		Texture1D,
		Texture1DArray,
		Texture2D,
		Texture2DArray,
		Texture2DMS,
		Texture2DMSArray,
		Texture3D,
		TextureCube,
		TextureCubeArray,

		RWTexture1D,
		RWTexture1DArray,
		RWTexture2D,
		RWTexture2DArray,
		RWTexture3D,

		Count,
	};

	enum class PrimitiveTopology
	{
		PointList,
		LineList,
		LineStrip,
		TriangleList,
		TriangleStrip,

		Count,
	};

	enum class IndexSize
	{
		UInt16,
		UInt32,

		Count,
	};

	enum class ColorBlendFactor
	{
		Zero,
		One,
		SrcColor,
		InvSrcColor,
		SrcAlpha,
		InvSrcAlpha,
		DstAlpha,
		InvDstAlpha,
		DstColor,
		InvDstColor,
		ConstantColor,
		InvConstantColor,
		ConstantAlpha,
		InvConstantAlpha,

		Count,
	};

	enum class BlendOp
	{
		Add,
		Subtract,
		ReverseSubtract,
		Min,
		Max,

		Count,
	};

	enum class AlphaBlendFactor
	{
		Zero,
		One,
		SrcAlpha,
		InvSrcAlpha,
		DstAlpha,
		InvDstAlpha,
		ConstantAlpha,
		InvConstantAlpha,

		Count,
	};

	enum class RenderTargetFormat
	{
		RGBA_UNorm8,
		RGBA_UNorm8_sRGB,

		Count,
	};

	enum class DepthStencilFormat
	{
		DepthFloat32,
		DepthFloat32_Stencil8_Undefined24,
		DepthUNorm24_Stencil8,
		DepthUNorm16,

		Count,
	};

	enum class SwapChainWriteBehavior
	{
		RenderTarget,
		Copy,
	};

} } // rkit::render
