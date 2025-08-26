#include "AnoxLoadEntireFileJob.h"

#include "AnoxGameFileSystem.h"

#include "rkit/Core/AsyncFile.h"
#include "rkit/Core/Future.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/JobQueue.h"
#include "rkit/Core/Path.h"
#include "rkit/Core/Vector.h"

namespace anox
{
	class AnoxFileResourcePostIOLoadJobRunner final : public rkit::IJobRunner
	{
	public:
		AnoxFileResourcePostIOLoadJobRunner(rkit::IJobQueue &jobQueue, const rkit::RCPtr<rkit::Vector<uint8_t>> &fileBlob,
			const rkit::Future<rkit::AsyncFileOpenReadResult> &openFileFuture, rkit::RCPtr<rkit::JobSignaller> &&signaller);

		rkit::Result Run() override;

	private:
		rkit::IJobQueue &m_jobQueue;
		rkit::RCPtr<rkit::Vector<uint8_t>> m_fileBlob;
		rkit::Future<rkit::AsyncFileOpenReadResult> m_openFileFuture;
		rkit::RCPtr<rkit::JobSignaller> m_signaller;
	};

	class AnoxFileResourceIOCompleter final
	{
	public:
		explicit AnoxFileResourceIOCompleter(const rkit::RCPtr<rkit::JobSignaller> &signaller, const rkit::RCPtr<rkit::Vector<uint8_t>> &fileBlob, size_t expectedBytes);

		void SetSelf(rkit::UniquePtr<AnoxFileResourceIOCompleter> &&self);

		static void CompleteCallback(void *userdata, const rkit::Result &result, size_t bytesRead);

	private:
		void Complete(const rkit::Result &result, size_t bytesRead);

		rkit::RCPtr<rkit::JobSignaller> m_signaller;
		rkit::UniquePtr<AnoxFileResourceIOCompleter> m_self;
		rkit::RCPtr<rkit::Vector<uint8_t>> m_fileBlob;
		size_t m_expectedBytes;
	};

	AnoxFileResourcePostIOLoadJobRunner::AnoxFileResourcePostIOLoadJobRunner(rkit::IJobQueue &jobQueue, const rkit::RCPtr<rkit::Vector<uint8_t>> &fileBlob, const rkit::Future<rkit::AsyncFileOpenReadResult> &openFileFuture, rkit::RCPtr<rkit::JobSignaller> &&signaller)
		: m_jobQueue(jobQueue)
		, m_fileBlob(fileBlob)
		, m_openFileFuture(openFileFuture)
		, m_signaller(std::move(signaller))
	{
	}

	rkit::Result AnoxFileResourcePostIOLoadJobRunner::Run()
	{
		const rkit::FilePos_t fileSize = m_openFileFuture.GetResult().m_initialSize;

		if (fileSize > std::numeric_limits<size_t>::max())
			return rkit::ResultCode::kOutOfMemory;

		const size_t size = static_cast<size_t>(fileSize);

		RKIT_CHECK(m_fileBlob->Resize(size));

		if (size == 0)
		{
			m_signaller->SignalDone(rkit::ResultCode::kOK);
			return rkit::ResultCode::kOK;
		}

		rkit::UniquePtr<rkit::IAsyncReadRequester> requester;
		RKIT_CHECK(m_openFileFuture.GetResult().m_file->CreateReadRequester(requester));

		rkit::UniquePtr<AnoxFileResourceIOCompleter> completer;
		RKIT_CHECK(rkit::New<AnoxFileResourceIOCompleter>(completer, m_signaller, m_fileBlob, size));

		AnoxFileResourceIOCompleter *completerPtr = completer.Get();
		completer->SetSelf(std::move(completer));

		requester->PostReadRequest(m_jobQueue, m_fileBlob->GetBuffer(), 0, size, completerPtr, AnoxFileResourceIOCompleter::CompleteCallback);

		return rkit::ResultCode::kOK;
	}

	AnoxFileResourceIOCompleter::AnoxFileResourceIOCompleter(const rkit::RCPtr<rkit::JobSignaller> &signaller, const rkit::RCPtr<rkit::Vector<uint8_t>> &fileBlob, size_t expectedBytes)
		: m_signaller(signaller)
		, m_fileBlob(fileBlob)
		, m_expectedBytes(expectedBytes)
	{
	}

	void AnoxFileResourceIOCompleter::SetSelf(rkit::UniquePtr<AnoxFileResourceIOCompleter> &&self)
	{
		m_self = std::move(self);
	}

	void AnoxFileResourceIOCompleter::CompleteCallback(void *userdata, const rkit::Result &result, size_t bytesRead)
	{
		static_cast<AnoxFileResourceIOCompleter *>(userdata)->Complete(result, bytesRead);
	}

	void AnoxFileResourceIOCompleter::Complete(const rkit::Result &result, size_t bytesRead)
	{
		if (rkit::utils::ResultIsOK(result) && bytesRead != m_expectedBytes)
			m_signaller->SignalDone(rkit::ResultCode::kIOReadError);
		else
			m_signaller->SignalDone(result);

		m_self.Reset();
	}

	rkit::Result CreateLoadEntireFileJob(rkit::RCPtr<rkit::Job> &outJob, const rkit::RCPtr<rkit::Vector<uint8_t>> &blob, AnoxGameFileSystemBase &fileSystem, const rkit::CIPathView &path)
	{
		rkit::RCPtr<rkit::Job> openJob;

		rkit::FutureContainerPtr<rkit::AsyncFileOpenReadResult> openFileFutureContainer;
		RKIT_CHECK(rkit::New<rkit::FutureContainer<rkit::AsyncFileOpenReadResult>>(openFileFutureContainer));

		RKIT_CHECK(fileSystem.OpenNamedFileAsync(openJob, openFileFutureContainer, path));

		rkit::Future<rkit::AsyncFileOpenReadResult> openFileFuture(openFileFutureContainer);

		rkit::IJobQueue &jobQueue = fileSystem.GetJobQueue();

		rkit::RCPtr<rkit::JobSignaller> signaller;
		rkit::RCPtr<rkit::Job> ioRequestJob;
		RKIT_CHECK(jobQueue.CreateSignalledJob(signaller, ioRequestJob));

		rkit::UniquePtr<rkit::IJobRunner> postIOJobRunner;
		rkit::RCPtr<rkit::Job> postIOJob;

		RKIT_CHECK(rkit::New<AnoxFileResourcePostIOLoadJobRunner>(postIOJobRunner, fileSystem.GetJobQueue(), blob, openFileFuture, std::move(signaller)));

		RKIT_CHECK(jobQueue.CreateJob(&postIOJob, rkit::JobType::kIO, std::move(postIOJobRunner), openJob));

		RKIT_CHECK(jobQueue.CreateJob(&outJob, rkit::JobType::kNormalPriority, rkit::UniquePtr<rkit::IJobRunner>(), ioRequestJob));

		return rkit::ResultCode::kOK;
	}
}
