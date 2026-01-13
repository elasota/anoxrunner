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
		rkit::data::ContentID m_entitySpawnContentID;
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
	struct BSPDataChunks
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

		template<class TVisitor>
		rkit::Result VisitAllChunks(const TVisitor &visitor)
		{
			return BSPDataChunks::StaticVisitAllChunks<BSPDataChunks, TVisitor>(*this, visitor);
		}

		template<class TVisitor>
		rkit::Result VisitAllChunks(const TVisitor &visitor) const
		{
			return BSPDataChunks::StaticVisitAllChunks<const BSPDataChunks, TVisitor>(*this, visitor);
		}


	private:
		template<class TSelf, class TVisitor>
		static rkit::Result StaticVisitAllChunks(TSelf &self, const TVisitor &visitor)
		{
			RKIT_CHECK(visitor.VisitMember(self.m_materials));
			RKIT_CHECK(visitor.VisitMember(self.m_lightmaps));
			RKIT_CHECK(visitor.VisitMember(self.m_normals));
			RKIT_CHECK(visitor.VisitMember(self.m_planes));
			RKIT_CHECK(visitor.VisitMember(self.m_treeNodes));
			RKIT_CHECK(visitor.VisitMember(self.m_treeNodeSplitBits));
			RKIT_CHECK(visitor.VisitMember(self.m_leafs));
			RKIT_CHECK(visitor.VisitMember(self.m_brushes));
			RKIT_CHECK(visitor.VisitMember(self.m_brushSides));
			RKIT_CHECK(visitor.VisitMember(self.m_drawVerts));
			RKIT_CHECK(visitor.VisitMember(self.m_drawFaces));
			RKIT_CHECK(visitor.VisitMember(self.m_materialGroups));
			RKIT_CHECK(visitor.VisitMember(self.m_lightmapGroups));
			RKIT_CHECK(visitor.VisitMember(self.m_modelGroups));
			RKIT_CHECK(visitor.VisitMember(self.m_drawClusters));
			RKIT_CHECK(visitor.VisitMember(self.m_models));
			RKIT_CHECK(visitor.VisitMember(self.m_modelDrawClusters));
			RKIT_CHECK(visitor.VisitMember(self.m_leafBrushes));
			RKIT_CHECK(visitor.VisitMember(self.m_leafFaces));
			RKIT_CHECK(visitor.VisitMember(self.m_triIndexes));

			return rkit::ResultCode::kOK;
		}
	};
} }
