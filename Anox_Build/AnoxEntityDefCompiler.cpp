#include "AnoxEntityDefCompiler.h"

#include "rkit/Core/QuickSort.h"
#include "rkit/Core/Algorithm.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/HashTable.h"
#include "rkit/Core/Stream.h"

#include "anox/AnoxModule.h"
#include "anox/Label.h"
#include "anox/Data/EntityDef.h"

#include "AnoxModelCompiler.h"
#include "anox/AnoxUtilitiesDriver.h"

namespace anox { namespace buildsystem {
	struct UserEntityDef2
	{
		rkit::ByteString m_className;
		rkit::AsciiString m_modelPath;
		uint32_t m_modelCode = 0;
		float m_scale[3] = { 0, 0, 0 };
		rkit::ByteString m_type;
		data::UserEntityShadowType m_shadowType = data::UserEntityShadowType::kShadow;
		float m_bboxMin[3] = { 0, 0, 0 };
		float m_bboxMax[3] = { 0, 0, 0 };
		uint8_t m_flags = 0;
		float m_walkSpeed = 0.f;
		float m_runSpeed = 0.f;
		float m_speed = 0.f;
		Label m_targetSequence;
		rkit::endian::LittleUInt32_t m_miscValue;
		Label m_startSequence;
		rkit::ByteString m_description;
	};

	class UserEntityDictionary final : public UserEntityDictionaryBase
	{
	public:
		explicit UserEntityDictionary(rkit::Vector<UserEntityDef2> &&edefs);
		bool FindEntityDef(const rkit::ByteStringSliceView &name, uint32_t &outEDefID) const override;

		rkit::ByteStringView GetEDefType(uint32_t edefID) const override;
		uint32_t GetEDefCount() const override;

		rkit::Result WriteEDef(rkit::IWriteStream &stream, uint32_t edefID) const override;

		const UserEntityDef2 &GetEDef(uint32_t edefID) const;

	private:
		rkit::Vector<UserEntityDef2> m_defs;
	};


	class EntityDefCompiler final : public EntityDefCompilerBase
	{
	public:
		bool HasAnalysisStage() const override;

		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;

		uint32_t GetVersion() const override;

		static rkit::Result ParseLabel(const rkit::ConstSpan<uint8_t> &span, Label &outLabel);

	private:
		static rkit::Result IndexString(rkit::Vector<rkit::AsciiString> &strings, rkit::HashMap<rkit::AsciiString, uint16_t> &stringToIndex, const rkit::AsciiString &str, uint16_t &outIndex);
	};

	UserEntityDictionary::UserEntityDictionary(rkit::Vector<UserEntityDef2> &&edefs)
		: m_defs(std::move(edefs))
	{
		rkit::QuickSort(m_defs.begin(), m_defs.end(), [](const UserEntityDef2 &a, const UserEntityDef2 &b)
			{
				return a.m_className.ToByteView() < b.m_className.ToByteView();
			});
	}

	bool UserEntityDictionary::FindEntityDef(const rkit::ByteStringSliceView &name, uint32_t &outEDefID) const
	{
		size_t minInclusive = 0;
		size_t maxExclusive = m_defs.Count();

		while (minInclusive != maxExclusive)
		{
			const size_t testIndex = (minInclusive + maxExclusive) / 2;

			rkit::Ordering ordering = m_defs[testIndex].m_className.ToByteView().Compare(name);
			if (ordering == rkit::Ordering::kEqual)
			{
				outEDefID = static_cast<uint32_t>(testIndex);
				return true;
			}

			if (ordering == rkit::Ordering::kGreater)
				maxExclusive = testIndex;
			else
			{
				RKIT_ASSERT(ordering == rkit::Ordering::kLess);
				minInclusive = testIndex + 1;
			}
		}

		return false;
	}

	rkit::ByteStringView UserEntityDictionary::GetEDefType(uint32_t edefID) const
	{
		return m_defs[edefID].m_type;
	}

	uint32_t UserEntityDictionary::GetEDefCount() const
	{
		return static_cast<uint32_t>(m_defs.Count());
	}

