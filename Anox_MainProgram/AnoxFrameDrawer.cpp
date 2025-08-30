#include "AnoxFrameDrawer.h"

#include "AnoxGameWindowResources.h"
#include "AnoxPeriodicResources.h"
#include "AnoxRecordJobRunner.h"
#include "AnoxSubmitJobRunner.h"
#include "AnoxRenderedWindow.h"

#include "anox/AnoxGraphicsSubsystem.h"

#include "rkit/Render/Barrier.h"
#include "rkit/Render/CommandBatch.h"
#include "rkit/Render/CommandAllocator.h"
#include "rkit/Render/CommandEncoder.h"
#include "rkit/Render/ImageRect.h"
#include "rkit/Render/PipelineStage.h"
#include "rkit/Render/RenderTargetClear.h"

#include "rkit/Core/EnumMask.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/JobDependencyList.h"
#include "rkit/Core/JobQueue.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/SpanProtos.h"

namespace anox
{
	class FrameDrawer final : public IFrameDrawer
	{
	public:
		rkit::Result DrawFrame(IGraphicsSubsystem &graphicsSubsystem, const rkit::RCPtr<PerFrameResources> &perFrameResources, RenderedWindowBase &renderedWindow) override;

		rkit::Result Initialize();

	private:
		class SubmitTestCommandsJobRunner final : public IGraphicsSubmitJobRunner_t
		{
		public:
			rkit::Result RunSubmit(rkit::render::IGraphicsCommandQueue &commandQueue) override;

			rkit::render::IGraphicsCommandBatch **GetCmdBatchRef();

		private:
			rkit::render::IGraphicsCommandBatch *m_cmdBatch = nullptr;
		};

		class RecordTestCommandsJobRunner final : public IGraphicsRecordJobRunner_t
		{
		public:
			explicit RecordTestCommandsJobRunner(rkit::render::IGraphicsCommandBatch **cmdBatchRef, const rkit::RCPtr<PerFramePerDisplayResources> &perDisplayResources);

			rkit::Result RunRecord(rkit::render::IGraphicsCommandAllocator &commandAllocator) override;

		private:
			rkit::render::IGraphicsCommandBatch **m_cmdBatchRef;
			rkit::RCPtr<PerFramePerDisplayResources> m_perDisplayResources;
		};
	};

	FrameDrawer::RecordTestCommandsJobRunner::RecordTestCommandsJobRunner(rkit::render::IGraphicsCommandBatch **cmdBatchRef, const rkit::RCPtr<PerFramePerDisplayResources> &perDisplayResources)
		: m_cmdBatchRef(cmdBatchRef)
		, m_perDisplayResources(perDisplayResources)
	{
	}

