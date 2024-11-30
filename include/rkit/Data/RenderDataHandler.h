#pragma once

#include <cstddef>
#include <cstdint>

#include "rkit/Render/RenderDefProtos.h"
#include "rkit/Core/Vector.h"

namespace rkit
{
	struct Result;

	template<class T>
	class UniquePtr;

	struct IWriteStream;
}

namespace rkit::data
{
	enum class RenderRTTIMainType
	{
		ValueType,

		// String indexes
		GlobalStringIndex,
		ConfigStringIndex,
		TempStringIndex,

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
		DescriptorType,
		IndexSize,
		PrimitiveTopology,
		FillMode,
		CullMode,
		ReadWriteAccess,
		ColorBlendFactor,
		AlphaBlendFactor,
		BlendOp,
		InputLayoutVertexInputStepping,
		DepthStencilFormat,
		StencilOp,

		// Structs
		SamplerDesc,
		PushConstantDesc,
		PushConstantListDesc,
		StructureMemberDesc,
		StructureType,
		DescriptorDesc,
		RenderTargetFormat,
		RenderTargetDesc,
		DescriptorLayoutDesc,
		GraphicsPipelineDesc,
		InputLayoutVertexInputDesc,
		InputLayoutDesc,
		VectorNumericType,
		CompoundNumericType,
		ShaderDesc,
		DepthStencilDesc,
		StencilOpDesc,
		ContentKey,

		Count,

		Invalid,
	};

	enum class RenderRTTIIndexableStructType
	{
		DepthStencilDesc,
		GraphicsPipelineDesc,
		RenderTargetDesc,
		PushConstantDesc,
		PushConstantListDesc,
		ShaderDesc,
		StructureType,
		StructureMemberDesc,
		InputLayoutDesc,
		DescriptorLayoutDesc,
		DescriptorDesc,
		InputLayoutVertexInputDesc,
		VectorNumericType,
		CompoundNumericType,
		SamplerDesc,
		ContentKey,

		Count,

		NotIndexable,
	};

	enum class RenderRTTIType
	{
		Enum,
		Structure,
		Number,
		ValueType,
		StringIndex,
		ObjectPtr,
		ObjectPtrSpan,
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

		unsigned int m_maxValueExclusive;

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
		bool m_isNullable;

		void *(*m_getMemberPtrFunc)(void *ptr);
	};

	struct RenderRTTIStructType
	{
		RenderRTTITypeBase m_base;

		const RenderRTTIStructField *m_fields;
		size_t m_numFields;

		RenderRTTIIndexableStructType m_indexableType;
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

		const RenderRTTIStructType *(*m_getTypeFunc)();
		void (*m_writeFunc)(void *ptrLoc, const void *value);
		const void *(*m_readFunc)(const void *ptrLoc);
	};

	struct RenderRTTIObjectPtrSpanType
	{

		typedef const void *ConstVoidPtr_t;

		RenderRTTITypeBase m_base;

		size_t m_ptrSize;

		const RenderRTTIObjectPtrType *(*m_getPtrTypeFunc)();
		void (*m_setFunc)(void *spanPtr, const void *elements, size_t count);
		void (*m_getFunc)(const void *spanPtr, ConstVoidPtr_t &outElements, size_t &outCount);
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

	struct RenderRTTINumberTypeIOFunctions
	{
		double (*m_readFloatFunc)(const void *valuePtr);
		void (*m_writeValueFloatFunc)(void *valuePtr, double value);

		uint64_t(*m_readValueUIntFunc)(const void *valuePtr);
		void (*m_writeValueUIntFunc)(void *valuePtr, uint64_t value);

		int64_t(*m_readValueSIntFunc)(const void *valuePtr);
		void (*m_writeValueSIntFunc)(void *valuePtr, int64_t value);
	};

	struct RenderRTTINumberType
	{
		RenderRTTITypeBase m_base;

