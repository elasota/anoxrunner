#include "RenderDataHandler.h"

#include "rkit/Render/RenderDefs.h"

#include "rkit/Core/Algorithm.h"
#include "rkit/Core/FourCC.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/Vector.h"

#include "rkit/Utilities/Sha2.h"

namespace rkit { namespace data
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
			{ RenderRTTIType::Number, RenderRTTIMainType::Invalid },

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
			static void Set(void *spanPtr, const void *elements, size_t count)
			{

				static_assert(RTTIResolver<T>::kIsIndexable);
				*static_cast<ConstSpan<const T *> *>(spanPtr) = ConstSpan<const T *>(static_cast<T const*const*>(elements), count);
			}

			static void Get(const void *spanPtr, const void *&outElements, size_t &outCount)
			{
				static_assert(RTTIResolver<T>::kIsIndexable);

				const ConstSpan<const T *> *span = static_cast<const ConstSpan<const T *>*>(spanPtr);

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
		struct RTTIResolver<ConstSpan<const T *>> : public RTTIAutoObjectPtrSpan<T>
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

		template<>
		struct RTTIResolver<render::BinaryContent>
		{
			static const RenderRTTITypeBase *GetRTTIType()
			{
				return &ms_type;
			}

		private:
			static RenderRTTITypeBase ms_type;
		};

		RenderRTTITypeBase RTTIResolver<render::BinaryContent>::ms_type =
		{
			RenderRTTIType::BinaryContent,

			RenderRTTIMainType::BinaryContent,

			"BinaryContent",
			sizeof("BinaryContent") - 1,
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

		template<class T>
		class RenderRTTIObjectPtrList final : public IRenderRTTIObjectPtrList
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

			const void *GetElement(size_t index) const override
			{
				return m_vector[index];
			}

			const void *GetElementPtr(size_t index) const override
			{
				return &m_vector[index];
			}

			Result Append(const void *ptr) override
			{
				return m_vector.Append(static_cast<const T *>(ptr));
			}

		private:
			Vector<const T *> m_vector;
		};

		RTTI_DEFINE_STRING_INDEX(Global)
		RTTI_DEFINE_STRING_INDEX(Config)
		RTTI_DEFINE_STRING_INDEX(Temp)
		RTTI_DEFINE_STRING_INDEX(ShaderPermutation)

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

			RTTI_ENUM_OPTION(UNorm8)
			RTTI_ENUM_OPTION(UNorm16)
		RTTI_ENUM_END

		RTTI_ENUM_BEGIN(VectorOrScalarDimension)
			RTTI_ENUM_OPTION(Scalar)
			RTTI_ENUM_OPTION(Dimension2)
			RTTI_ENUM_OPTION(Dimension3)
			RTTI_ENUM_OPTION(Dimension4)
		RTTI_ENUM_END

		RTTI_ENUM_BEGIN(VectorDimension)
			RTTI_ENUM_OPTION(Dimension2)
			RTTI_ENUM_OPTION(Dimension3)
			RTTI_ENUM_OPTION(Dimension4)
		RTTI_ENUM_END

		RTTI_STRUCT_BEGIN_INDEXABLE(VectorOrScalarNumericType)
			RTTI_STRUCT_FIELD(numericType)
			RTTI_STRUCT_FIELD(cols)
		RTTI_STRUCT_END

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
			RTTI_ENUM_OPTION(Pixel)
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

		RTTI_STRUCT_BEGIN_INDEXABLE(InputLayoutVertexFeedDesc)
			RTTI_STRUCT_FIELD(feedName)
			RTTI_STRUCT_FIELD(inputSlot)
			RTTI_STRUCT_FIELD(byteStride)
			RTTI_STRUCT_FIELD(stepping)
		RTTI_STRUCT_END

		RTTI_STRUCT_BEGIN_INDEXABLE(InputLayoutVertexInputDesc)
			RTTI_STRUCT_FIELD(inputFeed)
			RTTI_STRUCT_FIELD(memberName)
			RTTI_STRUCT_FIELD(byteOffset)
			RTTI_STRUCT_FIELD(numericType)
		RTTI_STRUCT_END

		RTTI_STRUCT_BEGIN_INDEXABLE(InputLayoutDesc)
			RTTI_STRUCT_FIELD(vertexInputs)
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

		RTTI_STRUCT_BEGIN_INDEXABLE(PipelineLayoutDesc)
			RTTI_STRUCT_FIELD(descriptorLayouts)
			RTTI_STRUCT_FIELD_NULLABLE(pushConstantList)
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

		RTTI_STRUCT_BEGIN_INDEXABLE(DepthStencilOperationDesc)
			RTTI_STRUCT_FIELD(depthTest)
			RTTI_STRUCT_FIELD(depthWrite)
			RTTI_STRUCT_FIELD(depthCompareOp)

			RTTI_STRUCT_FIELD(depthBias)
			RTTI_STRUCT_FIELD(depthBiasClamp)
			RTTI_STRUCT_FIELD(depthBiasSlopeScale)

			RTTI_STRUCT_FIELD(stencilTest)
			RTTI_STRUCT_FIELD(stencilWrite)

			RTTI_STRUCT_FIELD(stencilCompareOp)

			RTTI_STRUCT_FIELD(stencilCompareMask)
			RTTI_STRUCT_FIELD(stencilWriteMask)
			RTTI_STRUCT_FIELD(stencilFrontOps)
			RTTI_STRUCT_FIELD(stencilBackOps)

			RTTI_STRUCT_FIELD(dynamicStencilReference)
			RTTI_STRUCT_FIELD(stencilReference)
		RTTI_STRUCT_END

		RTTI_STRUCT_BEGIN_INDEXABLE(ShaderPermutationTreeBranch)
			RTTI_STRUCT_FIELD(keyValue)
			RTTI_STRUCT_FIELD(subTree)
		RTTI_STRUCT_END

		RTTI_STRUCT_BEGIN_INDEXABLE(ShaderPermutationTree)
			RTTI_STRUCT_FIELD(width)
			RTTI_STRUCT_FIELD(keyName)
			RTTI_STRUCT_FIELD(branches)
		RTTI_STRUCT_END

		RTTI_ENUM_BEGIN(RenderPassLoadOp)
			RTTI_ENUM_OPTION(Discard)
			RTTI_ENUM_OPTION(Clear)
			RTTI_ENUM_OPTION(Load)
		RTTI_ENUM_END

		RTTI_ENUM_BEGIN(RenderPassStoreOp)
			RTTI_ENUM_OPTION(Discard)
			RTTI_ENUM_OPTION(Clear)
			RTTI_ENUM_OPTION(Store)
		RTTI_ENUM_END

		RTTI_ENUM_BEGIN(ImageLayout)
			RTTI_ENUM_OPTION(Undefined)
			RTTI_ENUM_OPTION(RenderTarget)
			RTTI_ENUM_OPTION(PresentSource)
		RTTI_ENUM_END

		RTTI_STRUCT_BEGIN_INDEXABLE(DepthStencilTargetDesc)
			RTTI_STRUCT_FIELD(depthLoadOp)
			RTTI_STRUCT_FIELD(depthStoreOp)
			RTTI_STRUCT_FIELD(stencilLoadOp)
			RTTI_STRUCT_FIELD(stencilStoreOp)
			RTTI_STRUCT_FIELD(format)
		RTTI_STRUCT_END

		RTTI_STRUCT_BEGIN_INDEXABLE(RenderPassDesc)
			RTTI_STRUCT_FIELD_NULLABLE(depthStencilTarget)
			RTTI_STRUCT_FIELD(renderTargets)
			RTTI_STRUCT_FIELD(allowInternalTransitions)
		RTTI_STRUCT_END

		RTTI_STRUCT_BEGIN_INDEXABLE(RenderPassNameLookup)
			RTTI_STRUCT_FIELD(name)

			RTTI_STRUCT_FIELD(renderPass)
		RTTI_STRUCT_END

		RTTI_STRUCT_BEGIN_INDEXABLE(GraphicsPipelineDesc)
			RTTI_STRUCT_FIELD(executeInPass)
			RTTI_STRUCT_FIELD_INVISIBLE(pipelineLayout)

			RTTI_STRUCT_FIELD_INVISIBLE(inputLayout)
			RTTI_STRUCT_FIELD_INVISIBLE_NULLABLE(vertexShaderOutput)

			RTTI_STRUCT_FIELD_INVISIBLE(vertexShader)
			RTTI_STRUCT_FIELD_INVISIBLE(pixelShader)

			RTTI_STRUCT_FIELD_INVISIBLE_NULLABLE(compiledContentKeys)

			RTTI_STRUCT_FIELD(indexSize)

			RTTI_STRUCT_FIELD(primitiveRestart)

			RTTI_STRUCT_FIELD(primitiveTopology)

			RTTI_STRUCT_FIELD(alphaToCoverage)

			RTTI_STRUCT_FIELD(dynamicBlendConstants)

			RTTI_STRUCT_FIELD(blendConstantRed)
			RTTI_STRUCT_FIELD(blendConstantGreen)
			RTTI_STRUCT_FIELD(blendConstantBlue)
			RTTI_STRUCT_FIELD(blendConstantAlpha)

			RTTI_STRUCT_FIELD(fillMode)
			RTTI_STRUCT_FIELD(cullMode)

			RTTI_STRUCT_FIELD_NULLABLE(depthStencil)
			RTTI_STRUCT_FIELD(renderTargets)
		RTTI_STRUCT_END

		RTTI_STRUCT_BEGIN_INDEXABLE(GraphicsPipelineNameLookup)
			RTTI_STRUCT_FIELD(name)
			RTTI_STRUCT_FIELD(pipeline)
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

		RTTI_STRUCT_BEGIN_INDEXABLE(RenderOperationDesc)
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

		RTTI_STRUCT_BEGIN_INDEXABLE(RenderTargetDesc)
			RTTI_STRUCT_FIELD_INVISIBLE(name)

			RTTI_STRUCT_FIELD(loadOp)
			RTTI_STRUCT_FIELD(storeOp)

			RTTI_STRUCT_FIELD(format)
		RTTI_STRUCT_END

		RTTI_STRUCT_BEGIN_INDEXABLE(ContentKey)
			RTTI_STRUCT_FIELD_INVISIBLE(content)
		RTTI_STRUCT_END

		RTTI_STRUCT_BEGIN_INDEXABLE(ShaderDesc)
			RTTI_STRUCT_FIELD(source)
			RTTI_STRUCT_FIELD(entryPoint)
		RTTI_STRUCT_END
	}

