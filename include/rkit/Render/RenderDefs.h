#pragma once

#include "rkit/Core/Span.h"
#include "rkit/Core/StringView.h"
#include "rkit/Core/Hasher.h"

#include "RenderDefProtos.h"

#include <cstdint>

namespace rkit::render
{
	struct StructureMemberDesc;

	template<int TPurpose>
	class StringIndex
	{
	public:
		typedef size_t IndexType_t;
		static const int kPurpose = TPurpose;

		StringIndex() : m_index(0) {}
		explicit StringIndex(IndexType_t index) : m_index(index) {}
		StringIndex(const StringIndex<TPurpose> &) = default;

		bool operator==(const StringIndex<TPurpose> &other) const { return m_index == other.m_index; }
		bool operator!=(const StringIndex<TPurpose> &other) const { return !((*this) == other); }

		IndexType_t GetIndex() const { return m_index; }

		HashValue_t ComputeHash(HashValue_t base) const
		{
			return Hasher<IndexType_t>::ComputeHash(base, m_index);
		}

	private:
		IndexType_t m_index;
	};
}

namespace rkit
{
	template<int TPurpose>
	struct Hasher<rkit::render::StringIndex<TPurpose>> : public DefaultSpanHasher<rkit::render::StringIndex<TPurpose>>
	{
		static HashValue_t ComputeHash(HashValue_t base, const rkit::render::StringIndex<TPurpose> &value)
		{
			return value.ComputeHash(base);
		}
	};
}

namespace rkit::render
{
	struct StructureType;

	template<class TFloat, int TDefault>
	struct FloatDefaultResolver
	{
		static bool HasDefault() { return true; }
		static TFloat GetDefault() { return static_cast<TFloat>(TDefault); }
	};

	template<class T>
	struct NoDefault
	{
		static bool HasDefault() { return false; }
		static T GetDefault() { return T(); }
	};

	enum class ConfigurableValueState : uint8_t
	{
		Default,
		Configured,
		Explicit,
	};

	template<class T>
	union ConfigurableValueUnion
	{
		ConfigurableValueUnion();
		ConfigurableValueUnion(const T &value);
		ConfigurableValueUnion(T &&value);
		ConfigurableValueUnion(const render::ConfigStringIndex_t &stringIndex);

		T m_value;
		render::ConfigStringIndex_t m_configName;
	};

	template<class T>
	struct ConfigurableValueBase
	{
		ConfigurableValueBase();
		ConfigurableValueBase(const T &value);
		ConfigurableValueBase(const render::ConfigStringIndex_t &stringIndex);
		ConfigurableValueBase(const ConfigurableValueBase &other);
		ConfigurableValueBase(ConfigurableValueBase &&other) noexcept;
		~ConfigurableValueBase();

		ConfigurableValueBase &operator=(const ConfigurableValueBase &other);
		ConfigurableValueBase &operator=(ConfigurableValueBase &&other) noexcept;

		ConfigurableValueState m_state;
		ConfigurableValueUnion<T> m_u;

	private:
		void Clear();
	};

	template<class T, class TDefaultResolver>
	struct ConfigurableValueWithDefault final : public ConfigurableValueBase<T>
	{
		ConfigurableValueWithDefault() = default;
		ConfigurableValueWithDefault(const T &value) : ConfigurableValueBase<T>(value) {}
		ConfigurableValueWithDefault(const render::ConfigStringIndex_t &stringIndex) : ConfigurableValueBase<T>(stringIndex) {}

		ConfigurableValueWithDefault(const ConfigurableValueWithDefault &other) = default;
		ConfigurableValueWithDefault(ConfigurableValueWithDefault &&other) = default;

		static bool HasDefault();
		static T GetDefault();
	};

	template<class T, T TDefault>
	struct DefaultResolver
	{
		static bool HasDefault() { return true; }
		static T GetDefault() { return TDefault; }
	};

	template<class T>
	using ConfigurableValueNoDefault = ConfigurableValueWithDefault<T, NoDefault<T>>;

	template<class T, T TDefault>
	using ConfigurableValue = ConfigurableValueWithDefault<T, DefaultResolver<T, TDefault>>;

