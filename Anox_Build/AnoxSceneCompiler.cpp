#include "AnoxSceneCompiler.h"

#include "rkit/Core/CoreLib.h"
#include "rkit/Core/HashTable.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Optional.h"
#include "rkit/Core/Stream.h"

#include "rkit/Math/Vec.h"

#include "rkit/Utilities/NumberParser.h"

#include "anox/AnoxModule.h"
#include "anox/Label.h"

#include "AnoxEntityDefCompiler.h"

#include "SceneCommandsDefs.generated.inl"

#include "anox/Data/Scene.h"
#include "anox/Data/SceneCommandOpcodes.generated.h"

#include "rkit/Data/ContentID.h"

namespace anox::buildsystem
{
	union SceneCommandParam
	{
		float m_float;
		uint32_t m_uint;
		void *m_ptr;
		const void *m_constPtr;
		size_t m_size;
		bool m_bool;
	};

	struct SceneCommand
	{
		data::SceneCommandOpcode m_opcode;
		size_t m_paramOffset;

		static size_t ParamCountForType(SceneCommandParamType paramType);
	};

	struct ISceneParserConsumer
	{
		virtual rkit::Result ProcessCineID(uint32_t id) = 0;
		virtual rkit::Result ProcessInterrupt() = 0;
		virtual rkit::Result ProcessScript(rkit::ByteStringSliceView name, uint32_t version, uint32_t blockCount) = 0;
		virtual rkit::Result ProcessBlock(rkit::ByteStringSliceView name, uint32_t flags) = 0;
		virtual rkit::Result ProcessPath(uint32_t order, rkit::ByteStringSliceView name, uint32_t type, uint32_t flags, uint32_t timeOffs, uint32_t maxLen, uint32_t color, uint32_t count) = 0;
		virtual rkit::Result ProcessCubicNode(uint32_t flags, uint32_t timeLen, rkit::math::Vec3 position, rkit::math::Vec3 velocityVector, uint32_t relativeMode) = 0;
		virtual rkit::Result ProcessFocusNode(uint32_t flags, uint32_t timeLen, uint32_t focusTarget, rkit::ByteStringSliceView name) = 0;
		virtual rkit::Result ProcessCommandNode(uint32_t flags, uint32_t timeLen, rkit::ConstSpan<SceneCommand> commands, rkit::ConstSpan<SceneCommandParam> params) = 0;
		virtual rkit::Result ProcessScaleNode(uint32_t flags, uint32_t timeLen, rkit::math::Vec3 scale, rkit::math::Vec3 delta) = 0;
		virtual rkit::Result ProcessRollNode(uint32_t flags, uint32_t timeLen, float value, float rate) = 0;
		virtual rkit::Result ProcessFOVNode(uint32_t flags, uint32_t timeLen, float value, float rate) = 0;
	};

	class SceneParser
	{
	public:
		rkit::Result ParseSceneFile(rkit::ConstSpan<uint8_t> stream, ISceneParserConsumer &consumer);

	private:
		typedef rkit::Result(SceneParser:: *ParseMethod_t)(rkit::ConstSpan<rkit::ByteStringSliceView> params, ISceneParserConsumer &consumer);

		struct NamedLineParser
		{
			const rkit::AsciiStringView m_name;
			const ParseMethod_t m_method = nullptr;
		};

		rkit::Result ParseLine(rkit::ConstSpan<uint8_t> tokens, ISceneParserConsumer &consumer);

		rkit::Result ParseCineIDLine(rkit::ConstSpan<rkit::ByteStringSliceView> params, ISceneParserConsumer &consumer);
		rkit::Result ParseInterruptLine(rkit::ConstSpan<rkit::ByteStringSliceView> params, ISceneParserConsumer &consumer);
		rkit::Result ParseScriptLine(rkit::ConstSpan<rkit::ByteStringSliceView> params, ISceneParserConsumer &consumer);
		rkit::Result ParseBlockLine(rkit::ConstSpan<rkit::ByteStringSliceView> params, ISceneParserConsumer &consumer);
		rkit::Result ParsePathLine(rkit::ConstSpan<rkit::ByteStringSliceView> params, ISceneParserConsumer &consumer);
		rkit::Result ParseNodeLine(rkit::ConstSpan<rkit::ByteStringSliceView> params, ISceneParserConsumer &consumer);

		rkit::Result ParsePathNodeLine(uint32_t flags, uint32_t timeLen, rkit::ConstSpan<rkit::ByteStringSliceView> params, ISceneParserConsumer &consumer);
		rkit::Result ParseRollNodeLine(uint32_t flags, uint32_t timeLen, rkit::ConstSpan<rkit::ByteStringSliceView> params, ISceneParserConsumer &consumer);
		rkit::Result ParseCommandNodeLine(uint32_t flags, uint32_t timeLen, rkit::ConstSpan<rkit::ByteStringSliceView> params, ISceneParserConsumer &consumer);
		rkit::Result ParseFocusNodeLine(uint32_t flags, uint32_t timeLen, rkit::ConstSpan<rkit::ByteStringSliceView> params, ISceneParserConsumer &consumer);
		rkit::Result ParseFOVNodeLine(uint32_t flags, uint32_t timeLen, rkit::ConstSpan<rkit::ByteStringSliceView> params, ISceneParserConsumer &consumer);
		rkit::Result ParseScaleNodeLine(uint32_t flags, uint32_t timeLen, rkit::ConstSpan<rkit::ByteStringSliceView> params, ISceneParserConsumer &consumer);

		static rkit::Result TryParseCommandParams(bool &outSucceeded, rkit::Vector<SceneCommandParam> &paramList, const SceneCommandDef &cmdDef, rkit::ConstSpan<rkit::ByteStringSliceView> params);

		static bool TryParseFrameTime(uint32_t &outFrameTime, rkit::ByteStringSliceView str);
		static bool TryParseVec3(rkit::math::Vec3 &outVec, rkit::ByteStringSliceView str);

		static rkit::Result SplitString(rkit::Vector<rkit::ByteStringSliceView> &outSubStrings, rkit::ByteStringSliceView str, uint8_t ch);
	};

	class SceneAnalyzer : public ISceneParserConsumer
	{
	public:
		explicit SceneAnalyzer(rkit::UniquePtr<UserEntityDictionaryBase> dict, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);

