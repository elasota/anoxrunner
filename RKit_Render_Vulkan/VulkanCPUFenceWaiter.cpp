#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/HybridVector.h"
#include "rkit/Core/Vector.h"

#include "VulkanCheck.h"
#include "VulkanCPUFenceWaiter.h"
#include "VulkanDevice.h"
#include "VulkanAPI.h"
#include "VulkanFence.h"

namespace rkit { namespace render { namespace vulkan {
	class VulkanCPUFenceWaiter final : public VulkanCPUFenceWaiterBase
	{
	public:
		explicit VulkanCPUFenceWaiter(VulkanDeviceBase &device);

		Result WaitForFences(const Span<const Pair<ICPUVisibleTimelineFence *, TimelinePoint_t>> &timelineWaits, bool waitAll) override;
		Result WaitForFencesTimed(bool &outTimeout, const Span<const Pair<ICPUVisibleTimelineFence *, TimelinePoint_t>> &timelineWaits, uint64_t timeoutMSec, bool waitAll) override;
		Result WaitForBinaryFences(const Span<IBinaryCPUWaitableFence *> &binaryWaits, bool waitAll) override;
		Result WaitForBinaryFencesTimed(bool &outTimeout, const Span<IBinaryCPUWaitableFence *> &binaryWaits, uint64_t timeoutMSec, bool waitAll) override;

	private:
		Result WaitForFencesNanoSec(bool &outTimeout, const Span<const Pair<ICPUVisibleTimelineFence *, TimelinePoint_t>> &timelineWaits, uint64_t timeoutNSec, bool waitAll) const;
		Result WaitForBinaryFencesNanoSec(bool &outTimeout, const Span<IBinaryCPUWaitableFence *> &binaryWaits, uint64_t timeoutNSec, bool waitAll) const;

		VulkanDeviceBase &m_device;
	};

	VulkanCPUFenceWaiter::VulkanCPUFenceWaiter(VulkanDeviceBase &device)
		: m_device(device)
	{
	}

	Result VulkanCPUFenceWaiter::WaitForFencesNanoSec(bool &outTimeout, const Span<const Pair<ICPUVisibleTimelineFence *, TimelinePoint_t>> &timelineWaits, uint64_t timeoutNSec, bool waitAll) const
	{
		const size_t numSemaphores = timelineWaits.Count();

		if (numSemaphores == 0)
		{
			outTimeout = false;
			return ResultCode::kOK;
		}

		if (numSemaphores > std::numeric_limits<uint32_t>::max())
			return ResultCode::kOutOfMemory;

		HybridVector<VkSemaphore, 16> semaphores;
		HybridVector<uint64_t, 16> values;

		RKIT_CHECK(semaphores.Reserve(numSemaphores));
		RKIT_CHECK(values.Reserve(numSemaphores));

		for (const Pair<ICPUVisibleTimelineFence *, TimelinePoint_t> &tw : timelineWaits)
		{
			RKIT_CHECK(semaphores.Append(static_cast<VulkanTimelineFence *>(tw.First())->GetSemaphore()));
			RKIT_CHECK(values.Append(tw.Second()));
		}

		VkSemaphoreWaitInfoKHR semaWaitInfo = {};
		semaWaitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR;

		if (numSemaphores > 1 && !waitAll)
			semaWaitInfo.flags |= VK_SEMAPHORE_WAIT_ANY_BIT_KHR;

		semaWaitInfo.semaphoreCount = static_cast<uint32_t>(numSemaphores);
		semaWaitInfo.pSemaphores = semaphores.GetBuffer();
		semaWaitInfo.pValues = values.GetBuffer();

		const VkResult result = m_device.GetDeviceAPI().vkWaitSemaphoresKHR(m_device.GetDevice(), &semaWaitInfo, timeoutNSec);
		if (result == VK_TIMEOUT)
		{
			outTimeout = true;
			return ResultCode::kOK;
		}

		RKIT_VK_CHECK(result);

		outTimeout = false;

		return ResultCode::kOK;
	}

