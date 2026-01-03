#include "AnoxMDAModelResource.h"

#include "rkit/Core/Future.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/JobQueue.h"
#include "rkit/Core/MemoryStream.h"
#include "rkit/Core/Vector.h"

#include "anox/Data/MDAModel.h"

#include "AnoxGameFileSystem.h"
#include "AnoxLoadEntireFileJob.h"
#include "AnoxAbstractSingleFileResource.h"

namespace anox
{
	struct AnoxMDAModelLoaderInfo;

	struct AnoxMDAModelResourceLoaderState final : public AnoxAbstractSingleFileResourceLoaderState
	{
		data::MDAModelHeader m_header;
		rkit::FilePos_t m_postContentIDsPos = 0;
	};

	class AnoxMDAModelResource final : public AnoxMDAModelResourceBase
	{
	public:
		friend struct AnoxMDAModelLoaderInfo;

		struct Pass
		{
		};

		struct Skin
		{
			data::MDASortMode m_sortMode = data::MDASortMode::kCount;
			rkit::ConstSpan<Pass> m_passes;
		};

		struct Profile
		{
			rkit::AsciiStringView m_condition;
			uint32_t m_fourCC = 0;
			rkit::ConstSpan<Skin> m_skins;
		};

	private:
		rkit::Vector<char> m_conditionChars;
		rkit::Vector<Profile> m_profiles;
		rkit::Vector<Skin> m_profileSkins;
		rkit::Vector<Pass> m_skinPasses;
	};

	struct AnoxMDAModelLoaderInfo
	{
		typedef AnoxMDAModelResourceLoaderBase LoaderBase_t;
		typedef AnoxMDAModelResource Resource_t;
		typedef AnoxMDAModelResourceLoaderState State_t;

		static constexpr bool kHasDependencies = true;
		static constexpr bool kHasAnalysisPhase = true;
		static constexpr bool kHasLoadPhase = true;

		static rkit::Result AnalyzeFile(State_t &state, Resource_t &resource, rkit::traits::TraitRef<rkit::VectorTrait<rkit::RCPtr<rkit::Job>>> outDeps);
		static rkit::Result LoadFile(State_t &state, Resource_t &resource);
	};

