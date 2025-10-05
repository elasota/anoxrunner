#include "rkit/Core/Endian.h"

#include "CompressedNormal.h"
#include "CompressedQuat.h"

namespace anox { namespace data
{
	struct MDAModelHeader
	{
		uint8_t m_numSkins = 0;
		uint8_t m_numSubModels = 0;
		uint8_t m_numProfiles = 0;
		uint8_t m_numAnimations = 0;
		rkit::endian::LittleUInt16_t m_numMorphKeys;
		rkit::endian::LittleUInt16_t m_numBones;
		rkit::endian::LittleUInt16_t m_numFrames;
		rkit::endian::LittleUInt32_t m_numPoints;
		rkit::endian::LittleUInt32_t m_numMorphedPoints;

		// MDAProfile m_profiles[m_numProfiles]
		// char m_profileConditions[m_numProfiles][profile.m_conditionLength]
		// MDASkin m_skins[m_numProfiles][m_numSkins]
		// MDASkinPass m_skinPasses[m_numProfiles][m_numSkins][skin.m_numPasses]
		// MDAAnimation m_animations[m_numAnimations]
		// char m_animationNameChars[m_numAnimations][anim.m_nameLength]
		// MDAModelMorphKey m_morphKeys[m_numMorphKeys]
		// MDAModelBone m_bones[m_numBones]
		// MDAModelBoneFrame m_boneFrames[m_numFrames][m_numBones]
		// MDAModelSubModel m_subModels[m_numSubModels]

		// MDAModelTri m_tris[m_numSubmodels][submodel.m_numTris]
		// MDAModelVert m_verts[m_numSubmodels][submodel.m_numVerts]
		// MDAModelPoint m_points[m_numFrames][m_numPoints]
		// MDAModelVertMorph m_vertMorphs[m_numSubmodels][m_numMorphkeys][submodel.m_numMorphedVerts]
	};

	struct MDAAnimation
	{
		uint8_t m_categoryLength = 0;
		rkit::endian::LittleUInt32_t m_animNumber;
		rkit::endian::LittleUInt16_t m_firstFrame;
		rkit::endian::LittleUInt16_t m_numFrames;
	};

	struct MDASkinPass
	{
		rkit::data::ContentID m_materialContentID;
		uint8_t m_clampFlag = 0;
	};

	struct MDASkin
	{
		uint8_t m_numPasses = 0;
	};

	struct MDAProfile
	{
		rkit::endian::BigUInt32_t m_fourCC;
		rkit::endian::LittleUInt32_t m_conditionLength;
	};

	struct MDAModelBoneFrame
	{
		rkit::endian::LittleFloat32_t m_position[3];
		CompressedQuat m_orientation;
	};

	struct MDAModelMorphKey
	{
		rkit::endian::BigUInt32_t m_morphKeyFourCC;
	};

	struct MDAModelBone
	{
		rkit::endian::BigUInt32_t m_boneIDFourCC;
	};

	struct MDAModelVert
	{
		rkit::endian::LittleUInt32_t m_pointID;
		rkit::endian::LittleUInt16_t m_texCoordU;
		rkit::endian::LittleUInt16_t m_texCoordV;
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
		rkit::endian::LittleUInt16_t m_numVerts;
	};

} } // anox::data
