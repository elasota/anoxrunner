#include "AnoxSceneCompiler.h"

#include "rkit/Core/CoreLib.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Optional.h"
#include "rkit/Core/Stream.h"
#include "rkit/Math/Vec.h"

#include "rkit/Utilities/NumberParser.h"

#include "AnoxEntityDefCompiler.h"
#include "anox/AnoxModule.h"

namespace anox::buildsystem
{
	enum class SceneCommandType
	{
		kInvalid = 0,

		kNewEnt,
		kFloor,
		kLighting,
		kLightSrc,
		kRotV,
	};

	enum class SceneCommandFormat
	{
		kNewEnt,

		kUInt32,
		kFloat3,
		kFloat4,
	};

	struct SceneCommandDef
	{
		rkit::AsciiStringSliceView m_name;
		SceneCommandType m_type;
		SceneCommandFormat m_format;
		uint8_t m_argDelimiter = 0;

		static const SceneCommandDef ms_defs[];
	};

	namespace scenecommands
	{
		struct PODStringView
		{
			const uint8_t *m_chars;
			size_t m_len;
		};

		struct NewEnt
		{
			PODStringView m_name;
		};

		struct Float3
		{
			float m_v0;
			float m_v1;
			float m_v2;
		};

		struct Float4
		{
			float m_v0;
			float m_v1;
			float m_v2;
			float m_v3;
		};
	}

	union SceneCommandUnion
	{
		scenecommands::NewEnt m_newEnt;
		scenecommands::Float3 m_float3;
		scenecommands::Float4 m_float4;
		uint32_t m_uint;
	};

	struct SceneCommand
	{
		SceneCommandType m_commandType = SceneCommandType::kInvalid;
		SceneCommandUnion m_union = {};

		static scenecommands::PODStringView WrapString(const rkit::ByteStringSliceView &str);
		static rkit::ByteStringSliceView UnwrapString(const scenecommands::PODStringView &str);
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
		virtual rkit::Result ProcessCommandNode(uint32_t flags, uint32_t timeLen, rkit::ConstSpan<SceneCommand> commands) = 0;
	};

	class SceneParser
	{
	public:
		rkit::Result ParseSceneFile(rkit::ConstSpan<uint8_t> stream, ISceneParserConsumer &consumer);

	private:
		typedef rkit::Result(SceneParser:: *ParseMethod_t)(rkit::ConstSpan<rkit::ByteStringSliceView> params, ISceneParserConsumer &consumer);
		typedef rkit::Result(SceneParser:: *CommandParseMethod_t)(SceneCommand &outCommand, rkit::ConstSpan<rkit::ByteStringSliceView> params);

		struct NamedLineParser
		{
			const rkit::AsciiStringView m_name;
			const ParseMethod_t m_method = nullptr;
		};

		struct NamedCommandParser
		{
			const rkit::AsciiStringView m_name;
			const CommandParseMethod_t m_method = nullptr;
			const size_t m_expectedArgCount = 0;
			const uint8_t m_argDelimiter = 0;
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

		rkit::Result ParseNewEntCommand(SceneCommand &outCommand, rkit::ConstSpan<rkit::ByteStringSliceView> params);
		rkit::Result ParseUInt32Command(SceneCommand &outCommand, rkit::ConstSpan<rkit::ByteStringSliceView> params);
		rkit::Result ParseFloat3Command(SceneCommand &outCommand, rkit::ConstSpan<rkit::ByteStringSliceView> params);
		rkit::Result ParseFloat4Command(SceneCommand &outCommand, rkit::ConstSpan<rkit::ByteStringSliceView> params);

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
		rkit::Result ProcessCommandNode(uint32_t flags, uint32_t timeLen, rkit::ConstSpan<SceneCommand> commands);

	private:
		rkit::UniquePtr<UserEntityDictionaryBase> m_dict;
		rkit::buildsystem::IDependencyNodeCompilerFeedback *m_feedback;
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

	const SceneCommandDef SceneCommandDef::ms_defs[] =
	{
		{ "newent", SceneCommandType::kNewEnt, SceneCommandFormat::kNewEnt },
		{ "floor", SceneCommandType::kFloor, SceneCommandFormat::kUInt32 },
		{ "lighting", SceneCommandType::kLighting, SceneCommandFormat::kFloat4, '='},
		{ "lightsrc", SceneCommandType::kLightSrc, SceneCommandFormat::kUInt32 },
		{ "rotv", SceneCommandType::kRotV, SceneCommandFormat::kFloat3 },
	};

	scenecommands::PODStringView SceneCommand::WrapString(const rkit::ByteStringSliceView &str)
	{
		scenecommands::PODStringView result;
		result.m_chars = str.GetChars();
		result.m_len = str.Length();
		return result;
	}

