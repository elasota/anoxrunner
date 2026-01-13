#pragma once

#include "rkit/Core/Endian.h"
#include "rkit/Core/FourCC.h"
#include "rkit/Data/ContentID.h"

namespace anox { namespace data {
	struct BSPFile
	{
		static const uint32_t kFourCC = RKIT_FOURCC('B', 'S', 'P', 'M');
		static const uint32_t kVersion = 2;

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
		rkit::endian::LittleUInt32_t m_normal;	// Flippable
		rkit::endian::LittleFloat32_t m_uv[2];
		rkit::endian::LittleFloat32_t m_lightUV[2];
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

		uint8_t m_rootIsLeaf = 0;
	};
} }

#include "rkit/Core/Vector.h"

namespace anox { namespace data {
	struct BSPDataChunksSpans
	{
		rkit::Span<rkit::data::ContentID> m_materials;
		rkit::Span<rkit::data::ContentID> m_lightmaps;
		rkit::Span<data::BSPNormal> m_normals;
		rkit::Span<data::BSPPlane> m_planes;
		rkit::Span<data::BSPTreeNode> m_treeNodes;
		rkit::Span<uint8_t> m_treeNodeSplitBits;
		rkit::Span<data::BSPTreeLeaf> m_leafs;
		rkit::Span<data::BSPBrush> m_brushes;
		rkit::Span<data::BSPBrushSide> m_brushSides;
		rkit::Span<data::BSPDrawVertex> m_drawVerts;
		rkit::Span<data::BSPDrawFace> m_drawFaces;
		rkit::Span<data::BSPDrawMaterialGroup> m_materialGroups;
		rkit::Span<data::BSPDrawLightmapGroup> m_lightmapGroups;
		rkit::Span<data::BSPDrawModelGroup> m_modelGroups;
		rkit::Span<data::BSPDrawCluster> m_drawClusters;
		rkit::Span<data::BSPModel> m_models;
		rkit::Span<data::BSPModelDrawCluster> m_modelDrawClusters;
		rkit::Span<rkit::endian::LittleUInt16_t> m_leafBrushes;
		rkit::Span<rkit::endian::LittleUInt16_t> m_leafFaces;
		rkit::Span<rkit::endian::LittleUInt16_t> m_triIndexes;
	};

	struct BSPDataChunksVectors
	{
		rkit::Vector<rkit::data::ContentID> m_materials;
		rkit::Vector<rkit::data::ContentID> m_lightmaps;
		rkit::Vector<data::BSPNormal> m_normals;
		rkit::Vector<data::BSPPlane> m_planes;
		rkit::Vector<data::BSPTreeNode> m_treeNodes;
		rkit::Vector<uint8_t> m_treeNodeSplitBits;
		rkit::Vector<data::BSPTreeLeaf> m_leafs;
		rkit::Vector<data::BSPBrush> m_brushes;
		rkit::Vector<data::BSPBrushSide> m_brushSides;
		rkit::Vector<data::BSPDrawVertex> m_drawVerts;
		rkit::Vector<data::BSPDrawFace> m_drawFaces;
		rkit::Vector<data::BSPDrawMaterialGroup> m_materialGroups;
		rkit::Vector<data::BSPDrawLightmapGroup> m_lightmapGroups;
		rkit::Vector<data::BSPDrawModelGroup> m_modelGroups;
		rkit::Vector<data::BSPDrawCluster> m_drawClusters;
		rkit::Vector<data::BSPModel> m_models;
		rkit::Vector<data::BSPModelDrawCluster> m_modelDrawClusters;
		rkit::Vector<rkit::endian::LittleUInt16_t> m_leafBrushes;
		rkit::Vector<rkit::endian::LittleUInt16_t> m_leafFaces;
		rkit::Vector<rkit::endian::LittleUInt16_t> m_triIndexes;
	};

	struct BSPDataChunksProcessor
	{
		template<class TChunksClass, class TVisitor>
		static rkit::Result VisitAllChunks(TChunksClass &instance, const TVisitor &visitor)
		{
			RKIT_CHECK(visitor.template VisitMember<rkit::data::ContentID>(instance.m_materials));
			RKIT_CHECK(visitor.template VisitMember<rkit::data::ContentID>(instance.m_lightmaps));
			RKIT_CHECK(visitor.template VisitMember<data::BSPNormal>(instance.m_normals));
			RKIT_CHECK(visitor.template VisitMember<data::BSPPlane>(instance.m_planes));
			RKIT_CHECK(visitor.template VisitMember<data::BSPTreeNode>(instance.m_treeNodes));
			RKIT_CHECK(visitor.template VisitMember<uint8_t>(instance.m_treeNodeSplitBits));
			RKIT_CHECK(visitor.template VisitMember<data::BSPTreeLeaf>(instance.m_leafs));
			RKIT_CHECK(visitor.template VisitMember<data::BSPBrush>(instance.m_brushes));
			RKIT_CHECK(visitor.template VisitMember<data::BSPBrushSide>(instance.m_brushSides));
			RKIT_CHECK(visitor.template VisitMember<data::BSPDrawVertex>(instance.m_drawVerts));
			RKIT_CHECK(visitor.template VisitMember<data::BSPDrawFace>(instance.m_drawFaces));
			RKIT_CHECK(visitor.template VisitMember<data::BSPDrawMaterialGroup>(instance.m_materialGroups));
			RKIT_CHECK(visitor.template VisitMember<data::BSPDrawLightmapGroup>(instance.m_lightmapGroups));
			RKIT_CHECK(visitor.template VisitMember<data::BSPDrawModelGroup>(instance.m_modelGroups));
			RKIT_CHECK(visitor.template VisitMember<data::BSPDrawCluster>(instance.m_drawClusters));
			RKIT_CHECK(visitor.template VisitMember<data::BSPModel>(instance.m_models));
			RKIT_CHECK(visitor.template VisitMember<data::BSPModelDrawCluster>(instance.m_modelDrawClusters));
			RKIT_CHECK(visitor.template VisitMember<rkit::endian::LittleUInt16_t>(instance.m_leafBrushes));
			RKIT_CHECK(visitor.template VisitMember<rkit::endian::LittleUInt16_t>(instance.m_leafFaces));
			RKIT_CHECK(visitor.template VisitMember<rkit::endian::LittleUInt16_t>(instance.m_triIndexes));

			return rkit::ResultCode::kOK;
		}
	};
} }