	rkit::Result UserEntityDictionary::WriteEDef(rkit::IWriteStream &stream, uint32_t edefID) const
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	const UserEntityDef2 &UserEntityDictionary::GetEDef(uint32_t edefID) const
	{
		return m_defs[edefID];
	}

	bool EntityDefCompiler::HasAnalysisStage() const
	{
		return true;
	}

	rkit::Result EntityDefCompiler::RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		const rkit::StringView identifier = depsNode->GetIdentifier();
		const rkit::StringView prefix = u8"edefs/edef";
		if (!identifier.StartsWith(prefix))
			return rkit::ResultCode::kInternalError;

		uint32_t edefID = 0;
		if (!rkit::GetDrivers().m_utilitiesDriver->ParseUInt32(identifier.SubString(prefix.Length()).RemoveEncoding(), 10, edefID))
			return rkit::ResultCode::kInternalError;

		rkit::UniquePtr<UserEntityDictionaryBase> dictionary;
		RKIT_CHECK(EntityDefCompilerBase::LoadUserEntityDictionary(dictionary, feedback));

		const uint32_t numEDefs = dictionary->GetEDefCount();

		if (edefID >= numEDefs)
			return rkit::ResultCode::kInternalError;

		const UserEntityDef2 &edef = static_cast<UserEntityDictionary &>(*dictionary).GetEDef(edefID);
		const rkit::AsciiStringView modelPath = edef.m_modelPath;

		rkit::String modelPathStr;
		RKIT_CHECK(modelPathStr.ConvertFrom(modelPath));
		RKIT_CHECK(modelPathStr.MakeLower());

		if (modelPath.EndsWithNoCase(".md2"))
		{
			RKIT_CHECK(feedback->AddNodeDependency(kAnoxNamespaceID, kMD2ModelNodeID, rkit::buildsystem::BuildFileLocation::kSourceDir, modelPathStr));
		}
		else if (modelPath.EndsWithNoCase(".mda"))
		{
			RKIT_CHECK(feedback->AddNodeDependency(kAnoxNamespaceID, kMDAModelNodeID, rkit::buildsystem::BuildFileLocation::kSourceDir, modelPathStr));
		}
		else if (modelPath.EndsWithNoCase(".ctc"))
		{
			RKIT_CHECK(feedback->AddNodeDependency(kAnoxNamespaceID, kCTCModelNodeID, rkit::buildsystem::BuildFileLocation::kSourceDir, modelPathStr));
		}
		else
			return rkit::ResultCode::kDataError;

		RKIT_RETURN_OK;
	}

	rkit::Result EntityDefCompiler::RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		anox::IUtilitiesDriver *anoxUtils = static_cast<anox::IUtilitiesDriver *>(rkit::GetDrivers().FindDriver(kAnoxNamespaceID, u8"Utilities"));

		const rkit::StringView identifier = depsNode->GetIdentifier();
		const rkit::StringView prefix = u8"edefs/edef";
		if (!identifier.StartsWith(prefix))
			return rkit::ResultCode::kInternalError;

		uint32_t edefID = 0;
		if (!rkit::GetDrivers().m_utilitiesDriver->ParseUInt32(identifier.SubString(prefix.Length()).RemoveEncoding(), 10, edefID))
			return rkit::ResultCode::kInternalError;

		rkit::UniquePtr<UserEntityDictionaryBase> dictionary;
		RKIT_CHECK(EntityDefCompilerBase::LoadUserEntityDictionary(dictionary, feedback));

		const uint32_t numEDefs = dictionary->GetEDefCount();

		if (edefID >= numEDefs)
			return rkit::ResultCode::kInternalError;

		const UserEntityDef2 &edef = static_cast<UserEntityDictionary &>(*dictionary).GetEDef(edefID);
		const rkit::AsciiStringView modelPathStrView = edef.m_modelPath;

		if (edef.m_description.Length() > 255)
		{
			rkit::log::Error(u8"Description too long");
			return rkit::ResultCode::kDataError;
		}

		rkit::Optional<size_t> classDefIndex;