	rkit::ByteStringSliceView SceneCommand::UnwrapString(const scenecommands::PODStringView &str)
	{
		return rkit::ByteStringSliceView(str.m_chars, str.m_len);
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
			RKIT_CHECK(ParseLine(line, consumer));

			lineStart = lineEnd;
			if (lineStart < stream.Count())
				lineStart++;
		}
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
				RKIT_CHECK(tokens.Append(rkit::ByteStringSliceView(line.SubSpan(tokenStart, i - tokenStart))));
				tokenStart = i + 1;
			}
		}

		RKIT_CHECK(tokens.Append(rkit::ByteStringSliceView(line.SubSpan(tokenStart, line.Count() - tokenStart))));

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
				(this->*method)(tokens.ToSpan().SubSpan(1), consumer);
				RKIT_RETURN_OK;
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
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
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
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result SceneParser::ParseCommandNodeLine(uint32_t flags, uint32_t timeLen, rkit::ConstSpan<rkit::ByteStringSliceView> params, ISceneParserConsumer &consumer)
	{
		if (params.Count() != 1)
		{
			rkit::log::Error(u8"Invalid param count for command node");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		rkit::Vector<SceneCommand> commands;

		rkit::Vector<rkit::ByteStringSliceView> commandStrs;

		RKIT_CHECK(SplitString(commandStrs, params[0], ';'));

		for (rkit::ByteStringSliceView commandStr : commandStrs)
		{
			rkit::Optional<uint32_t> eqPos;
			for (size_t i = 0; i < commandStr.Length(); i++)
			{
				if (commandStr[i] == '=')
				{
					eqPos = i;
					break;
				}
			}

			if (!eqPos.IsSet())
			{
				rkit::log::Error(u8"Malformed command");
				RKIT_THROW(rkit::ResultCode::kDataError);
			}

			const rkit::ByteStringSliceView commandName = commandStr.SubString(0, eqPos.Get());
			const rkit::ByteStringSliceView commandParamsStr = commandStr.SubString(eqPos.Get() + 1);

			bool foundCommand = false;
			for (const SceneCommandDef *def = SceneCommandDef::ms_defs; def->m_type != SceneCommandType::kInvalid; def++)
			{
				if (def->m_name.RemoveEncoding() == commandName)
				{
					CommandParseMethod_t method = nullptr;

					size_t expectedArgCount = 1;
					switch (def->m_format)
					{
					case SceneCommandFormat::kNewEnt:
						method = &SceneParser::ParseNewEntCommand;
						break;
					case SceneCommandFormat::kFloat3:
						method = &SceneParser::ParseFloat3Command;
						expectedArgCount = 3;
						break;
					case SceneCommandFormat::kFloat4:
						method = &SceneParser::ParseFloat4Command;
						expectedArgCount = 4;
						break;
					case SceneCommandFormat::kUInt32:
						method = &SceneParser::ParseUInt32Command;
						break;
					default:
						RKIT_THROW(rkit::ResultCode::kInternalError);
					}

					rkit::Vector<rkit::ByteStringSliceView> commandParamsVector;
					rkit::ConstSpan<rkit::ByteStringSliceView> commandParams;

					if (expectedArgCount == 1)
					{
						commandParams = rkit::ConstSpan<rkit::ByteStringSliceView>(&commandParamsStr, 1);
					}
					else
					{
						uint8_t delimiter = ',';
						if (def->m_argDelimiter != 0)
							delimiter = def->m_argDelimiter;

						RKIT_CHECK(SplitString(commandParamsVector, commandParamsStr, delimiter));
						commandParams = commandParamsVector.ToSpan();
					}

					if (commandParams.Count() != expectedArgCount)
					{
						rkit::log::Error(u8"Command param count was wrong");
						RKIT_THROW(rkit::ResultCode::kDataError);
					}

					SceneCommand cmd = {};
					RKIT_CHECK((this->*method)(cmd, commandParams));

					cmd.m_commandType = def->m_type;

					foundCommand = true;
					RKIT_CHECK(commands.Append(cmd));
					break;
				}
			}

			if (!foundCommand)
			{
				rkit::log::Error(u8"Unknown command");
				RKIT_THROW(rkit::ResultCode::kDataError);
			}
		}

		return consumer.ProcessCommandNode(flags, timeLen, commands.ToSpan());
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
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result SceneParser::ParseScaleNodeLine(uint32_t flags, uint32_t timeLen, rkit::ConstSpan<rkit::ByteStringSliceView> params, ISceneParserConsumer &consumer)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result SceneParser::ParseNewEntCommand(SceneCommand &outCommand, rkit::ConstSpan<rkit::ByteStringSliceView> params)
	{
		outCommand.m_commandType = SceneCommandType::kNewEnt;
		outCommand.m_union.m_newEnt.m_name = SceneCommand::WrapString(params[0]);

		RKIT_RETURN_OK;
	}


	rkit::Result SceneParser::ParseUInt32Command(SceneCommand &outCommand, rkit::ConstSpan<rkit::ByteStringSliceView> params)
	{
		uint32_t value = 0;
		if (!rkit::utils::TryParseInteger(value, params[0], 10))
		{
			rkit::log::Error(u8"Invalid value for 'lightsrc' command");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		outCommand.m_union.m_uint = value;

		RKIT_RETURN_OK;
	}

	rkit::Result SceneParser::ParseFloat3Command(SceneCommand &outCommand, rkit::ConstSpan<rkit::ByteStringSliceView> params)
	{
		rkit::StaticArray<float, 3> colorParts;
		for (size_t i = 0; i < 3; i++)
		{
			if (!rkit::utils::TryParseFloat(colorParts[i], params[i]))
			{
				rkit::log::Error(u8"Invalid value for float3 command");
				RKIT_THROW(rkit::ResultCode::kDataError);
			}
		}

		outCommand.m_union.m_float3.m_v0 = colorParts[0];
		outCommand.m_union.m_float3.m_v1 = colorParts[1];
		outCommand.m_union.m_float3.m_v2 = colorParts[2];

		RKIT_RETURN_OK;
	}

	rkit::Result SceneParser::ParseFloat4Command(SceneCommand &outCommand, rkit::ConstSpan<rkit::ByteStringSliceView> params)
	{
		rkit::StaticArray<float, 4> colorParts;
		for (size_t i = 0; i < 4; i++)
		{
			if (!rkit::utils::TryParseFloat(colorParts[i], params[i]))
			{
				rkit::log::Error(u8"Invalid value for float4 command");
				RKIT_THROW(rkit::ResultCode::kDataError);
			}
		}

		outCommand.m_union.m_float4.m_v0 = colorParts[0];
		outCommand.m_union.m_float4.m_v1 = colorParts[1];
		outCommand.m_union.m_float4.m_v2 = colorParts[2];
		outCommand.m_union.m_float4.m_v3 = colorParts[3];

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
				RKIT_CHECK(outSubStrings.Append(str.SubString(start, i - start)));
				start = i + 1;
			}
		}
		RKIT_CHECK(outSubStrings.Append(str.SubString(start, str.Length() - start)));

		RKIT_RETURN_OK;
	}

	SceneAnalyzer::SceneAnalyzer(rkit::UniquePtr<UserEntityDictionaryBase> dict, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
		: m_dict(std::move(dict))
		, m_feedback(feedback)
	{
	}

	rkit::Result SceneAnalyzer::ProcessCommandNode(uint32_t flags, uint32_t timeLen, rkit::ConstSpan<SceneCommand> commands)
	{
		for (const SceneCommand &cmd : commands)
		{
			switch (cmd.m_commandType)
			{
			case SceneCommandType::kNewEnt:
				{
					uint32_t edefID = 0;;
					const rkit::ByteStringSliceView entClass = SceneCommand::UnwrapString(cmd.m_union.m_newEnt.m_name);
					if (!m_dict->FindEntityDef(entClass, edefID))
					{
						rkit::log::Error(u8"Unknown entity type");
						RKIT_THROW(rkit::ResultCode::kDataError);
					}

					rkit::String edefIdentifier;
					RKIT_CHECK(EntityDefCompilerBase::FormatEDef(edefIdentifier, edefID));

					RKIT_CHECK(m_feedback->AddNodeDependency(kAnoxNamespaceID, buildsystem::kEntityDefNodeID, rkit::buildsystem::BuildFileLocation::kIntermediateDir, edefIdentifier));
				}
				break;
			default:
				break;
			}
		}

		RKIT_RETURN_OK;
	}

	rkit::Result SceneCompiler::RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		rkit::Vector<uint8_t> script;
		RKIT_CHECK(ReadScriptInput(script, depsNode, feedback));

		rkit::UniquePtr<UserEntityDictionaryBase> dict;
		RKIT_CHECK(EntityDefCompilerBase::LoadUserEntityDictionary(dict, feedback));

		SceneAnalyzer analyzer(std::move(dict), feedback);

		SceneParser parser;
		RKIT_CHECK(parser.ParseSceneFile(script.ToSpan(), analyzer));

		RKIT_RETURN_OK;
	}

	rkit::Result SceneCompiler::RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::Result SceneCompiler::ReadScriptInput(rkit::Vector<uint8_t> &outVector, rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		rkit::CIPath path;
		RKIT_CHECK(path.Set(depsNode->GetIdentifier()));

		rkit::UniquePtr<rkit::ISeekableReadStream> inputFile;
		RKIT_CHECK(feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kSourceDir, path, inputFile));

		if (inputFile->GetSize() > std::numeric_limits<size_t>::max())
			RKIT_THROW(rkit::ResultCode::kIntegerOverflow);

		const size_t size = static_cast<size_t>(inputFile->GetSize());

		RKIT_CHECK(outVector.Resize(size));
		RKIT_CHECK(inputFile->ReadAllSpan(outVector.ToSpan()));

		RKIT_RETURN_OK;
	}

	uint32_t SceneCompiler::GetVersion() const
	{
		return 1;
	}

	rkit::Result SceneCompilerBase::Create(rkit::UniquePtr<SceneCompilerBase> &outCompiler)
	{
		return rkit::New<SceneCompiler>(outCompiler);
	}
}
