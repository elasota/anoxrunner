#include "rkit/Core/Endian.h"
#include "rkit/Data/ContentID.h"

#include "CompressedNormal.h"
#include "CompressedQuat.h"

namespace anox { namespace data
{
	enum class MDAAnimationType
	{
		kVertexAnimated = 0,
		kSkeletalAnimated = 1,
	};

	struct MDAModelHeader
	{
		uint8_t m_numSkins = 0;
		uint8_t m_numSubModels = 0;
		uint8_t m_numProfiles = 0;
		uint8_t m_numAnimations7_AnimationType1 = 0;
		rkit::endian::LittleUInt16_t m_numMorphKeys;
		rkit::endian::LittleUInt16_t m_numBones;
		rkit::endian::LittleUInt16_t m_numFrames;
		rkit::endian::LittleUInt16_t m_numMaterials;
		rkit::endian::LittleUInt32_t m_numPoints;
		rkit::endian::LittleUInt32_t m_numMorphedPoints;

		// ContentID m_materialContentIDs[m_numMaterials]
		// MDAProfile m_profiles[m_numProfiles]
		// char m_profileConditions[m_numProfiles][profile.m_conditionLength]
		// MDASkin m_skins[m_numProfiles][m_numSkins]
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
	};

	struct MDAAnimation
	{
		uint8_t m_categoryLength = 0;
		rkit::endian::LittleUInt32_t m_animNumber;
		rkit::endian::LittleUInt16_t m_firstFrame;
		rkit::endian::LittleUInt16_t m_numFrames;
	};

	enum class MDAAlphaTestMode
	{
		kDisabled,
		kGE128,
		kGT0,
		kLT128,

		kCount,
	};

	enum class MDABlendMode
	{
		kDisabled,
		kNormal,
		kAdd,
		kMultiply,
		kAlphaBlend,

		kCount,
	};

	enum class MDASortMode
	{
		kBlend,
		kOpaque,

		kCount,
	};

	enum class MDAUVGenMode
	{
		kDisabled,
		kSphere,

		kCount,
	};

	enum class MDARGBGenMode
	{
		kDefault,
		kIdentity,
		kDiffuseZero,

		kCount,
	};

	enum class MDADepthFunc
	{
		kDefault,
		kEqual,
		kLess,

		kCount,
	};

	enum class MDACullType
	{
		kBack,
		kFront,
		kDisable,

		kCount,
	};

	struct MDASkinPass
	{
		rkit::endian::LittleUInt16_t m_materialIndex;
		uint8_t m_clampFlag = 0;
		uint8_t m_depthWriteFlag = 1;
		uint8_t m_alphaTestMode = static_cast<uint8_t>(MDAAlphaTestMode::kDisabled);
		uint8_t m_blendMode = static_cast<uint8_t>(MDABlendMode::kDisabled);
		uint8_t m_uvGenMode = static_cast<uint8_t>(MDAUVGenMode::kDisabled);
		uint8_t m_rgbGenMode = static_cast<uint8_t>(MDARGBGenMode::kDefault);
		uint8_t m_depthFunc = static_cast<uint8_t>(MDADepthFunc::kDefault);
		uint8_t m_cullType = static_cast<uint8_t>(MDACullType::kBack);
		rkit::endian::LittleFloat32_t m_scrollU;
		rkit::endian::LittleFloat32_t m_scrollV;
	};

	struct MDASkin
	{
		uint8_t m_numPasses = 0;
		uint8_t m_sortMode = static_cast<uint8_t>(MDASortMode::kOpaque);
	};

	struct MDAProfile
	{
		rkit::endian::BigUInt32_t m_fourCC;
		rkit::endian::LittleUInt32_t m_conditionLength;
	};

	struct MDAModelTagBoneFrame
	{
		rkit::endian::LittleFloat32_t m_matrix[3][4];
	};

	struct MDAModelSkeletalBoneFrame
	{
		rkit::endian::LittleFloat32_t m_translation[3];
		CompressedQuat m_rotation;
	};

	struct MDAModelMorphKey
	{
		rkit::endian::BigUInt32_t m_morphKeyFourCC;
	};

	struct MDASkeletalModelBone
	{
		rkit::endian::LittleUInt16_t m_parentIndexPlusOne;
		rkit::endian::LittleFloat32_t m_baseMatrix[3][4];
	};

	struct MDAVertexModelBone
	{
		rkit::endian::BigUInt32_t m_boneIDFourCC;
	};

	struct MDAModelVert
	{
		rkit::endian::LittleUInt32_t m_pointID;
		rkit::endian::LittleUInt16_t m_texCoordU;
		rkit::endian::LittleUInt16_t m_texCoordV;
	};

	struct MDASkeletalModelPoint
	{
		rkit::endian::LittleFloat32_t m_point[3];
		CompressedNormal m_compressedNormal;
		rkit::endian::LittleUInt16_t m_boneIndex;
	};

	struct MDAModelPoint
	{
		rkit::endian::LittleFloat32_t m_point[3];
		CompressedNormal m_compressedNormal;
	};

	struct MDAModelVertMorph
	{
		rkit::endian::LittleFloat32_t m_delta[3];
	};

	struct MDAModelVertMorphKey
	{
		rkit::endian::LittleFloat32_t m_delta[3];
	};

	struct MDAModelTri
	{
		rkit::endian::LittleUInt16_t m_verts[3];
	};

	struct MDAModelSubModel
	{
		rkit::endian::LittleUInt32_t m_materialID;
		rkit::endian::LittleUInt32_t m_numTris;
		rkit::endian::LittleUInt16_t m_numVertsMinusOne;
	};

} } // anox::data
