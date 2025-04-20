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
		AnoxGameFileSystem();

		rkit::Result OpenNamedFile(rkit::UniquePtr<rkit::ISeekableReadStream> &stream, const rkit::CIPathView &path) override;
	};


	AnoxGameFileSystem::AnoxGameFileSystem()
	{
	}

	rkit::Result AnoxGameFileSystem::OpenNamedFile(rkit::UniquePtr<rkit::ISeekableReadStream> &stream, const rkit::CIPathView &path)
	{
		rkit::ISystemDriver *sysDriver = rkit::GetDrivers().m_systemDriver;

		rkit::CIPath extPath;
		RKIT_CHECK(extPath.Set(rkit::CIPathSliceView("files")));
		RKIT_CHECK(extPath.Append(path));

		rkit::UniquePtr<rkit::ISeekableReadStream> srStream;
		RKIT_CHECK(sysDriver->OpenFileRead(srStream, rkit::FileLocation::kGameDirectory, extPath));

		stream = std::move(srStream);

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxGameFileSystemBase::Create(rkit::UniquePtr<AnoxGameFileSystemBase> &outFileSystem)
	{
		rkit::UniquePtr<AnoxGameFileSystem> fileSystem;
		RKIT_CHECK(rkit::New<AnoxGameFileSystem>(fileSystem));

		outFileSystem = std::move(fileSystem);

		return rkit::ResultCode::kOK;
	}
}
