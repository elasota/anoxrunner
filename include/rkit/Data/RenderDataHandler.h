#pragma once

#include <cstddef>
#include <cstdint>

namespace rkit::data
{
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

	struct RenderRTTIEnumType
	{
		RenderRTTIType m_type;

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
		RenderRTTIType m_type;

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
		RenderRTTIType m_type;

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


	};
}
