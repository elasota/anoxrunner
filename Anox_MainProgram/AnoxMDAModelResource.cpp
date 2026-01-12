#include "AnoxMDAModelResource.h"

#include "rkit/Core/Future.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/JobQueue.h"
#include "rkit/Core/MemoryStream.h"
#include "rkit/Core/Pair.h"
#include "rkit/Core/Vector.h"

#include "rkit/Render/DeviceCaps.h"

#include "anox/AnoxGraphicsSubsystem.h"
#include "anox/Data/MDAModel.h"

#include "AnoxDataReader.h"
#include "AnoxGameFileSystem.h"
#include "AnoxLoadEntireFileJob.h"
#include "AnoxAbstractSingleFileResource.h"

#include "rkit/Math/TRMat34.h"

#if RKIT_PLATFORM_ARCH_HAVE_SSE2
#include <emmintrin.h>
#endif

#if RKIT_PLATFORM_ARCH_HAVE_SSE41
#include <smmintrin.h>
#endif

namespace anox
{
	struct AnoxMDAModelLoaderInfo;

	struct AnoxMDAModelResourceLoaderState final : public AnoxAbstractSingleFileResourceLoaderState
	{
		data::MDAModelHeader m_header;
		rkit::FilePos_t m_postContentIDsPos = 0;

		BufferInitializer m_triBufferInitializer;
		BufferInitializer m_vertBufferInitializer;
		BufferInitializer m_pointBufferInitializer;
		BufferInitializer m_boneIndexBufferInitializer;
		BufferInitializer m_morphBufferInitializer;

		rkit::Vector<BufferInitializer::CopyOperation> m_vertCopyOperations;
		rkit::Vector<BufferInitializer::CopyOperation> m_indexCopyOperations;

		BufferInitializer::CopyOperation m_pointCopyOperation;
		BufferInitializer::CopyOperation m_boneIndexCopyOperation;
		BufferInitializer::CopyOperation m_morphCopyOperation;


		rkit::Vector<rkit::Future<AnoxResourceRetrieveResult>> m_materials;
	};

	class AnoxMDAModelResource final : public AnoxMDAModelResourceBase
	{
	public:
		friend struct AnoxMDAModelLoaderInfo;

		struct Animation
		{
			uint32_t m_animNumber = 0;
			uint16_t m_firstFrame = 0;
			uint16_t m_numFrames = 0;
			rkit::AsciiStringView m_category;
		};

		struct MorphKey
		{
			uint32_t m_fourCC = 0;
		};

		struct Pass
		{
			uint16_t m_materialIndex = 0;
			bool m_clampFlag = false;
			bool m_depthWriteFlag = false;
			data::MDAAlphaTestMode m_alphaTestMode = data::MDAAlphaTestMode::kCount;
			data::MDABlendMode m_blendMode = data::MDABlendMode::kCount;
			data::MDAUVGenMode m_uvGenMode = data::MDAUVGenMode::kCount;
			data::MDARGBGenMode m_rgbGenMode = data::MDARGBGenMode::kCount;
			data::MDADepthFunc m_depthFunc = data::MDADepthFunc::kCount;
			data::MDACullType m_cullType = data::MDACullType::kCount;
			float m_scrollU = 0.f;
			float m_scrollV = 0.f;
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

		struct SkeletalModelBone
		{
			uint16_t m_parentIndexPlusOne = 0;
			rkit::math::TRMat34 m_transformMatrix;
		};

		struct VertexModelBone
		{
			uint32_t m_fourCC = 0;
		};

		struct VertexFrameBone
		{
			rkit::math::TRMat34 m_matrix;
		};

		struct VertexFrame
		{
			rkit::Span<const VertexFrameBone> m_bones;
		};

		struct SubModel
		{
			uint16_t m_materialIndex = 0;
			uint32_t m_numTris = 0;
			uint32_t m_numVerts = 0;

			size_t m_triBufferOffset = 0;
			size_t m_vertBufferOffset = 0;
		};

	private:
		rkit::Vector<char> m_conditionChars;
		rkit::Vector<char> m_categoryChars;

		rkit::Vector<Profile> m_profiles;
		rkit::Vector<Skin> m_profileSkins;
		rkit::Vector<Pass> m_skinPasses;
		rkit::Vector<Animation> m_animations;
		rkit::Vector<MorphKey> m_morphKeys;
		rkit::Vector<VertexFrame> m_vertexFrames;
		rkit::Vector<VertexFrameBone> m_vertexFrameBones;
		rkit::Vector<SubModel> m_subModels;

		data::MDAAnimationType m_animType = data::MDAAnimationType::kCount;

