#include "VulkanCommandBatch.h"

#include "VulkanDevice.h"

#include "rkit/Core/UniquePtr.h"

namespace rkit::render::vulkan
{
	class VulkanCommandBatch : public VulkanCommandBatchBase
	{
	public:
		explicit VulkanCommandBatch(VulkanDeviceBase &device, VulkanCommandAllocatorBase &cmdAlloc);

		Result ClearCommandBatch() override;
		Result OpenCommandBatch(bool cpuWaitable) override;

		Result Submit() override;
		Result WaitForCompletion() override;

		Result Initialize();

	private:
		VulkanDeviceBase &m_device;
		VulkanCommandAllocatorBase &m_cmdAlloc;
	};

	VulkanCommandBatch::VulkanCommandBatch(VulkanDeviceBase &device, VulkanCommandAllocatorBase &cmdAlloc)
		: m_device(device)
		, m_cmdAlloc(cmdAlloc)
	{
	}

	Result VulkanCommandBatch::ClearCommandBatch()
	{
		return ResultCode::kNotYetImplemented;
	}

	Result VulkanCommandBatch::OpenCommandBatch(bool cpuWaitable)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result VulkanCommandBatch::Submit()
	{
		return ResultCode::kNotYetImplemented;
	}

	Result VulkanCommandBatch::WaitForCompletion()
	{
		return ResultCode::kNotYetImplemented;
	}

	Result VulkanCommandBatch::Initialize()
	{
		return ResultCode::kOK;
	}

	Result VulkanCommandBatchBase::Create(UniquePtr<VulkanCommandBatchBase> &outCmdBatch, VulkanDeviceBase &device, VulkanCommandAllocatorBase &cmdAlloc)
	{
		UniquePtr<VulkanCommandBatch> cmdBatch;
		RKIT_CHECK(New<VulkanCommandBatch>(cmdBatch, device, cmdAlloc));

		RKIT_CHECK(cmdBatch->Initialize());

		outCmdBatch = std::move(cmdBatch);

		return ResultCode::kOK;
	}
};
