#include "CoroThread.h"

#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Vector.h"
#include "rkit/Core/Algorithm.h"

namespace rkit::utils
{
	class CoroThreadImpl final : public CoroThreadBase
	{
	public:
		explicit CoroThreadImpl(IMallocDriver *alloc);
		~CoroThreadImpl();

		coro::ThreadState GetState() const override;

		Result Resume() override;

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
		void PopStack(void *prevFrameBase);

		static void *StaticPushStack(void *userdata, const coro::FrameMetadataBase &metadata);
		static void StaticPopStack(void *userdata, void *prevFrameBase);

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
			m_context.m_freeStack(m_context.m_userdata, m_context.m_frame);
			m_context.m_frame = prevFrame;
		}

		if (m_topStackBlob.m_memoryBase)
			m_alloc->Free(m_topStackBlob.m_memoryBase);
	}

	coro::ThreadState CoroThreadImpl::GetState() const
	{
		if (m_context.m_disposition != coro::Disposition::kStackOverflow)
			return coro::ThreadState::kFaulted;

		if (m_context.m_frame == nullptr)
			return coro::ThreadState::kInactive;

		if (m_context.m_disposition != coro::Disposition::kResume)
			return coro::ThreadState::kSuspended;

		return coro::ThreadState::kFaulted;
	}

	Result CoroThreadImpl::Resume()
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

		return ResultCode::kNotYetImplemented;
	}

	Result CoroThreadImpl::Initialize(size_t stackSize)
	{
		m_topStackBlob.m_memoryBase = static_cast<uint8_t *>(m_alloc->Alloc(stackSize));
		if (!m_topStackBlob.m_memoryBase)
			return ResultCode::kOutOfMemory;

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

	void CoroThreadImpl::PopStack(void *prevFrameBase)
	{
		m_topStackBlob.m_frameCount--;
		m_topStackBlob.m_topFrame = static_cast<coro::StackFrameBase *>(prevFrameBase);

		RKIT_ASSERT((m_topStackBlob.m_frameCount == 0) == (prevFrameBase == nullptr));
	}

	void *CoroThreadImpl::StaticPushStack(void *userdata, const coro::FrameMetadataBase &metadata)
	{
		return static_cast<CoroThreadImpl *>(userdata)->PushStack(metadata);
	}

	void CoroThreadImpl::StaticPopStack(void *userdata, void *prevFrameBase)
	{
		static_cast<CoroThreadImpl *>(userdata)->PopStack(prevFrameBase);
	}

	Result CoroThreadBase::Create(UniquePtr<CoroThreadBase> &thread, size_t stackSize)
	{
		UniquePtr<CoroThreadImpl> threadImpl;
		RKIT_CHECK(New<CoroThreadImpl>(threadImpl, GetDrivers().m_mallocDriver));
		RKIT_CHECK(threadImpl->Initialize(stackSize));

		thread = std::move(threadImpl);

		return ResultCode::kOK;
	}
}
