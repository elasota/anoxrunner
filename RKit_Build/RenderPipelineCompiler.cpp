#include "RenderPipelineCompiler.h"

#include "rkit/Render/RenderDynamicDefs.h"

#include "rkit/Utilities/TextParser.h"

#include "rkit/Core/LogDriver.h"
#include "rkit/Core/HashTable.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/Vector.h"

#include <cstring>

namespace rkit::buildsystem::rpc_interchange
{
}

namespace rkit::buildsystem::rpc_analyzer
{
	class IncludedFileKey
	{
	public:
		IncludedFileKey(BuildFileLocation location, const String &str);

		bool operator==(const IncludedFileKey &other) const;
		bool operator!=(const IncludedFileKey &other) const;

		HashValue_t ComputeHash(HashValue_t baseHash) const;

	private:
		BuildFileLocation m_location = BuildFileLocation::kInvalid;
		String m_string;
	};

	class ShortTempToken
	{
	public:
		Result Set(const Span<const char> &span);
		Span<char> GetChars();
		void Truncate(size_t truncatedSize);

	private:
		static const size_t kStaticBufferSize = 32;

		Vector<char> m_chars;
		char m_staticBuffer[kStaticBufferSize];
		size_t m_length;
	};

	struct AnalyzerIncludeStack
	{
		AnalyzerIncludeStack();
		AnalyzerIncludeStack(BuildFileLocation location, const String &path, bool canTryAlternate);

		String m_path;
		BuildFileLocation m_location = BuildFileLocation::kInvalid;
		bool m_canTryAlternate = false;
		bool m_isScanning = true;

		UniquePtr<rkit::utils::ITextParser> m_textParser;
		Vector<uint8_t> m_fileContents;
	};

	class Analyzer
	{
	public:
		explicit Analyzer(IDependencyNodeCompilerFeedback *feedback);

		Result Run(IDependencyNode *depsNode);

	private:
		Result ScanTopStackItem(AnalyzerIncludeStack &item);
		Result ParseTopStackItem(AnalyzerIncludeStack &item);
		Result ParseDirective(const char *filePath, utils::ITextParser &parser, bool &outHaveDirective);

		Result ParseIncludeDirective(utils::ITextParser &parser);

		Result ResolveQuotedString(ShortTempToken &outToken, const Span<const char> &inToken);

		template<size_t TSize>
		static bool IsToken(const Span<const char> &span, const char(&tokenChars)[TSize]);

		void RemoveTopStackItem();

		HashSet<IncludedFileKey> m_includedFiles;

		Vector<AnalyzerIncludeStack> m_includeStack;
		IDependencyNodeCompilerFeedback *m_feedback;
	};
}

namespace rkit
{
	template<>
	struct Hasher<rkit::buildsystem::rpc_analyzer::IncludedFileKey> : public DefaultSpanHasher<rkit::buildsystem::rpc_analyzer::IncludedFileKey>
	{
		static HashValue_t ComputeHash(HashValue_t baseHash, const rkit::buildsystem::rpc_analyzer::IncludedFileKey &key)
		{
			return key.ComputeHash(baseHash);
		}
	};
}

namespace rkit::buildsystem::rpc_analyzer
{
	IncludedFileKey::IncludedFileKey(BuildFileLocation location, const String &str)
		: m_location(location)
		, m_string(str)
	{
	}

	HashValue_t IncludedFileKey::ComputeHash(HashValue_t baseHash) const
	{
		HashValue_t hash = baseHash;

		hash = Hasher<int32_t>::ComputeHash(hash, static_cast<int32_t>(m_location));
		hash = Hasher<String>::ComputeHash(hash, m_string);

		return hash;
	}

	bool IncludedFileKey::operator==(const IncludedFileKey &other) const
	{
		return m_location == other.m_location && m_string == other.m_string;
	}

	bool IncludedFileKey::operator!=(const IncludedFileKey &other) const
	{
		return !((*this) == other);
	}


