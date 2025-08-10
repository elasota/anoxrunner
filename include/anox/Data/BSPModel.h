#pragma once

#include "rkit/Core/Endian.h"
#include "rkit/Core/FourCC.h"

namespace anox { namespace data {
	struct BSPFile
	{
		static const uint32_t kFourCC = RKIT_FOURCC('B', 'S', 'P', 'M');
		static const uint32_t kVersion = 1;

		rkit::endian::BigUInt32_t m_fourCC;
		rkit::endian::LittleUInt32_t m_version;
	};

	struct BSPTreeNode
	{
		rkit::endian::LittleUInt32_t m_plane;

		rkit::endian::LittleInt16_t m_minBounds[3];
		rkit::endian::LittleInt16_t m_maxBounds[3];
	};

	struct BSPTreeLeaf
	{
		rkit::endian::LittleUInt32_t m_contentFlags;

		rkit::endian::LittleInt16_t m_cluster;
		rkit::endian::LittleInt16_t m_area;

		rkit::endian::LittleInt16_t m_minBounds[3];
		rkit::endian::LittleInt16_t m_maxBounds[3];

		rkit::endian::LittleUInt16_t m_numLeafBrushes;
		rkit::endian::LittleUInt16_t m_numLeafDrawSurfaces;
	};

	struct BSPMaterial
	{
		uint8_t m_nameLengthMinusOne;
	};

	struct BSPNormal
	{
		rkit::endian::LittleFloat32_t m_xyz[3];
	};

	struct BSPPlane
	{
		rkit::endian::LittleUInt32_t m_normal;	// Not flippable
		rkit::endian::LittleFloat32_t m_dist;
	};

	struct BSPBrush
	{
		rkit::endian::LittleUInt32_t m_numBrushSides;
		rkit::endian::LittleUInt32_t m_contents;
	};

	struct BSPBrushSide
	{
		rkit::endian::LittleUInt32_t m_plane;	// Flippable
		rkit::endian::LittleUInt32_t m_material;
		rkit::endian::LittleUInt32_t m_materialFlags;
	};

	struct BSPDrawVertex
	{
		rkit::endian::LittleFloat32_t m_xyz[3];
		rkit::endian::LittleFloat32_t m_uv[2];
		rkit::endian::LittleFloat32_t m_lightUV[2];
		rkit::endian::LittleUInt32_t m_normal;	// Flippable
	};

	struct BSPDrawFace
	{
		rkit::endian::LittleUInt32_t m_numTris;
	};

	struct BSPDrawMaterialGroup
	{
		rkit::endian::LittleUInt32_t m_materialIndex;
		rkit::endian::LittleUInt32_t m_numFaces;
	};

	struct BSPDrawLightmapGroup
	{
		rkit::endian::LittleUInt32_t m_atlasIndex;
		rkit::endian::LittleUInt32_t m_numMaterialGroups;
	};

	struct BSPDrawModelGroup
	{
		rkit::endian::LittleUInt32_t m_modelIndex;
		rkit::endian::LittleUInt32_t m_numLightmapGroups;
	};

	struct BSPDrawCluster
	{
		rkit::endian::LittleUInt16_t m_numModelGroups;
		rkit::endian::LittleUInt16_t m_numVerts;
	};

	struct BSPModelDrawCluster
	{
		rkit::endian::LittleUInt32_t m_drawClusterIndex;
		rkit::endian::LittleUInt16_t m_modelGroupIndex;
	};

	struct BSPModel
	{
		rkit::endian::LittleUInt32_t m_numModelDrawClusters;
		rkit::endian::LittleFloat32_t m_mins[3];
		rkit::endian::LittleFloat32_t m_maxs[3];
		rkit::endian::LittleFloat32_t m_origin[3];

		uint8_t m_rootIsLeaf;
	};
} }
