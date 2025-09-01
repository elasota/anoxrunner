#pragma once

namespace rkit { namespace render {
	enum class HeapType
	{
		kDefault,
		kUpload,
		kReadback,
	};

	struct HeapSpec
	{
		HeapType m_heapType = HeapType::kDefault;

		bool m_allowBuffers = false;
		bool m_allowRTDSImages = false;
		bool m_allowNonRTDSImages = false;
		bool m_cpuAccessible = false;
	};
} }
