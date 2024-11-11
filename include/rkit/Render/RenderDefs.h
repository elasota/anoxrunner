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
		static T GetDefault() { T(); }
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

		static bool HasDefault();
		static T GetDefault();

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
		VectorDimension m_cols = VectorDimension::Dimension1;

		bool operator==(const VectorNumericType &other) const;
		bool operator!=(const VectorNumericType &other) const;
	};

	struct CompoundNumericType
	{
		NumericType m_numericType = NumericType::Float32;
		VectorDimension m_rows = VectorDimension::Dimension1;
		VectorDimension m_cols = VectorDimension::Dimension1;

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
		Span<const StructureMemberDesc *> m_members;
	};

	enum class VertexInputStepping
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
		VertexInputStepping m_stepping = VertexInputStepping::Vertex;
	};

	struct InputLayoutDesc
	{
		Span<const InputLayoutVertexInputDesc *> m_vertexFeeds;
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

	struct PushConstantsListDesc
	{
		Span<const PushConstantDesc *> m_pushConstants;
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
		PushConstantsListDesc m_pushConstants;
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
			new (&m_u.m_configName) StringView(other.m_u.m_configName);
		else if (other.m_state == ConfigurableValueState::Explicit)
			new (&m_u.m_value) T(other.m_u.m_value);
	}

	template<class T>
	inline ConfigurableValueBase<T>::ConfigurableValueBase(ConfigurableValueBase &&other) noexcept
		: m_state(other.m_state)
	{
		if (other.m_state == ConfigurableValueState::Configured)
			new (&m_u.m_configName) StringView(other.m_u.m_configName);
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