	class Package final : public IRenderDataPackage
	{
	public:
		Package();

		IRenderRTTIListBase *GetIndexable(RenderRTTIIndexableStructType indexable) const override;
		const ConfigKey &GetConfigKey(size_t index) const override;
		size_t GetConfigKeyCount() const override;
		StringView GetString(size_t stringIndex) const override;

		size_t GetBinaryContentCount() const override;
		size_t GetBinaryContentSize(size_t binaryContentIndex) const override;
		const utils::Sha256DigestBytes &GetPackageUUID() const override;

		Result Load(const IRenderDataHandler *handler, bool allowTempStrings, IRenderDataConfigurator *configurator, IReadStream &stream);

	private:
		struct StringOffsetAndSize
		{
			size_t m_offset = 0;
			size_t m_size = 0;
		};

		struct ObjectSpanInfo
		{
			size_t m_start = 0;
			size_t m_count = 0;
		};

		static Result ReadUInt8(IReadStream &stream, uint8_t &outValue);
		static Result ReadUInt16(IReadStream &stream, uint16_t &outValue);
		static Result ReadUInt32(IReadStream &stream, uint32_t &outValue);
		static Result ReadUInt64(IReadStream &stream, uint64_t &outValue);

		static Result ReadSInt8(IReadStream &stream, int8_t &outValue);
		static Result ReadSInt16(IReadStream &stream, int16_t &outValue);
		static Result ReadSInt32(IReadStream &stream, int32_t &outValue);
		static Result ReadSInt64(IReadStream &stream, int64_t &outValue);

		static Result ReadFloat32(IReadStream &stream, float &outValue);
		static Result ReadFloat64(IReadStream &stream, double &outValue);

		static Result ReadVariableSizeUInt(IReadStream &stream, uint8_t size, uint64_t &outValue);
		static Result ReadUIntForSize(IReadStream &stream, size_t maxValue, uint64_t &outValue);

		static Result ReadCompactIndex(IReadStream &stream, size_t &outValue);

		static uint64_t DecodeUInt64(uint8_t (&bytes)[8]);

		Result ReadStructure(void *obj, const RenderRTTIStructType *rtti, IReadStream &stream, IRenderDataConfigurator *configurator) const;
		Result ReadObject(void *obj, const RenderRTTITypeBase *rtti, bool isConfigurable, bool isNullable, IReadStream &stream, IRenderDataConfigurator *configurator) const;
		Result ReadEnum(void *obj, const RenderRTTIEnumType *rtti, bool isConfigurable, IReadStream &stream, IRenderDataConfigurator *configurator) const;
		Result ReadNumber(void *obj, const RenderRTTINumberType *rtti, bool isConfigurable, IReadStream &stream, IRenderDataConfigurator *configurator) const;
		Result ReadValueType(void *obj, IReadStream &stream, IRenderDataConfigurator *configurator) const;
		Result ReadStringIndex(void *obj, const data::RenderRTTIStringIndexType *rtti, IReadStream &stream) const;
		Result ReadBinaryContent(void *obj, IReadStream &stream) const;
		Result ReadObjectPtr(void *obj, const data::RenderRTTIObjectPtrType *rtti, bool isNullable, IReadStream &stream) const;
		Result ReadObjectPtrSpan(void *obj, const data::RenderRTTIObjectPtrSpanType *rtti, bool isNullable, IReadStream &stream) const;