		rkit::Result ProcessCineID(uint32_t id) override { RKIT_RETURN_OK; }
		rkit::Result ProcessInterrupt() override { RKIT_RETURN_OK; }
		rkit::Result ProcessScript(rkit::ByteStringSliceView name, uint32_t version, uint32_t blockCount) override { RKIT_RETURN_OK; }
		rkit::Result ProcessBlock(rkit::ByteStringSliceView name, uint32_t flags) override { RKIT_RETURN_OK; }
		rkit::Result ProcessPath(uint32_t order, rkit::ByteStringSliceView name, uint32_t type, uint32_t flags, uint32_t timeOffs, uint32_t maxLen, uint32_t color, uint32_t count) override { RKIT_RETURN_OK; }
		rkit::Result ProcessCubicNode(uint32_t flags, uint32_t timeLen, rkit::math::Vec3 position, rkit::math::Vec3 velocityVector, uint32_t relativeMode) override { RKIT_RETURN_OK; }
		rkit::Result ProcessFocusNode(uint32_t flags, uint32_t timeLen, uint32_t focusTarget, rkit::ByteStringSliceView name) override { RKIT_RETURN_OK; }
		rkit::Result ProcessCommandNode(uint32_t flags, uint32_t timeLen, rkit::ConstSpan<SceneCommand> commands, rkit::ConstSpan<SceneCommandParam> params) override;
		rkit::Result ProcessScaleNode(uint32_t flags, uint32_t timeLen, rkit::math::Vec3 scale, rkit::math::Vec3 delta) override { RKIT_RETURN_OK; }
		rkit::Result ProcessRollNode(uint32_t flags, uint32_t timeLen, float value, float rate) override { RKIT_RETURN_OK; }
		rkit::Result ProcessFOVNode(uint32_t flags, uint32_t timeLen, float value, float rate) override { RKIT_RETURN_OK; }

	private:
		rkit::UniquePtr<UserEntityDictionaryBase> m_dict;
		rkit::buildsystem::IDependencyNodeCompilerFeedback *m_feedback;
	};


	class SceneCompilerConsumer : public ISceneParserConsumer
	{
	public:
		explicit SceneCompilerConsumer(rkit::UniquePtr<UserEntityDictionaryBase> dict, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);

		rkit::Result ProcessCineID(uint32_t id) override;
		rkit::Result ProcessInterrupt() override;
		rkit::Result ProcessScript(rkit::ByteStringSliceView name, uint32_t version, uint32_t blockCount) override;
		rkit::Result ProcessBlock(rkit::ByteStringSliceView name, uint32_t flags) override;
		rkit::Result ProcessPath(uint32_t order, rkit::ByteStringSliceView name, uint32_t type, uint32_t flags, uint32_t timeOffs, uint32_t maxLen, uint32_t color, uint32_t count) override;
		rkit::Result ProcessCubicNode(uint32_t flags, uint32_t timeLen, rkit::math::Vec3 position, rkit::math::Vec3 velocityVector, uint32_t relativeMode) override;
		rkit::Result ProcessFocusNode(uint32_t flags, uint32_t timeLen, uint32_t focusTarget, rkit::ByteStringSliceView name) override;
		rkit::Result ProcessCommandNode(uint32_t flags, uint32_t timeLen, rkit::ConstSpan<SceneCommand> commands, rkit::ConstSpan<SceneCommandParam> params) override;
		rkit::Result ProcessScaleNode(uint32_t flags, uint32_t timeLen, rkit::math::Vec3 scale, rkit::math::Vec3 delta) override;
		rkit::Result ProcessRollNode(uint32_t flags, uint32_t timeLen, float value, float rate) override;
		rkit::Result ProcessFOVNode(uint32_t flags, uint32_t timeLen, float value, float rate) override;

		rkit::Result RunExport(rkit::IWriteStream &stream);

	private:
		static rkit::Result NormalizePathType(data::ScenePathType &outType, uint32_t inType);
		static void CopyVec3(rkit::endian::LittleFloat32_t(&outVec)[3], rkit::math::Vec3 inVec);

		rkit::Result IndexString(uint32_t &outIndex, rkit::ByteStringSliceView str);
		rkit::Result IndexContentRef(uint32_t &outIndex, data::SceneContentRefType refType, const rkit::data::ContentID &cid);
		rkit::Result ProcessCommand(const SceneCommand &cmd, rkit::ConstSpan<SceneCommandParam> params);

		rkit::Result AddNode(data::SceneNodeCommon& common, data::ScenePathType type, uint32_t flags, uint32_t timeLen);

		rkit::UniquePtr<UserEntityDictionaryBase> m_dict;
		rkit::buildsystem::IDependencyNodeCompilerFeedback *m_feedback;

		rkit::HashMap<rkit::ByteString, uint32_t> m_strings;
		rkit::StaticArray<rkit::HashMap<rkit::data::ContentID, uint32_t>, static_cast<size_t>(data::SceneContentRefType::kCount)> m_contentIDs;

		data::SceneHeader m_header = {};
		rkit::Vector<data::SceneCubicNode> m_cubic;
		rkit::Vector<data::SceneFocusNode> m_focus;
		rkit::Vector<data::SceneCommandNode> m_cmd;
		rkit::Vector<data::SceneScaleNode> m_scale;
		rkit::Vector<data::SceneRollNode> m_roll;
		rkit::Vector<data::SceneFOVNode> m_fov;
		rkit::Vector<data::ScenePath> m_paths;
		rkit::Vector<data::SceneBlock> m_blocks;
		rkit::Vector<data::SceneCommandOpcode> m_cmdOpcodes;
		rkit::Vector<rkit::endian::LittleUInt32_t> m_cmdParamDWords;
	};

	class SceneCompiler : public SceneCompilerBase
	{
	public:
		bool HasAnalysisStage() const override { return true; }
		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;

		uint32_t GetVersion() const override;

	private:
		static rkit::Result ReadScriptInput(rkit::Vector<uint8_t> &outVector, rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);
	};


	size_t SceneCommand::ParamCountForType(SceneCommandParamType paramType)
	{
		switch (paramType)
		{
		case SceneCommandParamType::UInt:
		case SceneCommandParamType::Float:
		case SceneCommandParamType::HexUInt:
		case SceneCommandParamType::Label:
			return 1;
		case SceneCommandParamType::EntityType:
		case SceneCommandParamType::Str:
		case SceneCommandParamType::EntityID:
			return 2;
		default:
			RKIT_ASSERT(false);
			return 0;
		}
	}

	rkit::Result SceneParser::ParseSceneFile(rkit::ConstSpan<uint8_t> stream, ISceneParserConsumer &consumer)
	{
		size_t lineStart = 0;
		while (lineStart < stream.Count())
		{
			size_t lineEnd = lineStart;
			while (lineEnd < stream.Count())
			{
				const uint8_t ch = stream[lineEnd];
				if (ch == '\r' || ch == '\n')
					break;

				lineEnd++;
			}

			const rkit::ConstSpan<uint8_t> line = stream.SubSpan(lineStart, lineEnd - lineStart);
			ParseLine(line, consumer);

			lineStart = lineEnd;
			if (lineStart < stream.Count())
				lineStart++;
		}

		RKIT_RETURN_OK;
	}

