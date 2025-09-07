#include "AnoxEntityDefCompiler.h"

#include "rkit/Core/Algorithm.h"
#include "rkit/Core/HashTable.h"
#include "rkit/Core/Stream.h"

#include "anox/Label.h"
#include "anox/Data/EntityDef.h"

namespace anox { namespace buildsystem {
	class EntityDefCompiler final : public EntityDefCompilerBase
	{
	public:
		bool HasAnalysisStage() const override;

		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;

		static rkit::StringView GetEDefFilePath();

		uint32_t GetVersion() const override;

	private:
		static rkit::Result IndexString(rkit::Vector<rkit::AsciiString> &strings, rkit::HashMap<rkit::AsciiString, uint16_t> &stringToIndex, const rkit::AsciiString &str, uint16_t &outIndex);
		static rkit::Result ParseLabel(const rkit::ConstSpan<char> &span, Label &outLabel);
	};

	bool EntityDefCompiler::HasAnalysisStage() const
	{
		return false;
	}

	rkit::Result EntityDefCompiler::RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		return rkit::ResultCode::kInternalError;
	}

	rkit::Result EntityDefCompiler::RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		rkit::UniquePtr<rkit::ISeekableReadStream> inFile;
		RKIT_CHECK(feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kSourceDir, "models/entity.dat", inFile));

		if (inFile->GetSize() >= std::numeric_limits<size_t>::max())
			return rkit::ResultCode::kOutOfMemory;

		const size_t fileSize = static_cast<size_t>(inFile->GetSize());

		rkit::Vector<char> edefCharsArray;
		RKIT_CHECK(edefCharsArray.Resize(fileSize));

		RKIT_CHECK(inFile->ReadAll(edefCharsArray.GetBuffer(), edefCharsArray.Count()));

		rkit::ConstSpan<char> fileChars = edefCharsArray.ToSpan();


		rkit::Vector<rkit::AsciiString> strings;
		rkit::HashMap<rkit::AsciiString, uint16_t> stringToIndex;

		rkit::Vector<data::UserEntityDef> edefs;

		rkit::IUtilitiesDriver *utils = rkit::GetDrivers().m_utilitiesDriver;

		size_t lineStart = 0;
		for (size_t lineEnd = 0; lineEnd <= fileSize; lineEnd++)
		{
			if (lineEnd != fileSize && fileChars[lineEnd] != '\r' && fileChars[lineEnd] != '\n')
				continue;

			// Don't really care about CRLF compaction because we ignore empty lines for this
			rkit::ConstSpan<char> lineChars = fileChars.SubSpan(lineStart, lineEnd - lineStart);

			lineStart = lineEnd + 1;

			while (lineChars.Count() > 0 && rkit::IsASCIIWhitespace(lineChars[0]))
				lineChars = lineChars.SubSpan(1);

			while (lineChars.Count() > 0 && rkit::IsASCIIWhitespace(lineChars[lineChars.Count() - 1]))
				lineChars = lineChars.SubSpan(0, lineChars.Count() - 1);

			if (lineChars.Count() == 0)
				continue;

			if (lineChars.Count() >= 2 && lineChars[0] == '/' && lineChars[1] == '/')
				continue;

			// ???
			if (lineChars[0] == ';')
				continue;

			rkit::StaticArray<rkit::ConstSpan<char>, 24> fragments;
			size_t numFragments = 0;

			size_t fragmentStart = 0;
			for (size_t fragmentEnd = 0; fragmentEnd <= lineChars.Count(); fragmentEnd++)
			{
				if (fragmentEnd != lineChars.Count() && lineChars[fragmentEnd] != '|')
					continue;

				if (numFragments == fragments.Count())
					return rkit::ResultCode::kDataError;

				fragments[numFragments++] = lineChars.SubSpan(fragmentStart, fragmentEnd - fragmentStart);

				fragmentStart = fragmentEnd + 1;
			}

			anox::data::UserEntityDef edef;

			for (size_t realFragmentIndex = 0; realFragmentIndex < numFragments; realFragmentIndex++)
			{
				rkit::ConstSpan<char> fragment = fragments[realFragmentIndex];

				size_t fragmentIndex = realFragmentIndex;
				if (numFragments == 20 && fragmentIndex >= 18)
				{
					// A few weird items
					fragmentIndex += 4;
				}

				if (numFragments == 18 && fragmentIndex >= 16)
				{
					// ob_bindel
					fragmentIndex += 6;
				}

				switch (fragmentIndex)
				{
				case 0:
				case 1:
				case 23:
					{
						if (fragmentIndex == 1)
						{
							const size_t fragmentLen = fragment.Count();
							if (fragmentLen > 5 && fragment[fragmentLen - 5] == '!')
							{
								const rkit::ConstSpan<char> code = fragment.SubSpan(fragmentLen - 4);

								edef.m_modelCode = rkit::utils::ComputeFourCC(code[0], code[1], code[2], code[3]);

								fragment = fragment.SubSpan(0, fragment.Count() - 5);
							}
						}

						rkit::AsciiString str;
						RKIT_CHECK(str.Set(fragment));

						uint16_t stringIndex = 0;
						RKIT_CHECK(IndexString(strings, stringToIndex, str, stringIndex));

						if (fragmentIndex == 1)
							edef.m_classNameStringID = stringIndex;
						else if (fragmentIndex == 2)
							edef.m_modelPathStringID = stringIndex;
						else if (fragmentIndex == 23)
							edef.m_descriptionStringID = stringIndex;
					}
					break;
				case 2:
				case 3:
				case 4:
				case 6:
				case 7:
				case 8:
				case 9:
				case 10:
				case 11:
				case 14:
				case 15:
				case 16:
					{
						rkit::String str;
						RKIT_CHECK(str.Set(fragment));

						double d = 0.0;
						if (str == "128F")
						{
							// Broken ob_billkawa entry
							d = 128.0;
						}
						else
						{
							if (!utils->ParseDouble(str, d))
								return rkit::ResultCode::kDataError;
						}

						const float f = static_cast<float>(d);

						if (fragmentIndex == 14)
							edef.m_walkSpeed = f;
						else if (fragmentIndex == 15)
							edef.m_runSpeed = f;
						else if (fragmentIndex == 16)
							edef.m_speed = f;
						else if (fragmentIndex >= 9)
							edef.m_bboxMax[fragmentIndex - 9] = f;
						else if (fragmentIndex >= 6)
							edef.m_bboxMin[fragmentIndex - 6] = f;
						else if (fragmentIndex >= 2)
							edef.m_bboxMin[fragmentIndex - 2] = f;
					}
					break;
				case 5:
					{
						rkit::AsciiStringSliceView slice(fragment);
						data::UserEntityType userEntityType = data::UserEntityType::kCount;
						if (slice == "playerchar")
							userEntityType = data::UserEntityType::kPlayerChar;
						else if (slice == "char")
							userEntityType = data::UserEntityType::kChar;
						else if (slice == "charfly")
							userEntityType = data::UserEntityType::kCharFly;
						else if (slice == "noclip")
							userEntityType = data::UserEntityType::kNoClip;
						else if (slice == "general")
							userEntityType = data::UserEntityType::kGeneral;
						else if (slice == "trashspawn")
							userEntityType = data::UserEntityType::kTrashSpawn;
						else if (slice == "bugspawn")
							userEntityType = data::UserEntityType::kBugSpawn;
						else if (slice == "lottobot")
							userEntityType = data::UserEntityType::kLottoBot;
						else if (slice == "pickup")
							userEntityType = data::UserEntityType::kPickup;
						else if (slice == "scavenger")
							userEntityType = data::UserEntityType::kScavenger;
						else if (slice == "effect")
							userEntityType = data::UserEntityType::kEffect;
						else if (slice == "container")
							userEntityType = data::UserEntityType::kContainer;
						else if (slice == "lightsource")
							userEntityType = data::UserEntityType::kLightSource;
						else if (slice == "sunpoint")
							userEntityType = data::UserEntityType::kSunPoint;
						else
							return rkit::ResultCode::kDataError;

						edef.m_userEntityType = static_cast<uint8_t>(userEntityType);
					}
					break;
				case 12:
					{
						rkit::AsciiStringSliceView slice(fragment);
						data::UserEntityShadowType shadowType = data::UserEntityShadowType::kNoShadow;

						if (slice == "shadow")
							shadowType = data::UserEntityShadowType::kShadow;
						else if (slice == "noshadow")
							shadowType = data::UserEntityShadowType::kNoShadow;
						else if (slice == "lightning")
							shadowType = data::UserEntityShadowType::kLightning;
						else
							return rkit::ResultCode::kDataError;

						edef.m_shadowType = static_cast<uint8_t>(shadowType);
					}
					break;
				case 13:
				case 17:
				case 18:
				case 21:
					{
						rkit::AsciiStringSliceView slice(fragment);
						if (slice == "1")
						{
							if (fragmentIndex == 13)
								edef.m_flags |= static_cast<uint8_t>(data::UserEntityFlags::kSolid);
							else if (fragmentIndex == 17)
								edef.m_flags |= static_cast<uint8_t>(data::UserEntityFlags::kLighting);
							else if (fragmentIndex == 18)
								edef.m_flags |= static_cast<uint8_t>(data::UserEntityFlags::kBlending);
							else if (fragmentIndex == 21)
								edef.m_flags |= static_cast<uint8_t>(data::UserEntityFlags::kNoMip);
							else
								return rkit::ResultCode::kInternalError;
						}
						else if (slice == "0" || slice == "" || slice == "0:0")	// 0:0 for broken npc_alien_rowdy
						{
						}
						else
							return rkit::ResultCode::kDataError;
					}
					break;
				case 19:
					{
						Label label;
						RKIT_CHECK(ParseLabel(fragment, label));
						edef.m_targetSequenceID = label.RawValue();
					}
					break;
				case 20:
					{
						rkit::AsciiStringSliceView slice(fragment);
						if (slice != "0")
							return rkit::ResultCode::kDataError;
					}
					break;
				case 22:
					{
						rkit::AsciiStringSliceView slice(fragment);
						if (slice != "none")
						{
							Label label;
							RKIT_CHECK(ParseLabel(fragment, label));
							edef.m_startSequenceID = label.RawValue();
						}
					}
					break;
				default:
					return rkit::ResultCode::kInternalError;
				}
			}

			RKIT_CHECK(edefs.Append(edef));
		}

		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result EntityDefCompiler::IndexString(rkit::Vector<rkit::AsciiString> &strings, rkit::HashMap<rkit::AsciiString, uint16_t> &stringToIndex, const rkit::AsciiString &str, uint16_t &outIndex)
	{
		if (str.Length() > 256)
			return rkit::ResultCode::kDataError;

		rkit::HashValue_t hash = rkit::Hasher<rkit::AsciiString>::ComputeHash(0, str);
		rkit::HashMap<rkit::AsciiString, uint16_t>::ConstIterator_t it = stringToIndex.FindPrehashed(hash, str);
		if (it == stringToIndex.end())
		{
			if (strings.Count() == std::numeric_limits<uint16_t>::max())
				return rkit::ResultCode::kDataError;

			const uint16_t stringIndex = static_cast<uint16_t>(strings.Count());
			RKIT_CHECK(strings.Append(str));
			RKIT_CHECK(stringToIndex.SetPrehashed(hash, str, stringIndex));

			outIndex = stringIndex;
		}
		else
			outIndex = it.Value();

		return rkit::ResultCode::kOK;
	}

	rkit::Result EntityDefCompiler::ParseLabel(const rkit::ConstSpan<char> &span, Label &outLabel)
	{
		rkit::IUtilitiesDriver *utils = rkit::GetDrivers().m_utilitiesDriver;

		size_t dividerPos = 0;
		bool foundDivider = false;

		for (size_t i = 0; i < span.Count(); i++)
		{
			if (span[i] == ':')
			{
				if (foundDivider)
					return rkit::ResultCode::kDataError;
				else
				{
					dividerPos = i;
					foundDivider = true;
				}
			}
			else
			{
				if (span[i] < '0' || span[i] > '9')
					return rkit::ResultCode::kDataError;
			}
		}

		if (!foundDivider)
		{
			if (span.Count() == 1 && span[0] == '0')
			{
				// Deal with broken npc_rowdy_alien
				outLabel = Label();
				return rkit::ResultCode::kOK;
			}

			return rkit::ResultCode::kDataError;
		}

		uint32_t highPart = 0;
		uint32_t lowPart = 0;
		if (!utils->ParseUInt32(rkit::StringSliceView(span.SubSpan(0, dividerPos)), 10, highPart)
			|| !utils->ParseUInt32(rkit::StringSliceView(span.SubSpan(dividerPos + 1)), 10, lowPart)
			|| !Label::IsValid(highPart, lowPart))
			return rkit::ResultCode::kDataError;

		outLabel = Label(highPart, lowPart);

		return rkit::ResultCode::kOK;
	}

	uint32_t EntityDefCompiler::GetVersion() const
	{
		return 1;
	}

	rkit::StringView EntityDefCompilerBase::GetEDefFilePath()
	{
		return "entities.edef";
	}

	rkit::Result EntityDefCompilerBase::Create(rkit::UniquePtr<EntityDefCompilerBase> &outCompiler)
	{
		return rkit::New<EntityDefCompiler>(outCompiler);
	}
} }

