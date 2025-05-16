#include "AnoxFileResource.h"
#include "AnoxGameFileSystem.h"

#include "rkit/Core/FutureProtos.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/JobQueue.h"
#include "rkit/Core/Future.h"
#include "rkit/Core/AsyncFile.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Path.h"
#include "rkit/Core/StaticArray.h"
#include "rkit/Core/Vector.h"

namespace anox
{
	// Open -> Post IO Job       IO Complete
	//             ^-----> IO Request --^
	class AnoxFileResourcePostIOLoadJobRunner;

	class AnoxFileResource : public AnoxFileResourceBase
	{
	public:
		friend class AnoxFileResourcePostIOLoadJobRunner;

		AnoxFileResource();

		rkit::Span<const uint8_t> GetContents() const override;

	private:
		rkit::Vector<uint8_t> m_fileBytes;
	};

	class AnoxFileResourcePostIOLoadJobRunner final : public rkit::IJobRunner
	{
	public:
		AnoxFileResourcePostIOLoadJobRunner(rkit::IJobQueue &jobQueue, const rkit::RCPtr<AnoxFileResource> &fileResource,
			const rkit::Future<rkit::AsyncFileOpenReadResult> &openFileFuture, rkit::RCPtr<rkit::JobSignaller> &&signaller);

		rkit::Result Run() override;

	private:
		rkit::IJobQueue &m_jobQueue;
		rkit::RCPtr<AnoxFileResource> m_fileResource;
		rkit::Future<rkit::AsyncFileOpenReadResult> m_openFileFuture;
		rkit::RCPtr<rkit::JobSignaller> m_signaller;
	};

	class AnoxFileResourceIOCompleter final
	{
	public:
		explicit AnoxFileResourceIOCompleter(const rkit::RCPtr<rkit::JobSignaller> &signaller, const rkit::RCPtr<AnoxFileResource> &resource, size_t expectedBytes);

		void SetSelf(rkit::UniquePtr<AnoxFileResourceIOCompleter> &&self);

		static void CompleteCallback(void *userdata, const rkit::Result &result, size_t bytesRead);

	private:
		void Complete(const rkit::Result &result, size_t bytesRead);

		rkit::RCPtr<rkit::JobSignaller> m_signaller;
		rkit::UniquePtr<AnoxFileResourceIOCompleter> m_self;
		rkit::RCPtr<AnoxFileResource> m_resource;
		size_t m_expectedBytes;
	};

	class AnoxFileResourceIOCompleteJobRunner final : public rkit::IJobRunner
	{
	public:
		AnoxFileResourceIOCompleteJobRunner();

		rkit::Result Run() override;
	};

	class AnoxFileResourceLoader final : public AnoxFileResourceLoaderBase
	{
	public:
		rkit::Result CreateIOJob(const rkit::RCPtr<AnoxFileResourceBase> &resource, AnoxGameFileSystemBase &fileSystem, const rkit::CIPathView &key, rkit::RCPtr<rkit::Job> &outJob) override;
		rkit::Result RunProcessingTask(AnoxFileResourceBase &resource, const rkit::CIPathView &key) override;
		rkit::Result CreateResourceObject(rkit::UniquePtr<AnoxFileResourceBase> &outResource) override;
	};

	AnoxFileResource::AnoxFileResource()
	{
	}

	rkit::Span<const uint8_t> AnoxFileResource::GetContents() const
	{
		return m_fileBytes.ToSpan();
	}

	AnoxFileResourcePostIOLoadJobRunner::AnoxFileResourcePostIOLoadJobRunner(rkit::IJobQueue &jobQueue, const rkit::RCPtr<AnoxFileResource> &fileResource, const rkit::Future<rkit::AsyncFileOpenReadResult> &openFileFuture, rkit::RCPtr<rkit::JobSignaller> &&signaller)
		: m_jobQueue(jobQueue)
		, m_fileResource(fileResource)
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

		RKIT_CHECK(m_fileResource->m_fileBytes.Resize(size));

		if (size == 0)
		{
			m_signaller->SignalDone(rkit::ResultCode::kOK);
			return rkit::ResultCode::kOK;
		}

