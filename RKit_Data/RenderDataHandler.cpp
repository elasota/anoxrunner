#include "RenderDataHandler.h"

#include "rkit/Render/RenderDefs.h"

#include "rkit/Core/FourCC.h"
#include "rkit/Core/Vector.h"

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

#define RTTI_STRUCT_BEGIN_WITH_INDEX(name, indexableStructType)	\
	namespace render_rtti_ ## name\
	{\
		typedef render::name StructType_t;\
		const RenderRTTIMainType g_mainType = RenderRTTIMainType::name;\
		extern RenderRTTIStructType g_type;\
		const RenderRTTIIndexableStructType g_indexableStructType = RenderRTTIIndexableStructType::indexableStructType;\
	}\
	template<> struct RTTIResolver<render_rtti_ ## name::StructType_t>\
	{\
		static const RenderRTTITypeBase *GetRTTIType()\
		{\
			return &render_rtti_ ## name::g_type.m_base;\
		}\
		static const bool kIsIndexable = (RenderRTTIIndexableStructType::indexableStructType != RenderRTTIIndexableStructType::NotIndexable);\
	};\
	namespace render_rtti_ ## name\
	{\
		const char g_name[] = #name;\
		RenderRTTIStructField g_fields[] =\
		{

#define RTTI_STRUCT_BEGIN(name) RTTI_STRUCT_BEGIN_WITH_INDEX(name, NotIndexable)

#define RTTI_STRUCT_BEGIN_INDEXABLE(name) RTTI_STRUCT_BEGIN_WITH_INDEX(name, name)


#define RTTI_STRUCT_FIELD_WITH_VISIBILITY_AND_NULLABILITY(name, visibility, nullability)	\
	{\
		#name,\
		sizeof(#name) - 1,\
		RTTIResolver<decltype(StructType_t::m_ ## name)>::GetRTTIType,\
		visibility,\
		RTTIConfigurableResolver<decltype(StructType_t::m_ ## name)>::kIsConfigurable,\
		nullability,\
		StructHelper<StructType_t>::GetMemberPtr<decltype(StructType_t::m_ ## name), &StructType_t::m_ ## name>,\
	},

#define RTTI_STRUCT_FIELD(name)	RTTI_STRUCT_FIELD_WITH_VISIBILITY_AND_NULLABILITY(name, true, false)

#define RTTI_STRUCT_FIELD_INVISIBLE_NULLABLE(name)	RTTI_STRUCT_FIELD_WITH_VISIBILITY_AND_NULLABILITY(name, false, true)

#define RTTI_STRUCT_FIELD_INVISIBLE(name)	RTTI_STRUCT_FIELD_WITH_VISIBILITY_AND_NULLABILITY(name, false, false)

#define RTTI_STRUCT_FIELD_NULLABLE(name)	RTTI_STRUCT_FIELD_WITH_VISIBILITY_AND_NULLABILITY(name, true, true)

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
			g_indexableStructType,\
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

			{
				NumericHelper<T>::ReadValueFloat,
				NumericHelper<T>::WriteValueFloatFunc,

				NumericHelper<T>::ReadValueUInt,
				NumericHelper<T>::WriteValueUInt,

				NumericHelper<T>::ReadValueSInt,
				NumericHelper<T>::WriteValueSInt,
			},
			{
				NumericHelper<T>::ReadConfigurableValueFloatFunc,
				NumericHelper<T>::WriteConfigurableValueFloat,

				NumericHelper<T>::ReadConfigurableValueUIntFunc,
				NumericHelper<T>::WriteConfigurableValueUInt,

				NumericHelper<T>::ReadConfigurableValueSIntFunc,
				NumericHelper<T>::WriteConfigurableValueSInt,
			},

			NumericHelper<T>::GetConfigurableState,

			NumericHelper<T>::ReadConfigurableName,

			NumericHelper<T>::WriteConfigurableName,
			NumericHelper<T>::WriteConfigurableDefault,
		};

		template<class T>
		struct RTTIAutoObjectPtr
		{
		private:
			static const RenderRTTIObjectPtrType ms_type;

		public:
			static const RenderRTTITypeBase *GetRTTIType()
			{
				return &ms_type.m_base;
			}

		private:
			static void Set(void *ptrLoc, const void *value)
			{
				static_assert(RTTIResolver<T>::kIsIndexable);
				*static_cast<const T **>(ptrLoc) = static_cast<const T *>(value);
			}

			static const void *Get(const void *ptrLoc)
			{
				static_assert(RTTIResolver<T>::kIsIndexable);
				return *static_cast<T *const*const>(ptrLoc);
			}

			static const RenderRTTIStructType *GetType()
			{
				const RenderRTTITypeBase *baseType = RTTIResolver<T>::GetRTTIType();

				RKIT_ASSERT(baseType->m_type == RenderRTTIType::Structure);
				return reinterpret_cast<const RenderRTTIStructType *>(baseType);
			}
		};

		template<class T>
		const RenderRTTIObjectPtrType RTTIAutoObjectPtr<T>::ms_type =
		{
			{ RenderRTTIType::ObjectPtr },

			RTTIAutoObjectPtr<T>::GetType,
			RTTIAutoObjectPtr<T>::Set,
			RTTIAutoObjectPtr<T>::Get,
		};

		template<class T>
		struct RTTIAutoObjectPtrSpan
		{
		private:
			static const RenderRTTIObjectPtrSpanType ms_type;

		public:
			static const RenderRTTITypeBase *GetRTTIType()
			{
				return &ms_type.m_base;
			}

		private:
			static void Set(void *spanPtr, void *elements, size_t count)
			{
				static_assert(RTTIResolver<T>::kIsIndexable);
				*static_cast<Span<const T *> *>(spanPtr) = Span<const T *>(static_cast<const T **>(elements), count);
			}

			static void Get(const void *spanPtr, void *&outElements, size_t &outCount)
			{
				static_assert(RTTIResolver<T>::kIsIndexable);

				const Span<const T *> *span = static_cast<const Span<const T *>*>(spanPtr);

				outElements = span->Ptr();
				outCount = span->Count();
			}

			static const RenderRTTIObjectPtrType *GetPtrType()
			{
				return reinterpret_cast<const RenderRTTIObjectPtrType *>(RTTIResolver<const T *>::GetRTTIType());
			}
		};

		template<class T>
		const RenderRTTIObjectPtrSpanType RTTIAutoObjectPtrSpan<T>::ms_type =
		{
			{ RenderRTTIType::ObjectPtrSpan },

			sizeof(const T *),

			RTTIAutoObjectPtrSpan<T>::GetPtrType,
			RTTIAutoObjectPtrSpan<T>::Set,
			RTTIAutoObjectPtrSpan<T>::Get,
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

		template<class T>
		struct RTTIResolver<const T *> : public RTTIAutoObjectPtr<T>
		{
		};

		template<class T>
		struct RTTIResolver<Span<const T *>> : public RTTIAutoObjectPtrSpan<T>
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

		template<class T>
		class RenderRTTIList final : public IRenderRTTIListBase
		{
		public:
			Result Resize(size_t count) override
			{
				return m_vector.Resize(count);
			}

			size_t GetCount() const override
			{
				return m_vector.Count();
			}

			void *GetElementPtr(size_t index) override
			{
				return &m_vector[index];
			}

			const void *GetElementPtr(size_t index) const override
			{
				return &m_vector[index];
			}

			size_t GetElementSize() const override
			{
				return sizeof(T);
			}

		private:
			Vector<T> m_vector;
		};

		RTTI_DEFINE_STRING_INDEX(Global)
		RTTI_DEFINE_STRING_INDEX(Config)
		RTTI_DEFINE_STRING_INDEX(Temp)

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

			RTTI_ENUM_OPTION(SNorm8)
			RTTI_ENUM_OPTION(SNorm16)
			RTTI_ENUM_OPTION(SNorm32)

			RTTI_ENUM_OPTION(UNorm8)
			RTTI_ENUM_OPTION(UNorm16)
			RTTI_ENUM_OPTION(UNorm32)
		RTTI_ENUM_END

		RTTI_ENUM_BEGIN(VectorDimension)
			RTTI_ENUM_OPTION(Dimension1)
			RTTI_ENUM_OPTION(Dimension2)
			RTTI_ENUM_OPTION(Dimension3)
			RTTI_ENUM_OPTION(Dimension4)
		RTTI_ENUM_END

		RTTI_STRUCT_BEGIN_INDEXABLE(VectorNumericType)
			RTTI_STRUCT_FIELD(numericType)
			RTTI_STRUCT_FIELD(cols)
		RTTI_STRUCT_END

		RTTI_STRUCT_BEGIN_INDEXABLE(CompoundNumericType)
			RTTI_STRUCT_FIELD(numericType)
			RTTI_STRUCT_FIELD(rows)
			RTTI_STRUCT_FIELD(cols)
		RTTI_STRUCT_END

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

		RTTI_STRUCT_BEGIN_INDEXABLE(SamplerDesc)
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

		RTTI_ENUM_BEGIN(DescriptorType)
			RTTI_ENUM_OPTION(Sampler)

			RTTI_ENUM_OPTION(StaticConstantBuffer)
			RTTI_ENUM_OPTION(DynamicConstantBuffer)

			RTTI_ENUM_OPTION(Buffer)
			RTTI_ENUM_OPTION(RWBuffer)

			RTTI_ENUM_OPTION(ByteAddressBuffer)
			RTTI_ENUM_OPTION(RWByteAddressBuffer)

			RTTI_ENUM_OPTION(Texture1D)
			RTTI_ENUM_OPTION(Texture1DArray)
			RTTI_ENUM_OPTION(Texture2D)
			RTTI_ENUM_OPTION(Texture2DArray)
			RTTI_ENUM_OPTION(Texture2DMS)
			RTTI_ENUM_OPTION(Texture2DMSArray)
			RTTI_ENUM_OPTION(Texture3D)
			RTTI_ENUM_OPTION(TextureCube)
			RTTI_ENUM_OPTION(TextureCubeArray)

			RTTI_ENUM_OPTION(RWTexture1D)
			RTTI_ENUM_OPTION(RWTexture1DArray)
			RTTI_ENUM_OPTION(RWTexture2D)
			RTTI_ENUM_OPTION(RWTexture2DArray)
			RTTI_ENUM_OPTION(RWTexture3D)
		RTTI_ENUM_END

		RTTI_STRUCT_BEGIN_INDEXABLE(DescriptorDesc)
			RTTI_STRUCT_FIELD_INVISIBLE(name)
			RTTI_STRUCT_FIELD(visibility)
			RTTI_STRUCT_FIELD(descriptorType)
			RTTI_STRUCT_FIELD(arraySize)
			RTTI_STRUCT_FIELD(globallyCoherent)
			RTTI_STRUCT_FIELD(valueType)
			RTTI_STRUCT_FIELD(staticSamplerDesc)
		RTTI_STRUCT_END

		RTTI_STRUCT_BEGIN_INDEXABLE(PushConstantDesc)
			RTTI_STRUCT_FIELD(type)
			RTTI_STRUCT_FIELD(stageVisibility)
			RTTI_STRUCT_FIELD_INVISIBLE(name)
		RTTI_STRUCT_END

		RTTI_STRUCT_BEGIN_INDEXABLE(PushConstantListDesc)
			RTTI_STRUCT_FIELD(pushConstants)
		RTTI_STRUCT_END

		RTTI_ENUM_BEGIN(InputLayoutVertexInputStepping)
			RTTI_ENUM_OPTION(Vertex)
			RTTI_ENUM_OPTION(Instance)
		RTTI_ENUM_END

		RTTI_STRUCT_BEGIN_INDEXABLE(InputLayoutVertexInputDesc)
			RTTI_STRUCT_FIELD(feedName)
			RTTI_STRUCT_FIELD(memberName)
			RTTI_STRUCT_FIELD(inputSlot)
			RTTI_STRUCT_FIELD(byteOffset)
			RTTI_STRUCT_FIELD(numericType)
			RTTI_STRUCT_FIELD(stepping)
		RTTI_STRUCT_END

		RTTI_STRUCT_BEGIN_INDEXABLE(InputLayoutDesc)
			RTTI_STRUCT_FIELD(vertexFeeds)
		RTTI_STRUCT_END

		RTTI_STRUCT_BEGIN_INDEXABLE(StructureMemberDesc)
			RTTI_STRUCT_FIELD(type)
			RTTI_STRUCT_FIELD(name)
		RTTI_STRUCT_END

		RTTI_STRUCT_BEGIN_INDEXABLE(StructureType)
			RTTI_STRUCT_FIELD(members)
		RTTI_STRUCT_END

		RTTI_STRUCT_BEGIN_INDEXABLE(DescriptorLayoutDesc)
			RTTI_STRUCT_FIELD(descriptors)
		RTTI_STRUCT_END

		RTTI_ENUM_BEGIN(IndexSize)
			RTTI_ENUM_OPTION(UInt16)
			RTTI_ENUM_OPTION(UInt32)
		RTTI_ENUM_END

		RTTI_ENUM_BEGIN(PrimitiveTopology)
			RTTI_ENUM_OPTION(PointList)
			RTTI_ENUM_OPTION(LineList)
			RTTI_ENUM_OPTION(LineStrip)
			RTTI_ENUM_OPTION(TriangleList)
			RTTI_ENUM_OPTION(TriangleStrip)
		RTTI_ENUM_END

		RTTI_ENUM_BEGIN(FillMode)
			RTTI_ENUM_OPTION(Wireframe)
			RTTI_ENUM_OPTION(Solid)
		RTTI_ENUM_END

		RTTI_ENUM_BEGIN(CullMode)
			RTTI_ENUM_OPTION(None)
			RTTI_ENUM_OPTION(Front)
			RTTI_ENUM_OPTION(Back)
		RTTI_ENUM_END

		RTTI_ENUM_BEGIN(DepthStencilFormat)
			RTTI_ENUM_OPTION(DepthFloat32)
			RTTI_ENUM_OPTION(DepthFloat32_Stencil8_Undefined24)
			RTTI_ENUM_OPTION(DepthUNorm24_Stencil8)
			RTTI_ENUM_OPTION(DepthUNorm16)
		RTTI_ENUM_END

		RTTI_ENUM_BEGIN(StencilOp)
			RTTI_ENUM_OPTION(Keep)
			RTTI_ENUM_OPTION(Zero)
			RTTI_ENUM_OPTION(Replace)
			RTTI_ENUM_OPTION(IncrementSaturate)
			RTTI_ENUM_OPTION(DecrementSaturate)
			RTTI_ENUM_OPTION(Invert)
			RTTI_ENUM_OPTION(Increment)
			RTTI_ENUM_OPTION(Decrement)
		RTTI_ENUM_END

		RTTI_STRUCT_BEGIN(StencilOpDesc)
			RTTI_STRUCT_FIELD(passOp)
			RTTI_STRUCT_FIELD(failOp)
			RTTI_STRUCT_FIELD(depthFailOp)
			RTTI_STRUCT_FIELD(compareFunc)
		RTTI_STRUCT_END

		RTTI_STRUCT_BEGIN_INDEXABLE(DepthStencilDesc)
			RTTI_STRUCT_FIELD(depthTest)
			RTTI_STRUCT_FIELD(depthWrite)

			RTTI_STRUCT_FIELD(depthCompareOp)
			RTTI_STRUCT_FIELD(format)

			RTTI_STRUCT_FIELD(stencilTest)
			RTTI_STRUCT_FIELD(stencilWrite)

			RTTI_STRUCT_FIELD(stencilCompareOp)

			RTTI_STRUCT_FIELD(stencilCompareMask)
			RTTI_STRUCT_FIELD(stencilWriteMask)
			RTTI_STRUCT_FIELD(stencilFrontOps)
			RTTI_STRUCT_FIELD(stencilBackOps)
		RTTI_STRUCT_END

		RTTI_STRUCT_BEGIN_INDEXABLE(GraphicsPipelineDesc)
			RTTI_STRUCT_FIELD_INVISIBLE_NULLABLE(pushConstants)

			RTTI_STRUCT_FIELD_INVISIBLE(descriptorLayouts)

			RTTI_STRUCT_FIELD_INVISIBLE(inputLayout)
			RTTI_STRUCT_FIELD_INVISIBLE_NULLABLE(vertexShaderOutput)

			RTTI_STRUCT_FIELD_INVISIBLE(vertexShader)
			RTTI_STRUCT_FIELD_INVISIBLE(pixelShader)

			RTTI_STRUCT_FIELD(indexSize)

			RTTI_STRUCT_FIELD(primitiveRestart)

			RTTI_STRUCT_FIELD(primitiveTopology)

			RTTI_STRUCT_FIELD_INVISIBLE(renderTargets)
			RTTI_STRUCT_FIELD(depthStencil)

			RTTI_STRUCT_FIELD(alphaToCoverage)

			RTTI_STRUCT_FIELD(dynamicBlendConstants)

			RTTI_STRUCT_FIELD(blendConstantRed)
			RTTI_STRUCT_FIELD(blendConstantGreen)
			RTTI_STRUCT_FIELD(blendConstantBlue)
			RTTI_STRUCT_FIELD(blendConstantAlpha)

			RTTI_STRUCT_FIELD(fillMode)
			RTTI_STRUCT_FIELD(cullMode)
		RTTI_STRUCT_END

		RTTI_ENUM_BEGIN(ReadWriteAccess)
			RTTI_ENUM_OPTION(Read)
			RTTI_ENUM_OPTION(Write)
			RTTI_ENUM_OPTION(ReadWrite)
		RTTI_ENUM_END

		RTTI_ENUM_BEGIN(ColorBlendFactor)
			RTTI_ENUM_OPTION(Zero)
			RTTI_ENUM_OPTION(One)
			RTTI_ENUM_OPTION(SrcColor)
			RTTI_ENUM_OPTION(InvSrcColor)
			RTTI_ENUM_OPTION(SrcAlpha)
			RTTI_ENUM_OPTION(InvSrcAlpha)
			RTTI_ENUM_OPTION(DstAlpha)
			RTTI_ENUM_OPTION(InvDstAlpha)

			RTTI_ENUM_OPTION(DstColor)
			RTTI_ENUM_OPTION(InvDstColor)
			RTTI_ENUM_OPTION(ConstantColor)
			RTTI_ENUM_OPTION(InvConstantColor)
			RTTI_ENUM_OPTION(ConstantAlpha)
			RTTI_ENUM_OPTION(InvConstantAlpha)
		RTTI_ENUM_END

		RTTI_ENUM_BEGIN(AlphaBlendFactor)
			RTTI_ENUM_OPTION(Zero)
			RTTI_ENUM_OPTION(One)

			RTTI_ENUM_OPTION(SrcAlpha)
			RTTI_ENUM_OPTION(InvSrcAlpha)
			RTTI_ENUM_OPTION(DstAlpha)
			RTTI_ENUM_OPTION(InvDstAlpha)
			RTTI_ENUM_OPTION(ConstantAlpha)
			RTTI_ENUM_OPTION(InvConstantAlpha)
		RTTI_ENUM_END

		RTTI_ENUM_BEGIN(BlendOp)
			RTTI_ENUM_OPTION(Add)
			RTTI_ENUM_OPTION(Subtract)
			RTTI_ENUM_OPTION(ReverseSubtract)
			RTTI_ENUM_OPTION(Min)
			RTTI_ENUM_OPTION(Max)
		RTTI_ENUM_END

		RTTI_ENUM_BEGIN(RenderTargetFormat)
			RTTI_ENUM_OPTION(RGBA_UNorm8)
			RTTI_ENUM_OPTION(RGBA_UNorm8_sRGB)
		RTTI_ENUM_END

		RTTI_STRUCT_BEGIN_INDEXABLE(RenderTargetDesc)
			RTTI_STRUCT_FIELD_INVISIBLE(name)

			RTTI_STRUCT_FIELD(format)
			RTTI_STRUCT_FIELD(access)

			RTTI_STRUCT_FIELD(srcBlend)
			RTTI_STRUCT_FIELD(dstBlend)
			RTTI_STRUCT_FIELD(colorBlendOp)

			RTTI_STRUCT_FIELD(srcAlphaBlend)
			RTTI_STRUCT_FIELD(dstAlphaBlend)
			RTTI_STRUCT_FIELD(alphaBlendOp)

			RTTI_STRUCT_FIELD(writeRed)
			RTTI_STRUCT_FIELD(writeGreen)
			RTTI_STRUCT_FIELD(writeBlue)
			RTTI_STRUCT_FIELD(writeAlpha)
		RTTI_STRUCT_END

		RTTI_STRUCT_BEGIN(ContentKey)
			RTTI_STRUCT_FIELD(key0)
			RTTI_STRUCT_FIELD(key1)
			RTTI_STRUCT_FIELD(key2)
			RTTI_STRUCT_FIELD(key3)
		RTTI_STRUCT_END

		RTTI_STRUCT_BEGIN_INDEXABLE(ShaderDesc)
			RTTI_STRUCT_FIELD(source)
			RTTI_STRUCT_FIELD(entryPoint)

			RTTI_STRUCT_FIELD_INVISIBLE(contentKey)
		RTTI_STRUCT_END
	}

	const RenderRTTIStructType *RenderDataHandler::GetSamplerDescRTTI() const
	{
		return reinterpret_cast<const RenderRTTIStructType *>(render_rtti::RTTIResolver<render::SamplerDesc>::GetRTTIType());
	}

	const RenderRTTIStructType *RenderDataHandler::GetPushConstantDescRTTI() const
	{
		return reinterpret_cast<const RenderRTTIStructType *>(render_rtti::RTTIResolver<render::PushConstantDesc>::GetRTTIType());
	}

	const RenderRTTIEnumType *RenderDataHandler::GetInputLayoutVertexInputSteppingRTTI() const
	{
		return reinterpret_cast<const RenderRTTIEnumType *>(render_rtti::RTTIResolver<render::InputLayoutVertexInputStepping>::GetRTTIType());
	}

	const RenderRTTIStructType *RenderDataHandler::GetDescriptorDescRTTI() const
	{
		return reinterpret_cast<const RenderRTTIStructType *>(render_rtti::RTTIResolver<render::DescriptorDesc>::GetRTTIType());
	}

	const RenderRTTIEnumType *RenderDataHandler::GetDescriptorTypeRTTI() const
	{
		return reinterpret_cast<const RenderRTTIEnumType *>(render_rtti::RTTIResolver<render::DescriptorType>::GetRTTIType());
	}

	const RenderRTTIStructType *RenderDataHandler::GetGraphicsPipelineDescRTTI() const
	{
		return reinterpret_cast<const RenderRTTIStructType *>(render_rtti::RTTIResolver<render::GraphicsPipelineDesc>::GetRTTIType());
	}

	const RenderRTTIStructType *RenderDataHandler::GetRenderTargetDescRTTI() const
	{
		return reinterpret_cast<const RenderRTTIStructType *>(render_rtti::RTTIResolver<render::RenderTargetDesc>::GetRTTIType());
	}

	const RenderRTTIStructType *RenderDataHandler::GetShaderDescRTTI() const
	{
		return reinterpret_cast<const RenderRTTIStructType *>(render_rtti::RTTIResolver<render::ShaderDesc>::GetRTTIType());
	}

	const RenderRTTIStructType *RenderDataHandler::GetDepthStencilDescRTTI() const
	{
		return reinterpret_cast<const RenderRTTIStructType *>(render_rtti::RTTIResolver<render::DepthStencilDesc>::GetRTTIType());
	}

	const RenderRTTIEnumType *RenderDataHandler::GetNumericTypeRTTI() const
	{
		return reinterpret_cast<const RenderRTTIEnumType *>(render_rtti::RTTIResolver<render::NumericType>::GetRTTIType());
	}

	const RenderRTTIObjectPtrType *RenderDataHandler::GetVectorNumericTypePtrRTTI() const
	{
		return reinterpret_cast<const RenderRTTIObjectPtrType *>(render_rtti::RTTIResolver<const render::VectorNumericType *>::GetRTTIType());
	}

	const RenderRTTIObjectPtrType *RenderDataHandler::GetCompoundNumericTypePtrRTTI() const
	{
		return reinterpret_cast<const RenderRTTIObjectPtrType *>(render_rtti::RTTIResolver<const render::CompoundNumericType *>::GetRTTIType());
	}

	const RenderRTTIObjectPtrType *RenderDataHandler::GetStructureTypePtrRTTI() const
	{
		return reinterpret_cast<const RenderRTTIObjectPtrType *>(render_rtti::RTTIResolver<const render::StructureType *>::GetRTTIType());
	}

	uint32_t RenderDataHandler::GetPackageVersion() const
	{
		return 1;
	}

	uint32_t RenderDataHandler::GetPackageIdentifier() const
	{
		return RKIT_FOURCC('R', 'P', 'K', 'G');
	}

	Result RenderDataHandler::LoadPackage(IReadStream &stream, bool allowTempStrings, UniquePtr<IRenderDataPackage> &outPackage) const
	{
		return ResultCode::kNotYetImplemented;
	}

#define LINK_INDEXABLE_LIST_TYPE(type)	\
	case RenderRTTIIndexableStructType::type:\
		if (outList)\
		{\
			RKIT_CHECK(New<render_rtti::RenderRTTIList<render::type>>(*outList));\
		}\
		if (outRTTI)\
			*outRTTI = reinterpret_cast<const RenderRTTIStructType *>(render_rtti::RTTIResolver<render::type>::GetRTTIType());\
		return ResultCode::kOK;

	Result RenderDataHandler::ProcessIndexable(RenderRTTIIndexableStructType indexableStructType, UniquePtr<IRenderRTTIListBase> *outList, const RenderRTTIStructType **outRTTI) const
	{
		switch (indexableStructType)
		{
		LINK_INDEXABLE_LIST_TYPE(DepthStencilDesc)
		LINK_INDEXABLE_LIST_TYPE(GraphicsPipelineDesc)
		LINK_INDEXABLE_LIST_TYPE(RenderTargetDesc)
		LINK_INDEXABLE_LIST_TYPE(PushConstantDesc)
		LINK_INDEXABLE_LIST_TYPE(PushConstantListDesc)
		LINK_INDEXABLE_LIST_TYPE(ShaderDesc)
		LINK_INDEXABLE_LIST_TYPE(StructureType)
		LINK_INDEXABLE_LIST_TYPE(StructureMemberDesc)
		LINK_INDEXABLE_LIST_TYPE(InputLayoutDesc)
		LINK_INDEXABLE_LIST_TYPE(DescriptorLayoutDesc)
		LINK_INDEXABLE_LIST_TYPE(DescriptorDesc)
		LINK_INDEXABLE_LIST_TYPE(InputLayoutVertexInputDesc)
		LINK_INDEXABLE_LIST_TYPE(VectorNumericType)
		LINK_INDEXABLE_LIST_TYPE(SamplerDesc)

		default:
			return ResultCode::kInternalError;
		}
	}
}
