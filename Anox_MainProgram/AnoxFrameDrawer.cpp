#include "AnoxFrameDrawer.h"

#include "AnoxRecordJobRunner.h"

#include "anox/AnoxGraphicsSubsystem.h"

#include "rkit/Render/CommandAllocator.h"

#include "rkit/Core/Job.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/NewDelete.h"

namespace anox
{
	class FrameDrawer final : public IFrameDrawer
	{
	public:
		rkit::Result DrawFrame(IGraphicsSubsystem &graphicsSubsystem, const rkit::RCPtr<PerFrameResources> &perFrameResources, const rkit::RCPtr<PerFramePerDisplayResources> &perDisplayResources) override;

		rkit::Result Initialize();

	private:
		class SubmitTestCommandsJobRunner final : public rkit::IJobRunner
		{
		public:
			rkit::Result Run() override;

			rkit::render::IGraphicsCommandBatch **GetCmdBatchRef();

		private:
			rkit::render::IGraphicsCommandBatch *m_cmdBatch = nullptr;
		};

		class RecordTestCommandsJobRunner final : public IGraphicsRecordJobRunner_t
		{
		public:
			explicit RecordTestCommandsJobRunner(rkit::render::IGraphicsCommandBatch **cmdBatchRef);

			rkit::Result RunRecord(rkit::render::IGraphicsCommandAllocator &commandAllocator) override;

		private:
			rkit::render::IGraphicsCommandBatch **m_cmdBatchRef;
		};
	};

	FrameDrawer::RecordTestCommandsJobRunner::RecordTestCommandsJobRunner(rkit::render::IGraphicsCommandBatch **cmdBatchRef)
		: m_cmdBatchRef(cmdBatchRef)
	{
	}

	rkit::Result FrameDrawer::RecordTestCommandsJobRunner::RunRecord(rkit::render::IGraphicsCommandAllocator &commandAllocator)
	{
		//RKIT_CHECK(commandAllocator.OpenGraphicsCommandList(*m_cmdListRef));

		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result FrameDrawer::SubmitTestCommandsJobRunner::Run()
	{
		return rkit::ResultCode::kNotYetImplemented;
	}


	rkit::render::IGraphicsCommandBatch **FrameDrawer::SubmitTestCommandsJobRunner::GetCmdBatchRef()
	{
		return &m_cmdBatch;
	}

	rkit::Result FrameDrawer::DrawFrame(IGraphicsSubsystem &graphicsSubsystem, const rkit::RCPtr<PerFrameResources> &perFrameResources, const rkit::RCPtr<PerFramePerDisplayResources> &perDisplayResources)
	{
		rkit::RCPtr<rkit::Job> submitJob;
		rkit::UniquePtr<SubmitTestCommandsJobRunner> submitJobRunner;
		RKIT_CHECK(rkit::New<SubmitTestCommandsJobRunner>(submitJobRunner));

		rkit::RCPtr<rkit::Job> recordJob;
		rkit::UniquePtr<RecordTestCommandsJobRunner> recordJobRunner;
		RKIT_CHECK(rkit::New<RecordTestCommandsJobRunner>(recordJobRunner, submitJobRunner->GetCmdBatchRef()));

		RKIT_CHECK(graphicsSubsystem.CreateAndQueueRecordJob(&recordJob, LogicalQueueType::kGraphics, std::move(recordJobRunner)));

		rkit::Job *recordJobPtr = recordJob.Get();

		RKIT_CHECK(graphicsSubsystem.CreateAndQueueSubmitJob(&submitJob, LogicalQueueType::kGraphics, std::move(submitJobRunner), rkit::Span<rkit::Job *>(&recordJobPtr, 1).ToValueISpan()));

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