	rkit::Result FrameDrawer::RecordTestCommandsJobRunner::RunRecord(rkit::render::IGraphicsCommandAllocator &commandAllocator)
	{
		RKIT_CHECK(commandAllocator.OpenGraphicsCommandBatch(*m_cmdBatchRef, false));

		rkit::render::IGraphicsCommandBatch &cmdBatch = **m_cmdBatchRef;

		rkit::EnumMask<rkit::render::PipelineStage> pipelineStages({ rkit::render::PipelineStage::kColorOutput });

		GameWindowResources *resources = static_cast<GameWindowResources *>(m_perDisplayResources->m_windowResources);

		GameWindowSwapChainFrameResources &scFrameResources = resources->m_swapChainFrameResources[m_perDisplayResources->m_swapChainFrameIndex];

		rkit::render::IGraphicsCommandEncoder *encoder = nullptr;
		RKIT_CHECK(cmdBatch.OpenGraphicsCommandEncoder(encoder, *scFrameResources.m_simpleColorTargetRPI));

		RKIT_CHECK(encoder->WaitForSwapChainAcquire(*m_perDisplayResources->m_swapChainSyncPoint, pipelineStages));

		{
			rkit::render::ImageMemoryBarrier barrier;

			barrier.m_priorStages = rkit::render::PipelineStageMask_t({ rkit::render::PipelineStage::kPresentAcquire });
			barrier.m_subsequentStages = rkit::render::PipelineStageMask_t({ rkit::render::PipelineStage::kColorOutput });
			barrier.m_subsequentAccess = rkit::render::ResourceAccessMask_t({ rkit::render::ResourceAccess::kRenderTarget });
			barrier.m_subsequentLayout = rkit::render::ImageLayout::RenderTarget;

			barrier.m_image = scFrameResources.m_colorTargetImage;

			rkit::render::BarrierGroup barrierGroup;
			barrierGroup.m_imageMemoryBarriers = rkit::Span<rkit::render::ImageMemoryBarrier>(&barrier, 1);

			RKIT_CHECK(encoder->PipelineBarrier(barrierGroup));
		}

		{
			rkit::render::RenderTargetClear rtClear;

			rtClear.m_clearValue.m_float32[0] = 1.0f;
			rtClear.m_clearValue.m_float32[1] = 1.0f;
			rtClear.m_clearValue.m_float32[2] = 0.0f;
			rtClear.m_clearValue.m_float32[3] = 1.0f;
			rtClear.m_renderTargetIndex = 0;

			rkit::render::ImageRect2D imgRect = { 0, 0, 50, 50 };

			RKIT_CHECK(encoder->ClearTargets(rkit::ConstSpan<rkit::render::RenderTargetClear>(&rtClear, 1), nullptr, rkit::ConstSpan<rkit::render::ImageRect2D>(&imgRect, 1)));
		}

		{
			rkit::render::ImageMemoryBarrier barrier;

			barrier.m_priorStages = rkit::render::PipelineStageMask_t({ rkit::render::PipelineStage::kColorOutput });
			barrier.m_subsequentStages = rkit::render::PipelineStageMask_t({ rkit::render::PipelineStage::kPresentSubmit });
			barrier.m_priorAccess = rkit::render::ResourceAccessMask_t({ rkit::render::ResourceAccess::kRenderTarget });
			barrier.m_subsequentAccess = rkit::render::ResourceAccessMask_t({ rkit::render::ResourceAccess::kPresentSource });
			barrier.m_subsequentLayout = rkit::render::ImageLayout::PresentSource;

			barrier.m_image = scFrameResources.m_colorTargetImage;

			rkit::render::BarrierGroup barrierGroup;
			barrierGroup.m_imageMemoryBarriers = rkit::Span<rkit::render::ImageMemoryBarrier>(&barrier, 1);

			RKIT_CHECK(encoder->PipelineBarrier(barrierGroup));
		}

		RKIT_CHECK(encoder->SignalSwapChainPresentReady(*m_perDisplayResources->m_swapChainSyncPoint, pipelineStages));

		RKIT_CHECK(cmdBatch.CloseBatch());

		return rkit::ResultCode::kOK;
	}

	rkit::Result FrameDrawer::SubmitTestCommandsJobRunner::RunSubmit(rkit::render::IGraphicsCommandQueue &commandQueue)
	{
		RKIT_CHECK(m_cmdBatch->Submit());

		return rkit::ResultCode::kOK;
	}


	rkit::render::IGraphicsCommandBatch **FrameDrawer::SubmitTestCommandsJobRunner::GetCmdBatchRef()
	{
		return &m_cmdBatch;
	}

	rkit::Result FrameDrawer::DrawFrame(IGraphicsSubsystem &graphicsSubsystem, const rkit::RCPtr<PerFrameResources> &perFrameResources, RenderedWindowBase &renderedWindow)
	{
		rkit::RCPtr<PerFramePerDisplayResources> perDisplayResources = renderedWindow.GetCurrentFrameResources();

		rkit::RCPtr<rkit::Job> submitJob;
		rkit::UniquePtr<SubmitTestCommandsJobRunner> submitJobRunner;
		RKIT_CHECK(rkit::New<SubmitTestCommandsJobRunner>(submitJobRunner));

		rkit::RCPtr<rkit::Job> recordJob;
		rkit::UniquePtr<RecordTestCommandsJobRunner> recordJobRunner;
		RKIT_CHECK(rkit::New<RecordTestCommandsJobRunner>(recordJobRunner, submitJobRunner->GetCmdBatchRef(), perDisplayResources));

		RKIT_CHECK(graphicsSubsystem.CreateAndQueueRecordJob(&recordJob, LogicalQueueType::kGraphics, std::move(recordJobRunner), rkit::Span<rkit::RCPtr<rkit::Job>>(&perDisplayResources->m_acquireJob, 1)));

		RKIT_CHECK(graphicsSubsystem.CreateAndQueueSubmitJob(&submitJob, LogicalQueueType::kGraphics, std::move(submitJobRunner), rkit::Span<rkit::RCPtr<rkit::Job>>(&recordJob, 1)));

		return rkit::ResultCode::kOK;
	}

	rkit::Result FrameDrawer::Initialize()
	{
		return rkit::ResultCode::kOK;
	}

	rkit::Result IFrameDrawer::Create(rkit::UniquePtr<IFrameDrawer> &outFrameDrawer)
	{
		rkit::UniquePtr<FrameDrawer> frameDrawer;
		RKIT_CHECK(rkit::New<FrameDrawer>(frameDrawer));

		RKIT_CHECK(frameDrawer->Initialize());

		outFrameDrawer = std::move(frameDrawer);

		return rkit::ResultCode::kOK;
	}
}