		rkit::Vector<SkeletalModelBone> m_skeletalModelBones;
		rkit::Vector<VertexModelBone> m_vertexModelBones;

		rkit::RCPtr<IBuffer> m_triBuffer;
		rkit::RCPtr<IBuffer> m_vertBuffer;
		rkit::RCPtr<IBuffer> m_pointBuffer;
		rkit::RCPtr<IBuffer> m_boneIndexBuffer;
		rkit::RCPtr<IBuffer> m_morphBuffer;
	};

	struct AnoxMDAModelLoaderInfo
	{
		typedef AnoxMDAModelResourceLoaderBase LoaderBase_t;
		typedef AnoxMDAModelResource Resource_t;
		typedef AnoxMDAModelResourceLoaderState State_t;

		static constexpr size_t kNumPhases = 2;

		static rkit::Result LoadHeaderAndQueueDependencies(State_t &state, Resource_t &resource, rkit::traits::TraitRef<rkit::VectorTrait<rkit::RCPtr<rkit::Job>>> outDeps);
		static rkit::Result LoadContents(State_t &state, Resource_t &resource, rkit::traits::TraitRef<rkit::VectorTrait<rkit::RCPtr<rkit::Job>>> outDeps);

		static void BulkConvertTris(const rkit::Span<data::MDAModelTri> &tris, uint16_t maxVertIndex);
		static void BulkConvertVerts(const rkit::Span<data::MDAModelVert> &verts, uint32_t maxPointIndex);
		static void BulkConvertPoints(const rkit::Span<data::MDAModelPoint> &points);
		static void BulkConvertBoneIndexes(const rkit::Span<data::MDASkeletalModelBoneIndex> &boneIndexes);
		static void BulkConvertVertMorphs(const rkit::Span<data::MDAModelVertMorph> &vertMorphs);

		static bool PhaseHasDependencies(size_t phase)
		{
			return true;
		}

		static rkit::Result LoadPhase(State_t &state, Resource_t &resource, size_t phase, rkit::traits::TraitRef<rkit::VectorTrait<rkit::RCPtr<rkit::Job>>> outDeps)
		{
			switch (phase)
			{
			case 0:
				return LoadHeaderAndQueueDependencies(state, resource, outDeps);
			case 1:
				return LoadContents(state, resource, outDeps);
			default:
				return rkit::ResultCode::kInternalError;
			}
		}
	};

