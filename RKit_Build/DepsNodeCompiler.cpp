#include "DepsNodeCompiler.h"

#include "rkit/Core/HybridVector.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/Vector.h"

#include "rkit/Core/UtilitiesDriver.h"

#include "rkit/Utilities/TextParser.h"

namespace rkit::buildsystem
{
	DepsNodeCompiler::DepsNodeCompiler()
	{
	}

	bool DepsNodeCompiler::HasAnalysisStage() const
	{
		return true;
	}

	Result DepsNodeCompiler::RunAnalysis(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback)
	{
		GetDrivers().m_moduleDriver->LoadModule(rkit::IModuleDriver::kDefaultNamespace, "Data");

		CIPath nodePath;
		RKIT_CHECK(nodePath.Set(depsNode->GetIdentifier()));

		rkit::UniquePtr<ISeekableReadStream> stream;
		RKIT_CHECK(feedback->TryOpenInput(BuildFileLocation::kSourceDir, nodePath, stream));

		if (!stream.Get())
		{
			rkit::log::ErrorFmt("Failed to open deps file '%s'", depsNode->GetIdentifier().GetChars());
			return ResultCode::kFileOpenError;
		}

		IUtilitiesDriver *utils = GetDrivers().m_utilitiesDriver;

		Vector<uint8_t> fileContents;
		RKIT_CHECK(utils->ReadEntireFile(*stream, fileContents));

		stream.Reset();

		Span<const char> fileChars(reinterpret_cast<char *>(fileContents.GetBuffer()), fileContents.Count());

		UniquePtr<utils::ITextParser> parser;
		RKIT_CHECK(utils->CreateTextParser(fileChars, utils::TextParserCommentType::kBash, utils::TextParserLexerType::kSimple, parser));

		for (;;)
		{
			Span<const char> token;

			RKIT_CHECK(parser->SkipWhitespace());

			size_t line = 0;
			size_t col = 0;
			parser->GetLocation(line, col);

			bool haveToken = false;
			RKIT_CHECK(parser->ReadToken(haveToken, token));

			if (!haveToken)
				break;

			Vector<char> constructedPath;
			size_t numPathChunks = 0;
			size_t chunkStart = 0;
			for (size_t i = 0; true; i++)
			{
				if (i == token.Count() || token[i] == '/')
				{
					bool handledOK = false;
					RKIT_CHECK(HandlePathChunk(constructedPath, handledOK, numPathChunks, depsNode, token.SubSpan(chunkStart, i - chunkStart)));

					if (!handledOK)
					{
						rkit::log::ErrorFmt("%zu:%zu: Path is invalid", line, col);
						return ResultCode::kMalformedFile;
					}

					numPathChunks++;
					chunkStart = i + 1;
				}

				if (i == token.Count())
					break;
			}

			utils->NormalizeFilePath(constructedPath.ToSpan());

			if (!utils->ValidateFilePath(constructedPath.ToSpan(), true))
			{
				rkit::log::ErrorFmt("%zu:%zu: Path is invalid", line, col);
				return ResultCode::kMalformedFile;
			}

			RKIT_CHECK(constructedPath.Append('\0'));

			StringView pathStrView(constructedPath.GetBuffer(), constructedPath.Count() - 1);

			Vector<String> wildcardMatchStrings;
			HybridVector<StringView, 1> pathStringViews;

			if (utils->ContainsWildcards(pathStrView))
			{
				struct EnumeratorAdder
				{
					static Result EnumerateFile(void *userdata, const String &str)
					{
						return static_cast<Vector<String> *>(userdata)->Append(str);
					}
				};

				//RKIT_CHECK(feedback->EnumerateFiles(rkit::buildsystem::BuildFileLocation::kSourceDir, pathStrView, &wildcardMatchStrings, EnumeratorAdder::EnumerateFile));
				if (true)
					return ResultCode::kNotYetImplemented;

				const size_t numFiles = wildcardMatchStrings.Count();

				RKIT_CHECK(pathStringViews.Resize(numFiles));

				for (size_t i = 0; i < numFiles; i++)
					pathStringViews[i] = wildcardMatchStrings[i];
			}
			else
			{
				RKIT_CHECK(pathStringViews.Append(pathStrView));
			}

			for (const StringView &pathStrViewItem : pathStringViews)
			{
				StringView extStrView;
				if (!utils->FindFilePathExtension(pathStrViewItem, extStrView))
				{
					rkit::log::ErrorFmt("%zu:%zu: Path has no extension", line, col);
					return ResultCode::kMalformedFile;
				}

				uint32_t nodeNamespace = 0;
				uint32_t nodeType = 0;
				if (!feedback->FindNodeTypeByFileExtension(extStrView, nodeNamespace, nodeType))
				{
					rkit::log::ErrorFmt("%zu:%zu: Unrecognized file extension: '%s'", line, col, extStrView.GetChars());
					return ResultCode::kMalformedFile;
				}

				RKIT_CHECK(feedback->AddNodeDependency(nodeNamespace, nodeType, BuildFileLocation::kSourceDir, pathStrViewItem));
			}
		}

		return ResultCode::kOK;
	}

	Result DepsNodeCompiler::RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback)
	{
		return ResultCode::kOK;
	}

	uint32_t DepsNodeCompiler::GetVersion() const
	{
		return 1;
	}

	Result DepsNodeCompiler::HandlePathChunk(Vector<char> &constructedPath, bool &outOK, size_t chunkIndex, IDependencyNode *node, const Span<const char> &chunk)
	{
		if (chunk.Count() == 1 && chunk[0] == '.' && chunkIndex == 0)
		{

			StringView identifier = node->GetIdentifier();

			size_t baseDirEnd = 0;
			for (size_t i = 0; i < identifier.Length(); i++)
			{
				if (identifier[i] == '/')
					baseDirEnd = i;
			}

			constructedPath.Reset();
			RKIT_CHECK(constructedPath.Append(identifier.SubString(0, baseDirEnd).ToSpan()));

			outOK = true;
			return ResultCode::kOK;
		}

		if (chunk.Count() == 2 && chunk[0] == '.' && chunk[1] == '.')
		{
			size_t parentDirEnd = 0;
			for (size_t i = 0; i < constructedPath.Count(); i++)
			{
				if (constructedPath[i] == '/')
					parentDirEnd = i;
			}

			if (parentDirEnd == 0)
			{
				outOK = false;
				return ResultCode::kOK;
			}

			RKIT_CHECK(constructedPath.Resize(parentDirEnd));

			outOK = true;
			return ResultCode::kOK;
		}

		if (constructedPath.Count() != 0)
		{
			RKIT_CHECK(constructedPath.Append('/'));
		}

		RKIT_CHECK(constructedPath.Append(chunk));

		outOK = true;
		return ResultCode::kOK;
	}
}
