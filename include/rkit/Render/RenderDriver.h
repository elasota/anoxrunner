#pragma once

#include "rkit/Core/Drivers.h"
#include "rkit/Core/NoCopy.h"

#include "CommandQueueType.h"

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
	struct IRenderDevice;

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

	struct CommandQueueTypeRequest
	{
		CommandQueueType m_type;
		size_t m_numQueues;
		const float *m_queuePriorities = nullptr;
	};

	struct IRenderAdapter
	{
		virtual ~IRenderAdapter() {}

		virtual size_t GetCommandQueueCount(CommandQueueType type) const = 0;
	};

	struct IRenderDriver : public ICustomDriver
	{
		virtual ~IRenderDriver() {}

		virtual Result EnumerateAdapters(Vector<UniquePtr<IRenderAdapter>> &devices) const = 0;
		virtual Result CreateDevice(UniquePtr<IRenderDevice> &outDevice, const Span<CommandQueueTypeRequest> &queueRequests, IRenderAdapter &adapter) = 0;
	};
}


namespace rkit::render
{
	inline AdapterLUID::AdapterLUID()
		: m_luid{ 0, 0, 0, 0, 0, 0, 0, 0 }
	{
	}
}