	rkit::Result AnoxMDAModelLoaderInfo::LoadHeaderAndQueueDependencies(State_t &state, Resource_t &resource, rkit::traits::TraitRef<rkit::VectorTrait<rkit::RCPtr<rkit::Job>>> outDeps)
	{
		rkit::ReadOnlyMemoryStream stream(state.m_fileContents.GetBuffer(), state.m_fileContents.Count());

		RKIT_CHECK(stream.ReadOneBinary(state.m_header));

		const data::MDAModelHeader &header = state.m_header;

		rkit::Vector<rkit::data::ContentID> materialContentIDs;

		const size_t numMaterials = header.m_numMaterials.Get();
		RKIT_CHECK(materialContentIDs.Resize(numMaterials));

		RKIT_CHECK(stream.ReadAllSpan(materialContentIDs.ToSpan()));

		state.m_postContentIDsPos = stream.Tell();

		RKIT_CHECK(outDeps.Reserve(numMaterials));
		RKIT_CHECK(state.m_materials.Reserve(numMaterials));

		for (const rkit::data::ContentID &materialContentID : materialContentIDs)
		{
			rkit::RCPtr<rkit::Job> job;
			rkit::Future<AnoxResourceRetrieveResult> result;
			RKIT_CHECK(state.m_systems.m_resManager->GetContentIDKeyedResource(&job, result, resloaders::kModelMaterialTypeCode, materialContentID));

			RKIT_ASSERT(job.IsValid());
			RKIT_CHECK(outDeps.AppendRValue(std::move(job)));
			RKIT_CHECK(state.m_materials.Append(std::move(result)));
		}

		state.m_postContentIDsPos = stream.Tell();

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxMDAModelLoaderInfo::LoadContents(State_t &state, Resource_t &resource, rkit::traits::TraitRef<rkit::VectorTrait<rkit::RCPtr<rkit::Job>>> outDeps)
	{
		rkit::FixedSizeMemoryStream stream(state.m_fileContents.GetBuffer(), state.m_fileContents.Count());
		RKIT_CHECK(stream.SeekStart(state.m_postContentIDsPos));

		const data::MDAModelHeader &header = state.m_header;

		const uint16_t numMaterials = state.m_header.m_numMaterials.Get();

		const uint8_t numAnimations = (header.m_numAnimations7_AnimationType1 & 0x7f);
		const data::MDAAnimationType animType = static_cast<data::MDAAnimationType>(header.m_numAnimations7_AnimationType1 >> 7);

		const size_t numProfiles = header.m_numProfiles;
		const size_t numBones = header.m_numBones.Get();
		const size_t numSubModels = header.m_numSubModels;
		const size_t numPoints = header.m_numPoints.Get();
		const size_t numMorphedPoints = header.m_numMorphedPoints.Get();
		const size_t numMorphKeys = header.m_numMorphKeys.Get();
		const uint16_t numFrames = header.m_numFrames.Get();

		rkit::Vector<AnoxMDAModelResource::Profile> &outProfiles = resource.m_profiles;
		rkit::Vector<AnoxMDAModelResource::Skin> &outProfileSkins = resource.m_profileSkins;
		rkit::Vector<AnoxMDAModelResource::Pass> &outSkinPasses = resource.m_skinPasses;
		rkit::Vector<AnoxMDAModelResource::Animation> &outAnimations = resource.m_animations;
		rkit::Vector<AnoxMDAModelResource::MorphKey> &outMorphKeys = resource.m_morphKeys;
		rkit::Vector<AnoxMDAModelResource::SubModel> &outSubModels = resource.m_subModels;
		rkit::Vector<char> &outCategoryChars = resource.m_categoryChars;

		if (numPoints == 0 || numMorphedPoints > numPoints)
			return rkit::ResultCode::kDataError;

		resource.m_animType = animType;

		if (numFrames == 0)
			return rkit::ResultCode::kDataError;

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
				[numMaterials]
				(AnoxMDAModelResource::Pass &outPass, const data::MDASkinPass &inPass)
				-> rkit::Result
				{
					RKIT_CHECK(DataReader::ReadCheckUInt(outPass.m_materialIndex, inPass.m_materialIndex, numMaterials));
					RKIT_CHECK(DataReader::ReadCheckBool(outPass.m_clampFlag, inPass.m_clampFlag));
					RKIT_CHECK(DataReader::ReadCheckBool(outPass.m_depthWriteFlag, inPass.m_depthWriteFlag));
					RKIT_CHECK(DataReader::ReadCheckEnum(outPass.m_alphaTestMode, inPass.m_alphaTestMode));
					RKIT_CHECK(DataReader::ReadCheckEnum(outPass.m_blendMode, inPass.m_blendMode));
					RKIT_CHECK(DataReader::ReadCheckEnum(outPass.m_uvGenMode, inPass.m_uvGenMode));
					RKIT_CHECK(DataReader::ReadCheckEnum(outPass.m_rgbGenMode, inPass.m_rgbGenMode));
					RKIT_CHECK(DataReader::ReadCheckEnum(outPass.m_depthFunc, inPass.m_depthFunc));
					RKIT_CHECK(DataReader::ReadCheckEnum(outPass.m_cullType, inPass.m_cullType));
					RKIT_CHECK(DataReader::ReadCheckFloat(outPass.m_scrollU, inPass.m_scrollU, 10));
					RKIT_CHECK(DataReader::ReadCheckFloat(outPass.m_scrollV, inPass.m_scrollV, 10));
					return rkit::ResultCode::kOK;
				});

			RKIT_CHECK(checkResult);
		}

		{
			rkit::Vector<data::MDAAnimation> animations;
			RKIT_CHECK(animations.Resize(numAnimations));
			RKIT_CHECK(outAnimations.Resize(numAnimations));

			RKIT_CHECK(stream.ReadAllSpan(animations.ToSpan()));

			size_t numCategoryCharsPadded = 0;
			for (const data::MDAAnimation &animation : animations)
			{
				RKIT_CHECK(rkit::SafeAdd<size_t>(numCategoryCharsPadded, numCategoryCharsPadded, animation.m_categoryLength));
			}
			RKIT_CHECK(rkit::SafeAdd<size_t>(numCategoryCharsPadded, numCategoryCharsPadded, animations.Count()));

			RKIT_CHECK(outCategoryChars.Resize(numCategoryCharsPadded));

			{
				size_t charStartPos = 0;
				const rkit::Result checkResult = rkit::CheckedProcessParallelSpans(outAnimations.ToSpan(), animations.ToSpan(),
					[&charStartPos, &outCategoryChars, &stream, numFrames]
					(AnoxMDAModelResource::Animation &outAnimation, const data::MDAAnimation &inAnimation)
					->rkit::Result
					{
						const uint16_t lastFrame = numFrames - 1;

						// Process category
						const size_t catLength = inAnimation.m_categoryLength;
						const rkit::Span<char> chars = outCategoryChars.ToSpan().SubSpan(charStartPos, catLength);
						RKIT_CHECK(stream.ReadAllSpan(chars));

						charStartPos += catLength;
						outCategoryChars[charStartPos] = 0;
						charStartPos++;

						rkit::AsciiStringView category(chars.Ptr(), chars.Count());
						if (!category.Validate())
							return rkit::ResultCode::kDataError;

						// Process everything else
						outAnimation.m_animNumber = inAnimation.m_animNumber.Get();
						outAnimation.m_category = category;

						RKIT_CHECK(DataReader::ReadCheckUInt(outAnimation.m_firstFrame, inAnimation.m_firstFrame, lastFrame));

						const uint16_t maxNumFrames = numFrames - outAnimation.m_firstFrame;
						RKIT_CHECK(DataReader::ReadCheckUInt(outAnimation.m_firstFrame, inAnimation.m_numFrames, maxNumFrames));

						return rkit::ResultCode::kOK;
					});
				RKIT_CHECK(checkResult);
			}
		}

