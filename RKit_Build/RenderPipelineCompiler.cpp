#include "RenderPipelineCompiler.h"

#include "rkit/Render/RenderDynamicDefs.h"

#include "rkit/Utilities/TextParser.h"

#include "rkit/Data/DataDriver.h"
#include "rkit/Data/RenderDataHandler.h"

#include "rkit/Render/RenderDynamicDefs.h"

#include "rkit/Core/LogDriver.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/HashTable.h"
#include "rkit/Core/Module.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/StringPool.h"
#include "rkit/Core/Vector.h"

#include <cstring>

namespace rkit::buildsystem::rpc_interchange
{
	enum class EntityType
	{
		StaticSampler,
	};

	class Entity : public NoCopy
	{
	public:
		virtual ~Entity() {}
	};

	class StaticSamplerEntity final : public Entity
	{
	public:
		explicit StaticSamplerEntity(size_t index) : m_index(index) {}

		render::SamplerDescDynamic &GetDesc() { return m_desc; }
		const render::SamplerDescDynamic &GetDesc() const { return m_desc; }

	private:
		StaticSamplerEntity() = delete;

		size_t m_index = 0;
		render::SamplerDescDynamic m_desc;
	};

	class PushConstantsEntity final : public Entity
	{
	public:
		explicit PushConstantsEntity(size_t index) : m_index(index) {}

		render::PushConstantsDesc &GetDesc() { return m_desc; }

	private:
		size_t m_index = 0;
		render::PushConstantsDesc m_desc;
	};
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
		char m_staticBuffer[kStaticBufferSize] = {};
		size_t m_length = 0;
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
		explicit Analyzer(data::IDataDriver *dataDriver, IDependencyNodeCompilerFeedback *feedback);

		Result Run(IDependencyNode *depsNode);

	private:
		struct ConfigKey
		{
			render::GlobalStringIndex_t m_name;
			data::RenderRTTIMainType m_mainType = data::RenderRTTIMainType::Invalid;
		};

		Result ScanTopStackItem(AnalyzerIncludeStack &item);
		Result ParseTopStackItem(AnalyzerIncludeStack &item);
		Result ParseDirective(const char *filePath, utils::ITextParser &parser, bool &outHaveDirective);

		Result ParseIncludeDirective(const char *filePath, utils::ITextParser &parser);
		Result ParseStaticSampler(const char *filePath, utils::ITextParser &parser);

		Result ResolveQuotedString(ShortTempToken &outToken, const Span<const char> &inToken);

		Result ExpectIdentifier(const char *blamePath, Span<const char> &outToken, utils::ITextParser &parser);

		Result ParseEnum(const char *blamePath, const data::RenderRTTIEnumType *rtti, void *obj, bool isConfigurable, utils::ITextParser &parser);
		Result ParseValue(const char *blamePath, const data::RenderRTTITypeBase *rtti, void *obj, bool isConfigurable, utils::ITextParser &parser);
		Result ParseConfigurable(const char *blamePath, void *obj, data::RenderRTTIMainType mainType, void (*writeNameFunc)(void *, const render::ConfigStringIndex_t &), utils::ITextParser &parser);

		Result ParseStructMember(const char *blamePath, const Span<const char> &memberName, const data::RenderRTTIStructType *rtti, void *obj, bool isDynamic, utils::ITextParser &parser);

		template<class T>
		Result ParseDynamicStructMember(const char *blamePath, const Span<const char> &memberName, const data::RenderRTTIStructType *rtti, T &obj, utils::ITextParser &parser);

		template<size_t TSize>
		static bool IsToken(const Span<const char> &span, const char(&tokenChars)[TSize]);

		Result IndexString(const Span<const char> &span, render::GlobalStringIndex_t &outStringIndex);

		void RemoveTopStackItem();

		data::IDataDriver *m_dataDriver = nullptr;

		HashSet<IncludedFileKey> m_includedFiles;

		Vector<AnalyzerIncludeStack> m_includeStack;
		IDependencyNodeCompilerFeedback *m_feedback;

		HashMap<String, UniquePtr<rpc_interchange::Entity>> m_entities;
		StringPoolBuilder m_stringPool;

		HashMap<render::GlobalStringIndex_t, render::ConfigStringIndex_t> m_configNameToKey;
		Vector<ConfigKey> m_configKeys;

		size_t m_numStaticSamplers = 0;
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

	Analyzer::Analyzer(data::IDataDriver *dataDriver, IDependencyNodeCompilerFeedback *feedback)
		: m_feedback(feedback)
		, m_dataDriver(dataDriver)
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

		Result parseResult;
		if (IsToken(directiveToken, "include"))
			parseResult = ParseIncludeDirective(path, parser);
		else if (IsToken(directiveToken, "StaticSampler"))
			parseResult = ParseStaticSampler(path, parser);
		else
		{
			rkit::log::ErrorFmt("%s [%zu:%zu] Invalid directive", path, line, col);
			return ResultCode::kMalformedFile;
		}

