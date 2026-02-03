#include "AnoxGameFileSystem.h"

#include "rkit/Data/ContentID.h"

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

		rkit::Result OpenNamedFileBlocking(rkit::RCPtr<rkit::Job> &openJob, const rkit::FutureContainerPtr<rkit::UniquePtr<rkit::ISeekableReadStream>> &outStream, const rkit::CIPathView &path) override;
		rkit::Result OpenNamedFileAsync(rkit::RCPtr<rkit::Job> &openJob, const rkit::FutureContainerPtr<rkit::AsyncFileOpenReadResult> &outStream, const rkit::CIPathView &path) override;
		rkit::Result OpenContentFileAsync(rkit::RCPtr<rkit::Job> &openJob, const rkit::FutureContainerPtr<rkit::AsyncFileOpenReadResult> &outStream, const rkit::data::ContentID &contentID) override;

		rkit::IJobQueue &GetJobQueue() const override;

	private:
		rkit::IJobQueue &m_jobQueue;
	};

	AnoxGameFileSystem::AnoxGameFileSystem(rkit::IJobQueue &jobQueue)
		: m_jobQueue(jobQueue)
	{
	}

	rkit::Result AnoxGameFileSystem::OpenNamedFileBlocking(rkit::RCPtr<rkit::Job> &openJob, const rkit::FutureContainerPtr<rkit::UniquePtr<rkit::ISeekableReadStream>> &outStream, const rkit::CIPathView &path)
	{
		rkit::CIPath fullPath;
		RKIT_CHECK(fullPath.Set(rkit::CIPathView(u8"files")));
		RKIT_CHECK(fullPath.Append(path));

		RKIT_CHECK(rkit::GetDrivers().m_systemDriver->AsyncOpenFileRead(m_jobQueue, openJob, nullptr, outStream, rkit::FileLocation::kGameDirectory, fullPath));

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxGameFileSystem::OpenNamedFileAsync(rkit::RCPtr<rkit::Job> &openJob, const rkit::FutureContainerPtr<rkit::AsyncFileOpenReadResult> &outStream, const rkit::CIPathView &path)
	{
		rkit::CIPath fullPath;
		RKIT_CHECK(fullPath.Set(rkit::CIPathView(u8"files")));
		RKIT_CHECK(fullPath.Append(path));

		RKIT_CHECK(rkit::GetDrivers().m_systemDriver->AsyncOpenFileAsyncRead(m_jobQueue, openJob, nullptr, outStream, rkit::FileLocation::kGameDirectory, fullPath));
		RKIT_RETURN_OK;
	}

	rkit::Result AnoxGameFileSystem::OpenContentFileAsync(rkit::RCPtr<rkit::Job> &openJob, const rkit::FutureContainerPtr<rkit::AsyncFileOpenReadResult> &outStream, const rkit::data::ContentID &contentID)
	{
		rkit::data::ContentIDString contentIDString = contentID.ToString();

		rkit::CIPath fullPath;
		RKIT_CHECK(fullPath.Set(rkit::CIPathView(u8"content")));
		RKIT_CHECK(fullPath.Append(rkit::CIPathView(contentIDString.ToStringView())));

		RKIT_CHECK(rkit::GetDrivers().m_systemDriver->AsyncOpenFileAsyncRead(m_jobQueue, openJob, nullptr, outStream, rkit::FileLocation::kGameDirectory, fullPath));
		RKIT_RETURN_OK;
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

		RKIT_RETURN_OK;
	}
}
