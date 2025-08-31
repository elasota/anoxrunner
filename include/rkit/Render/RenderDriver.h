#pragma once

#include "rkit/Core/Drivers.h"
#include "rkit/Core/NoCopy.h"

#include "CommandQueueType.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	template<class T>
	class Vector;
}

namespace rkit { namespace render
{
	struct IRenderDevice;
	struct IRenderDeviceCaps;

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
		CommandQueueType m_type = CommandQueueType::kCount;
		size_t m_numQueues = 0;
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
		virtual Result CreateDevice(UniquePtr<IRenderDevice> &outDevice, const Span<CommandQueueTypeRequest> &queueRequests, const IRenderDeviceCaps &requiredCaps, const IRenderDeviceCaps &optionalCaps, IRenderAdapter &adapter) = 0;
	};
} } // rkit::render


namespace rkit { namespace render
{
	inline AdapterLUID::AdapterLUID()
		: m_luid{ 0, 0, 0, 0, 0, 0, 0, 0 }
	{
	}
} } // rkit::render