		{
			rkit::Vector<data::MDAModelMorphKey> morphKeys;
			RKIT_CHECK(morphKeys.Resize(numMorphKeys));
			RKIT_CHECK(outMorphKeys.Resize(numMorphKeys));

			RKIT_CHECK(stream.ReadAllSpan(morphKeys.ToSpan()));

			rkit::ProcessParallelSpans(outMorphKeys.ToSpan(), morphKeys.ToSpan(),
				[](AnoxMDAModelResource::MorphKey &outMorphKey, const data::MDAModelMorphKey &inMorphKey)
				{
					outMorphKey.m_fourCC = inMorphKey.m_morphKeyFourCC.Get();
				});
		}

		if (animType == data::MDAAnimationType::kSkeletalAnimated)
		{
			rkit::Vector<data::MDASkeletalModelBone> bones;
			rkit::Vector<AnoxMDAModelResource::SkeletalModelBone> &outBones = resource.m_skeletalModelBones;

			RKIT_CHECK(bones.Resize(numBones));
			RKIT_CHECK(outBones.Resize(numBones));

			RKIT_CHECK(stream.ReadAllSpan(bones.ToSpan()));

			uint16_t boneIndex = 0;
			const rkit::Result checkResult = rkit::CheckedProcessParallelSpans(outBones.ToSpan(), bones.ToSpan(),
				[&boneIndex]
				(AnoxMDAModelResource::SkeletalModelBone &outBone, const data::MDASkeletalModelBone &inBone)
				-> rkit::Result
				{
					RKIT_CHECK(DataReader::ReadCheckVec(outBone.m_transformMatrix[0], inBone.m_baseMatrix[0], 15));
					RKIT_CHECK(DataReader::ReadCheckUInt(outBone.m_parentIndexPlusOne, inBone.m_parentIndexPlusOne, boneIndex));

					boneIndex++;

					return rkit::ResultCode::kOK;
				});

			RKIT_CHECK(checkResult);

			return rkit::ResultCode::kNotYetImplemented;
		}

		if (animType == data::MDAAnimationType::kVertexAnimated)
		{
			rkit::Vector<data::MDAVertexModelBone> bones;
			rkit::Vector<AnoxMDAModelResource::VertexModelBone> &outBones = resource.m_vertexModelBones;

			RKIT_CHECK(bones.Resize(numBones));
			RKIT_CHECK(outBones.Resize(numBones));

			RKIT_CHECK(stream.ReadAllSpan(bones.ToSpan()));

			const rkit::Result checkResult = rkit::CheckedProcessParallelSpans(outBones.ToSpan(), bones.ToSpan(),
				[]
				(AnoxMDAModelResource::VertexModelBone &outBone, const data::MDAVertexModelBone &inBone)
				-> rkit::Result
				{
					outBone.m_fourCC = inBone.m_boneIDFourCC.Get();

					return rkit::ResultCode::kOK;
				});

			RKIT_CHECK(checkResult);
		}

		if (animType == data::MDAAnimationType::kSkeletalAnimated)
		{
			// MDAModelSkeletalBoneFrame m_boneFrames[m_numFrames][m_numBones]
			return rkit::ResultCode::kNotYetImplemented;
		}