	Result ShortTempToken::Set(const Span<const char> &span)
	{
		m_chars.Reset();
		m_length = 0;

		if (span.Count() <= kStaticBufferSize)
			memcpy(m_staticBuffer, span.Ptr(), span.Count());
		else
		{
			RKIT_CHECK(m_chars.Append(span));
		}

		m_length = span.Count();

		return ResultCode::kOK;
	}

	Span<char> ShortTempToken::GetChars()
	{
		if (m_chars.Count() > 0)
			return Span<char>(m_chars.GetBuffer(), m_length);
		else
			return Span<char>(m_staticBuffer, m_length);
	}

	void ShortTempToken::Truncate(size_t newLength)
	{
		RKIT_ASSERT(m_length >= newLength);
		m_length = newLength;
	}

	AnalyzerIncludeStack::AnalyzerIncludeStack()
	{
	}

	AnalyzerIncludeStack::AnalyzerIncludeStack(BuildFileLocation location, const String &path, bool canTryAlternate)
		: m_location(location)
		, m_path(path)
		, m_canTryAlternate(canTryAlternate)
	{
	}

	Analyzer::Analyzer(IDependencyNodeCompilerFeedback *feedback)
		: m_feedback(feedback)
	{
	}

	Result Analyzer::Run(IDependencyNode *depsNode)
	{
		String path;
		RKIT_CHECK(path.Set(depsNode->GetIdentifier()));

		RKIT_CHECK(m_includeStack.Append(AnalyzerIncludeStack(depsNode->GetInputFileLocation(), path, false)));

		while (m_includeStack.Count() > 0)
		{
			AnalyzerIncludeStack &topItem = m_includeStack[m_includeStack.Count() - 1];

			if (topItem.m_isScanning)
			{
				RKIT_CHECK(ScanTopStackItem(topItem));
			}
			else
			{
				RKIT_CHECK(ParseTopStackItem(topItem));
			}
		}

		return ResultCode::kOK;
	}

	Result Analyzer::ScanTopStackItem(AnalyzerIncludeStack &item)
	{
		bool exists = false;

		IncludedFileKey fKey(item.m_location, item.m_path);

		if (m_includedFiles.Contains(fKey))
		{
			RemoveTopStackItem();
			return ResultCode::kOK;
		}

		RKIT_CHECK(m_includedFiles.Add(fKey));

		UniquePtr<ISeekableReadStream> stream;
		RKIT_CHECK(m_feedback->TryOpenInput(item.m_location, item.m_path, stream));

		if (!stream.IsValid())
		{
			if (item.m_canTryAlternate)
			{
				if (item.m_location == BuildFileLocation::kIntermediateDir)
				{
					item.m_location = BuildFileLocation::kSourceDir;
					return ResultCode::kOK;
				}

				rkit::log::ErrorFmt("Could not open input file '%s'", item.m_path.CStr());
				return ResultCode::kFileOpenError;
			}
		}

		IUtilitiesDriver *utils = GetDrivers().m_utilitiesDriver;

		Vector<uint8_t> streamBytes;
		RKIT_CHECK(utils->ReadEntireFile(*stream, streamBytes));

		item.m_fileContents = std::move(streamBytes);

		RKIT_CHECK(utils->CreateTextParser(Span<const char>(reinterpret_cast<char *>(item.m_fileContents.GetBuffer()), item.m_fileContents.Count()), utils::TextParserCommentType::kC, utils::TextParserLexerType::kC, item.m_textParser));
		item.m_isScanning = false;

		return ResultCode::kOK;
	}

