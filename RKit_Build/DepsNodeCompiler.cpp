#include "DepsNodeCompiler.h"

#include "rkit/Core/HybridVector.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/Vector.h"

#include "rkit/Core/UtilitiesDriver.h"

#include "rkit/Utilities/TextParser.h"

namespace rkit { namespace buildsystem
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
			rkit::log::ErrorFmt("Failed to open deps file '{}'", depsNode->GetIdentifier().GetChars());
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

			Vector<CIPath> pathScans;

			RKIT_CHECK(pathScans.Resize(1));

			size_t numPathChunks = 0;
			size_t chunkStart = 0;

			bool haveAnyWildcards = false;

			while (chunkStart < token.Count())
			{
				size_t nextChunkStart = chunkStart;
				size_t chunkEnd = chunkStart;

				bool isFirst = (chunkStart == 0);
				bool isLast = false;

				for (size_t i = chunkStart; true; i++)
				{
					if (i == token.Count() || token[i] == '/')
					{
						chunkEnd = i;
						numPathChunks++;
						nextChunkStart = i;

						if (i == token.Count())
							isLast = true;
						else
							nextChunkStart++;

						break;
					}
				}

				ConstSpan<char> chunkSpan = token.SubSpan(chunkStart, chunkEnd - chunkStart);

				if (chunkSpan.Count() == 1 && chunkSpan[0] == '.')
				{
					if (!isFirst)
					{
						rkit::log::ErrorFmt("{}:{}: '.' path element may only be the first element", line, col);
						return ResultCode::kMalformedFile;
					}

					RKIT_ASSERT(pathScans.Count() == 1);
					RKIT_CHECK(pathScans[0].Set(nodePath.AbsSlice(nodePath.NumComponents() - 1)));
				}
				else
				{
					bool hasWildcards = false;
					for (char c : chunkSpan)
					{
						if (c == '?' || c == '*')
						{
							hasWildcards = true;
							haveAnyWildcards = true;
							break;
						}
					}

					StringSliceView chunkSlice = StringSliceView(chunkSpan.Ptr(), chunkSpan.Count());

					if (!hasWildcards)
					{

						if (CIPath::Validate(chunkSlice) == PathValidationResult::kInvalid)
						{
							rkit::log::ErrorFmt("{}:{}: Path is invalid", line, col);
							return ResultCode::kMalformedFile;
						}

						Vector<CIPath> newPaths;
						for (const CIPath &path : pathScans)
						{
							CIPath newPath = path;
							RKIT_CHECK(newPath.AppendComponent(chunkSlice));

							RKIT_CHECK(newPaths.Append(std::move(newPath)));
						}

						pathScans = std::move(newPaths);
					}
					else
					{
						class PathEnumerator
						{
						public:
							explicit PathEnumerator(const CIPath &basePath, Vector<CIPath> &newPaths, const StringSliceView &wildcard, IUtilitiesDriver *utils)
								: m_basePath(basePath)
								, m_newPaths(newPaths)
								, m_wildcard(wildcard)
								, m_utils(utils)
							{
							}

							Result ApplyResult(const CIPathView &pathView)
							{
								CIPathView::ComponentView_t lastComponent = pathView.LastComponent();

								if (m_utils->MatchesWildcard(lastComponent, m_wildcard))
								{
									CIPath path;
									RKIT_CHECK(path.Set(pathView));
									RKIT_CHECK(m_newPaths.Append(std::move(path)));
								}

								return ResultCode::kOK;
							}

							static Result StaticApplyResult(void *userdata, const CIPathView &path)
							{
								return static_cast<PathEnumerator *>(userdata)->ApplyResult(path);
							}

						private:
							const CIPath &m_basePath;
							Vector<CIPath> &m_newPaths;
							const StringSliceView &m_wildcard;
							IUtilitiesDriver *m_utils;
						};

						Vector<CIPath> newPaths;

						IUtilitiesDriver *utils = GetDrivers().m_utilitiesDriver;

						for (const CIPath &path : pathScans)
						{
							PathEnumerator enumerator(path, newPaths, chunkSlice, utils);

							if (isLast)
							{
								RKIT_CHECK(feedback->EnumerateFiles(BuildFileLocation::kSourceDir, path, &enumerator, PathEnumerator::StaticApplyResult));
							}
							else
							{
								RKIT_CHECK(feedback->EnumerateDirectories(BuildFileLocation::kSourceDir, path, &enumerator, PathEnumerator::StaticApplyResult));
							}
						}

						pathScans = std::move(newPaths);
					}
				}

				chunkStart = nextChunkStart;
			}

			for (const CIPath &path : pathScans)
			{
				bool exists = false;
				RKIT_CHECK(feedback->CheckInputExists(BuildFileLocation::kSourceDir, path, exists));

				if (haveAnyWildcards && !exists)
				{
					rkit::log::ErrorFmt("{}:{}: Input file was not found", line, col);
					return ResultCode::kMalformedFile;
				}

				StringSliceView extStrView;
				if (!utils->FindFilePathExtension(path[path.NumComponents() - 1], extStrView))
				{
					rkit::log::ErrorFmt("{}:{}: Path has no extension", line, col);
					return ResultCode::kMalformedFile;
				}

				uint32_t nodeNamespace = 0;
				uint32_t nodeType = 0;
				if (!feedback->FindNodeTypeByFileExtension(extStrView, nodeNamespace, nodeType))
				{
					rkit::log::ErrorFmt("{}:{}: Unrecognized file extension: '{}'", line, col, extStrView.GetChars());
					return ResultCode::kMalformedFile;
				}

				RKIT_CHECK(feedback->AddNodeDependency(nodeNamespace, nodeType, BuildFileLocation::kSourceDir, path.ToString()));
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
} } // rkit::buildsystem