		if (animType == data::MDAAnimationType::kVertexAnimated)
		{
			size_t numFrameBones = 0;
			RKIT_CHECK(rkit::SafeMul<size_t>(numFrameBones, numFrames, numBones));

			rkit::Vector<data::MDAModelTagBoneFrame> boneFrames;

			rkit::Vector<AnoxMDAModelResource::VertexFrameBone> &outVertexFrameBones = resource.m_vertexFrameBones;
			rkit::Vector<AnoxMDAModelResource::VertexFrame> &outVertexFrames = resource.m_vertexFrames;

			RKIT_CHECK(outVertexFrames.Resize(numFrames));
			RKIT_CHECK(boneFrames.Resize(numFrameBones));
			RKIT_CHECK(outVertexFrameBones.Resize(numFrameBones));

			RKIT_CHECK(stream.ReadAllSpan(boneFrames.ToSpan()));

			for (size_t i = 0; i < numFrames; i++)
			{
				outVertexFrames[i].m_bones = resource.m_vertexFrameBones.ToSpan().SubSpan(0, numBones);
			}

			const rkit::Result checkResult = rkit::CheckedProcessParallelSpans(outVertexFrameBones.ToSpan(), boneFrames.ToSpan(),
				[]
				(AnoxMDAModelResource::VertexFrameBone &outFrameBone, const data::MDAModelTagBoneFrame &inBoneFrame)
				-> rkit::Result
				{
					for (size_t row = 0; row < 3; row++)
					{
						RKIT_CHECK(DataReader::ReadCheckVec(outFrameBone.m_matrix[row], inBoneFrame.m_matrix[row], 15));
					}

					return rkit::ResultCode::kOK;
				});

			RKIT_CHECK(checkResult);
		}

		size_t numTrisTotal = 0;
		size_t numVertTotal = 0;

		{
			rkit::Vector<data::MDAModelSubModel> subModels;

			RKIT_CHECK(subModels.Resize(numSubModels));
			RKIT_CHECK(outSubModels.Resize(numSubModels));

			RKIT_CHECK(stream.ReadAllSpan(subModels.ToSpan()));

			const rkit::Result checkResult = rkit::CheckedProcessParallelSpans(outSubModels.ToSpan(), subModels.ToSpan(),
				[numMaterials, &numTrisTotal]
				(AnoxMDAModelResource::SubModel &outSubModel, const data::MDAModelSubModel &inSubModel)
				-> rkit::Result
				{
					RKIT_CHECK(DataReader::ReadCheckUInt(outSubModel.m_materialIndex, inSubModel.m_materialIndex, numMaterials));
					outSubModel.m_numTris = inSubModel.m_numTris.Get();
					outSubModel.m_numVerts = static_cast<uint32_t>(inSubModel.m_numVertsMinusOne.Get()) + 1u;

					RKIT_CHECK(rkit::SafeAdd<size_t>(numTrisTotal, numTrisTotal, outSubModel.m_numTris));

					return rkit::ResultCode::kOK;
				});

			RKIT_CHECK(checkResult);
		}

		const rkit::render::IRenderDeviceRequirements &deviceReqs = state.m_systems.m_graphicsSystem->GetDeviceRequirements();

		// Just align to 16 bytes for better GPU load characteristics
		const uint32_t bufferOffsetAlignment = rkit::Max<uint32_t>(16, deviceReqs.GetUInt32Req(rkit::render::RenderDeviceUInt32Requirement::kDataBufferOffsetAlignment));

		// Load tris
		{
			RKIT_CHECK(state.m_indexCopyOperations.Resize(resource.m_subModels.Count()));

			size_t indexBufferSize = 0;
			rkit::Result checkResult = rkit::CheckedProcessParallelSpans(state.m_indexCopyOperations.ToSpan(), resource.m_subModels.ToSpan(),
				[&indexBufferSize, &stream, bufferOffsetAlignment]
				(BufferInitializer::CopyOperation &copyOp, AnoxMDAModelResource::SubModel &subModel)
				-> rkit::Result
				{
					if (subModel.m_numTris == 0 || subModel.m_numVerts == 0)
						return rkit::ResultCode::kDataError;

					RKIT_CHECK(rkit::SafeAlignUp<size_t>(indexBufferSize, indexBufferSize, bufferOffsetAlignment));

					const size_t bufferOffset = indexBufferSize;

					size_t subModelIndexesSize = sizeof(data::MDAModelTri);
					RKIT_CHECK(rkit::SafeMul<size_t>(subModelIndexesSize, subModelIndexesSize, subModel.m_numTris));
					RKIT_CHECK(rkit::SafeAdd<size_t>(indexBufferSize, indexBufferSize, subModelIndexesSize));

					rkit::Span<data::MDAModelTri> triSpan;
					RKIT_CHECK(stream.ExtractSpan(triSpan, subModel.m_numTris));

					BulkConvertTris(triSpan, static_cast<uint16_t>(subModel.m_numVerts - 1u));

					copyOp.m_data = triSpan.ReinterpretCast<uint8_t>();
					copyOp.m_offset = bufferOffset;

					return rkit::ResultCode::kOK;
				});
			RKIT_CHECK(checkResult);

			BufferInitializer &bufInitializer = state.m_triBufferInitializer;
			bufInitializer.m_copyOperations = state.m_indexCopyOperations.ToSpan();
			bufInitializer.m_spec.m_size = indexBufferSize;
			bufInitializer.m_resSpec.m_usage = rkit::EnumMask<rkit::render::BufferUsageFlag>({ rkit::render::BufferUsageFlag::kIndexBuffer, rkit::render::BufferUsageFlag::kCopyDest });
		}