	Result Analyzer::ParseTopStackItem(AnalyzerIncludeStack &item)
	{
		utils::ITextParser *parser = item.m_textParser.Get();

		RKIT_CHECK(parser->SkipWhitespace());

		bool haveMore = true;
		while (haveMore)
		{
			haveMore = false;

			size_t oldStackCount = m_includeStack.Count();
			RKIT_CHECK(ParseDirective(item.m_path.CStr(), *item.m_textParser.Get(), haveMore));

			if (oldStackCount != m_includeStack.Count())
				return ResultCode::kOK;
		}

		// Parsing completed
		RemoveTopStackItem();

		return ResultCode::kOK;
	}


	Result Analyzer::ParseDirective(const char *path, utils::ITextParser &parser, bool &outHaveDirective)
	{
		size_t line = 0;
		size_t col = 0;
		parser.GetLocation(line, col);

		bool haveToken = false;
		rkit::Span<const char> directiveToken;
		RKIT_CHECK(parser.ReadToken(haveToken, directiveToken));

		if (!haveToken)
		{
			outHaveDirective = false;
			return ResultCode::kOK;
		}

		outHaveDirective = true;

		if (IsToken(directiveToken, "include"))
			return ParseIncludeDirective(parser);

		if (IsToken(directiveToken, "StaticSampler"))
			return ParseStaticSampler(parser);

		rkit::log::ErrorFmt("%s [%zu:%zu] Invalid directive", path, line, col);

		return ResultCode::kMalformedFile;
	}

	Result Analyzer::ParseIncludeDirective(utils::ITextParser &parser)
	{
		size_t line = 0;
		size_t col = 0;
		parser.GetLocation(line, col);

		ShortTempToken path;

		Span<const char> token;
		RKIT_CHECK(parser.RequireToken(token));

		RKIT_CHECK(ResolveQuotedString(path, token));

		IUtilitiesDriver *utils = GetDrivers().m_utilitiesDriver;

		utils->NormalizeFilePath(path.GetChars());
		if (!utils->ValidateFilePath(path.GetChars()))
		{
			rkit::log::ErrorFmt("[%zu:%zu] Invalid file path", line, col);
			return ResultCode::kMalformedFile;
		}

		BuildFileLocation loc = m_includeStack[m_includeStack.Count() - 1].m_location;

		String str;
		RKIT_CHECK(str.Set(path.GetChars()));

		RKIT_CHECK(m_includeStack.Append(AnalyzerIncludeStack(loc, str, true)));

		return ResultCode::kOK;
	}

	Result Analyzer::ResolveQuotedString(ShortTempToken &outToken, const Span<const char> &inToken)
	{
		RKIT_CHECK(outToken.Set(inToken));

		size_t newLength = 0;
		RKIT_CHECK(GetDrivers().m_utilitiesDriver->EscapeCStringInPlace(outToken.GetChars(), newLength));

		outToken.Truncate(newLength);

		return ResultCode::kOK;
	}

	template<size_t TSize>
	static bool Analyzer::IsToken(const Span<const char> &span, const char(&tokenChars)[TSize])
	{
		if (span.Count() != TSize - 1)
			return false;

		return !memcmp(span.Ptr(), tokenChars, TSize - 1);
	}

	void Analyzer::RemoveTopStackItem()
	{
		m_includeStack.RemoveRange(m_includeStack.Count() - 1, 1);
	}
}

namespace rkit::buildsystem
{
	RenderPipelineCompiler::RenderPipelineCompiler()
	{
	}

	bool RenderPipelineCompiler::HasAnalysisStage() const
	{
		return true;
	}

	Result RenderPipelineCompiler::RunAnalysis(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback)
	{
		UniquePtr<rpc_analyzer::Analyzer> analyzer;
		RKIT_CHECK(New<rpc_analyzer::Analyzer>(analyzer, feedback));

		return analyzer->Run(depsNode);
	}

	Result RenderPipelineCompiler::RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result RenderPipelineCompiler::CreatePrivateData(UniquePtr<IDependencyNodePrivateData> &outPrivateData)
	{
		return ResultCode::kOK;
	}

	uint32_t RenderPipelineCompiler::GetVersion() const
	{
		return 1;
	}
}