	template<class T, int TDefault>
	using ConfigurableValueFloat = ConfigurableValueWithDefault<T, FloatDefaultResolver<T, TDefault>>;

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
		ConfigurableValue<Filter, Filter::Linear> m_minFilter;
		ConfigurableValue<Filter, Filter::Linear> m_magFilter;
		ConfigurableValue<MipMapMode, MipMapMode::Linear> m_mipMapMode;
		ConfigurableValue<AddressMode, AddressMode::Repeat> m_addressModeU;
		ConfigurableValue<AddressMode, AddressMode::Repeat> m_addressModeV;
		ConfigurableValue<AddressMode, AddressMode::Repeat> m_addressModeW;
		ConfigurableValueFloat<float, 0> m_mipLodBias;
		ConfigurableValueFloat<float, 0> m_minLod;
		ConfigurableValueNoDefault<float> m_maxLod;

		ConfigurableValue<AnisotropicFiltering, AnisotropicFiltering::Anisotropic1> m_anisotropy;

		ConfigurableValue<ComparisonFunction, ComparisonFunction::Disabled> m_compareFunction;
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
		SNorm32,

		UNorm8,
		UNorm16,
		UNorm32,

		Count,
	};

	struct VectorNumericType
	{
		NumericType m_numericType = NumericType::Float32;
		VectorDimension m_cols = VectorDimension::Dimension4;

		bool operator==(const VectorNumericType &other) const;
		bool operator!=(const VectorNumericType &other) const;
	};

	struct CompoundNumericType
	{
		NumericType m_numericType = NumericType::Float32;
		VectorDimension m_rows = VectorDimension::Dimension4;
		VectorDimension m_cols = VectorDimension::Dimension4;

		bool operator==(const CompoundNumericType &other) const;
		bool operator!=(const CompoundNumericType &other) const;
	};

	enum class ValueTypeType
	{
		Numeric,
		CompoundNumeric,
		VectorNumeric,

		Structure,

		Count,
	};

	union ValueUnion
	{
		ValueUnion();
		ValueUnion(NumericType numericType);
		ValueUnion(const VectorNumericType *vectorNumericType);
		ValueUnion(const CompoundNumericType *compoundNumericType);
		ValueUnion(const StructureType *structureType);
		~ValueUnion();

		NumericType m_numericType;
		const VectorNumericType *m_vectorNumericType;
		const CompoundNumericType *m_compoundNumericType;
		const StructureType *m_structureType;
	};

	struct ValueType
	{
		ValueType();
		ValueType(const ValueType &other);
		ValueType(NumericType numericType);
		ValueType(const VectorNumericType *vectorNumericType);
		ValueType(const CompoundNumericType *compoundNumericType);
		ValueType(const StructureType *structureType);
		~ValueType();

		ValueType &operator=(const ValueType &other);

		ValueTypeType m_type = ValueTypeType::CompoundNumeric;
		ValueUnion m_value = ValueUnion(NumericType::Float32);

	public:
		void DestructUnion();
	};

	struct StructureMemberDesc
	{
		ValueType m_type;
		TempStringIndex_t m_name;
	};

	struct StructureType
	{
		ConstSpan<const StructureMemberDesc *> m_members;
	};

	enum class InputLayoutVertexInputStepping
	{
		Vertex,
		Instance,

		Count,
	};

	struct InputLayoutVertexInputDesc
	{
		TempStringIndex_t m_feedName;
		TempStringIndex_t m_memberName;
		uint32_t m_inputSlot = 0;
		uint32_t m_byteOffset = 0;
		const VectorNumericType *m_numericType = nullptr;
		InputLayoutVertexInputStepping m_stepping = InputLayoutVertexInputStepping::Vertex;
	};

	struct InputLayoutDesc
	{
		ConstSpan<const InputLayoutVertexInputDesc *> m_vertexFeeds;
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

	struct StencilOpDesc
	{
		ConfigurableValue<StencilOp, StencilOp::Keep> m_passOp;
		ConfigurableValue<StencilOp, StencilOp::Keep> m_failOp;
		ConfigurableValue<StencilOp, StencilOp::Keep> m_depthFailOp;
		ConfigurableValue<ComparisonFunction, ComparisonFunction::Equal> m_compareFunc;
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
		Hull,
		Domain,
		Geometry,
		Pixel,
		Amplification,
		Mesh,

		Count,
	};

	struct PushConstantDesc
	{
		TempStringIndex_t m_name;
		ValueType m_type;
		StageVisibility m_stageVisibility = StageVisibility::All;
	};

	struct PushConstantListDesc
	{
		ConstSpan<const PushConstantDesc *> m_pushConstants;
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

	struct DescriptorDesc
	{
		TempStringIndex_t m_name;

		StageVisibility m_visibility = StageVisibility::All;

		DescriptorType m_descriptorType = DescriptorType::Texture2D;
		uint32_t m_arraySize = 1;			// 0 = Unbounded
		bool m_globallyCoherent = false;
		ValueType m_valueType;

		const SamplerDesc *m_staticSamplerDesc;
	};

	struct DescriptorLayoutDesc
	{
		ConstSpan<const DescriptorDesc *> m_descriptors;
	};

	struct BinaryContent
	{
		size_t m_contentIndex = 0;
	};

	struct ContentKey
	{
		BinaryContent m_content;
	};

	struct ShaderPermutationTree;

	struct ShaderPermutationTreeBranch
	{
		int32_t m_keyValue;
		const ShaderPermutationTree *m_subTree;
	};

	struct ShaderPermutationTree
	{
		size_t m_width;
		ShaderPermutationStringIndex_t m_keyName;
		ConstSpan<const ShaderPermutationTreeBranch *> m_branches;
	};

	struct ShaderDesc
	{
		TempStringIndex_t m_source;
		TempStringIndex_t m_entryPoint;
	};

	enum class RenderTargetFormat
	{
		RGBA_UNorm8,
		RGBA_UNorm8_sRGB,

		Count,
	};

	struct RenderTargetDesc
	{
		TempStringIndex_t m_name;

		ConfigurableValue<RenderTargetFormat, RenderTargetFormat::RGBA_UNorm8> m_format;
		ReadWriteAccess m_access;

		ColorBlendFactor m_srcBlend = ColorBlendFactor::One;
		ColorBlendFactor m_dstBlend = ColorBlendFactor::Zero;
		BlendOp m_colorBlendOp = BlendOp::Add;

		AlphaBlendFactor m_srcAlphaBlend = AlphaBlendFactor::One;
		AlphaBlendFactor m_dstAlphaBlend = AlphaBlendFactor::Zero;
		BlendOp m_alphaBlendOp = BlendOp::Add;

		bool m_writeRed = true;
		bool m_writeGreen = true;
		bool m_writeBlue = true;
		bool m_writeAlpha = true;
	};

	enum class DepthStencilFormat
	{
		DepthFloat32,
		DepthFloat32_Stencil8_Undefined24,
		DepthUNorm24_Stencil8,
		DepthUNorm16,

		Count,
	};

	struct DepthStencilDesc
	{
		ConfigurableValue<DepthStencilFormat, DepthStencilFormat::DepthUNorm16> m_format;

		ConfigurableValue<bool, false> m_depthTest;
		ConfigurableValue<bool, false> m_depthWrite;
		ConfigurableValue<ComparisonFunction, ComparisonFunction::Always> m_depthCompareOp;

		ConfigurableValue<bool, false> m_stencilTest;
		ConfigurableValue<bool, false> m_stencilWrite;
		ConfigurableValue<ComparisonFunction, ComparisonFunction::Always> m_stencilCompareOp;

		ConfigurableValue<uint8_t, 0xffu> m_stencilCompareMask;
		ConfigurableValue<uint8_t, 0xffu> m_stencilWriteMask;

		StencilOpDesc m_stencilFrontOps;
		StencilOpDesc m_stencilBackOps;
	};

	struct GraphicsPipelineDesc
	{
		const PushConstantListDesc *m_pushConstants = nullptr;

		ConstSpan<const DescriptorLayoutDesc *> m_descriptorLayouts;

		const InputLayoutDesc *m_inputLayout = nullptr;
		const StructureType *m_vertexShaderOutput = nullptr;

		const ShaderDesc *m_vertexShader = nullptr;
		const ShaderDesc *m_pixelShader = nullptr;

		ConstSpan<const ContentKey *> m_compiledContentKeys;

		IndexSize m_indexSize = IndexSize::UInt32;
		bool m_primitiveRestart = false;
		PrimitiveTopology m_primitiveTopology = PrimitiveTopology::TriangleList;

		ConstSpan<const RenderTargetDesc *> m_renderTargets;
		const DepthStencilDesc *m_depthStencil = nullptr;

		bool m_alphaToCoverage = false;

		bool m_dynamicBlendConstants = false;

		float m_blendConstantRed = 0.0f;
		float m_blendConstantGreen = 0.0f;
		float m_blendConstantBlue = 0.0f;
		float m_blendConstantAlpha = 0.0f;

		FillMode m_fillMode = FillMode::Solid;
		CullMode m_cullMode = CullMode::Back;

		const ShaderPermutationTree *m_permutationTree;
	};

	struct GraphicsPipelineNameLookup
	{
		GlobalStringIndex_t m_name;
		const GraphicsPipelineDesc *m_pipeline = nullptr;
	};
}


#include <new>
#include <utility>

namespace rkit::render
{
	inline ValueType::ValueType()
		: m_type(ValueTypeType::Numeric)
		, m_value(NumericType::Float32)
	{
	}


	inline ValueType::ValueType(const ValueType &other)
		: m_type(ValueTypeType::Numeric)
		, m_value(NumericType::Float32)
	{
		(*this) = other;
	}

	inline ValueType::ValueType(NumericType numericType)
		: m_type(ValueTypeType::Numeric)
		, m_value(numericType)
	{
	}

	inline ValueType::ValueType(const VectorNumericType *vectorNumericType)
		: m_type(ValueTypeType::VectorNumeric)
		, m_value(vectorNumericType)
	{
	}

	inline ValueType::ValueType(const CompoundNumericType *compoundNumericType)
		: m_type(ValueTypeType::CompoundNumeric)
		, m_value(compoundNumericType)
	{
	}

	inline ValueType::ValueType(const StructureType *structureType)
		: m_type(ValueTypeType::Structure)
		, m_value(structureType)
	{
	}


	inline ValueType::~ValueType()
	{
		DestructUnion();
	}

	inline ValueType &rkit::render::ValueType::operator=(const ValueType &other)
	{
		if (this != &other)
		{
			DestructUnion();

			m_type = other.m_type;

			switch (m_type)
			{
			case ValueTypeType::Numeric:
				new (&m_value) ValueUnion(other.m_value.m_numericType);
				break;

			case ValueTypeType::CompoundNumeric:
				new (&m_value) ValueUnion(other.m_value.m_compoundNumericType);
				break;

			case ValueTypeType::VectorNumeric:
				new (&m_value) ValueUnion(other.m_value.m_vectorNumericType);
				break;

			case ValueTypeType::Structure:
				new (&m_value) ValueUnion(other.m_value.m_structureType);
				break;

			default:
				break;
			}
		}

		return *this;
	}

	inline void ValueType::DestructUnion()
	{
		m_value.~ValueUnion();
	}

	inline ValueUnion::ValueUnion()
		: m_numericType(NumericType::Float32)
	{
	}

	inline ValueUnion::ValueUnion(NumericType numericType)
		: m_numericType(numericType)
	{
	}

	inline ValueUnion::ValueUnion(const VectorNumericType *vectorNumericType)
		: m_vectorNumericType(vectorNumericType)
	{
	}

	inline ValueUnion::ValueUnion(const CompoundNumericType *compoundNumericType)
		: m_compoundNumericType(compoundNumericType)
	{
	}

	inline ValueUnion::ValueUnion(const StructureType *structureType)
		: m_structureType(structureType)
	{
	}

	inline ValueUnion::~ValueUnion()
	{
	}

	inline bool VectorNumericType::operator==(const VectorNumericType &other) const
	{
		return m_cols == other.m_cols && m_numericType == other.m_numericType;
	}

	inline bool VectorNumericType::operator!=(const VectorNumericType &other) const
	{
		return !((*this) == other);
	}

	inline bool CompoundNumericType::operator==(const CompoundNumericType &other) const
	{
		return m_cols == other.m_cols && m_numericType == other.m_numericType && m_rows == other.m_rows;
	}

	inline bool CompoundNumericType::operator!=(const CompoundNumericType &other) const
	{
		return !((*this) == other);
	}

	template<class T>
	ConfigurableValueUnion<T>::ConfigurableValueUnion()
	{
	}

	template<class T>
	ConfigurableValueUnion<T>::ConfigurableValueUnion(const T &value)
	{
		new (&m_value) T(value);
	}

	template<class T>
	ConfigurableValueUnion<T>::ConfigurableValueUnion(T &&value)
	{
		new (&m_value) T(std::move(value));
	}

	template<class T>
	ConfigurableValueUnion<T>::ConfigurableValueUnion(const ConfigStringIndex_t &configName)
	{
		new (&m_configName) ConfigStringIndex_t(configName);
	}

	template<class T>
	inline ConfigurableValueBase<T>::ConfigurableValueBase()
		: m_state(ConfigurableValueState::Default)
		, m_u(render::ConfigStringIndex_t())
	{
	}

	template<class T>
	inline ConfigurableValueBase<T>::ConfigurableValueBase(const T &value)
		: m_state(ConfigurableValueState::Explicit)
		, m_u(value)
	{
	}

	template<class T>
	inline ConfigurableValueBase<T>::ConfigurableValueBase(const ConfigStringIndex_t &value)
		: m_state(ConfigurableValueState::Configured)
		, m_u(value)
	{
	}

	template<class T>
	inline ConfigurableValueBase<T>::ConfigurableValueBase(const ConfigurableValueBase &other)
		: m_state(other.m_state)
	{
		if (other.m_state == ConfigurableValueState::Configured)
			new (&m_u.m_configName) ConfigStringIndex_t(other.m_u.m_configName);
		else if (other.m_state == ConfigurableValueState::Explicit)
			new (&m_u.m_value) T(other.m_u.m_value);
	}

	template<class T>
	inline ConfigurableValueBase<T>::ConfigurableValueBase(ConfigurableValueBase &&other) noexcept
		: m_state(other.m_state)
	{
		if (other.m_state == ConfigurableValueState::Configured)
			new (&m_u.m_configName) ConfigStringIndex_t(other.m_u.m_configName);
		else if (other.m_state == ConfigurableValueState::Explicit)
			new (&m_u.m_value) T(std::move(other.m_u.m_value));
	}

	template<class T>
	inline ConfigurableValueBase<T>::~ConfigurableValueBase()
	{
		Clear();
	}

	template<class T>
	inline void ConfigurableValueBase<T>::Clear()
	{
		if (m_state == ConfigurableValueState::Configured)
			m_u.m_configName.~ConfigStringIndex_t();
		else if (m_state == ConfigurableValueState::Explicit)
			m_u.m_value.~T();

		m_state = ConfigurableValueState::Default;
	}

	template<class T>
	inline ConfigurableValueBase<T> &ConfigurableValueBase<T>::operator=(const ConfigurableValueBase &other)
	{
		if (this != &other)
		{
			Clear();

			m_state = other.m_state;
			if (other.m_state == ConfigurableValueState::Configured)
				new (&m_u.m_configName) ConfigStringIndex_t(other.m_u.m_configName);
			else if (other.m_state == ConfigurableValueState::Explicit)
				new (&m_u.m_value) T(other.m_u.m_value);
		}

		return *this;
	}

	template<class T>
	inline ConfigurableValueBase<T> &ConfigurableValueBase<T>::operator=(ConfigurableValueBase &&other) noexcept
	{
		Clear();

		m_state = other.m_state;
		if (other.m_state == ConfigurableValueState::Configured)
			new (&m_u.m_configName) ConfigStringIndex_t(other.m_u.m_configName);
		else if (other.m_state == ConfigurableValueState::Explicit)
			new (&m_u.m_value) T(std::move(other.m_u.m_value));

		return *this;
	}

	template<class T, class TDefaultResolver>
	inline bool ConfigurableValueWithDefault<T, TDefaultResolver>::HasDefault()
	{
		return TDefaultResolver::HasDefault();
	}

	template<class T, class TDefaultResolver>
	inline T ConfigurableValueWithDefault<T, TDefaultResolver>::GetDefault()
	{
		return TDefaultResolver::GetDefault();
	}
}