	rkit::Result AnoxMDAModelLoaderInfo::AnalyzeFile(State_t &state, Resource_t &resource, rkit::traits::TraitRef<rkit::VectorTrait<rkit::RCPtr<rkit::Job>>> outDeps)
	{
		rkit::ReadOnlyMemoryStream stream(state.m_fileContents.GetBuffer(), state.m_fileContents.Count());

		RKIT_CHECK(stream.ReadOneBinary(state.m_header));

		const data::MDAModelHeader &header = state.m_header;

		rkit::Vector<rkit::data::ContentID> materialContentIDs;

		const size_t numMaterials = header.m_numMaterials.Get();
		RKIT_CHECK(materialContentIDs.Resize(numMaterials));

		state.m_postContentIDsPos = stream.Tell();

		RKIT_CHECK(outDeps.Reserve(numMaterials));

		for (const rkit::data::ContentID &materialContentID : materialContentIDs)
		{
			rkit::RCPtr<rkit::Job> job;
			rkit::Future<AnoxResourceRetrieveResult> result;
			RKIT_CHECK(state.m_systems.m_resManager->GetContentIDKeyedResource(&job, result, resloaders::kModelMaterialTypeCode, materialContentID));

			RKIT_CHECK(outDeps.AppendRValue(std::move(job)));
		}

#if 0
		const uint8_t numAnimations = (header.m_numAnimations7_AnimationType1 & 0x7f);
		const data::MDAAnimationType animType = static_cast<data::MDAAnimationType>(header.m_numAnimations7_AnimationType1 >> 7);

		const size_t numProfiles = header.m_numProfiles;

		rkit::Vector<AnoxMDAModelResource::Profile> &outProfiles = resource.m_profiles;
		rkit::Vector<AnoxMDAModelResource::Skin> &outProfileSkins = resource.m_profileSkins;
		rkit::Vector<AnoxMDAModelResource::Pass> &outSkinPasses = resource.m_skinPasses;

		{
			rkit::Vector<data::MDAProfile> profiles;
			RKIT_CHECK(profiles.Resize(numProfiles));
			RKIT_CHECK(outProfiles.Resize(numProfiles));

			RKIT_CHECK(stream.ReadAllSpan(profiles.ToSpan()));

			size_t conditionLengthTotal = 0;
			for (const data::MDAProfile &profile : profiles)
			{
				size_t conditionLength = profile.m_conditionLength.Get();

				RKIT_CHECK(rkit::SafeAdd(conditionLengthTotal, conditionLengthTotal, conditionLength));
			}

			size_t paddedConditionLengthTotal = 0;
			RKIT_CHECK(rkit::SafeAdd(paddedConditionLengthTotal, conditionLengthTotal, profiles.Count()));

			rkit::Vector<char> &profileConditionsChars = resource.m_conditionChars;
			RKIT_CHECK(profileConditionsChars.Resize(paddedConditionLengthTotal));

			{
				size_t startPos = 0;
				const rkit::Result processResult = rkit::CheckedProcessParallelSpans(resource.m_profiles.ToSpan(), profiles.ToSpan(),
					[&startPos, &profileConditionsChars, &stream]
					(AnoxMDAModelResource::Profile &destProfile, const data::MDAProfile &srcProfile) -> rkit::Result
					{
						const size_t conditionLength = srcProfile.m_conditionLength.Get();
						const rkit::Span<char> chars = profileConditionsChars.ToSpan().SubSpan(startPos, conditionLength);

						RKIT_CHECK(stream.ReadAllSpan(chars));
						profileConditionsChars[startPos + conditionLength] = 0;

						rkit::AsciiStringView conditionStr(chars.Ptr(), conditionLength);
						if (!conditionStr.Validate())
							return rkit::ResultCode::kDataError;

						destProfile.m_condition = conditionStr;
						destProfile.m_fourCC = srcProfile.m_fourCC.Get();

						startPos += conditionLength + 1;
						return rkit::ResultCode::kOK;
					});
				RKIT_CHECK(processResult);
			}
		}

		const size_t numSkins = header.m_numSkins;

		size_t numProfileSkins = 0;
		size_t numSkinPasses = 0;
		RKIT_CHECK(rkit::SafeMul(numProfileSkins, numProfiles, numSkins));

		{
			rkit::Vector<data::MDASkin> profileSkins;
			RKIT_CHECK(profileSkins.Resize(numProfileSkins));
			RKIT_CHECK(outProfileSkins.Resize(numProfileSkins));

			RKIT_CHECK(stream.ReadAllSpan(profileSkins.ToSpan()));

			for (size_t i = 0; i < numProfiles; i++)
				outProfiles[i].m_skins = resource.m_profileSkins.ToSpan().SubSpan(i * numSkins, numSkins);

			size_t numSkinPasses = 0;
			for (const data::MDASkin &profileSkin : profileSkins)
			{
				RKIT_CHECK(rkit::SafeAdd<size_t>(numSkinPasses, numSkinPasses, profileSkin.m_numPasses));
			}

			const rkit::Result checkResult = rkit::CheckedProcessParallelSpans(outProfileSkins.ToSpan(), profileSkins.ToSpan(),
				[&numSkinPasses]
				(AnoxMDAModelResource::Skin &outSkin, const data::MDASkin &inSkin)
					-> rkit::Result
				{
					const size_t numPasses = inSkin.m_numPasses;
					RKIT_CHECK(rkit::SafeAdd(numSkinPasses, numSkinPasses, numPasses));

					if (inSkin.m_sortMode >= static_cast<size_t>(data::MDASortMode::kCount))
						return rkit::ResultCode::kDataError;

					outSkin.m_sortMode = static_cast<data::MDASortMode>(inSkin.m_sortMode);

					return rkit::ResultCode::kOK;
				});

			RKIT_CHECK(checkResult);

			RKIT_CHECK(outSkinPasses.Resize(numSkinPasses));

			{
				size_t startPassOffset = 0;
				for (size_t i = 0; i < numProfileSkins; i++)
				{
					const data::MDASkin &skin = profileSkins[i];
					const size_t numPasses = skin.m_numPasses;
					outProfileSkins[i].m_passes = outSkinPasses.ToSpan().SubSpan(startPassOffset, numPasses);

					startPassOffset += numPasses;
				}
			}
		}

		{
			rkit::Vector<data::MDASkinPass> skinPasses;
			RKIT_CHECK(skinPasses.Resize(numSkinPasses));

			RKIT_CHECK(stream.ReadAllSpan(skinPasses.ToSpan()));


			const rkit::Result checkResult = rkit::CheckedProcessParallelSpans(outSkinPasses.ToSpan(), skinPasses.ToSpan(),
				[]
				(AnoxMDAModelResource::Pass &outPass, const data::MDASkinPass &inPass)
					-> rkit::Result
				{

					return rkit::ResultCode::kNotYetImplemented;
				});

			RKIT_CHECK(checkResult);
		}



		// MDASkinPass m_skinPasses[m_numProfiles][m_numSkins][skin.m_numPasses]
		// MDAAnimation m_animations[m_numAnimations]
		// char m_animationNameChars[m_numAnimations][anim.m_nameLength]
		// MDAModelMorphKey m_morphKeys[m_numMorphKeys]
		// if skeletal model:
		//     MDASkeletalModelBone m_bones[m_numBones]
		// if vertex model:
		//     MDAVertexModelBone m_bones[m_numBones]
		//
		// if skeletal model:
		//     MDAModelSkeletalBoneFrame m_boneFrames[m_numFrames][m_numBones]
		// if vertex model:
		//     MDAModelTagBoneFrame m_boneFrames[m_numFrames][m_numBones]
		//
		// MDAModelSubModel m_subModels[m_numSubModels]
		// MDAModelTri m_tris[m_numSubmodels][submodel.m_numTris]
		// MDAModelVert m_verts[m_numSubmodels][submodel.m_numVerts]
		// if skeletal model:
		//     MDASkeletalModelPoint m_points[m_numPoints]
		// if vertex model:
		//     MDAModelPoint m_points[m_numFrames][m_numPoints]
		// MDAModelVertMorph m_vertMorphs[m_numMorphKeys][m_numMorphedPoints]

		/*
		const data::UserMDAModel &edef = m_resource->GetMDAModel();

		if (edef.m_modelContentID.IsNull())
		{
			m_waitForDependenciesSignaler->SignalDone(rkit::ResultCode::kOK);
		}
		else
		{
			rkit::RCPtr<rkit::Job> loadModelJob;
			rkit::Future<AnoxResourceRetrieveResult> result;
			RKIT_CHECK(m_resManager.GetContentIDKeyedResource(&loadModelJob, result, resloaders::kMDAModelResourceTypeCode, edef.m_modelContentID));

			rkit::UniquePtr<rkit::IJobRunner> dependenciesDoneJobRunner;
			RKIT_CHECK(m_jobQueue.CreateSignalJobRunner(dependenciesDoneJobRunner, m_waitForDependenciesSignaler));
			RKIT_CHECK(m_jobQueue.CreateJob(nullptr, rkit::JobType::kNormalPriority, std::move(dependenciesDoneJobRunner), loadModelJob));
		}
		*/
#endif

		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result AnoxMDAModelLoaderInfo::LoadFile(State_t &state, Resource_t &resource)
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result AnoxMDAModelResourceLoaderBase::Create(rkit::RCPtr<AnoxMDAModelResourceLoaderBase> &outLoader)
	{
		typedef AnoxAbstractSingleFileResourceLoader<AnoxMDAModelLoaderInfo> Loader_t;

		rkit::RCPtr<Loader_t> loader;
		RKIT_CHECK(rkit::New<Loader_t>(loader));

		outLoader = std::move(loader);

		return rkit::ResultCode::kOK;
	}
}