		// Load verts
		{
			RKIT_CHECK(state.m_vertCopyOperations.Resize(resource.m_subModels.Count()));

			size_t vertBufferSize = 0;
			rkit::Result checkResult = rkit::CheckedProcessParallelSpans(state.m_vertCopyOperations.ToSpan(), resource.m_subModels.ToSpan(),
				[&vertBufferSize, &stream, bufferOffsetAlignment, numPoints]
				(BufferInitializer::CopyOperation &copyOp, AnoxMDAModelResource::SubModel &subModel)
				-> rkit::Result
				{
					if (subModel.m_numVerts == 0)
						return rkit::ResultCode::kDataError;

					RKIT_CHECK(rkit::SafeAlignUp<size_t>(vertBufferSize, vertBufferSize, bufferOffsetAlignment));

					const size_t bufferOffset = vertBufferSize;

					size_t subModelVertSize = sizeof(data::MDAModelVert);
					RKIT_CHECK(rkit::SafeMul<size_t>(subModelVertSize, subModelVertSize, subModel.m_numVerts));
					RKIT_CHECK(rkit::SafeAdd<size_t>(vertBufferSize, vertBufferSize, subModelVertSize));

					rkit::Span<data::MDAModelVert> vertSpan;
					RKIT_CHECK(stream.ExtractSpan(vertSpan, subModel.m_numVerts));

					BulkConvertVerts(vertSpan, static_cast<uint32_t>(numPoints - 1));

					copyOp.m_data = vertSpan.ReinterpretCast<uint8_t>();
					copyOp.m_offset = bufferOffset;

					return rkit::ResultCode::kOK;
				});
			RKIT_CHECK(checkResult);

			BufferInitializer &bufInitializer = state.m_vertBufferInitializer;
			bufInitializer.m_copyOperations = state.m_vertCopyOperations.ToSpan();
			bufInitializer.m_spec.m_size = vertBufferSize;
			bufInitializer.m_resSpec.m_usage = rkit::EnumMask<rkit::render::BufferUsageFlag>({ rkit::render::BufferUsageFlag::kVertexBuffer, rkit::render::BufferUsageFlag::kCopyDest });
		}

		// Load points
		{
			size_t numPointsTotal = numPoints;
			if (animType == data::MDAAnimationType::kVertexAnimated)
			{
				RKIT_CHECK(rkit::SafeMul<size_t>(numPointsTotal, numPointsTotal, numFrames));
			}

			if (numPoints == 0)
				return rkit::ResultCode::kDataError;

			size_t pointBufferSize = 0;
			RKIT_CHECK(rkit::SafeMul<size_t>(pointBufferSize, sizeof(data::MDAModelPoint), numPointsTotal));

			rkit::Span<data::MDAModelPoint> points;
			RKIT_CHECK(stream.ExtractSpan(points, numPointsTotal));

			BulkConvertPoints(points);

			BufferInitializer::CopyOperation &copyOperation = state.m_pointCopyOperation;
			copyOperation.m_data = points.ReinterpretCast<uint8_t>();
			copyOperation.m_offset = 0;

			BufferInitializer &bufInitializer = state.m_pointBufferInitializer;
			bufInitializer.m_copyOperations = rkit::Span<BufferInitializer::CopyOperation>(&copyOperation, 1);
			bufInitializer.m_spec.m_size = pointBufferSize;
			bufInitializer.m_resSpec.m_usage = rkit::EnumMask<rkit::render::BufferUsageFlag>({ rkit::render::BufferUsageFlag::kStorageBuffer, rkit::render::BufferUsageFlag::kCopyDest });
		}

		if (animType == data::MDAAnimationType::kSkeletalAnimated)
		{
			rkit::Span<data::MDASkeletalModelBoneIndex> boneIndexes;
			RKIT_CHECK(stream.ExtractSpan(boneIndexes, numPoints));

			BulkConvertBoneIndexes(boneIndexes);

			BufferInitializer::CopyOperation &copyOperation = state.m_boneIndexCopyOperation;
			copyOperation.m_data = boneIndexes.ReinterpretCast<uint8_t>();
			copyOperation.m_offset = 0;

			BufferInitializer &bufInitializer = state.m_boneIndexBufferInitializer;
			bufInitializer.m_copyOperations = rkit::Span<BufferInitializer::CopyOperation>(&copyOperation, 1);
			bufInitializer.m_spec.m_size = boneIndexes.ReinterpretCast<uint8_t>().Count();
			bufInitializer.m_resSpec.m_usage = rkit::EnumMask<rkit::render::BufferUsageFlag>({ rkit::render::BufferUsageFlag::kStorageBuffer, rkit::render::BufferUsageFlag::kCopyDest });
		}