		if (!parseResult.IsOK())
			rkit::log::ErrorFmt("%s [%zu:%zu] Directive parsing failed", path, line, col);

		return parseResult;
	}

	Result Analyzer::ParseStaticSampler(const char *blamePath, utils::ITextParser &parser)
	{
		size_t line = 0;
		size_t col = 0;
		parser.GetLocation(line, col);

		Span<const char> samplerNameSpan;
		RKIT_CHECK(ExpectIdentifier(blamePath, samplerNameSpan, parser));

		String samplerName;
		RKIT_CHECK(samplerName.Set(samplerNameSpan));

		if (m_entities.Find(samplerName) != m_entities.end())
		{
			rkit::log::ErrorFmt("%s [%zu:%zu] Sampler with this name already exists", blamePath, line, col);
			return ResultCode::kMalformedFile;
		}

		UniquePtr<rpc_interchange::Entity> entity;
		RKIT_CHECK(New<rpc_interchange::StaticSamplerEntity>(entity, m_numStaticSamplers++));

		rpc_interchange::StaticSamplerEntity *ss = static_cast<rpc_interchange::StaticSamplerEntity *>(entity.Get());

		RKIT_CHECK(m_entities.Set(samplerName, std::move(entity)));

		const data::RenderRTTIStructType *rtti = m_dataDriver->GetRenderDataHandler()->GetSamplerDescRTTI();

		RKIT_CHECK(parser.ExpectToken("{"));

		for (;;)
		{
			Span<const char> token;
			RKIT_CHECK(parser.RequireToken(token));

			if (IsToken(token, "}"))
				break;

			RKIT_CHECK(ParseDynamicStructMember(blamePath, token, rtti, ss->GetDesc(), parser));
		}

		return ResultCode::kOK;
	}

	Result Analyzer::ParseIncludeDirective(const char *blamePath, utils::ITextParser &parser)
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
			rkit::log::ErrorFmt("%s [%zu:%zu] Invalid file path", blamePath, line, col);
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

	Result Analyzer::ExpectIdentifier(const char *blamePath, Span<const char> &outToken, utils::ITextParser &parser)
	{
		size_t line = 0;
		size_t col = 0;
		parser.GetLocation(line, col);

		RKIT_CHECK(parser.RequireToken(outToken));

		const char *tokenChars = outToken.Ptr();
		size_t tokenLength = outToken.Count();

		for (size_t i = 0; i < tokenLength; i++)
		{
			bool isValid = false;
			char c = tokenChars[i];
			if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')
				isValid = true;
			else if ((c >= '0' && c <= '9') && i != 0)
				isValid = true;

			if (!isValid)
			{
				rkit::log::ErrorFmt("%s [%zu:%zu] Expected identifier", blamePath, line, col);
				return ResultCode::kMalformedFile;
			}
		}

		return ResultCode::kOK;
	}

	Result Analyzer::ParseValue(const char *blamePath, const data::RenderRTTITypeBase *rtti, void *obj, bool isConfigurable, utils::ITextParser &parser)
	{
		switch (rtti->m_type)
		{
		case data::RenderRTTIType::Enum:
			return ParseEnum(blamePath, reinterpret_cast<const data::RenderRTTIEnumType *>(rtti), obj, isConfigurable, parser);
		case data::RenderRTTIType::Number:
			return ResultCode::kNotYetImplemented;
		case data::RenderRTTIType::Structure:
			return ResultCode::kNotYetImplemented;
		default:
			return ResultCode::kInternalError;
		}
	}

	Result Analyzer::ParseConfigurable(const char *blamePath, void *obj, data::RenderRTTIMainType mainType, void (*writeNameFunc)(void *, const render::ConfigStringIndex_t &), utils::ITextParser &parser)
	{
		Span<const char> configName;

		RKIT_CHECK(parser.ExpectToken("("));

		size_t line = 0;
		size_t col = 0;
		parser.GetLocation(line, col);

		RKIT_CHECK(ExpectIdentifier(blamePath, configName, parser));
		RKIT_CHECK(parser.ExpectToken(")"));

		render::GlobalStringIndex_t configNameSI;
		RKIT_CHECK(IndexString(configName, configNameSI));

		HashMap<render::GlobalStringIndex_t, render::ConfigStringIndex_t>::ConstIterator_t keyIt = m_configNameToKey.Find(configNameSI);
		if (keyIt != m_configNameToKey.end())
		{
			const ConfigKey &configKey = m_configKeys[keyIt.Value().GetIndex()];

			if (configKey.m_mainType != mainType)
			{
				rkit::log::ErrorFmt("%s [%zu:%zu] Config key was already used for a different type", blamePath, line, col);
				return ResultCode::kMalformedFile;
			}

			writeNameFunc(obj, keyIt.Value());
			return ResultCode::kOK;
		}

		ConfigKey configKey;
		configKey.m_mainType = mainType;
		configKey.m_name = configNameSI;

		render::ConfigStringIndex_t configSI(m_configKeys.Count());

		RKIT_CHECK(m_configNameToKey.Set(configNameSI, configSI));
		RKIT_CHECK(m_configKeys.Append(std::move(configKey)));

		writeNameFunc(obj, configSI);

		return ResultCode::kOK;
	}

	Result Analyzer::ParseEnum(const char *blamePath, const data::RenderRTTIEnumType *rtti, void *obj, bool isConfigurable, utils::ITextParser &parser)
	{
		size_t line = 0;
		size_t col = 0;
		parser.GetLocation(line, col);

		Span<const char> option;
		RKIT_CHECK(parser.RequireToken(option));

		if (IsToken(option, "Config"))
		{
			if (!isConfigurable)
			{
				rkit::log::ErrorFmt("%s [%zu:%zu] Option is not configurable", blamePath, line, col);
				return ResultCode::kMalformedFile;
			}

			return ParseConfigurable(blamePath, obj, rtti->m_base.m_mainType, rtti->m_writeConfigurableNameFunc, parser);
		}

		for (size_t i = 0; i < rtti->m_numOptions; i++)
		{
			const data::RenderRTTIEnumOption *candidate = rtti->m_options + i;
			if (option.Count() == candidate->m_nameLength && !memcmp(option.Ptr(), candidate->m_name, option.Count()))
			{
				if (isConfigurable)
					rtti->m_writeConfigurableValueFunc(obj, candidate->m_value);
				else
					rtti->m_writeValueFunc(obj, candidate->m_value);

				return ResultCode::kOK;
			}
		}

		rkit::log::ErrorFmt("%s [%zu:%zu] Invalid value", blamePath, line, col);
		return ResultCode::kMalformedFile;
	}

	Result Analyzer::ParseStructMember(const char *blamePath, const Span<const char> &memberName, const data::RenderRTTIStructType *rtti, void *obj, bool isDynamic, utils::ITextParser &parser)
	{
		size_t line = 0;
		size_t col = 0;
		parser.GetLocation(line, col);

		const data::RenderRTTIStructField *resolvedField = nullptr;

		for (size_t fi = 0; fi < rtti->m_numFields; fi++)
		{
			const data::RenderRTTIStructField *field = rtti->m_fields + fi;
			const char *fieldName = field->m_name;
			size_t fieldNameLength = field->m_nameLength;

			if (memberName.Count() != fieldNameLength)
				continue;

			char firstCharUpper = fieldName[0];
			if (firstCharUpper >= 'a' && firstCharUpper <= 'z')
				firstCharUpper = static_cast<char>(static_cast<int>(firstCharUpper) + 'A' - 'a');

			if (memberName[0] != firstCharUpper)
				continue;

			bool matched = true;
			for (size_t ci = 1; ci < fieldNameLength; ci++)
			{
				if (fieldName[ci] != memberName[ci])
				{
					matched = false;
					break;
				}
			}

			if (!matched)
				continue;

			resolvedField = field;
		}

		if (!resolvedField)
		{
			rkit::log::ErrorFmt("%s [%zu:%zu] Invalid field", blamePath, line, col);
			return ResultCode::kMalformedFile;
		}

		const data::RenderRTTITypeBase *fieldType = resolvedField->m_getTypeFunc();

		size_t fieldOffset = isDynamic ? resolvedField->m_dynamicOffet : resolvedField->m_dynamicOffet;

		void *fieldPtr = static_cast<uint8_t *>(obj) + fieldOffset;

		RKIT_CHECK(parser.ExpectToken("="));

		RKIT_CHECK(ParseValue(blamePath, resolvedField->m_getTypeFunc(), fieldPtr, resolvedField->m_isConfigurable, parser));

		return ResultCode::kOK;
	}

	template<class T>
	Result Analyzer::ParseDynamicStructMember(const char *blamePath, const Span<const char> &memberName, const data::RenderRTTIStructType *rtti, T &obj, utils::ITextParser &parser)
	{
		return ParseStructMember(blamePath, memberName, rtti, &obj, true, parser);
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

	Result Analyzer::IndexString(const Span<const char> &span, render::GlobalStringIndex_t &outStringIndex)
	{
		String str;
		RKIT_CHECK(str.Set(span));

		size_t index = 0;
		RKIT_CHECK(m_stringPool.IndexString(str, index));

		outStringIndex = render::GlobalStringIndex_t(index);

		return ResultCode::kOK;
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
		rkit::IModule *dataModule = rkit::GetDrivers().m_moduleDriver->LoadModule(IModuleDriver::kDefaultNamespace, "Data");
		if (!dataModule)
		{
			rkit::log::Error("Couldn't load utilities module");
			return rkit::ResultCode::kModuleLoadFailed;
		}

		RKIT_CHECK(dataModule->Init(nullptr));

		data::IDataDriver *dataDriver = static_cast<data::IDataDriver *>(rkit::GetDrivers().FindDriver(IModuleDriver::kDefaultNamespace, "Data"));

		UniquePtr<rpc_analyzer::Analyzer> analyzer;
		RKIT_CHECK(New<rpc_analyzer::Analyzer>(analyzer, dataDriver, feedback));

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
