#pragma once

#include "AnoxResourceManager.h"

#include "rkit/Math/Vec.h"

namespace rkit
{
	template<class T>
	class Span;
}

namespace anox
{
	class AnoxBSPModelResourceBase;

	class AnoxBSPModelResourceLoaderBase : public AnoxCIPathKeyedResourceLoader<AnoxBSPModelResourceBase>
	{
	public:
		static rkit::Result Create(rkit::RCPtr<AnoxBSPModelResourceLoaderBase> &resLoader);
	};

	class AnoxBSPModelResourceBase : public AnoxResourceBase
	{
	public:
		struct Plane
		{
			uint32_t m_normalIndex;
			float m_dist;
		};

		struct BrushSide
		{
			uint32_t m_planeFlipBit : 1;
			uint32_t m_planeIndex : 31;
			uint32_t m_material;
			uint32_t m_materialFlags;
		};

		struct Brush
		{
			uint32_t m_contents;
			rkit::Span<const BrushSide> m_sides;
		};

		struct DrawSurfaceLocator
		{
			uint32_t m_drawSurfChunkIndex;
			uint32_t m_bits;
		};

		struct DrawSurface
		{
			uint32_t m_firstTriIndex;
			uint32_t m_numTris;
		};

		struct Leaf
		{
			uint32_t m_contentFlags;

			int16_t m_cluster;
			int16_t m_area;

			int16_t m_minBounds[3];
			int16_t m_maxBounds[3];

			rkit::Span<const uint16_t> m_brushes;
			rkit::Span<DrawSurfaceLocator> m_drawLocators;	// Only for model 0
		};

		struct TreeNode
		{
			uint32_t m_planeFlip : 1;
			uint32_t m_planeIndex : 31;

			int16_t m_minBounds[3];
			int16_t m_maxBounds[3];

			uint32_t m_frontNode : 31;
			uint32_t m_frontNodeIsLeaf : 1;

			uint32_t m_backNode : 31;
			uint32_t m_backNodeIsLeaf : 1;
		};

		struct Model
		{
			uint32_t m_firstModelDrawClusterModelGroupRefIndex;
			uint32_t m_numModelDrawClusterModelGroups;

			uint32_t m_firstDrawSurfaceIndex;
			uint32_t m_numDrawSurfaces;

			rkit::math::Vec3 m_mins;
			rkit::math::Vec3 m_maxs;
			rkit::math::Vec3 m_origin;

			uint32_t m_firstLeaf;
			uint32_t m_numLeafs;

			uint32_t m_rootIndex : 31;
			uint32_t m_rootIsLeaf : 1;
		};

		struct DrawLightmapGroup
		{
			uint32_t m_lightMapAtlasIndex;
			rkit::Span<const DrawSurface> m_drawSurfaces;
			DrawSurface m_combinedSurface;
		};

		struct DrawMaterialGroup
		{
			uint32_t m_materialIndex;
			rkit::Span<const DrawLightmapGroup> m_lightmapGroups;
			DrawSurface m_combinedSurface;
		};

		struct DrawModelGroup
		{
			rkit::Span<const DrawMaterialGroup> m_materialGroups;
			DrawSurface m_combinedSurface;
		};

		struct DrawCluster
		{
			rkit::Span<const DrawModelGroup> m_modelGroups;
			uint32_t m_firstVertIndex;
			uint32_t m_numVerts;
			DrawSurface m_combinedSurface;
		};

		struct DrawClusterModelGroupRef
		{
			uint32_t m_clusterIndex;
			uint32_t m_modelGroupIndex;
		};
	};
}
