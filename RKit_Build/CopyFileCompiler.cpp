#include "CopyFileCompiler.h"

#include "rkit/Core/Stream.h"

namespace rkit { namespace buildsystem
{
	CopyFileCompiler::CopyFileCompiler()
	{
	}

	bool CopyFileCompiler::HasAnalysisStage() const
	{
		return false;
	}

	Result CopyFileCompiler::RunAnalysis(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback)
	{
		return ResultCode::kInternalError;
	}

	Result CopyFileCompiler::RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback)
	{
		CIPath inPath;
		RKIT_CHECK(inPath.Set(depsNode->GetIdentifier()));

		UniquePtr<ISeekableReadStream> inFile;
		RKIT_CHECK(feedback->OpenInput(BuildFileLocation::kSourceDir, inPath, inFile));

		CIPath outPath;
		RKIT_CHECK(outPath.Set(StringSliceView(u8"loose")));
		RKIT_CHECK(outPath.Append(inPath));

		UniquePtr<ISeekableReadWriteStream> outFile;
		RKIT_CHECK(feedback->OpenOutput(BuildFileLocation::kOutputFiles, outPath, outFile));

		FilePos_t amountRemaining = inFile->GetSize();

		while (amountRemaining > 0)
		{
			uint8_t buffer[2048];
			size_t amountToCopy = sizeof(buffer);
			if (amountToCopy > amountRemaining)
				amountToCopy = static_cast<size_t>(amountRemaining);

			RKIT_CHECK(inFile->ReadAll(buffer, amountToCopy));
			RKIT_CHECK(outFile->WriteAll(buffer, amountToCopy));

			amountRemaining -= static_cast<FilePos_t>(amountToCopy);
		}

		RKIT_RETURN_OK;
	}

	uint32_t CopyFileCompiler::GetVersion() const
	{
		return 1;
	}
} } // rkit::buildsystem