	Result VulkanCPUFenceWaiter::WaitForBinaryFencesNanoSec(bool &outTimeout, const Span<IBinaryCPUWaitableFence *> &binaryWaits, uint64_t timeoutNSec, bool waitAll) const
	{
		const size_t kStaticSize = 16;

		const size_t numFences = binaryWaits.Count();

		if (numFences == 0)
		{
			outTimeout = false;
			return ResultCode::kOK;
		}

		if (numFences > std::numeric_limits<uint32_t>::max())
			return ResultCode::kOutOfMemory;

		HybridVector<VkFence, 16> fences;

		RKIT_CHECK(fences.Reserve(numFences));

		for (IBinaryCPUWaitableFence *fence : binaryWaits)
		{
			RKIT_CHECK(fences.Append(static_cast<VulkanBinaryCPUWaitableFence *>(fence)->GetFence()));
		}

		const VkResult result = m_device.GetDeviceAPI().vkWaitForFences(m_device.GetDevice(), static_cast<uint32_t>(numFences), fences.GetBuffer(), waitAll ? 1 : 0, timeoutNSec);

		if (result == VK_TIMEOUT)
		{
			outTimeout = true;
			return ResultCode::kOK;
		}

		RKIT_VK_CHECK(result);

		outTimeout = false;

		return ResultCode::kOK;
	}

	Result VulkanCPUFenceWaiter::WaitForFences(const Span<const Pair<ICPUVisibleTimelineFence *, TimelinePoint_t>> &timelineWaits, bool waitAll)
	{
		bool timeout = false;
		return this->WaitForFencesNanoSec(timeout, timelineWaits, std::numeric_limits<uint64_t>::max(), waitAll);
	}

	Result VulkanCPUFenceWaiter::WaitForFencesTimed(bool &outTimeout, const Span<const Pair<ICPUVisibleTimelineFence *, TimelinePoint_t>> &timelineWaits, uint64_t timeoutMSec, bool waitAll)
	{
		constexpr uint64_t maxNSec = (std::numeric_limits<uint64_t>::max() - 1) / 1000000;
		if (timeoutMSec > maxNSec)
			return ResultCode::kIntegerOverflow;

		return this->WaitForFencesNanoSec(outTimeout, timelineWaits, timeoutMSec * static_cast<uint64_t>(1000000), waitAll);
	}

	Result VulkanCPUFenceWaiter::WaitForBinaryFences(const Span<IBinaryCPUWaitableFence *> &binaryWaits, bool waitAll)
	{
		bool timeout = false;
		return this->WaitForBinaryFencesNanoSec(timeout, binaryWaits, std::numeric_limits<uint64_t>::max(), waitAll);
	}

	Result VulkanCPUFenceWaiter::WaitForBinaryFencesTimed(bool &outTimeout, const Span<IBinaryCPUWaitableFence *> &binaryWaits, uint64_t timeoutMSec, bool waitAll)
	{
		constexpr uint64_t maxNSec = (std::numeric_limits<uint64_t>::max() - 1) / 1000000;
		if (timeoutMSec > maxNSec)
			return ResultCode::kIntegerOverflow;

		return this->WaitForBinaryFencesNanoSec(outTimeout, binaryWaits, timeoutMSec * static_cast<uint64_t>(1000000), waitAll);
	}

	Result VulkanCPUFenceWaiterBase::Create(UniquePtr<VulkanCPUFenceWaiterBase> &outInstance, VulkanDeviceBase &device)
	{
		UniquePtr<VulkanCPUFenceWaiter> fenceWaiter;

		RKIT_CHECK(New<VulkanCPUFenceWaiter>(fenceWaiter, device));

		outInstance = std::move(fenceWaiter);

		return ResultCode::kOK;
	}

} } }
