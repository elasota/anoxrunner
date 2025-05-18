#include "CoroThread.h"

#include "rkit/Core/Future.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Vector.h"
#include "rkit/Core/Algorithm.h"

namespace rkit { namespace utils
{
	class CoroThreadImpl final : public CoroThreadBase
	{
	public:
		explicit CoroThreadImpl(IMallocDriver *alloc);
		~CoroThreadImpl();

		coro::ThreadState GetState() const override;

		Result Resume() override;
		bool TryUnblock() override;

		Result Initialize(size_t stackSize);

	protected:
		coro::Context &GetContext() override;

	private:
		CoroThreadImpl() = delete;

		struct StackBlob
		{
			coro::StackFrameBase *m_topFrame = nullptr;
			size_t m_frameCount = 0;

			uint8_t *m_memoryBase = nullptr;
			size_t m_memorySize = 0;
		};

		void *PushStack(const coro::FrameMetadataBase &metadata);
		void *PushStackNewBlob(size_t size, size_t alignment);
		void PopStack(void *frame, void *prevFrame);

		static void *StaticPushStack(void *userdata, const coro::FrameMetadataBase &metadata);
		static void StaticPopStack(void *userdata, void *frame, void *prevFrame);

		coro::Context m_context;
		StackBlob m_topStackBlob;
		IMallocDriver *m_alloc;
	};

	CoroThreadImpl::CoroThreadImpl(IMallocDriver *alloc)
		: m_alloc(alloc)
	{
		m_context.m_userdata = this;
		m_context.m_allocStack = StaticPushStack;
		m_context.m_freeStack = StaticPopStack;
	}

	CoroThreadImpl::~CoroThreadImpl()
	{
		while (m_context.m_frame != nullptr)
		{
			coro::StackFrameBase *prevFrame = m_context.m_frame->m_prevFrame;

			m_context.m_frame->m_destructFrame(m_context.m_frame);
			m_context.m_freeStack(m_context.m_userdata, m_context.m_frame, prevFrame);
			m_context.m_frame = prevFrame;
		}

		if (m_topStackBlob.m_memoryBase)
			m_alloc->Free(m_topStackBlob.m_memoryBase);
	}

	coro::ThreadState CoroThreadImpl::GetState() const
	{
		if (m_context.m_frame == nullptr)
			return coro::ThreadState::kInactive;

		if (m_context.m_disposition == coro::Disposition::kAwait)
			return coro::ThreadState::kBlocked;

		// In all faulted states, treat as suspended and Resume will catch the fault
		return coro::ThreadState::kSuspended;
	}

	Result CoroThreadImpl::Resume()
	{
		for (;;)
		{
			while (m_context.m_disposition == coro::Disposition::kResume)
			{
				if (m_context.m_frame == nullptr)
					return ResultCode::kOK;

				coro::StackFrameBase *frame = m_context.m_frame;
				coro::Code_t ip = frame->m_ip;

				while (ip != nullptr)
				{
					ip = ip(&m_context, frame).m_code;
				}
			}

			switch (m_context.m_disposition)
			{
			case coro::Disposition::kFailResult:
				return m_context.m_result;
			case coro::Disposition::kAwait:
				if (!TryUnblock())
					return ResultCode::kOK;
				break;
			default:
				return ResultCode::kInternalError;
			}
		}

		return ResultCode::kInternalError;
	}

	bool CoroThreadImpl::TryUnblock()
	{
		RKIT_ASSERT(GetState() == coro::ThreadState::kBlocked);
		RKIT_ASSERT(m_context.m_awaitFuture.IsValid());

		switch (m_context.m_awaitFuture->GetState())
		{
		case FutureState::kCompleted:
			m_context.m_disposition = coro::Disposition::kResume;
			m_context.m_awaitFuture.Reset();
			return true;
		case FutureState::kFailed:
		case FutureState::kAborted:
			m_context.m_disposition = coro::Disposition::kFailResult;
			m_context.m_result = ResultCode::kOperationFailed;
			m_context.m_awaitFuture.Reset();
			return true;
		case FutureState::kWaiting:
			return false;
		default:
			m_context.m_disposition = coro::Disposition::kFailResult;
			m_context.m_result = ResultCode::kInternalError;
			m_context.m_awaitFuture.Reset();
			return true;
		};
	}

	Result CoroThreadImpl::Initialize(size_t stackSize)
	{
		m_topStackBlob.m_memoryBase = static_cast<uint8_t *>(m_alloc->Alloc(stackSize));
		if (!m_topStackBlob.m_memoryBase)
			return ResultCode::kOutOfMemory;

		m_topStackBlob.m_memorySize = stackSize;

		return ResultCode::kOK;
	}

	coro::Context &CoroThreadImpl::GetContext()
	{
		return m_context;
	}

	void *CoroThreadImpl::PushStack(const coro::FrameMetadataBase &metadata)
	{
		const size_t alignment = metadata.m_frameAlign;

		size_t targetStackRemaining = 0;

		if (m_topStackBlob.m_topFrame != nullptr)
			targetStackRemaining = static_cast<size_t>(reinterpret_cast<uint8_t *>(m_topStackBlob.m_topFrame) - m_topStackBlob.m_memoryBase);
		else
			targetStackRemaining = m_topStackBlob.m_memorySize;

		if (targetStackRemaining < metadata.m_frameSize)
			return nullptr;

		targetStackRemaining -= metadata.m_frameSize;
		targetStackRemaining -= (targetStackRemaining & (alignment - 1));

		m_topStackBlob.m_topFrame = reinterpret_cast<coro::StackFrameBase *>(m_topStackBlob.m_memoryBase + targetStackRemaining);
		m_topStackBlob.m_frameCount++;

		return m_topStackBlob.m_topFrame;
	}

	void *CoroThreadImpl::PushStackNewBlob(size_t size, size_t alignment)
	{
		return nullptr;
	}

	void CoroThreadImpl::PopStack(void *frame, void *prevFrame)
	{
		m_topStackBlob.m_frameCount--;
		m_topStackBlob.m_topFrame = static_cast<coro::StackFrameBase *>(prevFrame);

		RKIT_ASSERT((m_topStackBlob.m_frameCount == 0) == (prevFrame == nullptr));
	}

	void *CoroThreadImpl::StaticPushStack(void *userdata, const coro::FrameMetadataBase &metadata)
	{
		return static_cast<CoroThreadImpl *>(userdata)->PushStack(metadata);
	}

	void CoroThreadImpl::StaticPopStack(void *userdata, void *frame, void *prevFrame)
	{
		static_cast<CoroThreadImpl *>(userdata)->PopStack(frame, prevFrame);
	}

	Result CoroThreadBase::Create(UniquePtr<CoroThreadBase> &thread, size_t stackSize)
	{
		UniquePtr<CoroThreadImpl> threadImpl;
		RKIT_CHECK(New<CoroThreadImpl>(threadImpl, GetDrivers().m_mallocDriver));
		RKIT_CHECK(threadImpl->Initialize(stackSize));

		thread = std::move(threadImpl);

		return ResultCode::kOK;
	}
} } // rkit::utils
