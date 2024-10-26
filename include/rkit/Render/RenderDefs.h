#pragma once

#include "rkit/Core/Span.h"
#include "rkit/Core/StringView.h"

namespace rkit::render
{
	struct StructureMemberDesc;

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
		Less,
		Equal,
		LessOrEqual,
		Greater,
		NotEqual,
		GreaterOrEqual,
		Always,

		Count,
	};

	enum class BorderColor
	{
		TransparentBlack,
		OpaqueBlack,
		OpaqueWhite,

		Count,
	};

	struct SamplerDesc
	{
		Filter m_minFilter = Filter::Linear;
		Filter m_magFilter = Filter::Nearest;
		MipMapMode m_mipMapMode = MipMapMode::Linear;
		AddressMode m_addressModeU = AddressMode::Repeat;
		AddressMode m_addressMoveV = AddressMode::Repeat;
		AddressMode m_addressModeW = AddressMode::Repeat;
		float m_mipLodBias = 0.f;
		float m_minLod = 0.f;
		float m_maxLod = 0.f;
		bool m_haveMaxLod = false;

		AnisotropicFiltering m_anisotropy = AnisotropicFiltering::Disabled;
		bool m_isCompare = false;

		ComparisonFunction m_compareFunction = ComparisonFunction::Disabled;
	};

	enum class VectorDimension
	{
		Dimension1,
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

		Count,
	};

	struct VectorNumericType
	{
		NumericType m_numericType = NumericType::Float32;
		VectorDimension m_cols = VectorDimension::Dimension1;
	};

	struct CompoundNumericType
	{
		NumericType m_numericType = NumericType::Float32;
		VectorDimension m_rows = VectorDimension::Dimension1;
		VectorDimension m_cols = VectorDimension::Dimension1;
	};

	struct StructureType
	{
		Span<const StructureMemberDesc> m_members;
	};

	enum class ValueTypeType
	{
		CompoundNumeric,
		Structure,

		Count,
	};

	union ValueUnion
	{
		ValueUnion();
		ValueUnion(CompoundNumericType compoundNumericType);
		ValueUnion(StructureType structureType);
		~ValueUnion();

		CompoundNumericType m_compoundNumericType;
		StructureType m_structure;
	};

	struct ValueType
	{
		ValueTypeType m_type = ValueTypeType::CompoundNumeric;
		ValueUnion m_value = ValueUnion(CompoundNumericType());
	};

	struct StructMemberDesc
	{
		ValueType m_valueType;
		StringView m_name;
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

	struct GraphicsPipelineRenderTargetDesc
	{
		ReadWriteAccess m_colorAccess;

	};

	struct GraphicsPipelineDepthDesc
	{
		bool m_testEnable = false;
		bool m_writeEnable = false;
		ComparisonFunction m_comparisonFunction = ComparisonFunction::Less;

		int32_t m_depthBias = 0;
		float m_depthBiasClamp = 0.f;
		float m_depthBiasSlopeScale = 0.f;

		bool m_depthClipEnable = true;
	};

	struct GraphicsPipelineStencilOpDesc
	{
		StencilOp m_passOp = StencilOp::Keep;
		StencilOp m_failOp = StencilOp::Keep;
		StencilOp m_depthFailOp = StencilOp::Keep;
		ComparisonFunction m_compareFunc = ComparisonFunction::Equal;
	};

	struct GraphicsPipelineStencilDesc
	{
		bool m_testEnable = false;
		bool m_writeEnable = false;
		ComparisonFunction m_comparisonFunction = ComparisonFunction::Less;
		uint8_t m_compareMask = 0xffu;
		uint8_t m_writeMask = 0xffu;
		GraphicsPipelineStencilOpDesc m_frontOps;
		GraphicsPipelineStencilOpDesc m_backOps;
	};

	enum class Topology
	{
		Triangle,
		TriangleStrip,

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

	struct GraphicsPipelineDesc
	{
		size_t m_descriptorLayoutID;
		size_t m_inputLayoutID;
		size_t m_vertexShaderID;
		size_t m_pixelShaderID;

		Topology m_topology = Topology::Triangle;
		FillMode m_fillMode = FillMode::Solid;
		CullMode m_cullMode = CullMode::Back;

		Span<const GraphicsPipelineRenderTargetDesc> m_renderTargets;

		GraphicsPipelineDepthDesc m_depthDesc;
		GraphicsPipelineStencilDesc m_stencilDesc;
	};
}