	rkit::Result SceneParser::ParseLine(rkit::ConstSpan<uint8_t> line, ISceneParserConsumer &consumer)
	{
		// Remove comments
		for (size_t i = 0; i < line.Count(); i++)
		{
			if (line[i] == '#')
			{
				line = line.SubSpan(0, i);
				break;
			}
		}

		while (line.Count() > 0 && line[0] <= ' ')
			line = line.SubSpan(1);

		while (line.Count() > 0 && line[line.Count() - 1] <= ' ')
			line = line.SubSpan(0, line.Count() - 1);

		if (line.Count() == 0)
			RKIT_RETURN_OK;

		rkit::Vector<rkit::ByteStringSliceView> tokens;

		size_t tokenStart = 0;
		for (size_t i = 0; i < line.Count(); i++)
		{
			if (line[i] == ':')
			{
				tokens.Append(rkit::ByteStringSliceView(line.SubSpan(tokenStart, i - tokenStart)));
				tokenStart = i + 1;
			}
		}

		tokens.Append(rkit::ByteStringSliceView(line.SubSpan(tokenStart, line.Count() - tokenStart)));

		if (tokens.Count() == 0)
			RKIT_RETURN_OK;

		const NamedLineParser parsers[] =
		{
			{ "cineid", &SceneParser::ParseCineIDLine, },
			{ "interrupt", &SceneParser::ParseInterruptLine, },
			{ "script", &SceneParser::ParseScriptLine, },
			{ "block", &SceneParser::ParseBlockLine, },
			{ "path", &SceneParser::ParsePathLine, },
			{ "node", &SceneParser::ParseNodeLine, },
		};

		for (const NamedLineParser &parser : parsers)
		{
			if (tokens[0] == parser.m_name.RemoveEncoding())
			{
				const ParseMethod_t method = parser.m_method;
				return (this->*method)(tokens.ToSpan().SubSpan(1), consumer);
			}
		}

		rkit::log::Error(u8"Unknown script line type");
		RKIT_THROW(rkit::ResultCode::kDataError);
	}

