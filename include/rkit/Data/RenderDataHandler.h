#pragma once

#include <cstddef>
#include <cstdint>

#include "rkit/Render/RenderDefProtos.h"

namespace rkit
{
	struct Result;
}

namespace rkit::data
{
	enum class RenderRTTIMainType
	{
		Invalid,

		ValueType,

		// String indexes
		GlobalStringIndex,
		ConfigStringIndex,
		TempStringIndex,

		// Object indexes
		StaticSamplerIndex,

		// Enums
		Filter,
		MipMapMode,
		AddressMode,
		AnisotropicFiltering,
		ComparisonFunction,
		BorderColor,
		VectorDimension,
		NumericType,
		StageVisibility,
		VertexInputStepping,
		DescriptorType,

		// Structs
		SamplerDesc,
		PushConstantDesc,
		PushConstantListDesc,
		StructureMemberDesc,
		StructureType,
		DescriptorDesc,

		Count,
	};

	enum class RenderRTTIType
	{
		Enum,
		Structure,
		Number,
		Array,
		ValueType,
		StringIndex,
		ObjectPtr,
	};

	struct RenderRTTIEnumOption
	{
		const char *m_name;
		size_t m_nameLength;

		unsigned int m_value;
	};

	struct RenderRTTITypeBase
	{
		RenderRTTIType m_type;

		RenderRTTIMainType m_mainType;

		const char *m_name;
		size_t m_nameLength;
	};

	struct RenderRTTIArrayType
	{
		RenderRTTITypeBase base;

		const RenderRTTITypeBase *m_elementType;
		size_t m_elementSize;

		Result (*m_appendVectorAndUpdateSpanFunc)(void *vectorPtr, void *spanPtr, void **outItemPtr);
		size_t (*m_getSpanCountFunc)(const void *spanPtr);
	};

	struct RenderRTTIEnumType
	{
		RenderRTTITypeBase m_base;

		const RenderRTTIEnumOption *m_options;
		size_t m_numOptions;

		unsigned int m_maxValue;

		unsigned int (*m_readValueFunc)(const void *valuePtr);
		void (*m_writeValueFunc)(void *valuePtr, unsigned int value);

		uint8_t (*m_getConfigurableStateFunc)(const void *configurablePtr);
		unsigned int (*m_readConfigurableValueFunc)(const void *configurablePtr);
		const render::ConfigStringIndex_t &(*m_readConfigurableNameFunc)(const void *configurablePtr);

		void (*m_writeConfigurableValueFunc)(void *configurablePtr, unsigned int value);
		void (*m_writeConfigurableNameFunc)(void *configurablePtr, const render::ConfigStringIndex_t &str);
		void (*m_writeConfigurableDefaultFunc)(void *configurablePtr);
	};

	struct RenderRTTIStructField
	{
		const char *m_name;
		size_t m_nameLength;

		const RenderRTTITypeBase *(*m_getTypeFunc)();
		bool m_isVisible;
		bool m_isConfigurable;

		void *(*m_getMemberPtrFunc)(void *ptr);
	};

	struct RenderRTTIStructType
	{
		RenderRTTITypeBase m_base;

		const RenderRTTIStructField *m_fields;
		size_t m_numFields;
	};

	struct RenderRTTIStringIndexType
	{
		RenderRTTITypeBase m_base;

		void (*m_writeStringIndexFunc)(void *ptr, size_t index);
		size_t (*m_readStringIndexFunc)(const void *ptr);
		int (*m_getPurposeFunc)();
	};

	struct RenderRTTIObjectPtrType
	{
		RenderRTTITypeBase m_base;

		const RenderRTTITypeBase *(*m_getTypeFunc)();
		void (*m_writeFunc)(void *ptrLoc, const void *value);
		const void *(*m_readFunc)(const void *ptrLoc);
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

		uint8_t(*m_getConfigurableStateFunc)(const void *configurablePtr);

		double (*m_readConfigurableValueFloatFunc)(const void *configurablePtr);
		uint64_t (*m_readConfigurableValueUIntFunc)(const void *configurablePtr);
		int64_t (*m_readConfigurableValueSIntFunc)(const void *configurablePtr);
		const render::ConfigStringIndex_t &(*m_readConfigurableNameFunc)(const void *configurablePtr);

		void (*m_writeConfigurableValueFloatFunc)(void *configurablePtr, double value);
		void (*m_writeConfigurableValueUIntFunc)(void *configurablePtr, uint64_t value);
		void (*m_writeConfigurableValueSIntFunc)(void *configurablePtr, int64_t value);
		void (*m_writeConfigurableNameFunc)(void *configurablePtr, const render::ConfigStringIndex_t &str);
		void (*m_writeConfigurableDefaultFunc)(void *configurablePtr);
	};

	struct IRenderDataHandler
	{
		virtual ~IRenderDataHandler() {}

		virtual const RenderRTTIStructType *GetSamplerDescRTTI() const = 0;
		virtual const RenderRTTIStructType *GetPushConstantDescRTTI() const = 0;
		virtual const RenderRTTIEnumType *GetVertexInputSteppingRTTI() const = 0;
		virtual const RenderRTTIStructType *GetDescriptorDescRTTI() const = 0;
		virtual const RenderRTTIEnumType *GetDescriptorTypeRTTI() const = 0;
	};
}