		Result ReadConfigurationKey(render::ConfigStringIndex_t &outCfgKey, IReadStream &stream) const;

		Result ValidateStructureType(const render::StructureType *structType, const render::StructureType *upperLimit, size_t depth, size_t &complexity);
		Result ValidateValueType(const render::ValueType &valueType, const render::StructureType *upperLimit, size_t depth, size_t &complexity);

		static const size_t kNumIndexables = static_cast<size_t>(RenderRTTIIndexableStructType::Count);

		Vector<ConfigKey> m_configKeys;
		Vector<StringOffsetAndSize> m_strings;
		Vector<char> m_stringChars;
		Vector<size_t> m_binaryContentSizes;

		bool m_hasTempStrings = false;

		UniquePtr<IRenderRTTIListBase> m_indexables[kNumIndexables];
		UniquePtr<IRenderRTTIObjectPtrList> m_objectPtrs[kNumIndexables];
		Vector<ObjectSpanInfo> m_spanInfos[kNumIndexables];

		utils::Sha256DigestBytes m_uuid = {};
	};

	Package::Package()
	{
	}

	IRenderRTTIListBase *Package::GetIndexable(RenderRTTIIndexableStructType indexable) const
	{
		size_t indexableValue = static_cast<size_t>(indexable);

		RKIT_ASSERT(indexableValue < kNumIndexables);
		return m_indexables[indexableValue].Get();
	}

	const IRenderDataPackage::ConfigKey &Package::GetConfigKey(size_t index) const
	{
		return m_configKeys[index];
	}

	size_t Package::GetConfigKeyCount() const
	{
		return m_configKeys.Count();
	}

	StringView Package::GetString(size_t stringIndex) const
	{
		const StringOffsetAndSize &str = m_strings[stringIndex];

		return StringView(&m_stringChars[str.m_offset], str.m_size);
	}

	size_t Package::GetBinaryContentCount() const
	{
		return m_binaryContentSizes.Count();
	}

	size_t Package::GetBinaryContentSize(size_t binaryContentIndex) const
	{
		return m_binaryContentSizes[binaryContentIndex];
	}

	const utils::Sha256DigestBytes &Package::GetPackageUUID() const
	{
		return m_uuid;
	}

	Result Package::Load(const IRenderDataHandler *handler, bool allowTempStrings, IRenderDataConfigurator *configurator, IReadStream &stream)
	{
		m_hasTempStrings = allowTempStrings;

		for (size_t i = 0; i < kNumIndexables; i++)
		{
			RKIT_CHECK(handler->ProcessIndexable(static_cast<RenderRTTIIndexableStructType>(i), &m_indexables[i], &m_objectPtrs[i], nullptr));
		}

		uint32_t identifier = 0;
		uint32_t packageVersion = 0;

		RKIT_CHECK(ReadUInt32(stream, identifier));
		RKIT_CHECK(ReadUInt32(stream, packageVersion));
		RKIT_CHECK(stream.ReadAll(&m_uuid, sizeof(m_uuid)));

		if (identifier != handler->GetPackageIdentifier())
		{
			rkit::log::Error("Package identifier doesn't match");
			return ResultCode::kMalformedFile;
		}

		if (packageVersion != handler->GetPackageVersion())
		{
			rkit::log::Error("Package version doesn't match");
			return ResultCode::kMalformedFile;
		}

		size_t numStrings = 0;
		size_t numConfigKeys = 0;
		size_t numBinaryContent = 0;

		RKIT_CHECK(ReadCompactIndex(stream, numStrings));
		RKIT_CHECK(ReadCompactIndex(stream, numConfigKeys));
		RKIT_CHECK(ReadCompactIndex(stream, numBinaryContent));

		RKIT_CHECK(m_strings.Resize(numStrings));
		RKIT_CHECK(m_configKeys.Resize(numConfigKeys));
		RKIT_CHECK(m_binaryContentSizes.Resize(numBinaryContent));

		size_t numCharsTotal = 0;

		for (size_t i = 0; i < numStrings; i++)
		{
			size_t stringLength = 0;
			RKIT_CHECK(ReadCompactIndex(stream, stringLength));

			StringOffsetAndSize &str = m_strings[i];
			str.m_offset = numCharsTotal;
			str.m_size = stringLength;

			size_t byteUsage = stringLength + 1;

			RKIT_CHECK(SafeAdd(numCharsTotal, numCharsTotal, byteUsage));
		}

		RKIT_CHECK(m_stringChars.Resize(numCharsTotal));

		if (numCharsTotal > 0)
		{
			RKIT_CHECK(stream.ReadAll(m_stringChars.GetBuffer(), numCharsTotal));

			const char *stringChars = m_stringChars.GetBuffer();
			for (const StringOffsetAndSize &str : m_strings)
			{
				if (stringChars[str.m_offset + str.m_size] != '\0')
				{
					rkit::log::Error("Malformed string data");
					return ResultCode::kMalformedFile;
				}
			}
		}

		for (ConfigKey &configKey : m_configKeys)
		{
			uint64_t mainType = 0;
			RKIT_CHECK(ReadCompactIndex(stream, configKey.m_stringIndex));
			RKIT_CHECK(ReadUIntForSize(stream, static_cast<uint64_t>(data::RenderRTTIMainType::Count) - 1, mainType));

			if (configKey.m_stringIndex >= m_strings.Count())
			{
				rkit::log::Error("Config key string index was invalid");
				return ResultCode::kMalformedFile;
			}

			if (mainType >= static_cast<uint64_t>(data::RenderRTTIMainType::Count))
			{
				rkit::log::Error("Config key main type was invalid");
				return ResultCode::kMalformedFile;
			}
		}

		for (size_t &bcSize : m_binaryContentSizes)
		{
			RKIT_CHECK(ReadCompactIndex(stream, bcSize));
		}

		for (size_t i = 0; i < kNumIndexables; i++)
		{
			size_t objectSpanCount = 0;
			RKIT_CHECK(ReadCompactIndex(stream, objectSpanCount));

			size_t indexableCount = 0;
			RKIT_CHECK(ReadCompactIndex(stream, indexableCount));

			RKIT_CHECK(m_indexables[i]->Resize(indexableCount));
			RKIT_CHECK(m_spanInfos[i].Resize(objectSpanCount));
		}

		for (size_t i = 0; i < kNumIndexables; i++)
		{
			IRenderRTTIListBase &objectList = *m_indexables[i];
			IRenderRTTIObjectPtrList &objectPtrList = *m_objectPtrs[i];
			Vector<ObjectSpanInfo> &spanInfoList = m_spanInfos[i];

			for (ObjectSpanInfo &spanInfo : spanInfoList)
			{
				size_t objectCount = 0;
				RKIT_CHECK(ReadCompactIndex(stream, objectCount));

				spanInfo.m_start = objectPtrList.GetCount();
				spanInfo.m_count = objectCount;

				for (size_t i = 0; i < objectCount; i++)
				{
					size_t objectIndex = 0;
					RKIT_CHECK(ReadCompactIndex(stream, objectIndex));
					if (objectIndex == 0)
					{
						RKIT_CHECK(objectPtrList.Append(nullptr));
					}
					else
					{
						objectIndex--;
						if (objectIndex >= objectList.GetCount())
						{
							rkit::log::Error("Invalid object index");
							return ResultCode::kMalformedFile;
						}

						RKIT_CHECK(objectPtrList.Append(objectList.GetElementPtr(objectIndex)));
					}
				}
			}
		}

		for (size_t i = 0; i < kNumIndexables; i++)
		{
			const RenderRTTIStructType *structType = nullptr;
			RKIT_CHECK(handler->ProcessIndexable(static_cast<RenderRTTIIndexableStructType>(i), nullptr, nullptr, &structType));

			IRenderRTTIListBase &objectList = *m_indexables[i];
			size_t count = objectList.GetCount();

			for (size_t j = 0; j < count; j++)
			{
				void *elementData = objectList.GetElementPtr(j);

				RKIT_CHECK(ReadStructure(elementData, structType, stream, configurator));
			}
		}

		IRenderRTTIListBase *structList = GetIndexable(RenderRTTIIndexableStructType::StructureType);
		size_t numStructs = structList->GetCount();
		for (size_t i = 0; i < numStructs; i++)
		{
			size_t complexity = 0;
			RKIT_CHECK(ValidateStructureType(static_cast<const render::StructureType *>(structList->GetElementPtr(i)), nullptr, 0, complexity));
		}

		return ResultCode::kOK;
	}

