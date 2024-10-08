#include "AnoxAFSArchive.h"

#include "anox/AnoxModule.h"
#include "anox/AFSFormat.h"
#include "anox/UtilitiesDriver.h"

#include "rkit/Core/DriverModuleStub.h"
#include "rkit/Core/UtilitiesDriver.h"
#include "rkit/Core/ModuleGlue.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/String.h"


namespace anox
{
	class UtilitiesDriver final : public anox::IUtilitiesDriver
	{
	public:

	private:
		rkit::Result InitDriver() override;
		void ShutdownDriver() override;

		uint32_t GetDriverNamespaceID() const override { return anox::kAnoxNamespaceID; }
		rkit::StringView GetDriverName() const override { return "Utilities"; }

		rkit::Result OpenAFSArchive(rkit::UniquePtr<rkit::ISeekableReadStream> &&stream, rkit::UniquePtr<anox::afs::IArchive> &outArchive) override;
	};

	typedef rkit::CustomDriverModuleStub<UtilitiesDriver> UtilitiesModule;
}

rkit::Result anox::UtilitiesDriver::OpenAFSArchive(rkit::UniquePtr<rkit::ISeekableReadStream> &&streamSrc, rkit::UniquePtr<anox::afs::IArchive> &outArchive)
{
	rkit::UniquePtr<rkit::ISeekableReadStream> stream(std::move(streamSrc));

	rkit::UniquePtr<anox::afs::Archive> archive;
	RKIT_CHECK(rkit::New<anox::afs::Archive>(archive, rkit::GetDrivers().m_mallocDriver));

	RKIT_CHECK(archive->Open(std::move(stream)));

	outArchive = rkit::UniquePtr<anox::afs::IArchive>(std::move(archive));

	return rkit::ResultCode::kOK;
}

rkit::Result anox::UtilitiesDriver::InitDriver()
{
	return rkit::ResultCode::kOK;
}

void anox::UtilitiesDriver::ShutdownDriver()
{
}


RKIT_IMPLEMENT_MODULE("Anox", "Utilities", ::anox::UtilitiesModule)