		{
			const data::EntityDefsSchema &schema = anoxUtils->GetEntityDefs();

			rkit::ByteString fullType;
			RKIT_CHECK(fullType.Set(rkit::StringView(u8"userentity_").RemoveEncoding()));
			RKIT_CHECK(fullType.Append(edef.m_type));

			for (size_t i = 0; i < schema.m_numClassDefs; i++)
			{
				const data::EntityClassDef &classDef = *schema.m_classDefs[i];

				rkit::StringSliceView className(classDef.m_name, classDef.m_nameLength);

				if (className.RemoveEncoding() == fullType)
				{
					classDefIndex = i;
					break;
				}
			}

			if (!classDefIndex.IsSet())
			{
				rkit::log::Error(u8"Invalid userentity type");
				return rkit::ResultCode::kDataError;
			}
		}

		data::UserEntityDef outDef = {};
		outDef.m_modelCode = edef.m_modelCode;

		for (size_t axis = 0; axis < 3; axis++)
		{
			outDef.m_scale[axis] = edef.m_scale[axis];
			outDef.m_bboxMin[axis] = edef.m_bboxMin[axis];
			outDef.m_bboxMax[axis] = edef.m_bboxMax[axis];
		}

		{
			const rkit::AsciiStringView modelPath = edef.m_modelPath;

			rkit::String modelPathStr;
			RKIT_CHECK(modelPathStr.ConvertFrom(modelPath));
			RKIT_CHECK(modelPathStr.MakeLower());

			rkit::CIPath outputModelPath;

			if (modelPath.EndsWithNoCase(".md2"))
			{
				RKIT_CHECK(AnoxMD2CompilerBase::ConstructOutputPath(outputModelPath, modelPathStr));
			}
			else if (modelPath.EndsWithNoCase(".mda"))
			{
				RKIT_CHECK(AnoxMDACompilerBase::ConstructOutputPath(outputModelPath, modelPathStr));
			}
			else if (modelPath.EndsWithNoCase(".ctc"))
			{
				RKIT_CHECK(AnoxCTCCompilerBase::ConstructOutputPath(outputModelPath, modelPathStr));
			}
			else
				return rkit::ResultCode::kDataError;

			RKIT_CHECK(feedback->IndexCAS(rkit::buildsystem::BuildFileLocation::kIntermediateDir, outputModelPath, outDef.m_modelContentID));
		}

		outDef.m_entityType = static_cast<uint32_t>(classDefIndex.Get());
		outDef.m_shadowType = static_cast<uint8_t>(edef.m_shadowType);
		outDef.m_flags = edef.m_flags;
		outDef.m_walkSpeed = edef.m_walkSpeed;
		outDef.m_runSpeed = edef.m_runSpeed;
		outDef.m_speed = edef.m_speed;
		outDef.m_targetSequenceID = edef.m_targetSequence.RawValue();
		outDef.m_miscValue = edef.m_miscValue;
		outDef.m_startSequenceID = edef.m_startSequence.RawValue();
		outDef.m_descriptionStringLength = static_cast<uint8_t>(edef.m_description.Length());

		rkit::CIPath edefPath;
		RKIT_CHECK(edefPath.Set(depsNode->GetIdentifier()));

		rkit::UniquePtr<rkit::ISeekableReadWriteStream> outFile;
		RKIT_CHECK(feedback->OpenOutput(rkit::buildsystem::BuildFileLocation::kIntermediateDir, edefPath, outFile));

		RKIT_CHECK(outFile->WriteAll(&outDef, sizeof(outDef)));
		RKIT_CHECK(outFile->WriteAllSpan(edef.m_description.ToSpan()));