	Result Package::ReadUInt8(IReadStream &stream, uint8_t &outValue)
	{
		return stream.ReadAll(&outValue, 1);
	}

	Result Package::ReadUInt16(IReadStream &stream, uint16_t &outValue)
	{
		uint64_t value = 0;
		RKIT_CHECK(ReadVariableSizeUInt(stream, 2, value));
		outValue = static_cast<uint16_t>(value);

		return ResultCode::kOK;
	}

	Result Package::ReadUInt32(IReadStream &stream, uint32_t &outValue)
	{
		uint64_t value = 0;
		RKIT_CHECK(ReadVariableSizeUInt(stream, 4, value));
		outValue = static_cast<uint32_t>(value);

		return ResultCode::kOK;
	}

	Result Package::ReadUInt64(IReadStream &stream, uint64_t &outValue)
	{
		return ReadVariableSizeUInt(stream, 8, outValue);
	}

	Result Package::ReadSInt8(IReadStream &stream, int8_t &outValue)
	{
		return stream.ReadAll(&outValue, 1);
	}

	Result Package::ReadSInt16(IReadStream &stream, int16_t &outValue)
	{
		uint16_t value = 0;
		RKIT_CHECK(ReadUInt16(stream, value));
		memcpy(&outValue, &value, sizeof(value));

		return ResultCode::kOK;
	}

	Result Package::ReadSInt32(IReadStream &stream, int32_t &outValue)
	{
		uint32_t value = 0;
		RKIT_CHECK(ReadUInt32(stream, value));
		memcpy(&outValue, &value, sizeof(value));

		return ResultCode::kOK;
	}

	Result Package::ReadSInt64(IReadStream &stream, int64_t &outValue)
	{
		uint64_t value = 0;
		RKIT_CHECK(ReadUInt64(stream, value));
		memcpy(&outValue, &value, sizeof(value));

		return ResultCode::kOK;
	}

	Result Package::ReadFloat32(IReadStream &stream, float &outValue)
	{
		uint32_t value = 0;
		RKIT_CHECK(ReadUInt32(stream, value));
		memcpy(&outValue, &value, sizeof(value));

		return ResultCode::kOK;
	}

	Result Package::ReadFloat64(IReadStream &stream, double &outValue)
	{
		uint64_t value = 0;
		RKIT_CHECK(ReadUInt64(stream, value));
		memcpy(&outValue, &value, sizeof(value));

		return ResultCode::kOK;
	}