		RenderRTTINumberBitSize m_bitSize;
		RenderRTTINumberRepresentation m_representation;

		RenderRTTINumberTypeIOFunctions m_valueFunctions;
		RenderRTTINumberTypeIOFunctions m_configurableFunctions;

		uint8_t(*m_getConfigurableStateFunc)(const void *configurablePtr);

		const render::ConfigStringIndex_t &(*m_readConfigurableNameFunc)(const void *configurablePtr);

		void (*m_writeConfigurableNameFunc)(void *configurablePtr, const render::ConfigStringIndex_t &str);
		void (*m_writeConfigurableDefaultFunc)(void *configurablePtr);
	};

	class IRenderRTTIListBase
	{
	public:
		virtual ~IRenderRTTIListBase() {}

		virtual Result Resize(size_t count) = 0;
		virtual size_t GetCount() const = 0;

		virtual void *GetElementPtr(size_t index) = 0;
		virtual const void *GetElementPtr(size_t index) const = 0;

		virtual size_t GetElementSize() const = 0;
	};


	struct IRenderRTTIObjectPtrList
	{
		virtual ~IRenderRTTIObjectPtrList() {}

		virtual Result Resize(size_t count) = 0;
		virtual size_t GetCount() const = 0;
		virtual const void *GetElementPtr(size_t index) const = 0;
		virtual const void *GetElement(size_t index) const = 0;
		virtual Result Append(const void *ptr) = 0;
	};

	struct IRenderDataPackage
	{
		struct ConfigKey
		{
			size_t m_stringIndex = 0;
			RenderRTTIMainType m_mainType = RenderRTTIMainType::Invalid;
		};

		virtual ~IRenderDataPackage() {}

		virtual IRenderRTTIListBase *GetIndexable(RenderRTTIIndexableStructType indexable) const = 0;
		virtual const ConfigKey &GetConfigKey(size_t index) const = 0;
		virtual StringView GetString(size_t stringIndex) const = 0;
	};

	struct IRenderDataHandler
	{
		virtual ~IRenderDataHandler() {}

		virtual const RenderRTTIStructType *GetSamplerDescRTTI() const = 0;
		virtual const RenderRTTIStructType *GetPushConstantDescRTTI() const = 0;
		virtual const RenderRTTIEnumType *GetInputLayoutVertexInputSteppingRTTI() const = 0;
		virtual const RenderRTTIStructType *GetDescriptorDescRTTI() const = 0;
		virtual const RenderRTTIEnumType *GetDescriptorTypeRTTI() const = 0;
		virtual const RenderRTTIStructType *GetGraphicsPipelineDescRTTI() const = 0;
		virtual const RenderRTTIStructType *GetRenderTargetDescRTTI() const = 0;
		virtual const RenderRTTIStructType *GetShaderDescRTTI() const = 0;
		virtual const RenderRTTIStructType *GetDepthStencilDescRTTI() const = 0;
		virtual const RenderRTTIEnumType *GetNumericTypeRTTI() const = 0;
		virtual const RenderRTTIObjectPtrType *GetVectorNumericTypePtrRTTI() const = 0;
		virtual const RenderRTTIObjectPtrType *GetCompoundNumericTypePtrRTTI() const = 0;
		virtual const RenderRTTIObjectPtrType *GetStructureTypePtrRTTI() const = 0;

		virtual Result ProcessIndexable(RenderRTTIIndexableStructType indexableStructType, UniquePtr<IRenderRTTIListBase> *outList, UniquePtr<IRenderRTTIObjectPtrList> *outPtrList, const RenderRTTIStructType **outRTTI) const = 0;

		virtual uint32_t GetPackageVersion() const = 0;
		virtual uint32_t GetPackageIdentifier() const = 0;

		virtual Result LoadPackage(IReadStream &stream, bool allowTempStrings, UniquePtr<IRenderDataPackage> &outPackage) const = 0;
	};
}