		if (numMorphedPoints > 0)
		{
			rkit::Span<data::MDAModelVertMorph> vertMorphs;
			RKIT_CHECK(stream.ExtractSpan(vertMorphs, numMorphedPoints));

			BulkConvertVertMorphs(vertMorphs);

			BufferInitializer::CopyOperation &copyOperation = state.m_morphCopyOperation;
			copyOperation.m_data = vertMorphs.ReinterpretCast<uint8_t>();
			copyOperation.m_offset = 0;

			BufferInitializer &bufInitializer = state.m_morphBufferInitializer;
			bufInitializer.m_copyOperations = rkit::Span<BufferInitializer::CopyOperation>(&copyOperation, 1);
			bufInitializer.m_spec.m_size = vertMorphs.ReinterpretCast<uint8_t>().Count();
			bufInitializer.m_resSpec.m_usage = rkit::EnumMask<rkit::render::BufferUsageFlag>({rkit::render::BufferUsageFlag::kStorageBuffer, rkit::render::BufferUsageFlag::kCopyDest });
		}

		if (stream.Tell() != stream.GetSize())
			return rkit::ResultCode::kDataError;

		typedef BufferInitializer (AnoxMDAModelResourceLoaderState:: *BufferInitializerField_t);
		typedef rkit::RCPtr<IBuffer> (AnoxMDAModelResource:: *BufferField_t);

		typedef rkit::Pair<BufferInitializerField_t, BufferField_t> FieldPair_t;

		const FieldPair_t fieldPairs[] =
		{
			FieldPair_t(&AnoxMDAModelResourceLoaderState::m_triBufferInitializer, &AnoxMDAModelResource::m_triBuffer),
			FieldPair_t(&AnoxMDAModelResourceLoaderState::m_vertBufferInitializer, &AnoxMDAModelResource::m_vertBuffer),
			FieldPair_t(&AnoxMDAModelResourceLoaderState::m_pointBufferInitializer, &AnoxMDAModelResource::m_pointBuffer),
			FieldPair_t(&AnoxMDAModelResourceLoaderState::m_boneIndexBufferInitializer, &AnoxMDAModelResource::m_boneIndexBuffer),
			FieldPair_t(&AnoxMDAModelResourceLoaderState::m_morphBufferInitializer, &AnoxMDAModelResource::m_morphBuffer),
		};

		for (const FieldPair_t &fieldPair : fieldPairs)
		{
			BufferInitializerField_t bufInitializerField = fieldPair.First();

			BufferInitializer &bufInitializer = state.*bufInitializerField;

			if (bufInitializer.m_copyOperations.Count() > 0)
			{
				BufferField_t bufField = fieldPair.Second();
				rkit::RCPtr<IBuffer> &buf = resource.*bufField;

				rkit::RCPtr<rkit::Job> job;
				rkit::RCPtr<AnoxMDAModelResourceLoaderState> stateRC(&state);

				RKIT_CHECK(state.m_systems.m_graphicsSystem->CreateAsyncCreateAndFillBufferJob(&job, buf, stateRC.FieldRef(bufInitializerField), nullptr));
				RKIT_CHECK(outDeps.Append(job));
			}
		}

		return rkit::ResultCode::kOK;
	}