		RKIT_RETURN_OK;
	}

	rkit::Result EntityDefCompiler::IndexString(rkit::Vector<rkit::AsciiString> &strings, rkit::HashMap<rkit::AsciiString, uint16_t> &stringToIndex, const rkit::AsciiString &str, uint16_t &outIndex)
	{
		if (str.Length() > 256)
			return rkit::ResultCode::kDataError;

		rkit::HashValue_t hash = rkit::Hasher<rkit::AsciiString>::ComputeHash(0, str);
		rkit::HashMap<rkit::AsciiString, uint16_t>::ConstIterator_t it = stringToIndex.FindPrehashed(hash, str);
		if (it == stringToIndex.end())
		{
			const uint16_t stringIndex = static_cast<uint16_t>(strings.Count());

			if (stringIndex == std::numeric_limits<uint16_t>::max())
				return rkit::ResultCode::kDataError;

			RKIT_CHECK(strings.Append(str));
			RKIT_CHECK(stringToIndex.SetPrehashed(hash, str, stringIndex));

			outIndex = stringIndex;
		}
		else
			outIndex = it.Value();

		RKIT_RETURN_OK;
	}

	uint32_t EntityDefCompiler::GetVersion() const
	{
		return 2;
	}

	rkit::Result EntityDefCompiler::ParseLabel(const rkit::ConstSpan<uint8_t> &span, Label &outLabel)
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
				RKIT_RETURN_OK;
			}

			return rkit::ResultCode::kDataError;
		}

		uint32_t highPart = 0;
		uint32_t lowPart = 0;
		if (!utils->ParseUInt32(rkit::ByteStringSliceView(span.SubSpan(0, dividerPos)), 10, highPart)
			|| !utils->ParseUInt32(rkit::ByteStringSliceView(span.SubSpan(dividerPos + 1)), 10, lowPart)
			|| !Label::IsValid(highPart, lowPart))
			return rkit::ResultCode::kDataError;

		outLabel = Label(highPart, lowPart);

		RKIT_RETURN_OK;
	}

	rkit::Result EntityDefCompilerBase::FormatEDef(rkit::String &edefIdentifier, uint32_t edefID)
	{
		return edefIdentifier.Format(u8"edefs/edef{}", edefID);
	}

	rkit::Result EntityDefCompilerBase::LoadUserEntityDictionary(rkit::UniquePtr<UserEntityDictionaryBase> &outDictionary, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		rkit::UniquePtr<rkit::ISeekableReadStream> inFile;
		RKIT_CHECK(feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kSourceDir, u8"models/entity.dat", inFile));

		if (inFile->GetSize() >= std::numeric_limits<size_t>::max())
			return rkit::ResultCode::kOutOfMemory;

		const size_t fileSize = static_cast<size_t>(inFile->GetSize());

		rkit::Vector<uint8_t> edefCharsArray;
		RKIT_CHECK(edefCharsArray.Resize(fileSize));

		RKIT_CHECK(inFile->ReadAllSpan(edefCharsArray.ToSpan()));

		rkit::ConstSpan<uint8_t> fileChars = edefCharsArray.ToSpan();

		rkit::Vector<rkit::ByteString> strings;
		rkit::HashMap<rkit::ByteString, uint16_t> stringToIndex;

		rkit::Vector<UserEntityDef2> edefs;

		rkit::IUtilitiesDriver *utils = rkit::GetDrivers().m_utilitiesDriver;

		size_t lineStart = 0;
		for (size_t lineEnd = 0; lineEnd <= fileSize; lineEnd++)
		{
			if (lineEnd != fileSize && fileChars[lineEnd] != '\r' && fileChars[lineEnd] != '\n')
				continue;

			// Don't really care about CRLF compaction because we ignore empty lines for this
			rkit::ConstSpan<uint8_t> lineChars = fileChars.SubSpan(lineStart, lineEnd - lineStart);

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

			rkit::StaticArray<rkit::ConstSpan<uint8_t>, 24> fragments;
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

			UserEntityDef2 edef;

			for (size_t realFragmentIndex = 0; realFragmentIndex < numFragments; realFragmentIndex++)
			{
				rkit::ConstSpan<uint8_t> fragment = fragments[realFragmentIndex];

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
								const rkit::ConstSpan<uint8_t> code = fragment.SubSpan(fragmentLen - 4);

								edef.m_modelCode = rkit::utils::ComputeFourCC(code[0], code[1], code[2], code[3]);

								fragment = fragment.SubSpan(0, fragment.Count() - 5);
							}
						}

						if (fragmentIndex == 0)
						{
							RKIT_CHECK(edef.m_className.Set(fragment));
						}
						else if (fragmentIndex == 1)
						{
							if (!rkit::CharacterEncodingValidator<rkit::CharacterEncoding::kASCII>::ValidateSpan(fragment))
								return rkit::ResultCode::kDataError;

							RKIT_CHECK(edef.m_modelPath.Set(rkit::AsciiStringSliceView(fragment.ReinterpretCast<const char>())));
						}
						else if (fragmentIndex == 23)
						{
							RKIT_CHECK(edef.m_description.Set(fragment));
						}
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
						rkit::ByteStringSliceView str(fragment);

						double d = 0.0;
						if (str == rkit::StringView(u8"128F").RemoveEncoding())
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
							edef.m_scale[fragmentIndex - 2] = f;
					}
					break;
				case 5:
					RKIT_CHECK(edef.m_type.Set(fragment));
					break;
				case 12:
					{
						rkit::ByteStringSliceView slice(fragment);
						data::UserEntityShadowType shadowType = data::UserEntityShadowType::kNoShadow;

						if (slice == rkit::StringView(u8"shadow").RemoveEncoding())
							shadowType = data::UserEntityShadowType::kShadow;
						else if (slice == rkit::StringView(u8"noshadow").RemoveEncoding())
							shadowType = data::UserEntityShadowType::kNoShadow;
						else if (slice == rkit::StringView(u8"lightning").RemoveEncoding())
							shadowType = data::UserEntityShadowType::kLightning;
						else
							return rkit::ResultCode::kDataError;

						edef.m_shadowType = shadowType;
					}
					break;
				case 13:
				case 17:
				case 18:
				case 21:
					{
						rkit::ByteStringSliceView slice(fragment);
						if (slice == rkit::AsciiStringView("1").RemoveEncoding())
						{
							if (fragmentIndex == 13)
								edef.m_flags |= static_cast<uint8_t>(1 << static_cast<int>(data::UserEntityFlags::kSolid));
							else if (fragmentIndex == 17)
								edef.m_flags |= static_cast<uint8_t>(1 << static_cast<int>(data::UserEntityFlags::kLighting));
							else if (fragmentIndex == 18)
								edef.m_flags |= static_cast<uint8_t>(1 << static_cast<int>(data::UserEntityFlags::kBlending));
							else if (fragmentIndex == 21)
								edef.m_flags |= static_cast<uint8_t>(1 << static_cast<int>(data::UserEntityFlags::kNoMip));
							else
								return rkit::ResultCode::kInternalError;
						}
						else if (slice == rkit::AsciiStringView("0").RemoveEncoding()
							|| slice.Length() == 0
							|| slice == rkit::AsciiStringView("0:0").RemoveEncoding())	// 0:0 for broken npc_alien_rowdy
						{
						}
						else
							return rkit::ResultCode::kDataError;
					}
					break;
				case 19:
					{
						Label label;
						RKIT_CHECK(EntityDefCompiler::ParseLabel(fragment, edef.m_targetSequence));
					}
					break;
				case 20:
					{
						rkit::ByteStringSliceView slice(fragment);
						if (slice != rkit::AsciiStringView("0").RemoveEncoding())
							return rkit::ResultCode::kDataError;
					}
					break;
				case 22:
					{
						rkit::ByteStringSliceView slice(fragment);
						if (slice != rkit::AsciiStringView("none").RemoveEncoding())
						{
							Label label;
							RKIT_CHECK(EntityDefCompiler::ParseLabel(fragment, edef.m_startSequence));
						}
					}
					break;
				default:
					return rkit::ResultCode::kInternalError;
				}
			}

			RKIT_CHECK(edefs.Append(edef));
		}

		RKIT_CHECK(rkit::New<UserEntityDictionary>(outDictionary, std::move(edefs)));

		RKIT_RETURN_OK;
	}

	rkit::Result EntityDefCompilerBase::Create(rkit::UniquePtr<EntityDefCompilerBase> &outCompiler)
	{
		return rkit::New<EntityDefCompiler>(outCompiler);
	}
} }