	rkit::Result SceneParser::ParseCineIDLine(rkit::ConstSpan<rkit::ByteStringSliceView> params, ISceneParserConsumer &consumer)
	{
		if (params.Count() != 1)
		{
			rkit::log::Error(u8"Wrong param count for 'script'");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		uint32_t cineID = 0;
		if (!rkit::utils::TryParseInteger(cineID, params[0], 10))
		{
			rkit::log::Error(u8"Invalid cinematic ID");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		return consumer.ProcessCineID(cineID);
	}

	rkit::Result SceneParser::ParseInterruptLine(rkit::ConstSpan<rkit::ByteStringSliceView> params, ISceneParserConsumer &consumer)
	{
		return consumer.ProcessInterrupt();
	}

	rkit::Result SceneParser::ParseScriptLine(rkit::ConstSpan<rkit::ByteStringSliceView> params, ISceneParserConsumer &consumer)
	{
		if (params.Count() != 3)
		{
			rkit::log::Error(u8"Wrong param count for 'script'");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		uint32_t version = 0;
		uint32_t blockCount = 0;

		if (!rkit::utils::TryParseInteger(version, params[1], 10) || !rkit::utils::TryParseInteger(blockCount, params[2], 10))
		{
			rkit::log::Error(u8"Invalid script param");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		return consumer.ProcessScript(params[0], version, blockCount);
	}

	rkit::Result SceneParser::ParseBlockLine(rkit::ConstSpan<rkit::ByteStringSliceView> params, ISceneParserConsumer &consumer)
	{
		if (params.Count() != 2)
		{
			rkit::log::Error(u8"Wrong param count for 'block'");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		uint32_t flags = 0;

		if (!rkit::utils::TryParseInteger(flags, params[1], 16))
		{
			rkit::log::Error(u8"Invalid block param");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		return consumer.ProcessBlock(params[0], flags);
	}

	rkit::Result SceneParser::ParsePathLine(rkit::ConstSpan<rkit::ByteStringSliceView> params, ISceneParserConsumer &consumer)
	{
		if (params.Count() != 8)
		{
			rkit::log::Error(u8"Wrong param count for 'path'");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		uint32_t order = 0;
		uint32_t type = 0;
		uint32_t flags = 0;
		uint32_t timeOffs = 0;
		uint32_t maxLen = 0;
		uint32_t color = 0;
		uint32_t count = 0;

		if (!rkit::utils::TryParseInteger(order, params[0], 10)
			|| !rkit::utils::TryParseInteger(type, params[2], 10)
			|| !rkit::utils::TryParseInteger(flags, params[3], 16)
			|| !TryParseFrameTime(timeOffs, params[4])
			|| !TryParseFrameTime(maxLen, params[5])
			|| !rkit::utils::TryParseInteger(color, params[6], 16)
			|| !rkit::utils::TryParseInteger(count, params[7], 10))
		{
			rkit::log::Error(u8"Invalid path param");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		return consumer.ProcessPath(order, params[1], type, flags, timeOffs, maxLen, color, count);
	}

	rkit::Result SceneParser::ParseNodeLine(rkit::ConstSpan<rkit::ByteStringSliceView> params, ISceneParserConsumer &consumer)
	{
		if (params.Count() < 3)
		{
			rkit::log::Error(u8"Not enough params for node");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		uint32_t typeID = 0;
		uint32_t flags = 0;
		uint32_t timeLen = 0;

		if (!rkit::utils::TryParseInteger(typeID, params[0], 10)
			|| !rkit::utils::TryParseInteger(flags, params[1], 16)
			|| !TryParseFrameTime(timeLen, params[2]))
		{
			rkit::log::Error(u8"Invalid params for node");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		params = params.SubSpan(3);

		switch (typeID)
		{
		case 2:
			return ParsePathNodeLine(flags, timeLen, params, consumer);
		case 3:
			return ParseRollNodeLine(flags, timeLen, params, consumer);
		case 4:
			return ParseCommandNodeLine(flags, timeLen, params, consumer);
		case 6:
			return ParseFocusNodeLine(flags, timeLen, params, consumer);
		case 7:
			return ParseFOVNodeLine(flags, timeLen, params, consumer);
		case 8:
			return ParseScaleNodeLine(flags, timeLen, params, consumer);
		default:
			rkit::log::Error(u8"Invalid type for node");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}
	}

	rkit::Result SceneParser::ParsePathNodeLine(uint32_t flags, uint32_t timeLen, rkit::ConstSpan<rkit::ByteStringSliceView> params, ISceneParserConsumer &consumer)
	{
		if (params.Count() != 4)
		{
			rkit::log::Error(u8"Invalid param count for path node");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		rkit::math::Vec3 position;
		rkit::math::Vec3 velocityVector;
		uint32_t relativeMode = 0;

		if (!TryParseVec3(position, params[0])
			|| !TryParseVec3(velocityVector, params[1])
			|| !rkit::utils::TryParseInteger(relativeMode, params[2], 10))
		{
			rkit::log::Error(u8"Invalid param for path node");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		if (params[3].Length() > 0)
			RKIT_THROW(rkit::ResultCode::kNotYetImplemented);

		if (relativeMode > 7)
		{
			rkit::log::Error(u8"Invalid relative mode param for cubic node");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		return consumer.ProcessCubicNode(flags, timeLen, position, velocityVector, relativeMode);
	}

	rkit::Result SceneParser::ParseRollNodeLine(uint32_t flags, uint32_t timeLen, rkit::ConstSpan<rkit::ByteStringSliceView> params, ISceneParserConsumer &consumer)
	{
		if (params.Count() != 3)
		{
			rkit::log::Error(u8"Invalid param count for roll node");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		float value = 0;
		float rate = 0;

		if (!rkit::utils::TryParseFloat(value, params[0])
			|| !rkit::utils::TryParseFloat(rate, params[1]))
		{
			rkit::log::Error(u8"Invalid param for roll node");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		return consumer.ProcessRollNode(flags, timeLen, value, rate);
	}

	rkit::Result SceneParser::ParseCommandNodeLine(uint32_t flags, uint32_t timeLen, rkit::ConstSpan<rkit::ByteStringSliceView> params, ISceneParserConsumer &consumer)
	{
		if (params[0].Length() == 0)
			RKIT_RETURN_OK;

		rkit::ByteString recombinedStr;
		{
			rkit::Vector<uint8_t> recombinedParams;
			for (size_t i = 0; i < params.Count(); i++)
			{
				if (i != 0)
				{
					recombinedParams.Append(':');
				}
				recombinedParams.Append(params[i].ToSpan());
			}

			rkit::ByteStringConstructionBuffer cbuf;
			cbuf.Allocate(recombinedParams.Count());

			rkit::CopySpanNonOverlapping(cbuf.GetSpan(), recombinedParams.ToSpan());

			recombinedStr = rkit::ByteString(std::move(cbuf));
		}

		rkit::Vector<SceneCommand> commands;
		rkit::Vector<SceneCommandParam> paramList;

		rkit::Vector<rkit::ByteStringSliceView> commandStrs;

		SplitString(commandStrs, recombinedStr, ';');

		for (rkit::ByteStringSliceView commandStr : commandStrs)
		{
			if (commandStr.Length() == 0)
				continue;

			rkit::Optional<uint32_t> eqPos;
			for (size_t i = 0; i < commandStr.Length(); i++)
			{
				if (commandStr[i] == '=')
				{
					eqPos = i;
					break;
				}
			}

			const rkit::ByteStringSliceView commandName = eqPos.IsSet() ? commandStr.SubString(0, eqPos.Get()) : commandStr;
			const rkit::ByteStringSliceView commandParamsStr = eqPos.IsSet() ? commandStr.SubString(eqPos.Get() + 1) : rkit::ByteStringSliceView();

			size_t opcodeIndex = 0;
			bool foundCommand = false;
			for (const SceneCommandDef &def : g_sceneCommands)
			{
				if (rkit::AsciiStringView(def.m_name, def.m_nameLength).RemoveEncoding() == commandName)
				{
					rkit::Vector<rkit::ByteStringSliceView> commandParamsVector;
					rkit::ConstSpan<rkit::ByteStringSliceView> commandParams;

					if (def.m_paramCount == 0)
					{
						if (eqPos.IsSet())
						{
							rkit::log::Error(u8"Malformed command");
							RKIT_THROW(rkit::ResultCode::kDataError);
						}
					}
					else if (def.m_paramCount == 1)
					{
						commandParams = rkit::ConstSpan<rkit::ByteStringSliceView>(&commandParamsStr, 1);
					}
					else
					{
						SplitString(commandParamsVector, commandParamsStr, def.m_delimiter);
						commandParams = commandParamsVector.ToSpan();
					}

					const size_t expectedArgCount = def.m_paramCount;
					const size_t numOptionalArgs = def.m_numOptionalParameters;

					SceneCommand cmd = {};
					bool succeeded = false;
					size_t paramStart = paramList.Count();

					bool isTruncated = false;

					if (commandParams.Count() > 0 && commandParams[commandParams.Count() - 1].Length() == 0)
						isTruncated = true;
					else if (commandParams.Count() <= expectedArgCount && commandParams.Count() >= expectedArgCount - numOptionalArgs)
					{
						cmd.m_opcode = static_cast<data::SceneCommandOpcode>(opcodeIndex);

						TryParseCommandParams(succeeded, paramList, def, commandParams);
					}

					if (succeeded)
					{
						cmd.m_paramOffset = paramStart;
						commands.Append(cmd);
					}
					else
					{
						paramList.ShrinkToSize(paramStart);
						if (!def.m_mayFail || !isTruncated)
						{
							rkit::log::Error(u8"Command was malformed");
							RKIT_THROW(rkit::ResultCode::kDataError);
						}
					}

					foundCommand = true;
					break;
				}

				opcodeIndex++;
			}

			if (!foundCommand)
			{
				rkit::log::ErrorFmt(u8"Unknown command {}", commandName);
				RKIT_THROW(rkit::ResultCode::kDataError);
			}
		}

		return consumer.ProcessCommandNode(flags, timeLen, commands.ToSpan(), paramList.ToSpan());
	}

	rkit::Result SceneParser::ParseFocusNodeLine(uint32_t flags, uint32_t timeLen, rkit::ConstSpan<rkit::ByteStringSliceView> params, ISceneParserConsumer &consumer)
	{
		if (params.Count() != 2)
		{
			rkit::log::Error(u8"Invalid param count for focus node");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		uint32_t focusTarget = 0;

		if (!rkit::utils::TryParseInteger(focusTarget, params[0], 10))
		{
			rkit::log::Error(u8"Invalid param for focus node");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		return consumer.ProcessFocusNode(flags, timeLen, focusTarget, params[1]);
	}

	rkit::Result SceneParser::ParseFOVNodeLine(uint32_t flags, uint32_t timeLen, rkit::ConstSpan<rkit::ByteStringSliceView> params, ISceneParserConsumer &consumer)
	{
		if (params.Count() != 3)
		{
			rkit::log::Error(u8"Invalid param count for roll node");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		float value = 0;
		float rate = 0;

		if (!rkit::utils::TryParseFloat(value, params[0])
			|| !rkit::utils::TryParseFloat(rate, params[1]))
		{
			rkit::log::Error(u8"Invalid param for roll node");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		return consumer.ProcessFOVNode(flags, timeLen, value, rate);
	}

	rkit::Result SceneParser::ParseScaleNodeLine(uint32_t flags, uint32_t timeLen, rkit::ConstSpan<rkit::ByteStringSliceView> params, ISceneParserConsumer &consumer)
	{
		if (params.Count() != 4)
		{
			rkit::log::Error(u8"Invalid param count for scale node");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		rkit::math::Vec3 scale;
		rkit::math::Vec3 delta;
		uint32_t alwaysZero = 0;

		if (!TryParseVec3(scale, params[0])
			|| !TryParseVec3(delta, params[1])
			|| !rkit::utils::TryParseInteger(alwaysZero, params[2], 10)
			|| alwaysZero != 0)
		{
			rkit::log::Error(u8"Invalid param for scale node");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		return consumer.ProcessScaleNode(flags, timeLen, scale, delta);
	}

	rkit::Result SceneParser::TryParseCommandParams(bool &outSucceeded, rkit::Vector<SceneCommandParam> &paramList, const SceneCommandDef &cmdDef, rkit::ConstSpan<rkit::ByteStringSliceView> params)
	{
		const size_t numRequiredParams = cmdDef.m_paramCount - cmdDef.m_numOptionalParameters;

		outSucceeded = false;
		if (params.Count() > cmdDef.m_paramCount)
		{
			RKIT_RETURN_OK;
		}

		auto parseLabel = [](rkit::Optional<Label> &outLabel, rkit::ByteStringSliceView paramStr) -> rkit::Result
			{
				rkit::Vector<rkit::ByteStringSliceView> labelParts;
				SplitString(labelParts, paramStr, ':');
				if (labelParts.Count() != 2)
					RKIT_RETURN_OK;

				uint32_t labelHigh = 0;
				uint32_t labelLow = 0;
				if (!rkit::utils::TryParseInteger(labelHigh, labelParts[0], 10)
					|| !rkit::utils::TryParseInteger(labelLow, labelParts[1], 10)
					|| !Label::IsValid(labelHigh, labelLow))
				{
					RKIT_RETURN_OK;
				}

				outLabel = Label(labelHigh, labelLow);

				RKIT_RETURN_OK;
			};

		for (size_t paramIndex = 0; paramIndex < cmdDef.m_paramCount; paramIndex++)
		{
			const SceneCommandParamDef &paramDef = g_sceneCommandParamDefs[cmdDef.m_paramDefsOffset + paramIndex];

			if (paramIndex >= params.Count())
			{
				if (paramIndex < numRequiredParams)
				{
					RKIT_RETURN_OK;
				}

				const size_t paramSize = SceneCommand::ParamCountForType(paramDef.m_paramType);

				SceneCommandParam blankParam = {};
				memset(&blankParam, 0, sizeof(blankParam));

				for (size_t i = 0; i < paramSize; i++)
				{
					paramList.Append(blankParam);
				}
			}
			else
			{
				const rkit::ByteStringSliceView paramStr = params[paramIndex];

				switch (paramDef.m_paramType)
				{
				case SceneCommandParamType::UInt:
					{
						SceneCommandParam uintParam;
						if (paramStr.Length() >= 3 && paramStr[0] == '0' && paramStr[1] == 'x')
						{
							if (!rkit::utils::TryParseInteger(uintParam.m_uint, paramStr.SubString(2), 16))
								RKIT_RETURN_OK;
						}
						else
						{
							if (!rkit::utils::TryParseInteger(uintParam.m_uint, paramStr, 10))
								RKIT_RETURN_OK;
						}

						paramList.Append(uintParam);
					}
					break;
				case SceneCommandParamType::HexUInt:
					{
						SceneCommandParam uintParam;

						if (!rkit::utils::TryParseInteger(uintParam.m_uint, paramStr, 16))
							RKIT_RETURN_OK;

						paramList.Append(uintParam);
					}
					break;
				case SceneCommandParamType::Float:
					{
						SceneCommandParam floatParam;
						if (!rkit::utils::TryParseFloat(floatParam.m_float, paramStr))
							RKIT_RETURN_OK;

						paramList.Append(floatParam);
					}
					break;
				case SceneCommandParamType::EntityType:
				case SceneCommandParamType::Str:
					{
						SceneCommandParam ptrParam;
						SceneCommandParam sizeParam;

						ptrParam.m_constPtr = paramStr.GetChars();
						sizeParam.m_size = paramStr.Length();

						paramList.Append(ptrParam);
						paramList.Append(sizeParam);
					}
					break;
				case SceneCommandParamType::Label:
					{
						SceneCommandParam uintParam;

						rkit::Optional<Label> label;
						parseLabel(label, paramStr);

						if (!label.IsSet())
							RKIT_RETURN_OK;

						uintParam.m_uint = label.Get().RawValue();

						paramList.Append(uintParam);
					}
					break;
				case SceneCommandParamType::EntityID:
					{
						const rkit::ByteStringView prefixStr = rkit::AsciiStringView("PlayerChar").RemoveEncoding();

						SceneCommandParam flagParam;
						SceneCommandParam uintParam;

						if (paramStr.StartsWithNoCase(prefixStr))
						{
							flagParam.m_bool = true;
							if (!rkit::utils::TryParseInteger(uintParam.m_uint, paramStr.SubString(prefixStr.Length()), 10))
							{
								RKIT_RETURN_OK;
							}
						}
						else
						{
							rkit::Optional<Label> label;
							parseLabel(label, paramStr);

							if (!label.IsSet())
							{
								RKIT_RETURN_OK;
							}

							flagParam.m_bool = false;
							uintParam.m_uint = label.Get().RawValue();
						}

						paramList.Append(flagParam);
						paramList.Append(uintParam);
					}
					break;
				default:
					RKIT_ASSERT(false);
					RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
				}
			}
		}

		outSucceeded = true;
		RKIT_RETURN_OK;
	}

	bool SceneParser::TryParseFrameTime(uint32_t &outFrameTime, rkit::ByteStringSliceView str)
	{
		uint32_t integral = 0;
		uint32_t fraction = 0;

		if (str.Length() >= 3 && str[str.Length() - 2] == '.')
		{
			const uint8_t lastDigit = str[str.Length() - 1];
			if (lastDigit < '0' || lastDigit > '9')
				return false;

			if (!rkit::utils::TryParseInteger(integral, str.SubString(0, str.Length() - 2), 10))
				return false;

			fraction = lastDigit - '0';
		}
		else
		{
			if (!rkit::utils::TryParseInteger(integral, str, 10))
				return false;
		}

		if (std::numeric_limits<uint32_t>::max() / 10u < integral)
			return false;

		const uint32_t integralMul10 = integral * 10u;

		if (std::numeric_limits<uint32_t>::max() - integralMul10 < fraction)
			return false;

		outFrameTime = integralMul10 + fraction;

		return true;
	}

	bool SceneParser::TryParseVec3(rkit::math::Vec3 &outVec, rkit::ByteStringSliceView str)
	{
		rkit::StaticArray<size_t, 2> spaceLocations;
		size_t numSpaces = 0;
		for (size_t i = 0; i < str.Length(); i++)
		{
			if (str[i] == ' ')
			{
				if (numSpaces == 2)
					return false;

				spaceLocations[numSpaces++] = i;
			}
		}

		if (numSpaces != 2)
			return false;

		return rkit::utils::TryParseFloat(outVec[0], str.SubString(0, spaceLocations[0]))
			&& rkit::utils::TryParseFloat(outVec[1], str.SubString(spaceLocations[0] + 1, spaceLocations[1] - spaceLocations[0] - 1))
			&& rkit::utils::TryParseFloat(outVec[2], str.SubString(spaceLocations[1] + 1, str.Length() - spaceLocations[1] - 1));
	}

	rkit::Result SceneParser::SplitString(rkit::Vector<rkit::ByteStringSliceView> &outSubStrings, rkit::ByteStringSliceView str, uint8_t ch)
	{
		size_t start = 0;
		for (size_t i = 0; i < str.Length(); i++)
		{
			if (str[i] == ch)
			{
				outSubStrings.Append(str.SubString(start, i - start));
				start = i + 1;
			}
		}
		outSubStrings.Append(str.SubString(start, str.Length() - start));

		RKIT_RETURN_OK;
	}

	SceneAnalyzer::SceneAnalyzer(rkit::UniquePtr<UserEntityDictionaryBase> dict, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
		: m_dict(std::move(dict))
		, m_feedback(feedback)
	{
	}

	rkit::Result SceneAnalyzer::ProcessCommandNode(uint32_t flags, uint32_t timeLen, rkit::ConstSpan<SceneCommand> commands, rkit::ConstSpan<SceneCommandParam> params)
	{
		for (const SceneCommand &cmd : commands)
		{
			size_t paramOffset = cmd.m_paramOffset;

			const SceneCommandDef &cmdDef = g_sceneCommands[static_cast<size_t>(cmd.m_opcode)];
			const rkit::ConstSpan<SceneCommandParamDef> paramDefs = rkit::ConstSpan<SceneCommandParamDef>(g_sceneCommandParamDefs + cmdDef.m_paramDefsOffset, cmdDef.m_paramCount);

			for (const SceneCommandParamDef &paramDef : paramDefs)
			{
				if (paramDef.m_paramType == SceneCommandParamType::EntityType)
				{
					uint32_t edefID = 0;
					const rkit::ByteStringSliceView entClass(static_cast<const uint8_t *>(params[paramOffset].m_constPtr), params[paramOffset + 1].m_size);

					if (!m_dict->FindEntityDef(entClass, edefID))
					{
						rkit::log::Error(u8"Unknown entity type");
						RKIT_THROW(rkit::ResultCode::kDataError);
					}

					rkit::String edefIdentifier;
					EntityDefCompilerBase::FormatEDef(edefIdentifier, edefID);

					m_feedback->AddNodeDependency(kAnoxNamespaceID, buildsystem::kEntityDefNodeID, rkit::buildsystem::BuildFileLocation::kIntermediateDir, edefIdentifier);
				}

				paramOffset += SceneCommand::ParamCountForType(paramDef.m_paramType);
			}
		}

		RKIT_RETURN_OK;
	}

	SceneCompilerConsumer::SceneCompilerConsumer(rkit::UniquePtr<UserEntityDictionaryBase> dict, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
		: m_dict(std::move(dict))
		, m_feedback(feedback)
	{
		m_header.m_magic = data::SceneHeader::kExpectedMagic;
	}

	rkit::Result SceneCompilerConsumer::ProcessCineID(uint32_t id)
	{
		m_header.m_cineID = id;
		RKIT_RETURN_OK;
	}

	rkit::Result SceneCompilerConsumer::ProcessInterrupt()
	{
		m_header.m_isInterrupt = true;
		RKIT_RETURN_OK;
	}

	rkit::Result SceneCompilerConsumer::ProcessScript(rkit::ByteStringSliceView name, uint32_t version, uint32_t blockCount)
	{
		RKIT_RETURN_OK;
	}

	rkit::Result SceneCompilerConsumer::ProcessBlock(rkit::ByteStringSliceView name, uint32_t flags)
	{
		data::SceneBlock block;
		block.m_numPaths = 0;
		block.m_flags = flags;

		m_blocks.Append(block);

		RKIT_RETURN_OK;
	}

	rkit::Result SceneCompilerConsumer::ProcessPath(uint32_t order, rkit::ByteStringSliceView name, uint32_t type, uint32_t flags, uint32_t timeOffs, uint32_t maxLen, uint32_t color, uint32_t count)
	{
		if (m_blocks.Count() == 0)
			RKIT_THROW(rkit::ResultCode::kDataError);

		data::SceneBlock &lastBlock = m_blocks[m_blocks.Count() - 1];
		lastBlock.m_numPaths = lastBlock.m_numPaths.Get() + 1;

		data::ScenePathType pathType = {};
		NormalizePathType(pathType, type);

		data::ScenePath path = {};
		path.m_pathType = pathType;
		path.m_numNodes = 0;
		path.m_timeOffs = timeOffs;
		path.m_maxLen = maxLen;

		path.m_isGlobal = ((flags & 0x4) != 0) ? 1 : 0;

		m_paths.Append(path);

		RKIT_RETURN_OK;
	}

	rkit::Result SceneCompilerConsumer::ProcessCubicNode(uint32_t flags, uint32_t timeLen, rkit::math::Vec3 position, rkit::math::Vec3 velocityVector, uint32_t relativeMode)
	{
		data::SceneCubicNode node = {};
		AddNode(node.m_common, data::ScenePathType::kCubic, flags, timeLen);

		CopyVec3(node.m_position, position);
		CopyVec3(node.m_velocity, velocityVector);
		node.m_relativeMode = relativeMode;

		m_cubic.Append(node);

		RKIT_RETURN_OK;
	}

	rkit::Result SceneCompilerConsumer::ProcessFocusNode(uint32_t flags, uint32_t timeLen, uint32_t focusTarget, rkit::ByteStringSliceView name)
	{
		data::SceneFocusNode node = {};
		AddNode(node.m_common, data::ScenePathType::kFocus, flags, timeLen);

		node.m_focusTarget = focusTarget;

		m_focus.Append(node);

		RKIT_RETURN_OK;
	}

	rkit::Result SceneCompilerConsumer::ProcessCommandNode(uint32_t flags, uint32_t timeLen, rkit::ConstSpan<SceneCommand> commands, rkit::ConstSpan<SceneCommandParam> params)
	{
		data::SceneCommandNode node = {};
		AddNode(node.m_common, data::ScenePathType::kCommand, flags, timeLen);

		const size_t prevDWordCount = m_cmdParamDWords.Count();
		const size_t prevCmds = m_cmdOpcodes.Count();

		for (const SceneCommand &cmd : commands)
		{
			ProcessCommand(cmd, params);
		}

		const size_t numCmds = m_cmdOpcodes.Count() - prevCmds;
		const size_t numParamDWords = m_cmdParamDWords.Count() - prevDWordCount;

		if (numCmds > std::numeric_limits<uint32_t>::max() || numParamDWords > std::numeric_limits<uint32_t>::max())
			RKIT_THROW(rkit::ResultCode::kIntegerOverflow);

		node.m_numCommands = static_cast<uint32_t>(numCmds);
		node.m_numParamDWords = static_cast<uint32_t>(numParamDWords);

		m_cmd.Append(node);

		RKIT_RETURN_OK;
	}

	rkit::Result SceneCompilerConsumer::ProcessScaleNode(uint32_t flags, uint32_t timeLen, rkit::math::Vec3 scale, rkit::math::Vec3 delta)
	{
		data::SceneScaleNode node = {};
		AddNode(node.m_common, data::ScenePathType::kScale, flags, timeLen);

		CopyVec3(node.m_scale, scale);
		CopyVec3(node.m_delta, delta);

		m_scale.Append(node);

		RKIT_RETURN_OK;
	}

	rkit::Result SceneCompilerConsumer::ProcessRollNode(uint32_t flags, uint32_t timeLen, float value, float rate)
	{
		data::SceneRollNode node = {};
		AddNode(node.m_common, data::ScenePathType::kRoll, flags, timeLen);

		node.m_value = value;
		node.m_rate = rate;

		m_roll.Append(node);

		RKIT_RETURN_OK;
	}

	rkit::Result SceneCompilerConsumer::ProcessFOVNode(uint32_t flags, uint32_t timeLen, float value, float rate)
	{
		data::SceneFOVNode node = {};
		AddNode(node.m_common, data::ScenePathType::kFOV, flags, timeLen);

		node.m_value = value;
		node.m_rate = rate;

		m_fov.Append(node);

		RKIT_RETURN_OK;
	}

	rkit::Result SceneCompilerConsumer::RunExport(rkit::IWriteStream &stream)
	{
		rkit::Vector<rkit::ByteString> strings;
		strings.Resize(m_strings.Count());

		for (const rkit::HashMapKeyValueView<rkit::ByteString, const uint32_t> &kv : m_strings)
			strings[kv.Value()] = kv.Key();

		rkit::Vector<rkit::endian::LittleUInt32_t> stringLengths;
		stringLengths.Resize(strings.Count());

		auto processOne = [](rkit::endian::LittleUInt32_t &outLength, const rkit::ByteString &inStr) -> rkit::Result
			{
				if (inStr.Length() > std::numeric_limits<uint32_t>::max())
					RKIT_THROW(rkit::ResultCode::kIntegerOverflow);

				outLength = static_cast<uint32_t>(inStr.Length());

				RKIT_RETURN_OK;
			};

		rkit::CheckedProcessParallelSpans(stringLengths.ToSpan(), strings.ToSpan(), processOne);

		m_header.m_numStrings = static_cast<uint32_t>(strings.Count());

		for (size_t contentTypeIndex = 0; contentTypeIndex < static_cast<size_t>(data::SceneContentRefType::kCount); contentTypeIndex++)
			m_header.m_contentCounts[contentTypeIndex] = static_cast<uint32_t>(m_contentIDs[contentTypeIndex].Count());

		if (m_paths.Count() > std::numeric_limits<uint32_t>::max())
			RKIT_THROW(rkit::ResultCode::kIntegerOverflow);

		m_header.m_numPaths = static_cast<uint32_t>(m_paths.Count());

		if (m_blocks.Count() > std::numeric_limits<uint32_t>::max())
			RKIT_THROW(rkit::ResultCode::kIntegerOverflow);

		m_header.m_numBlocks = static_cast<uint32_t>(m_blocks.Count());

		// Write everything
		stream.WriteOneBinary(m_header);

		stream.WriteAllSpan(stringLengths.ToSpan());

		for (const rkit::ByteString &str : strings)
		{
			stream.WriteAllSpan(str.ToSpan());
		}

		stream.WriteAllSpan(m_blocks.ToSpan());
		stream.WriteAllSpan(m_paths.ToSpan());
		stream.WriteAllSpan(m_cubic.ToSpan());
		stream.WriteAllSpan(m_focus.ToSpan());
		stream.WriteAllSpan(m_cmd.ToSpan());
		stream.WriteAllSpan(m_scale.ToSpan());
		stream.WriteAllSpan(m_roll.ToSpan());
		stream.WriteAllSpan(m_fov.ToSpan());
		stream.WriteAllSpan(m_cmdOpcodes.ToSpan());
		stream.WriteAllSpan(m_cmdParamDWords.ToSpan());

		for (size_t contentTypeIndex = 0; contentTypeIndex < static_cast<size_t>(data::SceneContentRefType::kCount); contentTypeIndex++)
		{
			const rkit::HashMap<rkit::data::ContentID, uint32_t> &map = m_contentIDs[contentTypeIndex];

			rkit::Vector<rkit::data::ContentID> contentIDs;
			contentIDs.Resize(map.Count());

			for (const rkit::HashMapKeyValueView<rkit::data::ContentID, const uint32_t> &kv : map)
				contentIDs[kv.Value()] = kv.Key();

			stream.WriteAllSpan(contentIDs.ToSpan());
		}


		RKIT_RETURN_OK;
	}

	rkit::Result SceneCompilerConsumer::NormalizePathType(data::ScenePathType &outType, uint32_t inType)
	{
		switch (inType)
		{
		case 2:
			outType = data::ScenePathType::kCubic;
			RKIT_RETURN_OK;
		case 3:
			outType = data::ScenePathType::kRoll;
			RKIT_RETURN_OK;
		case 4:
			outType = data::ScenePathType::kCommand;
			RKIT_RETURN_OK;
		case 6:
			outType = data::ScenePathType::kFocus;
			RKIT_RETURN_OK;
		case 7:
			outType = data::ScenePathType::kFOV;
			RKIT_RETURN_OK;
		case 8:
			outType = data::ScenePathType::kScale;
			RKIT_RETURN_OK;
		default:
			RKIT_THROW(rkit::ResultCode::kInternalError);
		}
	}

	void SceneCompilerConsumer::CopyVec3(rkit::endian::LittleFloat32_t(&outVec)[3], rkit::math::Vec3 inVec)
	{
		outVec[0] = inVec[0];
		outVec[1] = inVec[1];
		outVec[2] = inVec[2];
	}

	rkit::Result SceneCompilerConsumer::IndexString(uint32_t &outIndex, rkit::ByteStringSliceView strView)
	{
		const rkit::HashValue_t hashValue = rkit::Hasher<rkit::ByteStringSliceView>::ComputeHash(0, strView);

		uint32_t index = 0;
		rkit::HashMap<rkit::ByteString, uint32_t>::ConstIterator_t it = m_strings.FindPrehashed(hashValue, strView);
		if (it == m_strings.end())
		{
			if (m_strings.Count() == std::numeric_limits<uint32_t>::max())
				RKIT_THROW(rkit::ResultCode::kIntegerOverflow);

			index = static_cast<uint32_t>(m_strings.Count());

			rkit::ByteString bstr;
			bstr.Set(strView);

			m_strings.SetPrehashed(hashValue, std::move(bstr), index);
		}
		else
			index = it.Value();

		outIndex = index;

		RKIT_RETURN_OK;
	}

	rkit::Result SceneCompilerConsumer::IndexContentRef(uint32_t &outIndex, data::SceneContentRefType refType, const rkit::data::ContentID &cid)
	{
		rkit::HashMap<rkit::data::ContentID, uint32_t> &map = m_contentIDs[static_cast<size_t>(refType)];

		const rkit::HashValue_t hashValue = rkit::Hasher<rkit::data::ContentID>::ComputeHash(0, cid);

		uint32_t index = 0;
		rkit::HashMap<rkit::data::ContentID, uint32_t>::ConstIterator_t it = map.FindPrehashed(hashValue, cid);
		if (it == map.end())
		{
			if (map.Count() == std::numeric_limits<uint32_t>::max())
				RKIT_THROW(rkit::ResultCode::kIntegerOverflow);

			index = static_cast<uint32_t>(map.Count());
			map.SetPrehashed(hashValue, cid, index);
		}
		else
			index = it.Value();

		outIndex = index;

		RKIT_RETURN_OK;
	}

	rkit::Result SceneCompilerConsumer::ProcessCommand(const SceneCommand &cmd, rkit::ConstSpan<SceneCommandParam> params)
	{
		const SceneCommandDef &def = g_sceneCommands[static_cast<size_t>(cmd.m_opcode)];

		const SceneCommandParamDef *paramDefs = g_sceneCommandParamDefs + def.m_paramDefsOffset;

		size_t inParamIndex = cmd.m_paramOffset;
		for (size_t i = 0; i < def.m_paramCount; i++)
		{
			const SceneCommandParamDef &paramDef = paramDefs[i];

			switch (paramDef.m_paramType)
			{
			case SceneCommandParamType::UInt:
			case SceneCommandParamType::Label:
			case SceneCommandParamType::HexUInt:
				m_cmdParamDWords.Append(rkit::endian::LittleUInt32_t(params[inParamIndex].m_uint));
				break;
			case SceneCommandParamType::Float:
				{
					uint32_t bits = 0;
					memcpy(&bits, &params[inParamIndex].m_float, 4);
					m_cmdParamDWords.Append(rkit::endian::LittleUInt32_t(bits));
				}
				break;
			case SceneCommandParamType::Str:
				{
					const rkit::ByteStringSliceView strView(static_cast<const uint8_t *>(params[inParamIndex].m_constPtr), params[inParamIndex + 1].m_size);

					uint32_t index = 0;
					IndexString(index, strView);

					m_cmdParamDWords.Append(rkit::endian::LittleUInt32_t(index));
				}
				break;
			case SceneCommandParamType::EntityID:
				{
					const bool isPlayerChar = params[inParamIndex].m_bool;
					uint32_t eid = params[inParamIndex + 1].m_uint;

					if (eid & 0x80000000u)
						RKIT_THROW(rkit::ResultCode::kDataError);

					eid <<= 1;
					if (isPlayerChar)
						eid |= 1;

					m_cmdParamDWords.Append(rkit::endian::LittleUInt32_t(eid));
				}
				break;
			case SceneCommandParamType::EntityType:
				{
					uint32_t edefID = 0;
					const rkit::ByteStringSliceView entClass(static_cast<const uint8_t *>(params[inParamIndex].m_constPtr), params[inParamIndex + 1].m_size);

					if (!m_dict->FindEntityDef(entClass, edefID))
					{
						rkit::log::Error(u8"Unknown entity type");
						RKIT_THROW(rkit::ResultCode::kDataError);
					}

					rkit::String edefIdentifier;
					EntityDefCompilerBase::FormatEDef(edefIdentifier, edefID);

					rkit::CIPath edefPath;
					edefPath.Set(edefIdentifier);

					rkit::data::ContentID contentID;
					m_feedback->IndexCAS(rkit::buildsystem::BuildFileLocation::kIntermediateDir, edefPath, contentID);

					uint32_t cidIndex = 0;
					IndexContentRef(cidIndex, data::SceneContentRefType::kEntityType, contentID);

					m_cmdParamDWords.Append(rkit::endian::LittleUInt32_t(cidIndex));
				}
				break;
			default:
				RKIT_ASSERT(false);
				RKIT_THROW(rkit::ResultCode::kInternalError);
			}


			inParamIndex += SceneCommand::ParamCountForType(paramDef.m_paramType);
		}

		m_cmdOpcodes.Append(cmd.m_opcode);

		RKIT_RETURN_OK;
	}

	rkit::Result SceneCompilerConsumer::AddNode(data::SceneNodeCommon& common, data::ScenePathType type, uint32_t flags, uint32_t timeLen)
	{
		rkit::endian::LittleUInt32_t &nodeCounter = m_header.m_nodeCounts[static_cast<size_t>(type)];

		if (nodeCounter.Get() == std::numeric_limits<uint32_t>::max())
			RKIT_THROW(rkit::ResultCode::kIntegerOverflow);

		if (m_paths.Count() == 0)
			RKIT_THROW(rkit::ResultCode::kDataError);

		data::ScenePath &lastPath = m_paths[m_paths.Count() - 1];
		lastPath.m_numNodes = lastPath.m_numNodes.Get() + 1;

		nodeCounter = nodeCounter.Get() + 1;

		common.m_flags = flags;
		common.m_timeLen = timeLen;

		RKIT_RETURN_OK;
	}

	rkit::Result SceneCompiler::RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		rkit::Vector<uint8_t> script;
		ReadScriptInput(script, depsNode, feedback);

		rkit::UniquePtr<UserEntityDictionaryBase> dict;
		EntityDefCompilerBase::LoadUserEntityDictionary(dict, feedback);

		SceneAnalyzer analyzer(std::move(dict), feedback);

		SceneParser parser;
		parser.ParseSceneFile(script.ToSpan(), analyzer);

		RKIT_RETURN_OK;
	}

	rkit::Result SceneCompiler::RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		rkit::Vector<uint8_t> script;
		ReadScriptInput(script, depsNode, feedback);

		rkit::UniquePtr<UserEntityDictionaryBase> dict;
		EntityDefCompilerBase::LoadUserEntityDictionary(dict, feedback);

		SceneCompilerConsumer compiler(std::move(dict), feedback);

		SceneParser parser;
		parser.ParseSceneFile(script.ToSpan(), compiler);

		rkit::String outPathStr;
		SceneCompilerBase::FormatOutputPath(outPathStr, depsNode->GetIdentifier());

		rkit::CIPath outPath;
		outPath.Set(outPathStr);

		rkit::UniquePtr<rkit::ISeekableReadWriteStream> stream;
		feedback->OpenOutput(rkit::buildsystem::BuildFileLocation::kIntermediateDir, outPath, stream);

		compiler.RunExport(*stream);

		RKIT_RETURN_OK;
	}

	rkit::Result SceneCompiler::ReadScriptInput(rkit::Vector<uint8_t> &outVector, rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		rkit::CIPath path;
		path.Set(depsNode->GetIdentifier());

		rkit::UniquePtr<rkit::ISeekableReadStream> inputFile;
		feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kSourceDir, path, inputFile);

		if (inputFile->GetSize() > std::numeric_limits<size_t>::max())
			RKIT_THROW(rkit::ResultCode::kIntegerOverflow);

		const size_t size = static_cast<size_t>(inputFile->GetSize());

		outVector.Resize(size);
		inputFile->ReadAllSpan(outVector.ToSpan());

		RKIT_RETURN_OK;
	}

	uint32_t SceneCompiler::GetVersion() const
	{
		return 2;
	}

	rkit::Result SceneCompilerBase::FormatOutputPath(rkit::String &outPath, rkit::StringSliceView identifier)
	{
		return outPath.Format(u8"ax_scn/{}", identifier);
	}

	rkit::Result SceneCompilerBase::Create(rkit::UniquePtr<SceneCompilerBase> &outCompiler)
	{
		return rkit::New<SceneCompiler>(outCompiler);
	}
}
