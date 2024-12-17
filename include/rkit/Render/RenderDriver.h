#pragma once

#include "rkit/Core/Drivers.h"
#include "rkit/Core/NoCopy.h"

namespace rkit
{
	struct Result;

	template<class T>
	class UniquePtr;

	template<class T>
	class Vector;
}

namespace rkit::render
{
	enum class ValidationLevel
	{
		kNone,
		kTrace,
		kSimple,
		kAggressive,
	};

	struct RenderDriverInitProperties : public DriverInitParameters
	{
		ValidationLevel m_validationLevel = ValidationLevel::kNone;
		bool m_enableLogging = false;
	};

	struct AdapterLUID
	{
		AdapterLUID();
		AdapterLUID(const AdapterLUID &other) = default;

		static const size_t kSize = 8;

		uint8_t m_luid[kSize];
	};

	struct IRenderAdapter
	{
		virtual ~IRenderAdapter() {}
	};

	struct IRenderDriver : public ICustomDriver
	{
		virtual ~IRenderDriver() {}

		virtual Result EnumerateAdapters(Vector<UniquePtr<IRenderAdapter>> &devices) const = 0;
		virtual Result CreateDevice(IRenderAdapter &adapter) = 0;
	};
}


namespace rkit::render
{
	inline AdapterLUID::AdapterLUID()
		: m_luid{ 0, 0, 0, 0, 0, 0, 0, 0 }
	{
	}
}
