#include "AnoxGameFileSystem.h"

#include "rkit/Core/Drivers.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/Path.h"

namespace anox
{
	class AnoxGameFileSystem final : public AnoxGameFileSystemBase
	{
	public:
		AnoxGameFileSystem(rkit::IJobQueue &jobQueue);

		rkit::Result OpenNamedFileBlocking(rkit::RCPtr<rkit::Job> &openJob, const rkit::Future<rkit::UniquePtr<rkit::ISeekableReadStream>> &outStream, const rkit::CIPathView &path) override;
		rkit::Result OpenNamedFileAsync(rkit::RCPtr<rkit::Job> &openJob, const rkit::Future<rkit::AsyncFileOpenReadResult> &outStream, const rkit::CIPathView &path) override;

		rkit::IJobQueue &GetJobQueue() const override;

	private:
		rkit::IJobQueue &m_jobQueue;
	};

	AnoxGameFileSystem::AnoxGameFileSystem(rkit::IJobQueue &jobQueue)
		: m_jobQueue(jobQueue)
	{
	}

	rkit::Result AnoxGameFileSystem::OpenNamedFileBlocking(rkit::RCPtr<rkit::Job> &openJob, const rkit::Future<rkit::UniquePtr<rkit::ISeekableReadStream>> &outStream, const rkit::CIPathView &path)
	{
		rkit::CIPath fullPath;
		RKIT_CHECK(fullPath.Set(rkit::CIPathView("files")));
		RKIT_CHECK(fullPath.Append(path));

		RKIT_CHECK(rkit::GetDrivers().m_systemDriver->AsyncOpenFileRead(m_jobQueue, openJob, nullptr, outStream, rkit::FileLocation::kGameDirectory, fullPath));

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxGameFileSystem::OpenNamedFileAsync(rkit::RCPtr<rkit::Job> &openJob, const rkit::Future<rkit::AsyncFileOpenReadResult> &outStream, const rkit::CIPathView &path)
	{
		rkit::CIPath fullPath;
		RKIT_CHECK(fullPath.Set(rkit::CIPathView("files")));
		RKIT_CHECK(fullPath.Append(path));

		RKIT_CHECK(rkit::GetDrivers().m_systemDriver->AsyncOpenFileAsyncRead(m_jobQueue, openJob, nullptr, outStream, rkit::FileLocation::kGameDirectory, fullPath));
		return rkit::ResultCode::kOK;
	}

	rkit::IJobQueue &AnoxGameFileSystem::GetJobQueue() const
	{
		return m_jobQueue;
	}

	rkit::Result AnoxGameFileSystemBase::Create(rkit::UniquePtr<AnoxGameFileSystemBase> &outFileSystem, rkit::IJobQueue &jobQueue)
	{
		rkit::UniquePtr<AnoxGameFileSystem> fileSystem;
		RKIT_CHECK(rkit::New<AnoxGameFileSystem>(fileSystem, jobQueue));

		outFileSystem = std::move(fileSystem);

		return rkit::ResultCode::kOK;
	}
}
