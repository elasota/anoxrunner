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
		RKIT_THROW(ResultCode::kInternalError);
	}

	Result CopyFileCompiler::RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback)
	{
		CIPath inPath;
		inPath.Set(depsNode->GetIdentifier());

		UniquePtr<ISeekableReadStream> inFile;
		feedback->OpenInput(BuildFileLocation::kSourceDir, inPath, inFile);

		CIPath outPath;
		outPath.Set(StringSliceView(u8"loose"));
		outPath.Append(inPath);

		UniquePtr<ISeekableReadWriteStream> outFile;
		feedback->OpenOutput(BuildFileLocation::kOutputFiles, outPath, outFile);

		FilePos_t amountRemaining = inFile->GetSize();

		while (amountRemaining > 0)
		{
			uint8_t buffer[2048];
			size_t amountToCopy = sizeof(buffer);
			if (amountToCopy > amountRemaining)
				amountToCopy = static_cast<size_t>(amountRemaining);

			inFile->ReadAll(buffer, amountToCopy);
			outFile->WriteAll(buffer, amountToCopy);

			amountRemaining -= static_cast<FilePos_t>(amountToCopy);
		}

		RKIT_RETURN_OK;
	}

	uint32_t CopyFileCompiler::GetVersion() const
	{
		return 1;
	}
} } // rkit::buildsystem