	Result Package::ReadVariableSizeUInt(IReadStream &stream, uint8_t size, uint64_t &outValue)
	{
		uint8_t bytes[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		RKIT_CHECK(stream.ReadAll(bytes, size));
		outValue = DecodeUInt64(bytes);

		return ResultCode::kOK;
	}

	Result Package::ReadUIntForSize(IReadStream &stream, size_t maxValue, uint64_t &outValue)
	{
		if (maxValue > 0xffffffffu)
			return ReadVariableSizeUInt(stream, 8, outValue);
		if (maxValue > 0xffffu)
			return ReadVariableSizeUInt(stream, 4, outValue);
		if (maxValue > 0xffu)
			return ReadVariableSizeUInt(stream, 2, outValue);
		return ReadVariableSizeUInt(stream, 1, outValue);
	}

	Result Package::ReadCompactIndex(IReadStream &stream, size_t &outValue)
	{
		uint8_t bytes[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

		RKIT_CHECK(stream.ReadAll(bytes, 1));

		switch (bytes[0] & 3)
		{
		case 0:
			break;
		case 1:
			RKIT_CHECK(stream.ReadAll(bytes + 1, 1));
			break;
		case 2:
			RKIT_CHECK(stream.ReadAll(bytes + 1, 3));
			break;
		case 3:
			RKIT_CHECK(stream.ReadAll(bytes + 1, 7));
			break;
		default:
			return ResultCode::kInternalError;
		};

		outValue = (DecodeUInt64(bytes) >> 2) & 0x3fffffffffffffffull;

		return ResultCode::kOK;
	}

	uint64_t Package::DecodeUInt64(uint8_t(&bytes)[8])
	{
		uint64_t result = 0;

		for (size_t i = 0; i < 8; i++)
			result |= static_cast<uint64_t>(static_cast<uint64_t>(bytes[i]) << (i * 8));

		return result;
	}

	Result Package::ReadStructure(void *obj, const RenderRTTIStructType *rtti, IReadStream &stream, IRenderDataConfigurator *configurator) const
	{
		for (size_t i = 0; i < rtti->m_numFields; i++)
		{
			const data::RenderRTTIStructField *field = rtti->m_fields + i;

			void *memberPtr = field->m_getMemberPtrFunc(obj);
			const data::RenderRTTITypeBase *fieldRTTI = field->m_getTypeFunc();

			RKIT_CHECK(ReadObject(memberPtr, fieldRTTI, field->m_isConfigurable, field->m_isNullable, stream, configurator));
		}

		return ResultCode::kOK;
	}

	Result Package::ReadObject(void *obj, const data::RenderRTTITypeBase *rtti, bool isConfigurable, bool isNullable, IReadStream &stream, IRenderDataConfigurator *configurator) const
	{
		switch (rtti->m_type)
		{
		case data::RenderRTTIType::Enum:
			return ReadEnum(obj, reinterpret_cast<const data::RenderRTTIEnumType *>(rtti), isConfigurable, stream, configurator);
		case data::RenderRTTIType::Structure:
			RKIT_ASSERT(!isConfigurable);
			return ReadStructure(obj, reinterpret_cast<const data::RenderRTTIStructType *>(rtti), stream, configurator);
		case data::RenderRTTIType::Number:
			return ReadNumber(obj, reinterpret_cast<const data::RenderRTTINumberType *>(rtti), isConfigurable, stream, configurator);
		case data::RenderRTTIType::ValueType:
			RKIT_ASSERT(!isConfigurable);
			return ReadValueType(obj, stream, configurator);
		case data::RenderRTTIType::StringIndex:
			RKIT_ASSERT(!isConfigurable);
			return ReadStringIndex(obj, reinterpret_cast<const data::RenderRTTIStringIndexType *>(rtti), stream);
		case data::RenderRTTIType::ObjectPtr:
			RKIT_ASSERT(!isConfigurable);
			return ReadObjectPtr(obj, reinterpret_cast<const data::RenderRTTIObjectPtrType *>(rtti), isNullable, stream);
		case data::RenderRTTIType::ObjectPtrSpan:
			RKIT_ASSERT(!isConfigurable);
			return ReadObjectPtrSpan(obj, reinterpret_cast<const data::RenderRTTIObjectPtrSpanType *>(rtti), isNullable, stream);
		case data::RenderRTTIType::BinaryContent:
			RKIT_ASSERT(!isConfigurable);
			return ReadBinaryContent(obj, stream);
		default:
			return ResultCode::kInternalError;
		}
	}

	Result Package::ReadEnum(void *obj, const RenderRTTIEnumType *rtti, bool isConfigurable, IReadStream &stream, IRenderDataConfigurator *configurator) const
	{
		if (isConfigurable)
		{
			uint8_t state = 0;

			RKIT_CHECK(ReadUInt8(stream, state));

			switch (state)
			{
			case static_cast<uint8_t>(render::ConfigurableValueState::Default):
				rtti->m_writeConfigurableDefaultFunc(obj);
				return ResultCode::kOK;
			case static_cast<uint8_t>(render::ConfigurableValueState::Configured):
				{
					render::ConfigStringIndex_t cfgKey;
					RKIT_CHECK(ReadConfigurationKey(cfgKey, stream));

					bool isConfiguredValue = false;
					unsigned int configuredValue = 0;
					if (configurator)
					{
						RKIT_CHECK(configurator->GetEnumConfigKey(cfgKey.GetIndex(), this->GetString(m_configKeys[cfgKey.GetIndex()].m_stringIndex), rtti->m_base.m_mainType, configuredValue));
						rtti->m_writeConfigurableValueFunc(obj, configuredValue);
					}
					else
						rtti->m_writeConfigurableNameFunc(obj, cfgKey);
				}
				return ResultCode::kOK;
			case static_cast<uint8_t>(render::ConfigurableValueState::Explicit):
				{
					uint64_t enumValue = 0;
					RKIT_CHECK(ReadUIntForSize(stream, rtti->m_maxValueExclusive - 1, enumValue));
					if (enumValue >= rtti->m_maxValueExclusive)
					{
						rkit::log::Error("Configurable enum value was out of range");
						return ResultCode::kMalformedFile;
					}

					rtti->m_writeConfigurableValueFunc(obj, static_cast<unsigned int>(enumValue));
				}
				return ResultCode::kOK;
			default:
				rkit::log::Error("Configurable enum state was invalid");
				return ResultCode::kMalformedFile;
			}
		}
		else
		{
			uint64_t enumValue = 0;
			RKIT_CHECK(ReadUIntForSize(stream, rtti->m_maxValueExclusive - 1, enumValue));
			if (enumValue >= rtti->m_maxValueExclusive)
			{
				rkit::log::Error("Enum value was out of range");
				return ResultCode::kMalformedFile;
			}

			rtti->m_writeValueFunc(obj, static_cast<unsigned int>(enumValue));
			return ResultCode::kOK;
		}
	}

	Result Package::ReadNumber(void *obj, const RenderRTTINumberType *rtti, bool isConfigurable, IReadStream &stream, IRenderDataConfigurator *configurator) const
	{
		const data::RenderRTTINumberTypeIOFunctions *ioFuncs = nullptr;

		if (isConfigurable)
		{
			uint8_t state = 0;
			RKIT_CHECK(ReadUInt8(stream, state));

			switch (state)
			{
			case static_cast<uint8_t>(render::ConfigurableValueState::Default):
				rtti->m_writeConfigurableDefaultFunc(obj);
				return ResultCode::kOK;
			case static_cast<uint8_t>(render::ConfigurableValueState::Configured):
				{
					render::ConfigStringIndex_t cfgKey;
					RKIT_CHECK(ReadConfigurationKey(cfgKey, stream));

					if (configurator)
					{
						StringView keyName = GetString(GetConfigKey(cfgKey.GetIndex()).m_stringIndex);

						switch (rtti->m_representation)
						{
						case data::RenderRTTINumberRepresentation::Float:
							{
								double value = 0;
								RKIT_CHECK(configurator->GetFloatConfigKey(cfgKey.GetIndex(), keyName, value));
								rtti->m_configurableFunctions.m_writeValueFloatFunc(obj, value);
							}
							break;
						case data::RenderRTTINumberRepresentation::SignedInt:
							{
								int64_t value = 0;
								RKIT_CHECK(configurator->GetSIntConfigKey(cfgKey.GetIndex(), keyName, value));
								rtti->m_configurableFunctions.m_writeValueSIntFunc(obj, value);
							}
							break;
						case data::RenderRTTINumberRepresentation::UnsignedInt:
							{
								uint64_t value = 0;
								RKIT_CHECK(configurator->GetUIntConfigKey(cfgKey.GetIndex(), keyName, value));
								rtti->m_configurableFunctions.m_writeValueUIntFunc(obj, value);
							}
							break;
						}
					}
					else
						rtti->m_writeConfigurableNameFunc(obj, cfgKey);
				}
				return ResultCode::kOK;
			case static_cast<uint8_t>(render::ConfigurableValueState::Explicit):
				ioFuncs = &rtti->m_configurableFunctions;
				break;
			default:
				rkit::log::Error("Invalid configurable number state");
				return ResultCode::kInternalError;
			}
		}
		else
			ioFuncs = &rtti->m_valueFunctions;

		switch (rtti->m_representation)
		{
		case data::RenderRTTINumberRepresentation::Float:
		{
			if (rtti->m_bitSize == data::RenderRTTINumberBitSize::BitSize32)
			{
				float f = 0.f;
				RKIT_CHECK(ReadFloat32(stream, f));
				ioFuncs->m_writeValueFloatFunc(obj, f);
				return ResultCode::kOK;
			}
			else if (rtti->m_bitSize == data::RenderRTTINumberBitSize::BitSize64)
			{
				double f = 0.0;
				RKIT_CHECK(ReadFloat64(stream, f));
				ioFuncs->m_writeValueFloatFunc(obj, f);
				return ResultCode::kOK;
			}
			else
				return ResultCode::kInternalError;
		}
		break;
		case data::RenderRTTINumberRepresentation::SignedInt:
		{
			int64_t value = ioFuncs->m_readValueSIntFunc(obj);
			if (rtti->m_bitSize == data::RenderRTTINumberBitSize::BitSize8)
			{
				int8_t v = 0;
				RKIT_CHECK(ReadSInt8(stream, v));
				ioFuncs->m_writeValueUIntFunc(obj, v);
				return ResultCode::kOK;
			}
			else if (rtti->m_bitSize == data::RenderRTTINumberBitSize::BitSize16)
			{
				int16_t v = 0;
				RKIT_CHECK(ReadSInt16(stream, v));
				ioFuncs->m_writeValueUIntFunc(obj, v);
				return ResultCode::kOK;
			}
			else if (rtti->m_bitSize == data::RenderRTTINumberBitSize::BitSize32)
			{
				int32_t v = 0;
				RKIT_CHECK(ReadSInt32(stream, v));
				ioFuncs->m_writeValueUIntFunc(obj, v);
				return ResultCode::kOK;
			}
			else if (rtti->m_bitSize == data::RenderRTTINumberBitSize::BitSize64)
			{
				int64_t v = 0;
				RKIT_CHECK(ReadSInt64(stream, v));
				ioFuncs->m_writeValueUIntFunc(obj, v);
				return ResultCode::kOK;
			}
			else
				return ResultCode::kInternalError;
		}
		case data::RenderRTTINumberRepresentation::UnsignedInt:
		{
			uint64_t value = ioFuncs->m_readValueUIntFunc(obj);
			if (rtti->m_bitSize == data::RenderRTTINumberBitSize::BitSize1)
			{
				uint8_t v = 0;
				RKIT_CHECK(ReadUInt8(stream, v));

				if (v >= 2)
				{
					rkit::log::Error("Invalid 1-bit value");
					return ResultCode::kMalformedFile;
				}

				ioFuncs->m_writeValueUIntFunc(obj, v);
				return ResultCode::kOK;
			}
			else if (rtti->m_bitSize == data::RenderRTTINumberBitSize::BitSize8)
			{
				uint8_t v = 0;
				RKIT_CHECK(ReadUInt8(stream, v));
				ioFuncs->m_writeValueUIntFunc(obj, v);
				return ResultCode::kOK;
			}
			else if (rtti->m_bitSize == data::RenderRTTINumberBitSize::BitSize16)
			{
				uint16_t v = 0;
				RKIT_CHECK(ReadUInt16(stream, v));
				ioFuncs->m_writeValueUIntFunc(obj, v);
				return ResultCode::kOK;
			}
			else if (rtti->m_bitSize == data::RenderRTTINumberBitSize::BitSize32)
			{
				uint32_t v = 0;
				RKIT_CHECK(ReadUInt32(stream, v));
				ioFuncs->m_writeValueUIntFunc(obj, v);
				return ResultCode::kOK;
			}
			else if (rtti->m_bitSize == data::RenderRTTINumberBitSize::BitSize64)
			{
				uint64_t v = 0;
				RKIT_CHECK(ReadUInt64(stream, v));
				ioFuncs->m_writeValueUIntFunc(obj, v);
				return ResultCode::kOK;
			}
			else
				return ResultCode::kInternalError;
		}
		default:
			return ResultCode::kInternalError;
		}
	}

	Result Package::ReadValueType(void *obj, IReadStream &stream, IRenderDataConfigurator *configurator) const
	{
		render::ValueType *vt = static_cast<render::ValueType *>(obj);

		uint8_t type = 0;
		RKIT_CHECK(ReadUInt8(stream, type));

		switch (type)
		{
		case static_cast<uint8_t>(render::ValueTypeType::Numeric):
			{
				render::NumericType nt = render::NumericType::UInt8;
				RKIT_CHECK(ReadEnum(&nt, reinterpret_cast<const RenderRTTIEnumType *>(render_rtti::RTTIResolver<render::NumericType>::GetRTTIType()), false, stream, configurator));
				*vt = render::ValueType(nt);
			}
			return ResultCode::kOK;
		case static_cast<uint8_t>(render::ValueTypeType::VectorNumeric):
			{
				const void *ptr = nullptr;
				RKIT_CHECK(ReadObjectPtr(&ptr, reinterpret_cast<const RenderRTTIObjectPtrType *>(render_rtti::RTTIResolver<const render::VectorNumericType *>::GetRTTIType()), false, stream));

				*vt = render::ValueType(static_cast<const render::VectorNumericType *>(ptr));
			}
			return ResultCode::kOK;
		case static_cast<uint8_t>(render::ValueTypeType::CompoundNumeric):
			{
				const void *ptr = nullptr;
				RKIT_CHECK(ReadObjectPtr(&ptr, reinterpret_cast<const RenderRTTIObjectPtrType *>(render_rtti::RTTIResolver<const render::CompoundNumericType *>::GetRTTIType()), false, stream));

				*vt = render::ValueType(static_cast<const render::CompoundNumericType *>(ptr));
			}
			return ResultCode::kOK;
		case static_cast<uint8_t>(render::ValueTypeType::Structure):
			{
				const void *ptr = nullptr;
				RKIT_CHECK(ReadObjectPtr(&ptr, reinterpret_cast<const RenderRTTIObjectPtrType *>(render_rtti::RTTIResolver<const render::StructureType *>::GetRTTIType()), false, stream));

				*vt = render::ValueType(static_cast<const render::StructureType *>(ptr));
			}
			return ResultCode::kOK;
		default:
			rkit::log::Error("Invalid valuetype");
			return ResultCode::kMalformedFile;
		}
	}

	Result Package::ReadStringIndex(void *obj, const data::RenderRTTIStringIndexType *rtti, IReadStream &stream) const
	{
		int purpose = rtti->m_getPurposeFunc();

		if (purpose == render::TempStringIndex_t::kPurpose && !m_hasTempStrings)
			return ResultCode::kOK;

		size_t stringIndex = 0;
		RKIT_CHECK(ReadCompactIndex(stream, stringIndex));

		if (purpose == render::TempStringIndex_t::kPurpose || purpose == render::GlobalStringIndex_t::kPurpose)
			rtti->m_writeStringIndexFunc(obj, stringIndex);
		else
			return ResultCode::kInternalError;

		return ResultCode::kOK;
	}

	Result Package::ReadBinaryContent(void *obj, IReadStream &stream) const
	{
		size_t binaryContentIndex = 0;
		RKIT_CHECK(ReadCompactIndex(stream, binaryContentIndex));

		static_cast<render::BinaryContent *>(obj)->m_contentIndex = binaryContentIndex;

		return ResultCode::kOK;
	}

	Result Package::ReadObjectPtr(void *obj, const data::RenderRTTIObjectPtrType *rtti, bool isNullable, IReadStream &stream) const
	{
		size_t objectIndex = 0;

		RKIT_CHECK(ReadCompactIndex(stream, objectIndex));

		if (isNullable)
		{
			if (objectIndex == 0)
			{
				rtti->m_writeFunc(obj, nullptr);
				return ResultCode::kOK;
			}
			else
				objectIndex--;
		}

		const RenderRTTIStructType *structType = rtti->m_getTypeFunc();

		if (static_cast<size_t>(structType->m_indexableType) >= kNumIndexables)
			return ResultCode::kInternalError;

		const IRenderRTTIListBase *list = m_indexables[static_cast<size_t>(structType->m_indexableType)].Get();
		if (objectIndex >= list->GetCount())
		{
			rkit::log::Error("Object index was out of range");
			return ResultCode::kMalformedFile;
		}

		rtti->m_writeFunc(obj, list->GetElementPtr(objectIndex));

		return ResultCode::kOK;
	}

	Result Package::ReadObjectPtrSpan(void *obj, const data::RenderRTTIObjectPtrSpanType *rtti, bool isNullable, IReadStream &stream) const
	{
		const data::RenderRTTIObjectPtrType *ptrType = rtti->m_getPtrTypeFunc();
		const data::RenderRTTIStructType *structType = ptrType->m_getTypeFunc();

		size_t indexableInt = static_cast<size_t>(structType->m_indexableType);
		if (indexableInt >= kNumIndexables)
			return ResultCode::kInternalError;

		size_t spanIndex = 0;
		RKIT_CHECK(ReadCompactIndex(stream, spanIndex));

		const IRenderRTTIObjectPtrList &ptrList = *m_objectPtrs[indexableInt];
		const Vector<ObjectSpanInfo> &spanInfos = m_spanInfos[indexableInt];

		if (spanIndex >= spanInfos.Count())
		{
			rkit::log::Error("Invalid object span index");
			return ResultCode::kMalformedFile;
		}

		const ObjectSpanInfo &spanInfo = spanInfos[spanIndex];

		size_t count = spanInfo.m_count;
		size_t start = spanInfo.m_start;

		const void *ptrsDataStart = nullptr;
		if (count > 0)
			ptrsDataStart = ptrList.GetElementPtr(start);

		if (!isNullable)
		{
			for (size_t i = 0; i < count; i++)
			{
				if (!ptrList.GetElement(start + i))
				{
					rkit::log::Error("Object ptr was invalid");
					return ResultCode::kMalformedFile;
				}
			}
		}

		rtti->m_setFunc(obj, ptrsDataStart, count);

		return ResultCode::kOK;
	}

	Result Package::ReadConfigurationKey(render::ConfigStringIndex_t &outCfgKey, IReadStream &stream) const
	{
		size_t index = 0;
		RKIT_CHECK(ReadCompactIndex(stream, index));

		if (index >= m_configKeys.Count())
		{
			rkit::log::Error("Configuration key index was out of range");
			return ResultCode::kMalformedFile;
		}

		outCfgKey = render::ConfigStringIndex_t(index);
		return ResultCode::kOK;
	}

	Result Package::ValidateStructureType(const render::StructureType *structType, const render::StructureType *upperLimit, size_t depth, size_t &complexity)
	{
		if (upperLimit != nullptr && structType >= upperLimit)
		{
			rkit::log::Error("Structure type was recursive");
			return ResultCode::kMalformedFile;
		}

		const size_t complexityLimit = 4096;

		if (structType->m_members.Count() >= complexityLimit || complexity + structType->m_members.Count() >= complexityLimit)
		{
			rkit::log::Error("Structure type has too many fields");
			return ResultCode::kMalformedFile;
		}

		complexity += structType->m_members.Count();

		if (depth > 16)
		{
			rkit::log::Error("Structure type tree is too deep");
			return ResultCode::kMalformedFile;
		}

		for (const render::StructureMemberDesc *member : structType->m_members)
		{
			RKIT_CHECK(ValidateValueType(member->m_type, structType, depth + 1, complexity));
		}

		return ResultCode::kOK;
	}

	Result Package::ValidateValueType(const render::ValueType &valueType, const render::StructureType *upperLimit, size_t depth, size_t &complexity)
	{
		if (valueType.m_type == render::ValueTypeType::Structure)
			return ValidateStructureType(valueType.m_value.m_structureType, upperLimit, depth, complexity);
		else
			return ResultCode::kOK;
	}

	const RenderRTTIStructType *RenderDataHandler::GetSamplerDescRTTI() const
	{
		return reinterpret_cast<const RenderRTTIStructType *>(render_rtti::RTTIResolver<render::SamplerDesc>::GetRTTIType());
	}

	const RenderRTTIStructType *RenderDataHandler::GetPushConstantDescRTTI() const
	{
		return reinterpret_cast<const RenderRTTIStructType *>(render_rtti::RTTIResolver<render::PushConstantDesc>::GetRTTIType());
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

	const RenderRTTIStructType *RenderDataHandler::GetGraphicsPipelineNameLookupRTTI() const
	{
		return reinterpret_cast<const RenderRTTIStructType *>(render_rtti::RTTIResolver<render::GraphicsPipelineNameLookup>::GetRTTIType());
	}

	const RenderRTTIStructType *RenderDataHandler::GetRenderTargetDescRTTI() const
	{
		return reinterpret_cast<const RenderRTTIStructType *>(render_rtti::RTTIResolver<render::RenderTargetDesc>::GetRTTIType());
	}

	const RenderRTTIStructType *RenderDataHandler::GetRenderOperationDescRTTI() const
	{
		return reinterpret_cast<const RenderRTTIStructType *>(render_rtti::RTTIResolver<render::RenderOperationDesc>::GetRTTIType());
	}

	const RenderRTTIStructType *RenderDataHandler::GetShaderDescRTTI() const
	{
		return reinterpret_cast<const RenderRTTIStructType *>(render_rtti::RTTIResolver<render::ShaderDesc>::GetRTTIType());
	}

	const RenderRTTIStructType *RenderDataHandler::GetDepthStencilOperationDescRTTI() const
	{
		return reinterpret_cast<const RenderRTTIStructType *>(render_rtti::RTTIResolver<render::DepthStencilOperationDesc>::GetRTTIType());
	}

	const RenderRTTIEnumType *RenderDataHandler::GetNumericTypeRTTI() const
	{
		return reinterpret_cast<const RenderRTTIEnumType *>(render_rtti::RTTIResolver<render::NumericType>::GetRTTIType());
	}

	const RenderRTTIEnumType *RenderDataHandler::GetInputLayoutVertexInputSteppingRTTI() const
	{
		return reinterpret_cast<const RenderRTTIEnumType *>(render_rtti::RTTIResolver<render::InputLayoutVertexInputStepping>::GetRTTIType());
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

	const RenderRTTIStructType *RenderDataHandler::GetRenderPassDescRTTI() const
	{
		return reinterpret_cast<const RenderRTTIStructType *>(render_rtti::RTTIResolver<render::RenderPassDesc>::GetRTTIType());
	}

	const RenderRTTIStructType *RenderDataHandler::GetRenderPassNameLookupRTTI() const
	{
		return reinterpret_cast<const RenderRTTIStructType *>(render_rtti::RTTIResolver<render::RenderPassNameLookup>::GetRTTIType());
	}

	const RenderRTTIStructType *RenderDataHandler::GetDepthStencilTargetDescRTTI() const
	{
		return reinterpret_cast<const RenderRTTIStructType *>(render_rtti::RTTIResolver<render::DepthStencilTargetDesc>::GetRTTIType());
	}

	uint32_t RenderDataHandler::GetPackageVersion() const
	{
		return 1;
	}

	uint32_t RenderDataHandler::GetPackageIdentifier() const
	{
		return RKIT_FOURCC('R', 'P', 'K', 'G');
	}

	Result RenderDataHandler::LoadPackage(IReadStream &stream, bool allowTempStrings, data::IRenderDataConfigurator *configurator, UniquePtr<IRenderDataPackage> &outPackage, Vector<Vector<uint8_t>> *outBinaryContent) const
	{
		UniquePtr<Package> package;
		RKIT_CHECK(New<Package>(package));

		RKIT_CHECK(package->Load(this, allowTempStrings, configurator, stream));

		if (outBinaryContent)
		{
			size_t numBinaryContent = package->GetBinaryContentCount();

			RKIT_CHECK(outBinaryContent->Resize(numBinaryContent));

			for (size_t i = 0; i < numBinaryContent; i++)
			{
				Vector<uint8_t> &contentInstance = (*outBinaryContent)[i];

				size_t contentSize = package->GetBinaryContentSize(i);
				RKIT_CHECK(contentInstance.Resize(contentSize));

				if (contentSize > 0)
				{
					RKIT_CHECK(stream.ReadAll(&contentInstance[0], contentSize));
				}
			}
		}

		outPackage = std::move(package);
		return ResultCode::kOK;
	}

#define LINK_INDEXABLE_LIST_TYPE(type)	\
	case RenderRTTIIndexableStructType::type:\
		if (outList)\
		{\
			RKIT_CHECK(New<render_rtti::RenderRTTIList<render::type>>(*outList));\
		}\
		if (outPtrList)\
		{\
			RKIT_CHECK(New<render_rtti::RenderRTTIObjectPtrList<render::type>>(*outPtrList));\
		}\
		if (outRTTI)\
			*outRTTI = reinterpret_cast<const RenderRTTIStructType *>(render_rtti::RTTIResolver<render::type>::GetRTTIType());\
		return ResultCode::kOK;

	Result RenderDataHandler::ProcessIndexable(RenderRTTIIndexableStructType indexableStructType, UniquePtr<IRenderRTTIListBase> *outList, UniquePtr<IRenderRTTIObjectPtrList> *outPtrList, const RenderRTTIStructType **outRTTI) const
	{
		switch (indexableStructType)
		{
		LINK_INDEXABLE_LIST_TYPE(DepthStencilTargetDesc)
		LINK_INDEXABLE_LIST_TYPE(DepthStencilOperationDesc)
		LINK_INDEXABLE_LIST_TYPE(GraphicsPipelineNameLookup)
		LINK_INDEXABLE_LIST_TYPE(GraphicsPipelineDesc)
		LINK_INDEXABLE_LIST_TYPE(RenderTargetDesc)
		LINK_INDEXABLE_LIST_TYPE(RenderOperationDesc)
		LINK_INDEXABLE_LIST_TYPE(PushConstantDesc)
		LINK_INDEXABLE_LIST_TYPE(PushConstantListDesc)
		LINK_INDEXABLE_LIST_TYPE(ShaderDesc)
		LINK_INDEXABLE_LIST_TYPE(StructureType)
		LINK_INDEXABLE_LIST_TYPE(StructureMemberDesc)
		LINK_INDEXABLE_LIST_TYPE(InputLayoutDesc)
		LINK_INDEXABLE_LIST_TYPE(PipelineLayoutDesc)
		LINK_INDEXABLE_LIST_TYPE(DescriptorLayoutDesc)
		LINK_INDEXABLE_LIST_TYPE(DescriptorDesc)
		LINK_INDEXABLE_LIST_TYPE(InputLayoutVertexFeedDesc)
		LINK_INDEXABLE_LIST_TYPE(InputLayoutVertexInputDesc)
		LINK_INDEXABLE_LIST_TYPE(CompoundNumericType)
		LINK_INDEXABLE_LIST_TYPE(VectorNumericType)
		LINK_INDEXABLE_LIST_TYPE(VectorOrScalarNumericType)
		LINK_INDEXABLE_LIST_TYPE(SamplerDesc)
		LINK_INDEXABLE_LIST_TYPE(ContentKey)
		LINK_INDEXABLE_LIST_TYPE(ShaderPermutationTree)
		LINK_INDEXABLE_LIST_TYPE(ShaderPermutationTreeBranch)
		LINK_INDEXABLE_LIST_TYPE(RenderPassDesc)
		LINK_INDEXABLE_LIST_TYPE(RenderPassNameLookup)

		default:
			return ResultCode::kInternalError;
		}
	}
#undef LINK_INDEXABLE_LIST_TYPE

} } // rkit::data
