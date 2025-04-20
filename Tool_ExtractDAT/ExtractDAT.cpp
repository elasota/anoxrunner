#include "anox/AnoxModule.h"
#include "anox/AFSArchive.h"
#include "anox/UtilitiesDriver.h"

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
	rkit::IModule *anoxUtilsModule = rkit::GetDrivers().m_moduleDriver->LoadModule(anox::kAnoxNamespaceID, "Utilities");
	if (!anoxUtilsModule)
		return rkit::ResultCode::kModuleLoadFailed;

	rkit::Span<const rkit::StringView> args = rkit::GetDrivers().m_systemDriver->GetCommandLine();

	if (args.Count() != 2)
	{
		::rkit::log::Error("Usage: ExtractDAT <input> <output>");
		return rkit::ResultCode::kInvalidParameter;
	}

	rkit::OSAbsPath inPath;
	RKIT_CHECK((inPath.SetFromEncodedString<char, rkit::PathEncoding::kUTF8>(args[0])));

	rkit::UniquePtr<rkit::ISeekableReadStream> datFileStream;
	rkit::Result openResult = rkit::GetDrivers().m_systemDriver->OpenFileReadAbs(datFileStream, inPath);
	if (rkit::utils::GetResultCode(openResult) == rkit::ResultCode::kFileOpenError)
	{
		::rkit::log::Error("Failed to open input file");
		return rkit::ResultCode::kFileOpenError;
	}

	RKIT_CHECK(openResult);

	anox::IUtilitiesDriver *anoxUtils = static_cast<anox::IUtilitiesDriver*>(rkit::GetDrivers().FindDriver(kAnoxNamespaceID, "Utilities"));

	RKIT_ASSERT(anoxUtils);

	rkit::UniquePtr<anox::afs::IArchive> archive;
	RKIT_CHECK(anoxUtils->OpenAFSArchive(std::move(datFileStream), archive));

	for (anox::afs::FileHandle fh : archive->GetFiles())
	{
		rkit::UniquePtr<rkit::ISeekableReadStream> fileStream;
		RKIT_CHECK(fh.Open(fileStream));

		uint32_t fileSize = fh.GetFileSize();
		rkit::StringView filePath = fh.GetFilePath();


		rkit::OSAbsPath outPath;
		RKIT_CHECK((outPath.SetFromEncodedString<char, rkit::PathEncoding::kUTF8>(args[1])));

		rkit::log::LogInfo(fh.GetFilePath().GetChars());

		if (true)
			return rkit::ResultCode::kNotYetImplemented;	// fix path handling here

		rkit::OSRelPath fpath;
		RKIT_CHECK((fpath.SetFromEncodedString<char, rkit::PathEncoding::kUTF8>(fh.GetFilePath())));

		RKIT_CHECK(outPath.Append(fpath));

		rkit::UniquePtr<rkit::ISeekableWriteStream> writeStream;
		openResult = rkit::GetDrivers().m_systemDriver->OpenFileWriteAbs(writeStream, outPath, true, true, true);
		if (rkit::utils::GetResultCode(openResult) == rkit::ResultCode::kFileOpenError)
			return rkit::ResultCode::kFileOpenError;

		RKIT_CHECK(openResult);

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

	return rkit::ResultCode::kOK;
}

RKIT_IMPLEMENT_MODULE("Tool", "ExtractDAT", ::anox::ExtractDATModule)