		rkit::UniquePtr<rkit::IAsyncReadRequester> requester;
		RKIT_CHECK(m_openFileFuture.GetResult().m_file->CreateReadRequester(requester));

		rkit::UniquePtr<AnoxFileResourceIOCompleter> completer;
		RKIT_CHECK(rkit::New<AnoxFileResourceIOCompleter>(completer, m_signaller, m_fileResource, size));

		AnoxFileResourceIOCompleter *completerPtr = completer.Get();
		completer->SetSelf(std::move(completer));

		requester->PostReadRequest(m_jobQueue, m_fileResource->m_fileBytes.GetBuffer(), 0, size, completerPtr, AnoxFileResourceIOCompleter::CompleteCallback);

		return rkit::ResultCode::kOK;
	}

	AnoxFileResourceIOCompleter::AnoxFileResourceIOCompleter(const rkit::RCPtr<rkit::JobSignaller> &signaller, const rkit::RCPtr<AnoxFileResource> &resource, size_t expectedBytes)
		: m_signaller(signaller)
		, m_resource(resource)
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

	rkit::Result AnoxFileResourceLoader::CreateIOJob(const rkit::RCPtr<AnoxFileResourceBase> &resource, AnoxGameFileSystemBase &fileSystem, const rkit::CIPathView &key, rkit::RCPtr<rkit::Job> &outJob)
	{
		rkit::RCPtr<rkit::Job> openJob;

		rkit::CIPath loosePath;
		RKIT_CHECK(loosePath.AppendComponent("loose"));
		RKIT_CHECK(loosePath.Append(key));

		rkit::FutureContainerPtr<rkit::AsyncFileOpenReadResult> openFileFutureContainer;
		RKIT_CHECK(rkit::New<rkit::FutureContainer<rkit::AsyncFileOpenReadResult>>(openFileFutureContainer));

		RKIT_CHECK(fileSystem.OpenNamedFileAsync(openJob, openFileFutureContainer, loosePath));

		rkit::Future<rkit::AsyncFileOpenReadResult> openFileFuture(openFileFutureContainer);

		rkit::IJobQueue &jobQueue = fileSystem.GetJobQueue();

		rkit::RCPtr<rkit::JobSignaller> signaller;
		rkit::RCPtr<rkit::Job> ioRequestJob;
		RKIT_CHECK(jobQueue.CreateSignalledJob(signaller, ioRequestJob));

		rkit::UniquePtr<rkit::IJobRunner> postIOJobRunner;
		rkit::RCPtr<rkit::Job> postIOJob;

		RKIT_CHECK(rkit::New<AnoxFileResourcePostIOLoadJobRunner>(postIOJobRunner, fileSystem.GetJobQueue(), resource.StaticCast<AnoxFileResource>(), openFileFuture, std::move(signaller)));

		RKIT_CHECK(jobQueue.CreateJob(&postIOJob, rkit::JobType::kIO, std::move(postIOJobRunner), openJob));

		rkit::StaticArray<rkit::Job *, 2> completeDeps;
		completeDeps[0] = postIOJob.Get();
		completeDeps[1] = ioRequestJob.Get();

		RKIT_CHECK(jobQueue.CreateJob(&outJob, rkit::JobType::kNormalPriority, rkit::UniquePtr<rkit::IJobRunner>(), completeDeps.ToSpan()));

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxFileResourceLoader::RunProcessingTask(AnoxFileResourceBase &resource, const rkit::CIPathView &key)
	{
		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxFileResourceLoader::CreateResourceObject(rkit::UniquePtr<AnoxFileResourceBase> &outResource)
	{
		return rkit::New<AnoxFileResource>(outResource);
	}

	rkit::Result AnoxFileResourceLoaderBase::Create(rkit::RCPtr<AnoxFileResourceLoaderBase> &outResLoader)
	{
		rkit::RCPtr<AnoxFileResourceLoader> resLoader;
		RKIT_CHECK(rkit::New<AnoxFileResourceLoader>(resLoader));

		outResLoader = std::move(resLoader);

		return rkit::ResultCode::kOK;
	}
}
