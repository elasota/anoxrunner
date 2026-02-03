#include "anox/AnoxModule.h"
#include "anox/AFSArchive.h"
#include "anox/AnoxUtilitiesDriver.h"

#include "rkit/Core/Drivers.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Module.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/ModuleGlue.h"
#include "rkit/Core/Path.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/ProgramDriver.h"
#include "rkit/Core/DriverModuleStub.h"
#include "rkit/Core/ProgramStub.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/Span.h"
#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/StringProto.h"
#include "rkit/Core/String.h"

namespace anox
{
	class ExtractDATProgram final : public rkit::ISimpleProgram
	{
	public:
		rkit::Result Run() override;

	private:
	};

	typedef rkit::DriverModuleStub<rkit::ProgramStubDriver<ExtractDATProgram>, rkit::IProgramDriver, &rkit::Drivers::m_programDriver> ExtractDATModule;
}

rkit::Result anox::ExtractDATProgram::Run()
{
	rkit::IModule *anoxUtilsModule = rkit::GetDrivers().m_moduleDriver->LoadModule(anox::kAnoxNamespaceID, u8"Utilities");
	if (!anoxUtilsModule)
		RKIT_THROW(rkit::ResultCode::kModuleLoadFailed);

	rkit::Span<const rkit::StringView> args = rkit::GetDrivers().m_systemDriver->GetCommandLine();

	if (args.Count() != 2)
	{
		::rkit::log::Error(u8"Usage: ExtractDAT <input> <output>");
		RKIT_THROW(rkit::ResultCode::kInvalidParameter);
	}

	rkit::OSAbsPath inPath;
	RKIT_CHECK(inPath.SetFromEncodedString(args[0]));

	rkit::UniquePtr<rkit::ISeekableReadStream> datFileStream;
	RKIT_TRY_CATCH_RETHROW(rkit::GetDrivers().m_systemDriver->TryOpenFileReadAbs(datFileStream, inPath),
		rkit::CatchContext(
			[]
			{
				::rkit::log::Error(u8"Failed to open input file");
			}
		)
	);

	anox::IUtilitiesDriver *anoxUtils = static_cast<anox::IUtilitiesDriver*>(rkit::GetDrivers().FindDriver(kAnoxNamespaceID, u8"Utilities"));

	RKIT_ASSERT(anoxUtils);

	rkit::UniquePtr<anox::afs::IArchive> archive;
	RKIT_CHECK(anoxUtils->OpenAFSArchive(std::move(datFileStream), archive));

	for (anox::afs::FileHandle fh : archive->GetFiles())
	{
		rkit::UniquePtr<rkit::ISeekableReadStream> fileStream;
		RKIT_CHECK(fh.Open(fileStream));

		uint32_t fileSize = fh.GetFileSize();
		rkit::AsciiStringView filePath = fh.GetFilePath();


		rkit::OSAbsPath outPath;
		RKIT_CHECK(outPath.SetFromEncodedString(args[1]));

		if (true)
			RKIT_THROW(rkit::ResultCode::kNotYetImplemented);	// fix path handling here

		rkit::OSRelPath fpath;
		RKIT_CHECK(fpath.SetFromEncodedString(filePath.ToUTF8()));

		RKIT_CHECK(outPath.Append(fpath));

		rkit::UniquePtr<rkit::ISeekableWriteStream> writeStream;
		RKIT_CHECK(rkit::GetDrivers().m_systemDriver->TryOpenFileWriteAbs(writeStream, outPath, true, true, true));
		if (!writeStream.IsValid())
			RKIT_THROW(rkit::ResultCode::kFileOpenError);

		uint8_t buffer[1024];

		uint32_t sizeRemaining = fileSize;

		while (sizeRemaining > 0)
		{
			uint32_t chunkSize = sizeof(buffer);
			if (chunkSize > sizeRemaining)
				chunkSize = sizeRemaining;

			RKIT_CHECK(fileStream->ReadAll(buffer, chunkSize));
			RKIT_CHECK(writeStream->WriteAll(buffer, chunkSize));

			sizeRemaining -= chunkSize;
		}
	}

	RKIT_RETURN_OK;
}

RKIT_IMPLEMENT_MODULE("Tool", "ExtractDAT", ::anox::ExtractDATModule)