	void AnoxMDAModelLoaderInfo::BulkConvertTris(const rkit::Span<data::MDAModelTri> &tris, uint16_t maxTriIndex)
	{
		static_assert(sizeof(data::MDAModelTri) == 6, "Wrong tri size");

		const rkit::Span<rkit::endian::LittleUInt16_t> srcIndexes = tris.ReinterpretCast<rkit::endian::LittleUInt16_t>();

		rkit::endian::LittleUInt16_t *srcIndexesPtr = srcIndexes.Ptr();
		const size_t numIndexes = srcIndexes.Count();

#if defined(RKIT_PLATFORM_ARCH_HAVE_SSE41) || defined(RKIT_PLATFORM_ARCH_HAVE_SSE2)

#if defined(RKIT_PLATFORM_ARCH_HAVE_SSE41)
		const __m128i vmaxIndex = _mm_set1_epi16(static_cast<int16_t>(maxTriIndex));
#else
		const __m128i signBitFlip = _mm_set1_epi16(-0x8000);
		const __m128i vmaxIndexFlipped = _mm_xor_si128(_mm_set1_epi16(static_cast<int16_t>(maxTriIndex)), signBitFlip);
#endif

		const size_t kBulkSize = 8;
		const size_t numBulkIndexes = numIndexes / 8u * 8u;

		for (size_t startIndex = 0; startIndex < numBulkIndexes; startIndex += 8u)
		{
			const __m128i srcIndexes = _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcIndexesPtr + startIndex));

#if defined(RKIT_PLATFORM_ARCH_HAVE_SSE41)
			const __m128i adjusted = _mm_min_epu16(srcIndexes, vmaxIndex);
#else
			const __m128i adjusted = _mm_xor_si128(signBitFlip, _mm_min_epi16(_mm_xor_si128(signBitFlip, srcIndexes), vmaxIndexFlipped));
#endif
			_mm_storeu_si128(reinterpret_cast<__m128i *>(srcIndexesPtr + startIndex), adjusted);
		}
#endif

		for (size_t i = numBulkIndexes; i < numIndexes; i++)
		{
			rkit::endian::LittleUInt16_t *batchPtr = srcIndexesPtr + i;
			uint16_t adjusted = rkit::Min(srcIndexesPtr[i].Get(), maxTriIndex);
			memcpy(&srcIndexesPtr[i], &adjusted, sizeof(adjusted));
		}
	}

	void AnoxMDAModelLoaderInfo::BulkConvertVerts(const rkit::Span<data::MDAModelVert> &verts, uint32_t maxPointIndex)
	{
		static_assert(sizeof(data::MDAModelVert) == 8, "Wrong vert size");
		static_assert(offsetof(data::MDAModelVert, m_pointID) == 0, "Wrong point ID offset");
		static_assert(sizeof(data::MDAModelVert::m_pointID) == 4, "Wrong tri size");

		data::MDAModelVert *srcVertsPtr = verts.Ptr();
		const size_t numVerts = verts.Count();

#if defined(RKIT_PLATFORM_ARCH_HAVE_SSE41)
		const size_t numBulkVerts = numVerts / 2u * 2u;
		const __m128i pointIDMax = _mm_set_epi32(-1, static_cast<int32_t>(maxPointIndex), -1, static_cast<int32_t>(maxPointIndex));

		for (size_t startIndex = 0; startIndex < numBulkVerts; startIndex += 2u)
		{
			data::MDAModelVert *startVert = srcVertsPtr + startIndex;

			const __m128i verts = _mm_loadu_si128(reinterpret_cast<const __m128i *>(startVert));
			const __m128i adjusted = _mm_min_epu32(verts, pointIDMax);
			_mm_storeu_si128(reinterpret_cast<__m128i *>(startVert), adjusted);
		}
#else
		const size_t numBulkVerts = 0;
#endif

		for (size_t i = numBulkVerts; i < numVerts; i++)
		{
			data::MDAModelVert &modelVert = srcVertsPtr[i];

			const uint32_t pointID = rkit::Min(modelVert.m_pointID.Get(), maxPointIndex);
			const uint16_t compressedU = modelVert.m_texCoordU.Get();
			const uint16_t compressedV = modelVert.m_texCoordV.Get();

			memcpy(&modelVert.m_pointID, &pointID, sizeof(pointID));
			memcpy(&modelVert.m_texCoordU, &compressedU, sizeof(compressedU));
			memcpy(&modelVert.m_texCoordV, &compressedV, sizeof(compressedV));
		}
	}

	void AnoxMDAModelLoaderInfo::BulkConvertPoints(const rkit::Span<data::MDAModelPoint> &points)
	{
		for (data::MDAModelPoint &pt : points)
		{
			pt.m_compressedNormal.m_compressedNormal.ConvertToHostOrderInPlace();
			for (rkit::endian::LittleFloat32_t &pointElement : pt.m_point)
				pointElement.ConvertToHostOrderInPlace();
		}
	}

	void AnoxMDAModelLoaderInfo::BulkConvertBoneIndexes(const rkit::Span<data::MDASkeletalModelBoneIndex> &indexes)
	{
		for (data::MDASkeletalModelBoneIndex &idx : indexes)
		{
			idx.m_boneIndex.ConvertToHostOrderInPlace();
		}
	}

	void AnoxMDAModelLoaderInfo::BulkConvertVertMorphs(const rkit::Span<data::MDAModelVertMorph> &morphs)
	{
		for (data::MDAModelVertMorph &morph : morphs)
		{
			for (rkit::endian::LittleFloat32_t &deltaElement : morph.m_delta)
				deltaElement.ConvertToHostOrderInPlace();
		}
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
