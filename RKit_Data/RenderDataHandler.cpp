#include "RenderDataHandler.h"

#include "rkit/Render/RenderDefs.h"

namespace rkit::data
{
#define RTTI_ENUM_BEGIN(name)	\
	namespace render_rtti_ ## name\
	{\
		typedef render::name EnumType_t;\
		extern RenderRTTIEnumType g_type;\
		const RenderRTTIMainType g_mainType = RenderRTTIMainType::name;\
	}\
	template<> struct RTTIResolver<render_rtti_ ## name::EnumType_t>\
	{\
		static const RenderRTTITypeBase *GetRTTIType()\
		{\
			return &render_rtti_ ## name::g_type.m_base;\
		}\
	};\
	namespace render_rtti_ ## name\
	{\
		const char g_name[] = #name;\
		RenderRTTIEnumOption g_options[] =\
		{

#define RTTI_ENUM_OPTION(name)	\
			{ #name, sizeof(#name) - 1, static_cast<unsigned int>(EnumType_t::name) },


#define RTTI_ENUM_END	\
		};\
		RenderRTTIEnumType g_type =\
		{\
			{\
				RenderRTTIType::Enum,\
				g_mainType,\
				g_name,\
				sizeof(g_name) - 1,\
			},\
			g_options,\
			sizeof(g_options) / sizeof(RenderRTTIEnumOption),\
			static_cast<unsigned int>(EnumType_t::Count),\
			EnumHelper<EnumType_t>::ReadValue,\
			EnumHelper<EnumType_t>::WriteValue,\
			EnumHelper<EnumType_t>::GetConfigurableState,\
			EnumHelper<EnumType_t>::ReadConfigurableValue,\
			EnumHelper<EnumType_t>::ReadConfigurableName,\
			EnumHelper<EnumType_t>::WriteConfigurableValue,\
			EnumHelper<EnumType_t>::WriteConfigurableName,\
			EnumHelper<EnumType_t>::WriteConfigurableDefault,\
		};\
	}

#define RTTI_DEFINE_STRING_INDEX(name)	\
	namespace render_rtti_ ## name ## StringIndex\
	{\
		typedef render::name ## StringIndex_t StringIndexType_t;\
		extern RenderRTTIStringIndexType g_type;\
		const RenderRTTIMainType g_mainType = RenderRTTIMainType::name ## StringIndex;\
	}\
	template<> struct RTTIResolver<render_rtti_ ## name ## StringIndex::StringIndexType_t>\
	{\
		static const RenderRTTITypeBase *GetRTTIType()\
		{\
			return &render_rtti_ ## name ## StringIndex::g_type.m_base;\
		}\
	};\
	namespace render_rtti_ ## name ## StringIndex\
	{\
		const char g_name[] = #name;\
		RenderRTTIStringIndexType g_type =\
		{\
			{\
				RenderRTTIType::StringIndex,\
				g_mainType,\
				g_name,\
				sizeof(g_name) - 1,\
			},\
			StringIndexHelper<StringIndexType_t>::WriteValue,\
			StringIndexHelper<StringIndexType_t>::ReadValue,\
			StringIndexHelper<StringIndexType_t>::GetPurpose,\
		};\
	}

#define RTTI_STRUCT_BEGIN(name)	\
	namespace render_rtti_ ## name\
	{\
		typedef render::name StructType_t;\
		const RenderRTTIMainType g_mainType = RenderRTTIMainType::name;\
		extern RenderRTTIStructType g_type;\
	}\
	template<> struct RTTIResolver<render_rtti_ ## name::StructType_t>\
	{\
		static const RenderRTTITypeBase *GetRTTIType()\
		{\
			return &render_rtti_ ## name::g_type.m_base;\
		}\
	};\
	namespace render_rtti_ ## name\
	{\
		const char g_name[] = #name;\
		RenderRTTIStructField g_fields[] =\
		{

#define RTTI_STRUCT_FIELD_WITH_VISIBILITY(name, visibility)	\
	{\
		#name,\
		sizeof(#name) - 1,\
		RTTIResolver<decltype(StructType_t::m_ ## name)>::GetRTTIType,\
		visibility,\
		RTTIConfigurableResolver<decltype(StructType_t::m_ ## name)>::kIsConfigurable,\
		StructHelper<StructType_t>::GetMemberPtr<decltype(StructType_t::m_ ## name), &StructType_t::m_ ## name>,\
	},

#define RTTI_STRUCT_FIELD(name)	RTTI_STRUCT_FIELD_WITH_VISIBILITY(name, true)

#define RTTI_STRUCT_FIELD_INVISIBLE(name)	RTTI_STRUCT_FIELD_WITH_VISIBILITY(name, false)

#define RTTI_STRUCT_END	\
		};\
		RenderRTTIStructType g_type =\
		{\
			{\
				RenderRTTIType::Structure,\
				g_mainType,\
				g_name,\
				sizeof(g_name) - 1,\
			},\
			g_fields,\
			sizeof(g_fields) / sizeof(RenderRTTIStructField),\
		};\
	}


	namespace render_rtti
	{
		template<class T>
		struct RTTIResolver
		{
		};

		template<class T, class TDefaultResolver>
		struct RTTIResolver<render::ConfigurableValueWithDefault<T, TDefaultResolver>>
			: public RTTIResolver<T>
		{
		};

		template<class T>
		struct RTTIConfigurableResolver
		{
			static const bool kIsConfigurable = false;
		};

		template<class T, class TDefaultResolver>
		struct RTTIConfigurableResolver<render::ConfigurableValueWithDefault<T, TDefaultResolver>>
		{
			static const bool kIsConfigurable = true;
		};


		template<class T>
		struct EnumHelper
		{
			static unsigned int ReadValue(const void *valuePtr);
			static void WriteValue(void *valuePtr, unsigned int value);

			static uint8_t GetConfigurableState(const void *configurablePtr);
			static unsigned int ReadConfigurableValue(const void *configurablePtr);
			static const render::ConfigStringIndex_t &ReadConfigurableName(const void *configurablePtr);

			static void WriteConfigurableValue(void *configurablePtr, unsigned int value);
			static void WriteConfigurableName(void *configurablePtr, const render::ConfigStringIndex_t &str);
			static void WriteConfigurableDefault(void *configurablePtr);
		};

		template<class T>
		struct StructHelper
		{
			template<class TMemberType, TMemberType T::* TMember>
			static void *GetMemberPtr(void *ptr)
			{
				return &(static_cast<T *>(ptr)->*TMember);
			}
		};

		template<class T>
		unsigned int EnumHelper<T>::ReadValue(const void *valuePtr)
		{
			return static_cast<unsigned int>(*static_cast<const T *>(valuePtr));
		}

		template<class T>
		void EnumHelper<T>::WriteValue(void *valuePtr, unsigned int value)
		{
			*static_cast<T *>(valuePtr) = static_cast<T>(value);
		}

		template<class T>
		uint8_t EnumHelper<T>::GetConfigurableState(const void *configurablePtr)
		{
			return static_cast<uint8_t>(static_cast<const render::ConfigurableValueBase<T> *>(configurablePtr)->m_state);
		}

		template<class T>
		unsigned int EnumHelper<T>::ReadConfigurableValue(const void *configurablePtr)
		{
			return static_cast<unsigned int>(static_cast<const render::ConfigurableValueBase<T> *>(configurablePtr)->m_u.m_value);
		}

		template<class T>
		const render::ConfigStringIndex_t &EnumHelper<T>::ReadConfigurableName(const void *configurablePtr)
		{
			return static_cast<const render::ConfigurableValueBase<T> *>(configurablePtr)->m_u.m_configName;
		}

		template<class T>
		void EnumHelper<T>::WriteConfigurableValue(void *configurablePtr, unsigned int value)
		{
			*static_cast<render::ConfigurableValueBase<T> *>(configurablePtr) = render::ConfigurableValueBase<T>(static_cast<T>(value));
		}

		template<class T>
		void EnumHelper<T>::WriteConfigurableName(void *configurablePtr, const render::ConfigStringIndex_t &str)
		{
			*static_cast<render::ConfigurableValueBase<T> *>(configurablePtr) = render::ConfigurableValueBase<T>(str);
		}

		template<class T>
		void EnumHelper<T>::WriteConfigurableDefault(void *configurablePtr)
		{
			*static_cast<render::ConfigurableValueBase<T> *>(configurablePtr) = render::ConfigurableValueBase<T>();
		}


		template<class T>
		class StringIndexHelper
		{
		};

		template<int TPurpose>
		class StringIndexHelper<render::StringIndex<TPurpose>>
		{
		public:
			static size_t ReadValue(const void *ptr)
			{
				return static_cast<const render::StringIndex<TPurpose> *>(ptr)->GetIndex();
			}

			static void WriteValue(void *ptr, size_t index)
			{
				*static_cast<render::StringIndex<TPurpose> *>(ptr) = render::StringIndex<TPurpose>(index);
			}

			static int GetPurpose()
			{
				return TPurpose;
			}
		};

		template<class T>
		class NumericHelper
		{
		public:
			static double ReadValueFloat(const void *valuePtr)
			{
				return static_cast<double>(*static_cast<const T *>(valuePtr));
			}

			static void WriteValueFloatFunc(void *valuePtr, double value)
			{
				*static_cast<T *>(valuePtr) = static_cast<T>(value);
			}

			static uint64_t ReadValueUInt(const void *valuePtr)
			{
				return static_cast<uint64_t>(*static_cast<const T *>(valuePtr));
			}

			static void WriteValueUInt(void *valuePtr, uint64_t value)
			{
				*static_cast<T *>(valuePtr) = static_cast<T>(value);
			}

			static int64_t ReadValueSInt(const void *valuePtr)
			{
				return static_cast<int64_t>(*static_cast<const T *>(valuePtr));
			}

			static void WriteValueSInt(void *valuePtr, int64_t value)
			{
				*static_cast<T *>(valuePtr) = static_cast<T>(value);
			}

			static uint8_t GetConfigurableState(const void *configurablePtr)
			{
				return static_cast<uint8_t>(static_cast<const render::ConfigurableValueBase<T> *>(configurablePtr)->m_state);
			}

			static double ReadConfigurableValueFloatFunc(const void *configurablePtr)
			{
				return static_cast<double>(static_cast<const render::ConfigurableValueBase<T> *>(configurablePtr)->m_u.m_value);
			}

			static uint64_t ReadConfigurableValueUIntFunc(const void *configurablePtr)
			{
				return static_cast<uint64_t>(static_cast<const render::ConfigurableValueBase<T> *>(configurablePtr)->m_u.m_value);
			}

			static int64_t ReadConfigurableValueSIntFunc(const void *configurablePtr)
			{
				return static_cast<int64_t>(static_cast<const render::ConfigurableValueBase<T> *>(configurablePtr)->m_u.m_value);
			}

			static const render::ConfigStringIndex_t &ReadConfigurableName(const void *configurablePtr)
			{
				return static_cast<const render::ConfigurableValueBase<T> *>(configurablePtr)->m_u.m_configName;
			}

			static void WriteConfigurableValueFloat(void *configurablePtr, double value)
			{
				*static_cast<render::ConfigurableValueBase<T> *>(configurablePtr) = render::ConfigurableValueBase<T>(static_cast<T>(value));
			}

			static void WriteConfigurableValueUInt(void *configurablePtr, uint64_t value)
			{
				*static_cast<render::ConfigurableValueBase<T> *>(configurablePtr) = render::ConfigurableValueBase<T>(static_cast<T>(value));
			}

			static void WriteConfigurableValueSInt(void *configurablePtr, int64_t value)
			{
				*static_cast<render::ConfigurableValueBase<T> *>(configurablePtr) = render::ConfigurableValueBase<T>(static_cast<T>(value));
			}

			static void WriteConfigurableName(void *configurablePtr, const render::ConfigStringIndex_t &str)
			{
				*static_cast<render::ConfigurableValueBase<T> *>(configurablePtr) = render::ConfigurableValueBase<T>(str);
			}

			static void WriteConfigurableDefault(void *configurablePtr)
			{
				*static_cast<render::ConfigurableValueBase<T> *>(configurablePtr) = render::ConfigurableValueBase<T>();
			}
		};

		template<>
		class NumericHelper<bool>
		{
		public:
			static double ReadValueFloat(const void *valuePtr)
			{
				return (*static_cast<const bool *>(valuePtr)) ? 1 : 0;
			}

			static void WriteValueFloatFunc(void *valuePtr, double value)
			{
				*static_cast<bool *>(valuePtr) = (value != 0);
			}

			static uint64_t ReadValueUInt(const void *valuePtr)
			{
				return (*static_cast<const bool *>(valuePtr)) ? 1 : 0;
			}

			static void WriteValueUInt(void *valuePtr, uint64_t value)
			{
				*static_cast<bool *>(valuePtr) = (value != 0);
			}

			static int64_t ReadValueSInt(const void *valuePtr)
			{
				return (*static_cast<const bool *>(valuePtr)) ? 1 : 0;
			}

			static void WriteValueSInt(void *valuePtr, int64_t value)
			{
				*static_cast<bool *>(valuePtr) = (value != 0);
			}

			static uint8_t GetConfigurableState(const void *configurablePtr)
			{
				return static_cast<uint8_t>(static_cast<const render::ConfigurableValueBase<bool> *>(configurablePtr)->m_state);
			}

			static double ReadConfigurableValueFloatFunc(const void *configurablePtr)
			{
				return (static_cast<const render::ConfigurableValueBase<bool> *>(configurablePtr)->m_u.m_value) ? 1 : 0;
			}

			static uint64_t ReadConfigurableValueUIntFunc(const void *configurablePtr)
			{
				return (static_cast<const render::ConfigurableValueBase<bool> *>(configurablePtr)->m_u.m_value) ? 1 : 0;
			}

			static int64_t ReadConfigurableValueSIntFunc(const void *configurablePtr)
			{
				return (static_cast<const render::ConfigurableValueBase<bool> *>(configurablePtr)->m_u.m_value) ? 1 : 0;
			}

			static const render::ConfigStringIndex_t &ReadConfigurableName(const void *configurablePtr)
			{
				return static_cast<const render::ConfigurableValueBase<bool> *>(configurablePtr)->m_u.m_configName;
			}

			static void WriteConfigurableValueFloat(void *configurablePtr, double value)
			{
				*static_cast<render::ConfigurableValueBase<bool> *>(configurablePtr) = render::ConfigurableValueBase<bool>(value != 0);
			}

			static void WriteConfigurableValueUInt(void *configurablePtr, uint64_t value)
			{
				*static_cast<render::ConfigurableValueBase<bool> *>(configurablePtr) = render::ConfigurableValueBase<bool>(value != 0);
			}

			static void WriteConfigurableValueSInt(void *configurablePtr, int64_t value)
			{
				*static_cast<render::ConfigurableValueBase<bool> *>(configurablePtr) = render::ConfigurableValueBase<bool>(value != 0);
			}

			static void WriteConfigurableName(void *configurablePtr, const render::ConfigStringIndex_t &str)
			{
				*static_cast<render::ConfigurableValueBase<bool> *>(configurablePtr) = render::ConfigurableValueBase<bool>(str);
			}

			static void WriteConfigurableDefault(void *configurablePtr)
			{
				*static_cast<render::ConfigurableValueBase<bool> *>(configurablePtr) = render::ConfigurableValueBase<bool>();
			}
		};

		template<class T, RenderRTTINumberBitSize TBitSize, RenderRTTINumberRepresentation TRepresentation>
		class RTTIAutoNumber
		{
		private:
			static const RenderRTTINumberType ms_type;

		public:
			static const RenderRTTITypeBase *GetRTTIType()
			{
				return &ms_type.m_base;
			}
		};

		template<class T, RenderRTTINumberBitSize TBitSize, RenderRTTINumberRepresentation TRepresentation>
		const RenderRTTINumberType RTTIAutoNumber<T, TBitSize, TRepresentation>::ms_type =
		{
			{ RenderRTTIType::Number },

			TBitSize,
			TRepresentation,

			NumericHelper<T>::ReadValueFloat,
			NumericHelper<T>::WriteValueFloatFunc,
			NumericHelper<T>::ReadValueUInt,
			NumericHelper<T>::WriteValueUInt,
			NumericHelper<T>::ReadValueSInt,
			NumericHelper<T>::WriteValueSInt,
			NumericHelper<T>::GetConfigurableState,

			NumericHelper<T>::ReadConfigurableValueFloatFunc,
			NumericHelper<T>::ReadConfigurableValueUIntFunc,
			NumericHelper<T>::ReadConfigurableValueSIntFunc,
			NumericHelper<T>::ReadConfigurableName,

			NumericHelper<T>::WriteConfigurableValueFloat,
			NumericHelper<T>::WriteConfigurableValueUInt,
			NumericHelper<T>::WriteConfigurableValueSInt,
			NumericHelper<T>::WriteConfigurableName,
			NumericHelper<T>::WriteConfigurableDefault,
		};

		template<>
		struct RTTIResolver<bool> : public RTTIAutoNumber<bool, RenderRTTINumberBitSize::BitSize1, RenderRTTINumberRepresentation::UnsignedInt>
		{
		};

		template<>
		struct RTTIResolver<float> : public RTTIAutoNumber<float, RenderRTTINumberBitSize::BitSize32, RenderRTTINumberRepresentation::Float>
		{
		};

		template<>
		struct RTTIResolver<double> : public RTTIAutoNumber<double, RenderRTTINumberBitSize::BitSize64, RenderRTTINumberRepresentation::Float>
		{
		};

		template<>
		struct RTTIResolver<uint8_t> : public RTTIAutoNumber<uint8_t, RenderRTTINumberBitSize::BitSize8, RenderRTTINumberRepresentation::UnsignedInt>
		{
		};

		template<>
		struct RTTIResolver<uint16_t> : public RTTIAutoNumber<uint16_t, RenderRTTINumberBitSize::BitSize16, RenderRTTINumberRepresentation::UnsignedInt>
		{
		};

		template<>
		struct RTTIResolver<uint32_t> : public RTTIAutoNumber<uint32_t, RenderRTTINumberBitSize::BitSize32, RenderRTTINumberRepresentation::UnsignedInt>
		{
		};

		template<>
		struct RTTIResolver<uint64_t> : public RTTIAutoNumber<uint64_t, RenderRTTINumberBitSize::BitSize64, RenderRTTINumberRepresentation::UnsignedInt>
		{
		};

		template<>
		struct RTTIResolver<int8_t> : public RTTIAutoNumber<int8_t, RenderRTTINumberBitSize::BitSize8, RenderRTTINumberRepresentation::SignedInt>
		{
		};

		template<>
		struct RTTIResolver<int16_t> : public RTTIAutoNumber<int16_t, RenderRTTINumberBitSize::BitSize16, RenderRTTINumberRepresentation::SignedInt>
		{
		};

		template<>
		struct RTTIResolver<int32_t> : public RTTIAutoNumber<int32_t, RenderRTTINumberBitSize::BitSize32, RenderRTTINumberRepresentation::SignedInt>
		{
		};

		template<>
		struct RTTIResolver<int64_t> : public RTTIAutoNumber<int64_t, RenderRTTINumberBitSize::BitSize64, RenderRTTINumberRepresentation::SignedInt>
		{
		};

		struct RenderRTTIValueType
		{
		};

		template<>
		struct RTTIResolver<render::ValueType>
		{
			static const RenderRTTITypeBase *GetRTTIType()
			{
				return &ms_type;
			}

		private:
			static RenderRTTITypeBase ms_type;
		};

		RenderRTTITypeBase RTTIResolver<render::ValueType>::ms_type =
		{
			RenderRTTIType::ValueType,

			RenderRTTIMainType::ValueType,

			"ValueType",
			sizeof("ValueType") - 1,
		};

		RTTI_DEFINE_STRING_INDEX(Global)
		RTTI_DEFINE_STRING_INDEX(Config)
		RTTI_DEFINE_STRING_INDEX(Temp)

		RTTI_ENUM_BEGIN(Filter)
			RTTI_ENUM_OPTION(Nearest)
			RTTI_ENUM_OPTION(Linear)
		RTTI_ENUM_END

		RTTI_ENUM_BEGIN(MipMapMode)
			RTTI_ENUM_OPTION(Nearest)
			RTTI_ENUM_OPTION(Linear)
		RTTI_ENUM_END

		RTTI_ENUM_BEGIN(AddressMode)
			RTTI_ENUM_OPTION(Repeat)
			RTTI_ENUM_OPTION(MirrorRepeat)
			RTTI_ENUM_OPTION(ClampEdge)
			RTTI_ENUM_OPTION(ClampBorder)
		RTTI_ENUM_END

		RTTI_ENUM_BEGIN(AnisotropicFiltering)
			RTTI_ENUM_OPTION(Disabled)
			RTTI_ENUM_OPTION(Anisotropic1)
			RTTI_ENUM_OPTION(Anisotropic2)
			RTTI_ENUM_OPTION(Anisotropic4)
			RTTI_ENUM_OPTION(Anisotropic8)
			RTTI_ENUM_OPTION(Anisotropic16)
		RTTI_ENUM_END

		RTTI_ENUM_BEGIN(ComparisonFunction)
			RTTI_ENUM_OPTION(Disabled)
			RTTI_ENUM_OPTION(Never)
			RTTI_ENUM_OPTION(Less)
			RTTI_ENUM_OPTION(Equal)
			RTTI_ENUM_OPTION(LessOrEqual)
			RTTI_ENUM_OPTION(Greater)
			RTTI_ENUM_OPTION(NotEqual)
			RTTI_ENUM_OPTION(GreaterOrEqual)
			RTTI_ENUM_OPTION(Always)
		RTTI_ENUM_END

		RTTI_ENUM_BEGIN(BorderColor)
			RTTI_ENUM_OPTION(TransparentBlack)
			RTTI_ENUM_OPTION(OpaqueBlack)
			RTTI_ENUM_OPTION(OpaqueWhite)
		RTTI_ENUM_END

		RTTI_ENUM_BEGIN(VectorDimension)
			RTTI_ENUM_OPTION(Dimension1)
			RTTI_ENUM_OPTION(Dimension2)
			RTTI_ENUM_OPTION(Dimension3)
			RTTI_ENUM_OPTION(Dimension4)
		RTTI_ENUM_END

		RTTI_ENUM_BEGIN(NumericType)
			RTTI_ENUM_OPTION(Float16)
			RTTI_ENUM_OPTION(Float32)
			RTTI_ENUM_OPTION(Float64)
			RTTI_ENUM_OPTION(SInt8)
			RTTI_ENUM_OPTION(SInt16)
			RTTI_ENUM_OPTION(SInt32)
			RTTI_ENUM_OPTION(SInt64)
			RTTI_ENUM_OPTION(UInt8)
			RTTI_ENUM_OPTION(UInt16)
			RTTI_ENUM_OPTION(UInt32)
			RTTI_ENUM_OPTION(UInt64)
			RTTI_ENUM_OPTION(Bool)
		RTTI_ENUM_END

		RTTI_ENUM_BEGIN(StageVisibility)
			RTTI_ENUM_OPTION(All)
			RTTI_ENUM_OPTION(Vertex)
			RTTI_ENUM_OPTION(Hull)
			RTTI_ENUM_OPTION(Domain)
			RTTI_ENUM_OPTION(Geometry)
			RTTI_ENUM_OPTION(Pixel)
			RTTI_ENUM_OPTION(Amplification)
			RTTI_ENUM_OPTION(Mesh)
		RTTI_ENUM_END

		RTTI_STRUCT_BEGIN(SamplerDesc)
			RTTI_STRUCT_FIELD(minFilter)
			RTTI_STRUCT_FIELD(magFilter)
			RTTI_STRUCT_FIELD(mipMapMode)
			RTTI_STRUCT_FIELD(addressModeU)
			RTTI_STRUCT_FIELD(addressModeV)
			RTTI_STRUCT_FIELD(addressModeW)
			RTTI_STRUCT_FIELD(mipLodBias)
			RTTI_STRUCT_FIELD(minLod)
			RTTI_STRUCT_FIELD(maxLod)
			RTTI_STRUCT_FIELD(anisotropy)
			RTTI_STRUCT_FIELD(compareFunction)
		RTTI_STRUCT_END

		RTTI_STRUCT_BEGIN(PushConstantDesc)
			RTTI_STRUCT_FIELD(type)
			RTTI_STRUCT_FIELD(stageVisibility)
			RTTI_STRUCT_FIELD_INVISIBLE(name)
		RTTI_STRUCT_END

		RTTI_STRUCT_BEGIN(StructureMemberDesc)
			RTTI_STRUCT_FIELD(type)
			RTTI_STRUCT_FIELD(name)
		RTTI_STRUCT_END

		RTTI_ENUM_BEGIN(VertexInputStepping)
			RTTI_ENUM_OPTION(Vertex)
			RTTI_ENUM_OPTION(Instance)
		RTTI_ENUM_END

			/*
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

			*/
	}


	/*
	enum class RenderRTTIType
	{
		Enum,
		Structure,
		Number,
		Array,
	};

	struct RenderEnumOptions
	{
		const char *m_name;
	};

	struct RenderRTTITypeBase
	{
		RenderRTTIType m_type;
	};

	struct RenderRTTIEnumType
	{
		RenderRTTITypeBase m_base;

		const char *m_name;
		size_t m_nameLength;

		const RenderEnumOptions *m_fields;
		unsigned int m_numFields;

		unsigned int (*m_readValueFunc)(const void *valuePtr);
		void (*m_writeValueFunc)(void *valuePtr, unsigned int value);
	};

	struct RenderRTTIStructField
	{
		const char *m_name;
		RenderRTTIType *m_type;
	};

	struct RenderRTTIStructType
	{
		RenderRTTITypeBase m_base;

		const char *m_name;

		const RenderRTTIStructField *m_fields;
		size_t m_numFields;
	};

	enum class RenderRTTINumberBitSize
	{
		BitSize1,

		BitSize8,
		BitSize16,
		BitSize32,
		BitSize64,
	};

	enum class RenderRTTINumberRepresentation
	{
		Float,
		SignedInt,
		UnsignedInt,
	};

	struct RenderRTTINumberType
	{
		RenderRTTITypeBase m_base;

		RenderRTTINumberBitSize m_bitSize;
		RenderRTTINumberRepresentation m_representation;

		double (*m_readValueFloatFunc)(const void *valuePtr);
		void (*m_writeValueFloatFunc)(void *valuePtr, double value);

		uint64_t(*m_readValueUIntFunc)(const void *valuePtr);
		void (*m_writeValueUIntFunc)(void *valuePtr, uint64_t value);

		int64_t(*m_readValueSIntFunc)(const void *valuePtr);
		void (*m_writeValueSIntFunc)(void *valuePtr, int64_t value);
	};

	struct IRenderDataHandler
	{
		virtual ~IRenderDataHandler() {}

		virtual const RenderRTTITypeBase *GetStaticSamplerRTTI() const = 0;
	};
	*/


	const RenderRTTIStructType *RenderDataHandler::GetSamplerDescRTTI() const
	{
		return reinterpret_cast<const RenderRTTIStructType *>(render_rtti::RTTIResolver<render::SamplerDesc>::GetRTTIType());
	}

	const RenderRTTIStructType *RenderDataHandler::GetPushConstantDescRTTI() const
	{
		return reinterpret_cast<const RenderRTTIStructType *>(render_rtti::RTTIResolver<render::PushConstantDesc>::GetRTTIType());
	}

	const RenderRTTIEnumType *RenderDataHandler::GetVertexInputSteppingRTTI() const
	{
		return reinterpret_cast<const RenderRTTIEnumType *>(render_rtti::RTTIResolver<render::VertexInputStepping>::GetRTTIType());
	}
}
