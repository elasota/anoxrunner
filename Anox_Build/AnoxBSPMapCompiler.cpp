#include "AnoxBSPMapCompiler.h"

#include "AnoxMaterialCompiler.h"

#include "anox/AnoxModule.h"
#include "anox/Data/BSPModel.h"

#include "rkit/Core/BoolVector.h"
#include "rkit/Core/HashTable.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Endian.h"
#include "rkit/Core/Optional.h"
#include "rkit/Core/Pair.h"
#include "rkit/Core/QuickSort.h"
#include "rkit/Core/Stream.h"

#include "rkit/Data/ContentID.h"
#include "rkit/Data/DDSFile.h"

#include "rkit/Math/SoftFloat.h"


#include <cmath>

#define ANOX_CLUSTER_LIGHTMAPS 1

namespace anox { namespace buildsystem
{
	namespace priv
	{
		struct LightmapIdentifier
		{
			uint32_t m_faceIndex;
			uint8_t m_uniqueStyleIndex;
		};

		struct LightmapTreeNode
		{
			uint32_t m_children[2];	// 0 = Leaf

			uint16_t m_x;
			uint16_t m_y;
			uint16_t m_width;
			uint16_t m_height;

			LightmapIdentifier m_ident;
			bool m_isOccupied;
		};

		struct LightmapTree
		{
			rkit::Vector<LightmapTreeNode> m_nodes;
			uint32_t m_expansionLevel = 0;

			static const uint32_t kMaxExpansionLevel = 24;	// (1 << 12) = 4096
		};
	}

	enum class BSPLumpIndex
	{
		kEntities,
		kPlanes,
		kVerts,
		kVisibility,
		kNodes,
		kTexInfo,
		kFaces,
		kLightmaps,
		kLeafs,
		kLeafFaces,
		kLeafBrushes,
		kEdges,
		kFaceEdges,
		kModels,
		kBrushes,
		kBrushSides,
		kPop,
		kAreas,
		kAreaPortals,

		kCount,
	};

	struct BSPLump
	{
		rkit::endian::LittleUInt32_t m_offset;
		rkit::endian::LittleUInt32_t m_length;
	};

	struct BSPNode
	{
		rkit::endian::LittleUInt32_t m_plane;

		rkit::endian::LittleInt32_t m_front;
		rkit::endian::LittleInt32_t m_back;

		rkit::endian::LittleInt16_t m_minBounds[3];
		rkit::endian::LittleInt16_t m_maxBounds[3];

		rkit::endian::LittleUInt16_t m_firstFace;
		rkit::endian::LittleUInt16_t m_numFaces;
	};

	struct BSPLeaf
	{
		rkit::endian::LittleUInt32_t m_contentFlags;

		rkit::endian::LittleInt16_t m_cluster;
		rkit::endian::LittleInt16_t m_area;

		rkit::endian::LittleInt16_t m_minBounds[3];
		rkit::endian::LittleInt16_t m_maxBounds[3];

		rkit::endian::LittleUInt16_t m_firstLeafFace;
		rkit::endian::LittleUInt16_t m_numLeafFaces;

		rkit::endian::LittleUInt16_t m_firstLeafBrush;
		rkit::endian::LittleUInt16_t m_numLeafBrushes;
	};

	struct BSPTexInfo
	{
		static const uint32_t kFlagSky = (1 << 2);
		static const uint32_t kFlagWater = (1 << 3);

		rkit::endian::LittleFloat32_t m_uvVectors[2][4];
		rkit::endian::LittleUInt32_t m_flags;
		rkit::endian::LittleInt32_t m_value;
		char m_textureName[32];
		rkit::endian::LittleInt32_t m_nextFrame;
	};

	struct BSPVertex
	{
		rkit::endian::LittleFloat32_t x;
		rkit::endian::LittleFloat32_t y;
		rkit::endian::LittleFloat32_t z;
	};

	struct BSPEdge
	{
		rkit::endian::LittleUInt16_t m_verts[2];
	};

	struct BSPFace
	{
		rkit::endian::LittleUInt16_t m_plane;
		rkit::endian::LittleInt16_t m_side;

		rkit::endian::LittleUInt32_t m_firstEdge;
		rkit::endian::LittleUInt16_t m_numEdges;
		rkit::endian::LittleUInt16_t m_texture;

		uint8_t m_lmStyles[4];
		rkit::endian::LittleUInt32_t m_lightmapDataPos;
	};

	struct BSPPlane
	{
		rkit::endian::LittleFloat32_t m_normal[3];
		rkit::endian::LittleFloat32_t m_dist;
		rkit::endian::LittleUInt32_t m_flags;
	};

	struct BSPBrush
	{
		rkit::endian::LittleUInt32_t m_firstBrushSide;
		rkit::endian::LittleUInt32_t m_numBrushSides;
		rkit::endian::LittleUInt32_t m_contents;
	};

	struct BSPBrushSide
	{
		rkit::endian::LittleUInt16_t m_plane;
		rkit::endian::LittleUInt16_t m_texInfo;
	};

	struct BSPModel
	{
		rkit::endian::LittleFloat32_t m_mins[3];
		rkit::endian::LittleFloat32_t m_maxs[3];
		rkit::endian::LittleFloat32_t m_origin[3];
		rkit::endian::LittleInt32_t m_headNode;
		rkit::endian::LittleUInt32_t m_firstFace;
		rkit::endian::LittleUInt32_t m_numFaces;
	};

	typedef rkit::endian::LittleInt32_t BSPFaceEdge_t;

	struct BSPDataCollection
	{
		rkit::Vector<BSPModel> m_models;
		rkit::Vector<BSPNode> m_nodes;
		rkit::Vector<BSPTexInfo> m_texInfos;
		rkit::Vector<BSPFaceEdge_t> m_faceEdges;
		rkit::Vector<BSPFace> m_faces;
		rkit::Vector<BSPEdge> m_edges;
		rkit::Vector<BSPVertex> m_verts;
		rkit::Vector<uint8_t> m_lightMapData;
		rkit::Vector<BSPLeaf> m_leafs;
		rkit::Vector<rkit::endian::LittleUInt16_t> m_leafFaces;
		rkit::Vector<rkit::endian::LittleUInt16_t> m_leafBrushes;
		rkit::Vector<BSPPlane> m_planes;
		rkit::Vector<BSPBrush> m_brushes;
		rkit::Vector<BSPBrushSide> m_brushSides;
	};

	struct BSPHeader
	{
		rkit::endian::LittleUInt32_t m_magic;
		rkit::endian::LittleUInt32_t m_version;

		BSPLump m_lumps[static_cast<size_t>(BSPLumpIndex::kCount)];

		// Anachronox seems to have some extra data of unknown purpose after the header:
		// 01 00 34 12 <32-bit size> 00 00
		// <Data>
		// 02 00 34 12 <32-bit size> 00 00
		// <Data>
		// 03 00 34 12 <32-bit size> 00 00
		// <Data>
		//
		// These seem to be mostly zero fill, although the third occasionally has an ID
		// that may be the username of the last person to edit it?
	};

	struct BSPFaceStats
	{
		float m_lightmapUVMin[2] = {};
		uint16_t m_lightmapDimensions[2] = {};
		uint16_t m_lightmapPos[2] = {};
		uint8_t m_numUniqueStyles = 0;
		uint8_t m_numInputStyles = 0;
		uint8_t m_uniqueStyles[4] = {};

		rkit::Optional<uint64_t> m_atlasIndex;
	};

	struct BSPRenderableFace
	{
		uint32_t m_modelIndex;
		uint32_t m_atlasIndex;
		uint32_t m_materialIndex;
		uint32_t m_firstTri;
		uint32_t m_numTris;
		size_t m_originalFaceIndex;
		size_t m_mergeWidth;
	};

	template<size_t TSize>
	struct U32Cluster
	{
		uint32_t m_values[TSize] = {};

		bool operator==(const U32Cluster<TSize> &other) const;
		bool operator!=(const U32Cluster<TSize> &other) const;
	};

	// Packing: XYZ UV LightmapUV NormalIndex
	typedef U32Cluster<8> BSPGeometryVertex_t;
	typedef U32Cluster<3> BSPVec3_t;
	typedef U32Cluster<2> IndexedPlane_t;

	struct BSPGeometryClusterLookups
	{
		rkit::HashMap<BSPGeometryVertex_t, size_t> m_vertLookup;

		inline void Clear()
		{
			m_vertLookup.Clear();
		}
	};

	struct BSPGeometryCluster
	{
		static const size_t kMaxVerts = (1 << 16);

		rkit::Vector<BSPGeometryVertex_t> m_verts;
		rkit::Vector<uint16_t> m_indexes;
		rkit::Vector<BSPRenderableFace> m_faces;
	};

	class BSPMapCompilerBase2 : public BSPMapCompilerBase
	{
	protected:
		struct LumpLoader
		{
			template<class TStructure>
			LumpLoader(BSPLumpIndex lumpIndex, rkit::Vector<TStructure> &arr);

			BSPLumpIndex m_lumpIndex;
			void *m_arr;
			rkit::Result(*m_loadLump)(void *arr, rkit::ISeekableReadStream &stream, const BSPHeader &bspHeader, BSPLumpIndex lumpIndex);

		private:
			template<class TStructure>
			static rkit::Result LoadLumpThunk(void *arr, rkit::ISeekableReadStream &stream, const BSPHeader &bspHeader, BSPLumpIndex lumpIndex);
		};

		class LumpLoaderSortPred
		{
		public:
			LumpLoaderSortPred(const BSPHeader &bspHeader);

			bool operator()(const LumpLoader &a, const LumpLoader &b) const;

		private:
			const BSPHeader &m_header;
		};

		struct BSPOutput
		{
			rkit::Vector<data::BSPMaterial> m_materials;
			rkit::Vector<char> m_materialNameChars;
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

		struct VectorSerializer
		{
			typedef rkit::Result (*WriterCallback_t)(const void *vectorPtr, rkit::IWriteStream &stream);
			typedef size_t (*GetSizeCallback_t)(const void *vectorPtr);

			const void *m_vectorPtr;
			WriterCallback_t m_writerCallback;
			GetSizeCallback_t m_getSizeCallback;
		};

		struct FaceStatsHeader
		{
			uint32_t m_numFaces = 0;
			uint32_t m_numLightmapTrees = 0;
		};

		struct LightmapTreeSerializedBlock
		{
			uint32_t m_expansionLevel = 0;
			uint32_t m_numNodes = 0;
		};

		template<class TStructure>
		static rkit::Result LoadLump(rkit::Vector<TStructure> &arr, rkit::ISeekableReadStream &stream, const BSPHeader &bspHeader, BSPLumpIndex lumpIndex);

		static rkit::Result LoadFaceStats(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback,
			rkit::Vector<BSPFaceStats> &faceStats, rkit::Vector<rkit::UniquePtr<priv::LightmapTree>> &lightmapTrees);

		static rkit::Result ComputeSurfaceExtents(BSPDataCollection &bsp, rkit::Vector<BSPFaceStats> &faceStats);
		static rkit::Result BuildLightmapTrees(BSPDataCollection &bsp, const rkit::Vector<BSPFaceStats> &faceStats, rkit::Vector<rkit::UniquePtr<priv::LightmapTree>> &lightmapTrees);
		static rkit::Result InsertNodeIntoLightmapTreeList(const priv::LightmapIdentifier &ident, uint16_t width, uint16_t height, bool xAxisDominant, rkit::Vector<size_t> &stack, rkit::Vector<rkit::UniquePtr<priv::LightmapTree>> &lightmapTrees);
		static rkit::Result InsertNodeIntoExpandableLightmapTree(uint16_t width, uint16_t height, bool xAxisDominant, rkit::Vector<size_t> &stack, priv::LightmapTree &tree, rkit::Optional<size_t> &outInsertedIndex);
		static rkit::Result InsertNodeIntoLightmapTree(uint16_t width, uint16_t height, rkit::Vector<size_t> &stack, priv::LightmapTree &tree, rkit::Optional<size_t> &outInsertedIndex);
		static rkit::Result InitLightmapTree(priv::LightmapTree &tree, uint32_t expansionLevel, bool xAxisDominant);
		static rkit::Result CopyLightmapTree(priv::LightmapTree &newTree, const priv::LightmapTree &oldTree, rkit::Vector<size_t> &stack, bool &outCopiedOK);
		static rkit::Result ExportLightmaps(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback,
			rkit::Vector<rkit::data::ContentID> &outContentIDs, const BSPDataCollection &bsp, const rkit::Vector<BSPFaceStats> &faceStats, const rkit::Vector<rkit::UniquePtr<priv::LightmapTree>> &lightmapTrees);
		static bool LightmapFitsInNode(const priv::LightmapTreeNode &node, uint16_t width, uint16_t height);

		static rkit::Result BuildGeometry(BSPOutput &bspOutput, const BSPDataCollection &bsp,
			rkit::ConstSpan<BSPFaceStats> stats, rkit::Span<size_t> faceModelIndex,
			rkit::ConstSpan<size_t> texInfoToUniqueTexIndex, rkit::ConstSpan<rkit::Pair<uint16_t, uint16_t>> lightmapDimensions);

		static rkit::Result BuildMaterials(BSPOutput &bspOutput, rkit::ConstSpan<rkit::CIPath> paths);

		static rkit::Result LoadBSPData(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, BSPDataCollection &bsp, rkit::Vector<LumpLoader> &loaders);
		static rkit::Result WriteBSPModel(const BSPOutput &bspOutput, rkit::IWriteStream &outStream);

		template<class T>
		static VectorSerializer CreateVectorSerializer(const rkit::Vector<T> &arr);

		template<class T>
		static rkit::Result WriteVectorCB(const void *vectorPtr, rkit::IWriteStream &stream);

		template<class T>
		static size_t GetVectorSizeCB(const void *vectorPtr);

		static rkit::Result FormatLightmapPath(rkit::String &path, const rkit::StringView &identifier, size_t lightmapIndex);
		static rkit::Result FormatModelPath(rkit::String &path, const rkit::StringView &identifier);
		static rkit::Result FormatFaceStatsPath(rkit::String &path, const rkit::StringView &identifier);
	};

	class BSPMapCompiler final : public BSPMapCompilerBase2
	{
	public:
		bool HasAnalysisStage() const override;

		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;

		uint32_t GetVersion() const override;
	};

	class BSPLightingCompiler final : public BSPMapCompilerBase2
	{
	public:
		bool HasAnalysisStage() const override;

		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;

		uint32_t GetVersion() const override;
	};

	class BSPGeometryCompiler final : public BSPMapCompilerBase2
	{
	public:
		bool HasAnalysisStage() const override;

		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;

		uint32_t GetVersion() const override;
	};


	template<size_t TSize>
	bool U32Cluster<TSize>::operator==(const U32Cluster<TSize> &other) const
	{
		for (size_t i = 0; i < TSize; i++)
		{
			if (m_values[i] != other.m_values[i])
				return false;
		}

		return true;
	}

	template<size_t TSize>
	bool U32Cluster<TSize>::operator!=(const U32Cluster<TSize> &other) const
	{
		return !((*this) == other);
	}

	bool BSPMapCompiler::HasAnalysisStage() const
	{
		return true;
	}

	rkit::Result BSPMapCompiler::RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		BSPDataCollection bsp;

		rkit::Vector<LumpLoader> loaders;
		RKIT_CHECK(loaders.Append(LumpLoader(BSPLumpIndex::kTexInfo, bsp.m_texInfos)));

		RKIT_CHECK(LoadBSPData(depsNode, feedback, bsp, loaders));

		for (const BSPTexInfo &texInfo : bsp.m_texInfos)
		{
			char fixedName[sizeof(texInfo.m_textureName) + 1];

			size_t texNameLength = 0;
			while (texNameLength < sizeof(texInfo.m_textureName))
			{
				if (texInfo.m_textureName[texNameLength] == 0)
					break;

				fixedName[texNameLength] = texInfo.m_textureName[texNameLength];
				texNameLength++;
			}

			fixedName[texNameLength] = 0;

			for (size_t i = 0; i < texNameLength; i++)
			{
				if ((fixedName[i] & 0x80) != 0)
				{
					rkit::log::Error("Invalid material");
					return rkit::ResultCode::kDataError;
				}

				fixedName[i] = rkit::InvariantCharCaseAdjuster<char>::ToLower(fixedName[i]);
				if (fixedName[i] == '\\')
					fixedName[i] = '/';
			}

			const rkit::ConstSpan<char> span(fixedName, texNameLength);

			if (rkit::CompareSpansEqual(span, rkit::StringView("null").ToSpan()))
				continue;

			if (rkit::CIPath::Validate(rkit::StringSliceView(span)) != rkit::PathValidationResult::kValid)
			{
				rkit::log::Error("Texture name was invalid");
				return rkit::ResultCode::kDataError;
			}

			rkit::String texPath;
			RKIT_CHECK(texPath.Set("textures/"));
			RKIT_CHECK(texPath.Append(span));
			RKIT_CHECK(texPath.Append('.'));
			RKIT_CHECK(texPath.Append(MaterialCompiler::GetWorldMaterialExtension()));

			RKIT_CHECK(feedback->AddNodeDependency(kAnoxNamespaceID, buildsystem::kWorldMaterialNodeID, rkit::buildsystem::BuildFileLocation::kSourceDir, texPath));
		}

		RKIT_CHECK(feedback->AddNodeDependency(kAnoxNamespaceID, buildsystem::kBSPLightmapNodeID, rkit::buildsystem::BuildFileLocation::kSourceDir, depsNode->GetIdentifier()));

		return rkit::ResultCode::kOK;
	}


	bool BSPLightingCompiler::HasAnalysisStage() const
	{
		return false;
	}

	rkit::Result BSPLightingCompiler::RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		return rkit::ResultCode::kInternalError;
	}

	rkit::Result BSPLightingCompiler::RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		BSPDataCollection bsp;

		rkit::Vector<LumpLoader> loaders;
		RKIT_CHECK(loaders.Append(LumpLoader(BSPLumpIndex::kTexInfo, bsp.m_texInfos)));
		RKIT_CHECK(loaders.Append(LumpLoader(BSPLumpIndex::kFaceEdges, bsp.m_faceEdges)));
		RKIT_CHECK(loaders.Append(LumpLoader(BSPLumpIndex::kFaces, bsp.m_faces)));
		RKIT_CHECK(loaders.Append(LumpLoader(BSPLumpIndex::kEdges, bsp.m_edges)));
		RKIT_CHECK(loaders.Append(LumpLoader(BSPLumpIndex::kVerts, bsp.m_verts)));

		RKIT_CHECK(LoadBSPData(depsNode, feedback, bsp, loaders));

		rkit::Vector<BSPFaceStats> faceStats;
		RKIT_CHECK(ComputeSurfaceExtents(bsp, faceStats));

		rkit::Vector<rkit::UniquePtr<priv::LightmapTree>> lightmapTrees;
		RKIT_CHECK(BuildLightmapTrees(bsp, faceStats, lightmapTrees));

		FaceStatsHeader header;
		header.m_numFaces = static_cast<uint32_t>(faceStats.Count());
		header.m_numLightmapTrees = static_cast<uint32_t>(lightmapTrees.Count());

		rkit::String faceStatsPathStr;
		RKIT_CHECK(FormatFaceStatsPath(faceStatsPathStr, depsNode->GetIdentifier()));

		rkit::CIPath faceStatsPath;
		RKIT_CHECK(faceStatsPath.Set(faceStatsPathStr));

		rkit::UniquePtr<rkit::ISeekableReadWriteStream> stream;
		RKIT_CHECK(feedback->OpenOutput(rkit::buildsystem::BuildFileLocation::kIntermediateDir, faceStatsPath, stream));

		RKIT_CHECK(stream->WriteAll(&header, sizeof(header)));
		RKIT_CHECK(stream->WriteAll(faceStats.GetBuffer(), faceStats.Count() * sizeof(BSPFaceStats)));

		for (const rkit::UniquePtr<priv::LightmapTree> &treePtr : lightmapTrees)
		{
			const priv::LightmapTree &tree = *treePtr;

			LightmapTreeSerializedBlock serBlock;
			serBlock.m_expansionLevel = tree.m_expansionLevel;
			serBlock.m_numNodes = static_cast<uint32_t>(tree.m_nodes.Count());

			RKIT_CHECK(stream->WriteAll(&serBlock, sizeof(serBlock)));
			RKIT_CHECK(stream->WriteAll(tree.m_nodes.GetBuffer(), tree.m_nodes.Count() * sizeof(priv::LightmapTreeNode)));
		}

		stream.Reset();

		return rkit::ResultCode::kOK;
	}

	uint32_t BSPLightingCompiler::GetVersion() const
	{
		return 1;
	}


	bool BSPGeometryCompiler::HasAnalysisStage() const
	{
		return false;
	}

	rkit::Result BSPGeometryCompiler::RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		return rkit::ResultCode::kInternalError;
	}

	rkit::Result BSPGeometryCompiler::RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		return rkit::ResultCode::kOK;
	}

	uint32_t BSPGeometryCompiler::GetVersion() const
	{
		return 1;
	}

	template<class TStructure>
	rkit::Result BSPMapCompilerBase2::LoadLump(rkit::Vector<TStructure> &arr, rkit::ISeekableReadStream &stream, const BSPHeader &bspHeader, BSPLumpIndex lumpIndex)
	{
		const BSPLump &lump = bspHeader.m_lumps[static_cast<size_t>(lumpIndex)];
		const uint32_t lumpSize = lump.m_length.Get();
		const uint32_t lumpPos = lump.m_offset.Get();
		const size_t structureSize = sizeof(TStructure);

		if (lumpSize % structureSize != 0)
		{
			rkit::log::ErrorFmt("Size of lump {} was invalid", static_cast<int>(lumpIndex));
			return rkit::ResultCode::kDataError;
		}

		RKIT_CHECK(arr.Resize(lumpSize / structureSize));

		if (lumpSize > 0)
		{
			RKIT_CHECK(stream.SeekStart(lumpPos));
			RKIT_CHECK(stream.ReadAll(arr.GetBuffer(), lumpSize));
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result BSPMapCompilerBase2::LoadFaceStats(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback,
		rkit::Vector<BSPFaceStats> &faceStats, rkit::Vector<rkit::UniquePtr<priv::LightmapTree>> &lightmapTrees)
	{
		rkit::String pathStr;
		RKIT_CHECK(FormatFaceStatsPath(pathStr, depsNode->GetIdentifier()));

		rkit::CIPath path;
		RKIT_CHECK(path.Set(pathStr));

		rkit::UniquePtr<rkit::ISeekableReadStream> stream;
		RKIT_CHECK(feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kIntermediateDir, path, stream));

		FaceStatsHeader header;
		RKIT_CHECK(stream->ReadAll(&header, sizeof(header)));

		RKIT_CHECK(faceStats.Resize(header.m_numFaces));
		RKIT_CHECK(lightmapTrees.Resize(header.m_numLightmapTrees));

		RKIT_CHECK(stream->ReadAll(faceStats.GetBuffer(), faceStats.Count() * sizeof(BSPFaceStats)));

		for (size_t i = 0; i < lightmapTrees.Count(); i++)
		{
			LightmapTreeSerializedBlock block = {};
			RKIT_CHECK(stream->ReadAll(&block, sizeof(block)));

			rkit::UniquePtr<priv::LightmapTree> tree;
			RKIT_CHECK(rkit::New<priv::LightmapTree>(tree));

			RKIT_CHECK(tree->m_nodes.Resize(block.m_numNodes));
			tree->m_expansionLevel = block.m_expansionLevel;
			RKIT_CHECK(stream->ReadAll(tree->m_nodes.GetBuffer(), tree->m_nodes.Count() * sizeof(priv::LightmapTreeNode)));

			lightmapTrees[i] = std::move(tree);
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result BSPMapCompilerBase2::ComputeSurfaceExtents(BSPDataCollection &bsp, rkit::Vector<BSPFaceStats> &faceStatsVector)
	{
		rkit::math::SoftFloat80 rcp16(0.0625);

		RKIT_CHECK(faceStatsVector.Resize(bsp.m_faces.Count()));

		// Have to use emulated XPFloats to compute this exactly
		for (size_t faceIndex = 0; faceIndex < bsp.m_faces.Count(); faceIndex++)
		{
			const BSPFace &face = bsp.m_faces[faceIndex];
			BSPFaceStats &faceStats = faceStatsVector[faceIndex];

			const uint16_t textureIndex = face.m_texture.Get();
			if (textureIndex >= bsp.m_texInfos.Count())
			{
				rkit::log::Error("Texture index was out of range");
				return rkit::ResultCode::kDataError;
			}

			const BSPTexInfo &texInfo = bsp.m_texInfos[textureIndex];

			const uint32_t texFlags = texInfo.m_flags.Get();

			if (texFlags & (BSPTexInfo::kFlagSky | BSPTexInfo::kFlagWater))
				continue;

			for (int i = 0; i < 4; i++)
			{
				if (face.m_lmStyles[i] == 0xff)
					break;

				uint8_t usi = 0;
				for (usi = 0; usi < faceStats.m_numUniqueStyles; usi++)
				{
					if (faceStats.m_uniqueStyles[usi] == face.m_lmStyles[i])
						break;
				}

				if (usi == faceStats.m_numUniqueStyles)
					faceStats.m_uniqueStyles[faceStats.m_numUniqueStyles++];

				faceStats.m_numInputStyles++;
			}

			if (faceStats.m_numUniqueStyles == 0)
				continue;

			const uint32_t firstEdge = face.m_firstEdge.Get();

			if (firstEdge > bsp.m_faceEdges.Count())
			{
				rkit::log::Error("Face leading edge was out of range");
				return rkit::ResultCode::kDataError;
			}

			const uint32_t numEdges = face.m_numEdges.Get();

			if (bsp.m_faceEdges.Count() - firstEdge < numEdges)
			{
				rkit::log::Error("Face trailing edge was out of range");
				return rkit::ResultCode::kDataError;
			}

			rkit::math::SoftFloat32 extMin[2];
			rkit::math::SoftFloat32 extMax[2];

			// qrad3 used x87 ops with 80-bit intermediates and we need to exactly
			// reproduce the rounding behavior here to get accurate lightmap resolutions.
			// The intermediate values are intentionally truncated to Float32.
			for (size_t edgeIndex = 0; edgeIndex < numEdges; edgeIndex++)
			{
				int32_t edge = bsp.m_faceEdges[edgeIndex + firstEdge].Get();

				if (edge == std::numeric_limits<int32_t>::min())
				{
					rkit::log::Error("Edge index is invalid");
					return rkit::ResultCode::kDataError;
				}

				uint8_t edgeVertIndex = 0;

				if (edge >= 0)
					edgeVertIndex = 0;
				else
				{
					edge = -edge;
					edgeVertIndex = 1;
				}

				const uint16_t vertIndex = bsp.m_edges[edge].m_verts[edgeVertIndex].Get();

				if (vertIndex >= bsp.m_verts.Count())
				{
					rkit::log::Error("Edge vert index is invalid");
					return rkit::ResultCode::kDataError;
				}

				const BSPVertex &vert = bsp.m_verts[vertIndex];

				for (int axis = 0; axis < 2; axis++)
				{
					const rkit::math::SoftFloat32 vert0 = rkit::math::SoftFloat32::FromBits(vert.x.GetBits());
					const rkit::math::SoftFloat32 vert1 = rkit::math::SoftFloat32::FromBits(vert.y.GetBits());
					const rkit::math::SoftFloat32 vert2 = rkit::math::SoftFloat32::FromBits(vert.z.GetBits());

					const rkit::math::SoftFloat32 vec0bits = rkit::math::SoftFloat32::FromBits(texInfo.m_uvVectors[axis][0].GetBits());
					const rkit::math::SoftFloat32 vec1bits = rkit::math::SoftFloat32::FromBits(texInfo.m_uvVectors[axis][1].GetBits());
					const rkit::math::SoftFloat32 vec2bits = rkit::math::SoftFloat32::FromBits(texInfo.m_uvVectors[axis][2].GetBits());
					const rkit::math::SoftFloat32 vec3bits = rkit::math::SoftFloat32::FromBits(texInfo.m_uvVectors[axis][3].GetBits());

					const rkit::math::SoftFloat32 v = (rkit::math::SoftFloat80(vert0) * vec0bits
						+ rkit::math::SoftFloat80(vert1) * vec1bits
						+ rkit::math::SoftFloat80(vert2) * vec2bits
						+ vec3bits).ToFloat32();

					if (edgeIndex == 0 || v < extMin[axis])
						extMin[axis] = v;
					if (edgeIndex == 0 || v > extMax[axis])
						extMax[axis] = v;
				}
			}

			uint16_t lightmapSize[2] = {};

			for (int axis = 0; axis < 2; axis++)
			{
				const rkit::math::SoftFloat80 lightmapMinF80 = (extMin[axis].ToFloat80() * rcp16).Floor();
				const rkit::math::SoftFloat80 lightmapMaxF80 = (extMax[axis].ToFloat80() * rcp16).Ceil();

				const int64_t intMin = lightmapMinF80.ToInt64();
				const int64_t intMax = lightmapMaxF80.ToInt64();

				if (intMax < intMin)
				{
					rkit::log::Error("Invalid lightmap dimensions");
					return rkit::ResultCode::kDataError;
				}

				constexpr uint16_t maxDimension = 4096;
				int64_t maxOfIntMax = std::numeric_limits<int64_t>::max();
				if (intMin < std::numeric_limits<int64_t>::max() - static_cast<int64_t>(maxDimension) + 1)
				{
					maxOfIntMax = intMin + maxDimension - 1;
				}

				if (intMax > maxOfIntMax)
				{
					rkit::log::Error("Invalid lightmap dimensions");
					return rkit::ResultCode::kDataError;
				}

				faceStats.m_lightmapDimensions[axis] = static_cast<uint16_t>(intMax - intMin + 1);
				faceStats.m_lightmapUVMin[axis] = lightmapMinF80.ToSingle();
			}
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result BSPMapCompilerBase2::BuildLightmapTrees(BSPDataCollection &bsp, const rkit::Vector<BSPFaceStats> &faceStats, rkit::Vector<rkit::UniquePtr<priv::LightmapTree>> &lightmapTrees)
	{
		uint64_t xAxisTotal = 0;
		uint64_t yAxisTotal = 0;

		rkit::Vector<size_t> stack;

		const size_t kMaxLightmapSizeIncrement = 12;	// 1 << 6 == 4096

		for (const BSPFaceStats &stats : faceStats)
		{
			xAxisTotal += stats.m_lightmapDimensions[0];
			yAxisTotal += stats.m_lightmapDimensions[1] * stats.m_numUniqueStyles;
		}

		bool xAxisDominant = (xAxisTotal >= yAxisTotal);

		for (size_t faceIndex = 0; faceIndex < faceStats.Count(); faceIndex++)
		{
			const BSPFaceStats &stats = faceStats[faceIndex];
			const BSPFace &face = bsp.m_faces[faceIndex];

			if (!stats.m_numUniqueStyles)
				continue;

#if ANOX_CLUSTER_LIGHTMAPS
			priv::LightmapIdentifier ident = {};
			ident.m_faceIndex = static_cast<uint32_t>(faceIndex);
			ident.m_uniqueStyleIndex = 0;
			RKIT_CHECK(InsertNodeIntoLightmapTreeList(ident, stats.m_lightmapDimensions[0], stats.m_lightmapDimensions[1] * stats.m_numUniqueStyles, xAxisDominant, stack, lightmapTrees));
#else
			for (uint8_t styleIndex = 0; styleIndex < 4; styleIndex++)
			{
				if (face.m_lmStyles[styleIndex] == 0xff)
					break;

				priv::LightmapIdentifier ident = {};
				ident.m_faceIndex = static_cast<uint32_t>(faceIndex);
				ident.m_styleIndex = styleIndex;
				RKIT_CHECK(InsertNodeIntoLightmapTreeList(ident, stats.m_lightmapDimensions[0], stats.m_lightmapDimensions[1], xAxisDominant, stack, lightmapTrees));
			}
#endif
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result BSPMapCompilerBase2::InsertNodeIntoLightmapTreeList(const priv::LightmapIdentifier &ident, uint16_t width, uint16_t height, bool xAxisDominant, rkit::Vector<size_t> &stack, rkit::Vector<rkit::UniquePtr<priv::LightmapTree>> &lightmapTrees)
	{
		// Based on this algorithm: https://blackpawn.com/texts/lightmaps/
		// Use a vector stack because this can potentially get very deep (4096 levels!)

		for (int attempts = 0; attempts < 2; attempts++)
		{
			if (lightmapTrees.Count() > 0)
			{
				priv::LightmapTree &lastTree = *lightmapTrees[lightmapTrees.Count() - 1];
				rkit::Optional<size_t> insertedIndex;
				RKIT_CHECK(InsertNodeIntoExpandableLightmapTree(width, height, xAxisDominant, stack, lastTree, insertedIndex));

				if (insertedIndex.IsSet())
				{
					priv::LightmapTreeNode &treeNode = lastTree.m_nodes[insertedIndex.Get()];
					treeNode.m_ident = ident;

					return rkit::ResultCode::kOK;
				}
			}

			rkit::UniquePtr<priv::LightmapTree> newTree;
			RKIT_CHECK(rkit::New<priv::LightmapTree>(newTree));
			RKIT_CHECK(InitLightmapTree(*newTree, 0, xAxisDominant));

			RKIT_CHECK(lightmapTrees.Append(std::move(newTree)));
		}

		return rkit::ResultCode::kInternalError;
	}

	rkit::Result BSPMapCompilerBase2::InitLightmapTree(priv::LightmapTree &tree, uint32_t expansionLevel, bool xAxisDominant)
	{
		uint16_t width = (1 << (expansionLevel / 2));
		uint16_t height = (1 << (expansionLevel / 2));

		if (expansionLevel & 1)
		{
			if (xAxisDominant)
				width *= 2;
			else
				height *= 2;
		}

		priv::LightmapTreeNode rootNode = {};
		rootNode.m_width = width;
		rootNode.m_height = height;

		RKIT_CHECK(tree.m_nodes.Append(rootNode));
		tree.m_expansionLevel = expansionLevel;

		return rkit::ResultCode::kOK;
	}

	rkit::Result BSPMapCompilerBase2::CopyLightmapTree(priv::LightmapTree &newTree, const priv::LightmapTree &oldTree, rkit::Vector<size_t> &stack, bool &outCopiedOK)
	{
		for (const priv::LightmapTreeNode &oldNode : oldTree.m_nodes)
		{
			if (!oldNode.m_isOccupied)
				continue;

			rkit::Optional<size_t> newIndex;
			RKIT_CHECK(InsertNodeIntoLightmapTree(oldNode.m_width, oldNode.m_height, stack, newTree, newIndex));
			if (!newIndex.IsSet())
			{
				outCopiedOK = false;
				return rkit::ResultCode::kOK;
			}

			priv::LightmapTreeNode &newNode = newTree.m_nodes[newIndex.Get()];
			newNode.m_ident = oldNode.m_ident;
		}

		outCopiedOK = true;
		return rkit::ResultCode::kOK;
	}

	rkit::Result BSPMapCompilerBase2::InsertNodeIntoExpandableLightmapTree(uint16_t width, uint16_t height, bool xAxisDominant, rkit::Vector<size_t> &stack, priv::LightmapTree &tree, rkit::Optional<size_t> &outInsertedIndex)
	{
		outInsertedIndex.Reset();

		uint16_t requiredExpansionLevel = 0;

		uint32_t tryExpansionLevel = tree.m_expansionLevel;

		while (tryExpansionLevel < priv::LightmapTree::kMaxExpansionLevel)
		{
			priv::LightmapTree replacementTree;
			priv::LightmapTree *treeToInsertInto = nullptr;

			if (tree.m_expansionLevel == tryExpansionLevel)
				treeToInsertInto = &tree;
			else
			{
				RKIT_CHECK(InitLightmapTree(replacementTree, tryExpansionLevel, xAxisDominant));

				bool copiedOK = false;

				if (replacementTree.m_nodes[0].m_width >= width && replacementTree.m_nodes[0].m_height >= height)
				{
					RKIT_CHECK(CopyLightmapTree(replacementTree, tree, stack, copiedOK));

					if (copiedOK)
						treeToInsertInto = &replacementTree;
				}
			}

			if (treeToInsertInto)
			{
				RKIT_CHECK(InsertNodeIntoLightmapTree(width, height, stack, *treeToInsertInto, outInsertedIndex));

				if (outInsertedIndex.IsSet())
				{
					// Inserted OK
					if (treeToInsertInto != &tree)
						tree = std::move(*treeToInsertInto);

					return rkit::ResultCode::kOK;
				}
			}

			tryExpansionLevel++;
		}

		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result BSPMapCompilerBase2::InsertNodeIntoLightmapTree(uint16_t width, uint16_t height, rkit::Vector<size_t> &stack, priv::LightmapTree &tree, rkit::Optional<size_t> &outInsertedIndex)
	{
		size_t currentNode = 0;
		size_t stackDepth = 0;

		for (;;)
		{
			priv::LightmapTreeNode &node = tree.m_nodes[currentNode];

			if (node.m_children[0])
			{
				if (node.m_children[1])
				{
					if (stackDepth == stack.Count())
					{
						RKIT_CHECK(stack.Append(0));
					}

					stack[stackDepth++] = node.m_children[1];
				}

				currentNode = node.m_children[0];
				continue;
			}
			else if (node.m_children[1])
			{
				currentNode = node.m_children[1];
				continue;
			}

			if (node.m_isOccupied || !LightmapFitsInNode(node, width, height))
			{
				// Doesn't fit in this node
				if (stackDepth == 0)
					return rkit::ResultCode::kOK;

				stackDepth--;
				currentNode = stack[stackDepth];
				continue;
			}

			uint16_t dw = node.m_width - width;
			uint16_t dh = node.m_height - height;

			if (dw == 0 && dh == 0)
			{
				// Fits in this node
				node.m_isOccupied = true;
				outInsertedIndex = currentNode;
				return rkit::ResultCode::kOK;
			}

			// Split
			size_t child0Index = tree.m_nodes.Count();
			size_t child1Index = child0Index + 1;

			node.m_children[0] = static_cast<uint32_t>(child0Index);
			node.m_children[1] = static_cast<uint32_t>(child1Index);

			// node will be invalid on append, so we have to copy out information now
			priv::LightmapTreeNode baseNewNode = {};
			baseNewNode.m_x = node.m_x;
			baseNewNode.m_y = node.m_y;
			baseNewNode.m_width = node.m_width;
			baseNewNode.m_height = node.m_height;


			if (dw > dh)
			{
				{
					priv::LightmapTreeNode newNode = baseNewNode;
					newNode.m_width = width;
					RKIT_CHECK(tree.m_nodes.Append(newNode));
				}
				{
					priv::LightmapTreeNode newNode = baseNewNode;
					newNode.m_x += width;
					newNode.m_width -= width;
					RKIT_CHECK(tree.m_nodes.Append(newNode));
				}
			}
			else
			{
				{
					priv::LightmapTreeNode newNode = baseNewNode;
					newNode.m_height = height;
					RKIT_CHECK(tree.m_nodes.Append(newNode));
				}
				{
					priv::LightmapTreeNode newNode = baseNewNode;
					newNode.m_y += height;
					newNode.m_height -= height;
					RKIT_CHECK(tree.m_nodes.Append(newNode));
				}
			}

			currentNode = child0Index;
		}
	}

	rkit::Result ScanLeaf(const BSPDataCollection &bsp, rkit::Vector<size_t> &leafOrder, rkit::BoolVector &leafUsedBits, int32_t leafValue)
	{
		const uint32_t leafIndex = ~static_cast<uint32_t>(leafValue);

		if (leafIndex >= leafUsedBits.Count())
		{
			rkit::log::Error("Leaf index out of range");
			return rkit::ResultCode::kDataError;
		}

		if (leafUsedBits[leafIndex])
		{
			rkit::log::Error("Leaf used multiple times");
			return rkit::ResultCode::kDataError;
		}

		leafUsedBits.Set(leafIndex, true);

		RKIT_CHECK(leafOrder.Append(leafIndex));

		return rkit::ResultCode::kOK;
	}

	static uint32_t SanitizeFloatBits(uint32_t floatBits)
	{
		if (floatBits == 0x80000000u)
			return 0;
		return floatBits;
	}

	class WrappingVertSpan final : public rkit::ISpan<const BSPGeometryVertex_t &>
	{
	public:
		explicit WrappingVertSpan(const rkit::ISpan<const BSPGeometryVertex_t &> &baseSpan, size_t start, size_t count);

		size_t Count() const override;
		const BSPGeometryVertex_t &operator[](size_t index) const override;

	private:
		const rkit::ISpan<const BSPGeometryVertex_t &> &m_baseSpan;
		size_t m_baseCount;
		size_t m_first;
		size_t m_count;
	};


	WrappingVertSpan::WrappingVertSpan(const rkit::ISpan<const BSPGeometryVertex_t &> &baseSpan, size_t start, size_t count)
		: m_baseSpan(baseSpan)
		, m_baseCount(m_baseSpan.Count())
		, m_first(start)
		, m_count(count)
	{
	}

	size_t WrappingVertSpan::Count() const
	{
		return m_count;
	}

	const BSPGeometryVertex_t &WrappingVertSpan::operator[](size_t index) const
	{
		RKIT_ASSERT(index < m_count);
		return m_baseSpan[(m_first + index) % m_baseCount];
	}

	static rkit::Result OptimizeWinding(const rkit::ISpan<const BSPGeometryVertex_t &> &winding, rkit::Vector<rkit::StaticArray<BSPGeometryVertex_t, 3>> &outTris, int depth)
	{
		const size_t numVerts = winding.Count();
		if (numVerts < 3)
			return rkit::ResultCode::kInternalError;

		size_t bestTriplet[3] = {};
		double bestTripletAreaToPerimeterRatio = 0.0;

		for (size_t p0offset = 2; p0offset < numVerts; p0offset++)
		{
			for (size_t p1offset = 1; p1offset < p0offset; p1offset++)
			{
				for (size_t p2offset = 0; p2offset < p1offset; p2offset++)
				{
					size_t triplet[3] = { p2offset, p1offset, p0offset };

					// Reorder the triplet to ascending order inside of the winding
					while (triplet[2] >= winding.Count())
					{
						triplet[2] -= winding.Count();
						rkit::Swap(triplet[2], triplet[1]);
						rkit::Swap(triplet[1], triplet[0]);
					}

					rkit::StaticArray<rkit::StaticArray<double, 3>, 3> points = {};

					for (int pointIndex = 0; pointIndex < 3; pointIndex++)
					{
						for (int axis = 0; axis < 3; axis++)
							points[pointIndex][axis] = rkit::math::SoftFloat32::FromBits(winding[triplet[pointIndex]].m_values[axis]).ToDouble();
					}

					rkit::StaticArray<rkit::StaticArray<double, 3>, 3> lineVectors = {};

					double perimeter = 0.0;
					for (int pointIndex = 0; pointIndex < 3; pointIndex++)
					{
						const rkit::StaticArray<double, 3> &v1 = points[pointIndex];
						const rkit::StaticArray<double, 3> &v2 = points[(pointIndex + 1) % 3];

						double lineLengthSq = 0.0;
						for (int axis = 0; axis < 3; axis++)
						{
							const double axisDelta = v2[axis] - v1[axis];
							lineVectors[pointIndex][axis] = axisDelta;
							lineLengthSq += axisDelta * axisDelta;
						}

						perimeter += sqrt(lineLengthSq);
					}

					double crossProduct[3] =
					{
						lineVectors[0][1] * lineVectors[1][2] - lineVectors[0][2] * lineVectors[1][1],
						lineVectors[0][2] * lineVectors[1][0] - lineVectors[0][0] * lineVectors[1][2],
						lineVectors[0][0] * lineVectors[1][1] - lineVectors[0][1] * lineVectors[1][0]
					};

					// Actually 2x area but since this is all scaled by 2x it doesn't matter
					const double area = sqrt(crossProduct[0] * crossProduct[0] + crossProduct[1] * crossProduct[1] + crossProduct[2] * crossProduct[2]);

					double areaToPerimeterRatio = 0.0;
					if (perimeter != 0.0)
					{
						areaToPerimeterRatio = area / perimeter;
						if (!std::isfinite(areaToPerimeterRatio))
							areaToPerimeterRatio = 0.0;
					}

					if (areaToPerimeterRatio >= bestTripletAreaToPerimeterRatio)
					{
						for (int axis = 0; axis < 3; axis++)
							bestTriplet[axis] = triplet[axis];

						bestTripletAreaToPerimeterRatio = areaToPerimeterRatio;
					}
				}
			}
		}

		rkit::StaticArray<BSPGeometryVertex_t, 3> bestTri = {};
		for (int pointIndex = 0; pointIndex < 3; pointIndex++)
			bestTri[pointIndex] = winding[bestTriplet[pointIndex]];

		RKIT_CHECK(outTris.Append(bestTri));

		// Add sub-windings
		for (int firstPointIndex = 0; firstPointIndex < 3; firstPointIndex++)
		{
			const size_t firstPoint = bestTriplet[firstPointIndex];
			const size_t secondPoint = bestTriplet[(firstPointIndex + 1) % 3];

			const size_t secondPointWrapped = (secondPoint < firstPoint) ? (secondPoint + numVerts) : secondPoint;

			const size_t slicedWindingSize = (secondPointWrapped - firstPoint) + 1u;
			if (slicedWindingSize >= 3)
			{
				RKIT_CHECK(OptimizeWinding(WrappingVertSpan(winding, firstPoint, slicedWindingSize), outTris, depth + 1));
			}
		}

		return rkit::ResultCode::kOK;
	}

	static rkit::Result IndexNormal(uint32_t &outNormalIndex, rkit::HashMap<BSPVec3_t, size_t> &normalLookup, const BSPVec3_t &normal)
	{
		rkit::HashMap<BSPVec3_t, size_t>::ConstIterator_t it = normalLookup.Find(normal);
		if (it == normalLookup.end())
		{
			BSPVec3_t flippedNormal = {};
			for (int axis = 0; axis < 3; axis++)
				flippedNormal.m_values[axis] = SanitizeFloatBits(normal.m_values[axis] ^ 0x80000000u);

			it = normalLookup.Find(flippedNormal);
			if (it == normalLookup.end())
			{
				size_t newIndex = normalLookup.Count();

				if (newIndex == 0x80000000u)
				{
					rkit::log::Error("Too many unique normals");
					return rkit::ResultCode::kDataError;
				}

				RKIT_CHECK(normalLookup.Set(normal, newIndex));

				outNormalIndex = static_cast<uint32_t>(newIndex << 1);
			}
			else
				outNormalIndex = static_cast<uint32_t>((it.Value() << 1) | 1u);
		}
		else
			outNormalIndex = static_cast<uint32_t>((it.Value() << 1));

		return rkit::ResultCode::kOK;
	}

	static rkit::Result IndexPlane(uint32_t &outPlaneIndex, rkit::HashMap<IndexedPlane_t, size_t> &planeLookup,
		rkit::HashMap<BSPVec3_t, size_t> &normalLookup, const BSPVec3_t &normal, uint32_t distBits)
	{
		uint32_t normalIndex = 0;
		RKIT_CHECK(IndexNormal(normalIndex, normalLookup, normal));

		bool isFlipped = ((normalIndex & 1) != 0);
		if (isFlipped)
			distBits = SanitizeFloatBits(distBits ^ 0x80000000u);

		IndexedPlane_t planeKey = {};
		planeKey.m_values[0] = (normalIndex >> 1);
		planeKey.m_values[1] = distBits;

		uint32_t planeIndex = 0;
		rkit::HashMap<IndexedPlane_t, size_t>::ConstIterator_t it = planeLookup.Find(planeKey);
		if (it == planeLookup.end())
		{
			size_t newIndex = planeLookup.Count();

			if (newIndex == 0x80000000u)
			{
				rkit::log::Error("Too many unique planes");
				return rkit::ResultCode::kDataError;
			}

			RKIT_CHECK(planeLookup.Set(planeKey, newIndex));

			planeIndex = static_cast<uint32_t>(newIndex << 1);
		}
		else
			planeIndex = static_cast<uint32_t>(it.Value() << 1);

		if (isFlipped)
			planeIndex |= 1;

		outPlaneIndex = planeIndex;

		return rkit::ResultCode::kOK;
	}

	static rkit::Result IndexPlane(uint32_t &outPlaneIndex, rkit::HashMap<IndexedPlane_t, size_t> &planeLookup,
		rkit::HashMap<BSPVec3_t, size_t> &normalLookup, const BSPPlane &bspPlane)
	{
		BSPVec3_t normal = {};
		for (int axis = 0; axis < 3; axis++)
			normal.m_values[axis] = SanitizeFloatBits(bspPlane.m_normal[axis].GetBits());
		const uint32_t distBits = SanitizeFloatBits(bspPlane.m_dist.GetBits());

		return IndexPlane(outPlaneIndex, planeLookup, normalLookup, normal, distBits);
	}

	static rkit::Result EmitFace(bool &outEmitted, BSPGeometryCluster &geoCluster, BSPGeometryClusterLookups &lookups,
		rkit::HashMap<BSPVec3_t, size_t> &normalLookup,
		size_t originalFaceIndex, const BSPFaceStats &stats, size_t faceModelIndex,
		size_t uniqueTexIndex, const BSPDataCollection &bsp, rkit::ConstSpan<rkit::Pair<uint16_t, uint16_t>> atlasDimensions)
	{
		const float rcp16 = 0.0625f;

		const BSPFace &face = bsp.m_faces[originalFaceIndex];

		const uint16_t planeIndex = face.m_plane.Get();
		const uint16_t numFaceEdges = face.m_numEdges.Get();
		const uint32_t firstFaceEdge = face.m_firstEdge.Get();

		if (firstFaceEdge > bsp.m_faceEdges.Count() || (bsp.m_faceEdges.Count()) - firstFaceEdge < numFaceEdges)
		{
			rkit::log::Error("Invalid face edge range");
			return rkit::ResultCode::kDataError;
		}

		if (numFaceEdges < 3)
		{
			rkit::log::Error("Invalid face");
			return rkit::ResultCode::kDataError;
		}

		rkit::Vector<BSPGeometryVertex_t> windingVerts;
		RKIT_CHECK(windingVerts.Resize(numFaceEdges));

		float rcpLightmapDimensions[2] = {0.f, 0.f};
		if (stats.m_atlasIndex.IsSet())
		{
			const rkit::Pair<uint16_t, uint16_t> &lmDimensions16 = atlasDimensions[stats.m_atlasIndex.Get()];
			rcpLightmapDimensions[0] = 1.0f / static_cast<float>(lmDimensions16.GetAt<0>());
			rcpLightmapDimensions[1] = 1.0f / static_cast<float>(lmDimensions16.GetAt<1>());
		}

		BSPVec3_t normal;
		for (int axis = 0; axis < 3; axis++)
		{
			const uint16_t planeIndex = face.m_plane.Get();
			if (planeIndex >= bsp.m_planes.Count())
			{
				rkit::log::Error("Invalid plane index");
				return rkit::ResultCode::kDataError;
			}

			const BSPPlane &plane = bsp.m_planes[planeIndex];

			normal.m_values[axis] = SanitizeFloatBits(plane.m_normal[axis].GetBits());
		}

		uint32_t normalIndex = 0;
		RKIT_CHECK(IndexNormal(normalIndex, normalLookup, normal));

		for (size_t i = 0; i < numFaceEdges; i++)
		{
			U32Cluster<2> lmUVBits = {};
			U32Cluster<2> texUVBits = {};
			U32Cluster<3> xyzBits = {};

			int32_t edgeIndex = bsp.m_faceEdges[firstFaceEdge + i].Get();

			if (edgeIndex == std::numeric_limits<int32_t>::min())
			{
				rkit::log::Error("Edge index is invalid");
				return rkit::ResultCode::kDataError;
			}

			uint8_t edgeVertIndex = 0;

			if (edgeIndex >= 0)
				edgeVertIndex = 0;
			else
			{
				edgeIndex = -edgeIndex;
				edgeVertIndex = 1;
			}

			if (static_cast<uint32_t>(edgeIndex) >= bsp.m_edges.Count())
			{
				rkit::log::Error("Edge index is invalid");
				return rkit::ResultCode::kDataError;
			}

			const BSPEdge &edge = bsp.m_edges[edgeIndex];
			const uint16_t vertIndex = edge.m_verts[edgeVertIndex].Get();

			if (vertIndex >= bsp.m_verts.Count())
			{
				rkit::log::Error("Vert index is invalid");
				return rkit::ResultCode::kDataError;
			}

			const BSPVertex &vert = bsp.m_verts[vertIndex];

			xyzBits.m_values[0] = SanitizeFloatBits(vert.x.GetBits());
			xyzBits.m_values[1] = SanitizeFloatBits(vert.y.GetBits());
			xyzBits.m_values[2] = SanitizeFloatBits(vert.z.GetBits());

			const uint16_t texInfoIndex = face.m_texture.Get();

			if (texInfoIndex >= bsp.m_texInfos.Count())
			{
				rkit::log::Error("Tex info index is invalid");
				return rkit::ResultCode::kDataError;
			}

			const BSPTexInfo &texInfo = bsp.m_texInfos[texInfoIndex];

			const float xyz[3] =
			{
				rkit::math::SoftFloat32::FromBits(xyzBits.m_values[0]).ToSingle(),
				rkit::math::SoftFloat32::FromBits(xyzBits.m_values[1]).ToSingle(),
				rkit::math::SoftFloat32::FromBits(xyzBits.m_values[2]).ToSingle(),
			};

			for (int uvAxis = 0; uvAxis < 2; uvAxis++)
			{
				float coord = rkit::math::SoftFloat32::FromBits(texInfo.m_uvVectors[uvAxis][3].GetBits()).ToSingle();

				for (int xyzAxis = 0; xyzAxis < 3; xyzAxis++)
					coord += xyz[xyzAxis] * rkit::math::SoftFloat32::FromBits(texInfo.m_uvVectors[uvAxis][xyzAxis].GetBits()).ToSingle();

				texUVBits.m_values[uvAxis] = SanitizeFloatBits(rkit::math::SoftFloat32(coord).ToBits());

				if (stats.m_atlasIndex.IsSet())
				{
					const float lmPixelCoord = (coord * rcp16) - stats.m_lightmapUVMin[uvAxis];
					const float lmTextureCoord = (lmPixelCoord + 0.5f) * rcpLightmapDimensions[uvAxis];

					lmUVBits.m_values[uvAxis] = SanitizeFloatBits(rkit::math::SoftFloat32(lmTextureCoord).ToBits());
				}
			}

			BSPGeometryVertex_t bspVert;
			bspVert.m_values[0] = xyzBits.m_values[0];
			bspVert.m_values[1] = xyzBits.m_values[1];
			bspVert.m_values[2] = xyzBits.m_values[2];
			bspVert.m_values[3] = texUVBits.m_values[0];
			bspVert.m_values[4] = texUVBits.m_values[1];
			bspVert.m_values[5] = lmUVBits.m_values[0];
			bspVert.m_values[6] = lmUVBits.m_values[1];
			bspVert.m_values[7] = static_cast<uint32_t>(normalIndex);

			windingVerts[i] = bspVert;
		}

		rkit::Vector<rkit::StaticArray<BSPGeometryVertex_t, 3>> tris;
		RKIT_CHECK(OptimizeWinding(windingVerts.ToSpan().ToConstRefISpan(), tris, 0));

		size_t oldVertCount = geoCluster.m_verts.Count();

		// Scan unique verts.  This must be the exact same order as emit.
		for (const rkit::StaticArray<BSPGeometryVertex_t, 3> &tri : tris)
		{
			for (int pointIndex = 0; pointIndex < 3; pointIndex++)
			{
				const BSPGeometryVertex_t &vert = tri[pointIndex];

				rkit::HashMap<BSPGeometryVertex_t, size_t>::ConstIterator_t vertIt = lookups.m_vertLookup.Find(vert);
				if (vertIt == lookups.m_vertLookup.end())
				{
					if (lookups.m_vertLookup.Count() == BSPGeometryCluster::kMaxVerts)
					{
						outEmitted = false;
						return rkit::ResultCode::kOK;
					}

					RKIT_CHECK(lookups.m_vertLookup.Set(vert, lookups.m_vertLookup.Count()));
				}
			}
		}

		outEmitted = true;

		const size_t firstTri = geoCluster.m_indexes.Count() / 3u;

		// This must be the exact same order as the scan above.
		for (const rkit::StaticArray<BSPGeometryVertex_t, 3> &tri : tris)
		{
			rkit::StaticArray<uint16_t, 3> sortedIndexes;

			for (int pointIndex = 0; pointIndex < 3; pointIndex++)
			{
				const BSPGeometryVertex_t &vert = tri[pointIndex];

				rkit::HashMap<BSPGeometryVertex_t, size_t>::ConstIterator_t vertIt = lookups.m_vertLookup.Find(vert);
				RKIT_ASSERT(vertIt != lookups.m_vertLookup.end());

				const size_t vertIndex = vertIt.Value();

				if (vertIndex == geoCluster.m_verts.Count())
				{
					RKIT_CHECK(geoCluster.m_verts.Append(vertIt.Key()));
				}

				sortedIndexes[pointIndex] = static_cast<uint16_t>(vertIndex);
			}

			// Keep the lowest index first
			while (sortedIndexes[0] > sortedIndexes[1] || sortedIndexes[0] > sortedIndexes[2])
			{
				rkit::Swap(sortedIndexes[2], sortedIndexes[1]);
				rkit::Swap(sortedIndexes[1], sortedIndexes[0]);
			}

			RKIT_CHECK(geoCluster.m_indexes.Append(sortedIndexes.ToSpan()));
		}

		RKIT_ASSERT(geoCluster.m_verts.Count() == lookups.m_vertLookup.Count());

		BSPRenderableFace rface = {};
		rface.m_atlasIndex = std::numeric_limits<uint32_t>::max();
		if (stats.m_atlasIndex.IsSet())
			rface.m_atlasIndex = static_cast<uint32_t>(stats.m_atlasIndex.Get());
		rface.m_firstTri = static_cast<uint32_t>(firstTri);
		rface.m_numTris = static_cast<uint32_t>(geoCluster.m_indexes.Count() / 3u - firstTri);
		rface.m_materialIndex = static_cast<uint32_t>(uniqueTexIndex);
		rface.m_modelIndex = static_cast<uint32_t>(faceModelIndex);
		rface.m_originalFaceIndex = originalFaceIndex;
		rface.m_mergeWidth = 1;

		RKIT_CHECK(geoCluster.m_faces.Append(rface));

		return rkit::ResultCode::kOK;
	}

	rkit::Result BSPMapCompilerBase2::BuildGeometry(BSPOutput &bspOutput,
		const BSPDataCollection &bsp, rkit::ConstSpan<BSPFaceStats> stats,
		rkit::Span<size_t> faceModelIndex, rkit::ConstSpan<size_t> texInfoToUniqueTexIndex,
		rkit::ConstSpan<rkit::Pair<uint16_t, uint16_t>> lightmapDimensions)
	{
		rkit::Vector<BSPGeometryCluster> geoClusters;
		rkit::Vector<size_t> leafOrder;
		rkit::Vector<size_t> nodeOrder;
		rkit::Vector<uint16_t> faceOrder;
		rkit::Vector<size_t> leafModel;
		rkit::Vector<size_t> brushOrder;

		rkit::HashMap<BSPVec3_t, size_t> normalLookup;
		rkit::HashMap<IndexedPlane_t, size_t> planeLookup;

		rkit::BoolVector leafUsedBits;
		rkit::BoolVector nodeUsedBits;
		rkit::BoolVector faceUsedBits;
		rkit::BoolVector brushUsedBits;

		RKIT_CHECK(leafUsedBits.Resize(bsp.m_leafs.Count()));
		RKIT_CHECK(nodeUsedBits.Resize(bsp.m_nodes.Count()));
		RKIT_CHECK(faceUsedBits.Resize(bsp.m_faces.Count()));
		RKIT_CHECK(brushUsedBits.Resize(bsp.m_brushes.Count()));

		RKIT_CHECK(leafModel.Resize(bsp.m_leafs.Count()));

		// Index axial planes first
		for (uint32_t axis = 0; axis < 3; axis++)
		{
			BSPVec3_t normal = {};
			for (int subAxis = 0; subAxis < 3; subAxis++)
				normal.m_values[subAxis] = 0;

			normal.m_values[axis] = rkit::math::SoftFloat32(1.0f).ToBits();

			uint32_t normalIndex = 0;
			RKIT_CHECK(IndexNormal(normalIndex, normalLookup, normal));

			RKIT_ASSERT(normalIndex == (axis << 1));
		}

		// Gather BSP nodes
		for (size_t modelIndex = 0; modelIndex < bsp.m_models.Count(); modelIndex++)
		{
			const BSPModel &model = bsp.m_models[modelIndex];

			const size_t firstLeafInLeafOrder = leafOrder.Count();

			rkit::Vector<int32_t> backNodeStack;

			int32_t headNode = model.m_headNode.Get();

			if (headNode < 0)
			{
				RKIT_CHECK(ScanLeaf(bsp, leafOrder, leafUsedBits, headNode));
			}
			else
			{
				size_t currentNode = static_cast<size_t>(headNode);
				for (;;)
				{
					bool shouldPopStack = false;

					if (currentNode >= bsp.m_nodes.Count())
					{
						rkit::log::Error("Node out of range");
						return rkit::ResultCode::kDataError;
					}

					RKIT_CHECK(nodeOrder.Append(currentNode));

					if (nodeUsedBits[currentNode])
					{
						rkit::log::Error("Node was in the tree multiple times");
						return rkit::ResultCode::kDataError;
					}

					nodeUsedBits.Set(currentNode, true);

					const BSPNode &node = bsp.m_nodes[currentNode];

					const uint32_t inPlaneIndex = node.m_plane.Get();
					if (inPlaneIndex >= bsp.m_planes.Count())
					{
						rkit::log::Error("Plane index out of range");
						return rkit::ResultCode::kDataError;
					}

					uint32_t outPlaneIndex = 0;
					RKIT_CHECK(IndexPlane(outPlaneIndex, planeLookup, normalLookup, bsp.m_planes[inPlaneIndex]));

					int32_t backNode = node.m_back.Get();
					int32_t frontNode = node.m_front.Get();

					if (outPlaneIndex & 1)
						rkit::Swap(backNode, frontNode);

					if (frontNode < 0)
					{
						RKIT_CHECK(ScanLeaf(bsp, leafOrder, leafUsedBits, frontNode));

						if (backNode < 0)
						{
							RKIT_CHECK(ScanLeaf(bsp, leafOrder, leafUsedBits, backNode));

							bool done = (backNodeStack.Count() == 0);

							while (backNodeStack.Count() > 0)
							{
								const size_t newSize = backNodeStack.Count() - 1;
								const int32_t nextBackNode = backNodeStack[newSize];
								backNodeStack.ShrinkToSize(newSize);

								done = (backNodeStack.Count() == 0);

								if (nextBackNode < 0)
								{
									RKIT_CHECK(ScanLeaf(bsp, leafOrder, leafUsedBits, nextBackNode));
								}
								else
								{
									currentNode = static_cast<size_t>(nextBackNode);
									done = false;
									break;
								}
							}

							if (done)
								break;
						}
						else
						{
							currentNode = static_cast<size_t>(backNode);
						}
					}
					else
					{
						RKIT_CHECK(backNodeStack.Append(backNode));
						currentNode = static_cast<size_t>(frontNode);
					}
				}
			}

			const size_t lastLeafInLeafOrderExcl = leafOrder.Count();

			for (size_t leafOrderIndex = firstLeafInLeafOrder; leafOrderIndex < lastLeafInLeafOrderExcl; leafOrderIndex++)
				leafModel[leafOrder[leafOrderIndex]] = modelIndex;
		}

		// Gather brushes
		for (size_t leafIndex : leafOrder)
		{
			const BSPLeaf &leaf = bsp.m_leafs[leafIndex];

			const uint16_t firstLeafBrush = leaf.m_firstLeafBrush.Get();
			const uint16_t numLeafBrushes = leaf.m_numLeafBrushes.Get();

			if (firstLeafBrush > bsp.m_leafBrushes.Count() || bsp.m_leafBrushes.Count() - firstLeafBrush < numLeafBrushes)
			{
				rkit::log::Error("Leafbrush count out of range");
				return rkit::ResultCode::kDataError;
			}

			for (uint16_t lbi = 0; lbi < numLeafBrushes; lbi++)
			{
				const uint16_t brushIndex = bsp.m_leafBrushes[lbi + firstLeafBrush].Get();

				if (brushIndex > bsp.m_brushes.Count())
				{
					rkit::log::Error("Brush index out of range");
					return rkit::ResultCode::kDataError;
				}

				if (!brushUsedBits[brushIndex])
				{
					brushUsedBits.Set(brushIndex, true);
					RKIT_CHECK(brushOrder.Append(brushIndex));
				}
			}
		}

		// Gather brush planes
		for (size_t brushIndex : brushOrder)
		{
			const BSPBrush &brush = bsp.m_brushes[brushIndex];

			const uint16_t firstBrushSide = brush.m_firstBrushSide.Get();
			const uint16_t numBrushSides = brush.m_numBrushSides.Get();

			if (firstBrushSide > bsp.m_brushSides.Count() || bsp.m_brushSides.Count() - firstBrushSide < numBrushSides)
			{
				rkit::log::Error("Brushside count out of range");
				return rkit::ResultCode::kDataError;
			}

			for (uint16_t bsi = 0; bsi < numBrushSides; bsi++)
			{
				const BSPBrushSide &brushSide = bsp.m_brushSides[bsi + firstBrushSide];

				const uint16_t planeIndex = brushSide.m_plane.Get();

				if (planeIndex >= bsp.m_planes.Count())
				{
					rkit::log::Error("Plane index out of range");
					return rkit::ResultCode::kDataError;
				}

				uint32_t indexedPlane = 0;
				RKIT_CHECK(IndexPlane(indexedPlane, planeLookup, normalLookup, bsp.m_planes[planeIndex]));
			}
		}

		// CAUTION: Keyed by input face, values are output leafs (for sorting)
		rkit::Vector<rkit::Vector<size_t>> faceLeafs;

		RKIT_CHECK(faceLeafs.Resize(bsp.m_faces.Count()));

		// Gather leafs
		for (size_t outLeafIndex = 0; outLeafIndex < leafOrder.Count(); outLeafIndex++)
		{
			size_t inLeafIndex = leafOrder[outLeafIndex];

			const BSPLeaf &leaf = bsp.m_leafs[inLeafIndex];
			size_t modelIndex = leafModel[inLeafIndex];

			const uint16_t firstLeafFace = leaf.m_firstLeafFace.Get();
			const uint16_t numLeafFaces = leaf.m_numLeafFaces.Get();

			if (firstLeafFace > bsp.m_leafFaces.Count() || (bsp.m_leafFaces.Count() - firstLeafFace) < numLeafFaces)
			{
				rkit::log::Error("Invalid face list");
				return rkit::ResultCode::kDataError;
			}

			for (size_t lfi = 0; lfi < numLeafFaces; lfi++)
			{
				const uint16_t faceIndex = bsp.m_leafFaces[lfi + firstLeafFace].Get();
				if (faceIndex >= faceUsedBits.Count())
				{
					rkit::log::Error("Invalid face index");
					return rkit::ResultCode::kDataError;
				}

				RKIT_CHECK(faceLeafs[faceIndex].Append(outLeafIndex));

				if (faceUsedBits[faceIndex])
				{
					if (faceModelIndex[faceIndex] != modelIndex)
					{
						rkit::log::Error("Face used in multiple models");
						return rkit::ResultCode::kDataError;
					}
					continue;
				}

				faceUsedBits.Set(faceIndex, true);
				faceModelIndex[faceIndex] = modelIndex;

				RKIT_CHECK(faceOrder.Append(faceIndex));
			}
		}

		leafModel.Reset();

		// Sort all leaf lists
		for (rkit::Vector<size_t> &leafList : faceLeafs)
			rkit::QuickSort(leafList.begin(), leafList.end());

		rkit::QuickSort(faceOrder.begin(), faceOrder.end(), [&bsp, &faceLeafs, stats, texInfoToUniqueTexIndex, faceModelIndex](const uint16_t &fia, const uint16_t &fib)
		{
			const size_t modelIndexA = faceModelIndex[fia];
			const size_t modelIndexB = faceModelIndex[fib];

			if (modelIndexA != modelIndexB)
				return modelIndexA < modelIndexB;

			const rkit::Optional<size_t> &lmA = stats[fia].m_atlasIndex;
			const rkit::Optional<size_t> &lmB = stats[fib].m_atlasIndex;

			if (lmA.IsSet() != lmB.IsSet())
				return lmB.IsSet();

			const BSPFace &faceA = bsp.m_faces[fia];
			const BSPFace &faceB = bsp.m_faces[fib];

			const uint16_t texA = faceA.m_texture.Get();
			const uint16_t texB = faceB.m_texture.Get();

			if (texA != texB)
			{
				const size_t utexA = texInfoToUniqueTexIndex[texA];
				const size_t utexB = texInfoToUniqueTexIndex[texB];

				if (utexA != utexB)
					return utexA < utexB;
			}

			if (lmA.IsSet())
			{
				const size_t lmIndexA = lmA.Get();
				const size_t lmIndexB = lmB.Get();

				if (lmIndexA != lmIndexB)
					return lmIndexA < lmIndexB;
			}

			const rkit::Vector<size_t> &leafListA = faceLeafs[fia];
			const rkit::Vector<size_t> &leafListB = faceLeafs[fib];

			for (size_t i = 0; i < leafListA.Count(); i++)
			{
				if (i >= leafListB.Count())
					return true;

				const size_t leafA = leafListA[i];
				const size_t leafB = leafListB[i];

				if (leafA != leafB)
					return leafA < leafB;
			}

			if (leafListB.Count() > leafListA.Count())
				return false;

			return (&fia) < (&fib);
		});

		BSPGeometryClusterLookups lookups;
		for (size_t faceIndex : faceOrder)
		{
			const BSPFace &face = bsp.m_faces[faceIndex];

			bool emittedIntoLastCluster = false;
			if (geoClusters.Count() > 0)
			{
				RKIT_CHECK(EmitFace(emittedIntoLastCluster, geoClusters[geoClusters.Count() - 1], lookups, normalLookup, faceIndex, stats[faceIndex],
					faceModelIndex[faceIndex], texInfoToUniqueTexIndex[face.m_texture.Get()], bsp, lightmapDimensions));
			}

			if (!emittedIntoLastCluster)
			{
				lookups.Clear();
				RKIT_CHECK(geoClusters.Append(BSPGeometryCluster()));

				RKIT_CHECK(EmitFace(emittedIntoLastCluster, geoClusters[geoClusters.Count() - 1], lookups, normalLookup, faceIndex, stats[faceIndex],
					faceModelIndex[faceIndex], texInfoToUniqueTexIndex[face.m_texture.Get()], bsp, lightmapDimensions));

				if (!emittedIntoLastCluster)
				{
					rkit::log::Error("Failed to emit face");
					return rkit::ResultCode::kInternalError;
				}
			}
		}

		// Generate merges

		// Key and value are both input face indexes
		rkit::Vector<size_t> inFaceToOutFace;
		RKIT_CHECK(inFaceToOutFace.Resize(bsp.m_faces.Count()));

		{
			size_t outFaceIndex = 0;

			for (BSPGeometryCluster &geometryCluster : geoClusters)
			{
				const size_t numFaces = geometryCluster.m_faces.Count();

				if (numFaces >= 2)
				{
					BSPRenderableFace *baseFace = &geometryCluster.m_faces[0];

					for (size_t gcfi = 1; gcfi < numFaces; gcfi++)
					{
						BSPRenderableFace *candidateFace = &geometryCluster.m_faces[gcfi];

						if (baseFace->m_atlasIndex == candidateFace->m_atlasIndex
							&& baseFace->m_materialIndex == candidateFace->m_materialIndex
							&& baseFace->m_modelIndex == candidateFace->m_modelIndex)
						{
							const rkit::Vector<size_t> &baseFaceLeafs = faceLeafs[baseFace->m_originalFaceIndex];
							const rkit::Vector<size_t> &candidateFaceLeafs = faceLeafs[candidateFace->m_originalFaceIndex];

							if (rkit::CompareSpansEqual(baseFaceLeafs.ToSpan(), candidateFaceLeafs.ToSpan()))
								baseFace->m_mergeWidth++;
							else
								baseFace = candidateFace;
						}
					}
				}

				// Finalize mappings and collapse merges
				for (size_t gcfi = 0; gcfi < geometryCluster.m_faces.Count(); gcfi++)
				{
					inFaceToOutFace[geometryCluster.m_faces[gcfi].m_originalFaceIndex] = outFaceIndex;

					for (;;)
					{
						BSPRenderableFace &rface = geometryCluster.m_faces[gcfi];

						if (rface.m_mergeWidth <= 1)
							break;

						const BSPRenderableFace &faceToDelete = geometryCluster.m_faces[gcfi + 1];

						rface.m_mergeWidth--;
						rface.m_numTris += faceToDelete.m_numTris;

						inFaceToOutFace[faceToDelete.m_originalFaceIndex] = outFaceIndex;

						geometryCluster.m_faces.RemoveRange(gcfi + 1, 1);
					}

					outFaceIndex++;
				}
			}
		}

		// Generate reorderings
		rkit::Vector<size_t> inNodeToOutNode;
		RKIT_CHECK(inNodeToOutNode.Resize(bsp.m_nodes.Count()));

		rkit::Vector<size_t> inLeafToOutLeaf;
		RKIT_CHECK(inLeafToOutLeaf.Resize(bsp.m_leafs.Count()));

		rkit::Vector<size_t> inBrushToOutBrush;
		RKIT_CHECK(inBrushToOutBrush.Resize(bsp.m_brushes.Count()));

		for (size_t i = 0; i < nodeOrder.Count(); i++)
			inNodeToOutNode[nodeOrder[i]] = i;

		for (size_t i = 0; i < leafOrder.Count(); i++)
			inLeafToOutLeaf[leafOrder[i]] = i;

		for (size_t i = 0; i < brushOrder.Count(); i++)
			inBrushToOutBrush[brushOrder[i]] = i;

		// Emit all of the geometry
		RKIT_CHECK(bspOutput.m_treeNodes.Resize(nodeOrder.Count()));
		RKIT_CHECK(bspOutput.m_treeNodeSplitBits.Resize((nodeOrder.Count() + 3u) / 4u));
		RKIT_CHECK(bspOutput.m_leafs.Resize(leafOrder.Count()));
		RKIT_CHECK(bspOutput.m_planes.Resize(planeLookup.Count()));
		RKIT_CHECK(bspOutput.m_normals.Resize(normalLookup.Count()));
		RKIT_CHECK(bspOutput.m_brushes.Resize(brushOrder.Count()));

		// Emit normals
		for (rkit::HashMapKeyValueView<BSPVec3_t, const size_t> it : normalLookup)
		{
			const BSPVec3_t &inNormal = it.Key();
			data::BSPNormal &outNormal = bspOutput.m_normals[it.Value()];

			for (int axis = 0; axis < 3; axis++)
				outNormal.m_xyz[axis] = rkit::endian::LittleFloat32_t::FromBits(inNormal.m_values[axis]);
		}

		// Emit planes
		for (rkit::HashMapKeyValueView<IndexedPlane_t, const size_t> it : planeLookup)
		{
			const IndexedPlane_t &inPlane = it.Key();
			data::BSPPlane &outPlane = bspOutput.m_planes[it.Value()];

			outPlane.m_normal = inPlane.m_values[0];
			outPlane.m_dist = rkit::endian::LittleFloat32_t::FromBits(inPlane.m_values[1]);
		}

		for (size_t outNodeIndex = 0; outNodeIndex < nodeOrder.Count(); outNodeIndex++)
		{
			size_t inNodeIndex = nodeOrder[outNodeIndex];

			const BSPNode &inNode = bsp.m_nodes[inNodeIndex];
			data::BSPTreeNode &outNode = bspOutput.m_treeNodes[outNodeIndex];

			RKIT_ASSERT(nodeUsedBits[inNodeIndex]);

			uint32_t planeIndex = 0;
			RKIT_CHECK(IndexPlane(planeIndex, planeLookup, normalLookup, bsp.m_planes[inNode.m_plane.Get()]));

			for (int axis = 0; axis < 3; axis++)
			{
				outNode.m_minBounds[axis] = inNode.m_minBounds[axis];
				outNode.m_maxBounds[axis] = inNode.m_maxBounds[axis];
			}

			outNode.m_plane = (planeIndex >> 1);

			int32_t inFrontNode = inNode.m_front.Get();
			int32_t inBackNode = inNode.m_back.Get();

			if (planeIndex & 1)
				rkit::Swap(inFrontNode, inBackNode);

			uint8_t splitBits = 0;
			if (inFrontNode >= 0)
				splitBits |= 1;
			if (inBackNode >= 0)
				splitBits |= 2;

			uint8_t subPosition = outNodeIndex % 4;
			if (subPosition == 0)
				bspOutput.m_treeNodeSplitBits[outNodeIndex / 4] = splitBits;
			else
				bspOutput.m_treeNodeSplitBits[outNodeIndex / 4] |= (splitBits << (subPosition * 2));
		}

		for (size_t outLeafIndex = 0; outLeafIndex < leafOrder.Count(); outLeafIndex++)
		{
			size_t inLeafIndex = leafOrder[outLeafIndex];

			const BSPLeaf &inLeaf = bsp.m_leafs[inLeafIndex];
			data::BSPTreeLeaf &outLeaf = bspOutput.m_leafs[outLeafIndex];

			RKIT_ASSERT(leafUsedBits[inLeafIndex]);

			outLeaf.m_contentFlags = inLeaf.m_contentFlags;
			outLeaf.m_area = inLeaf.m_area;
			outLeaf.m_cluster = inLeaf.m_cluster;
			outLeaf.m_contentFlags = inLeaf.m_contentFlags;

			for (int axis = 0; axis < 3; axis++)
			{
				outLeaf.m_minBounds[axis] = inLeaf.m_minBounds[axis];
				outLeaf.m_maxBounds[axis] = inLeaf.m_maxBounds[axis];
			}

			// Emit brushes
			const uint16_t firstLeafBrush = inLeaf.m_firstLeafBrush.Get();
			const uint16_t numLeafBrushes = inLeaf.m_numLeafBrushes.Get();

			outLeaf.m_numLeafBrushes = numLeafBrushes;

			for (uint16_t lbi = 0; lbi < numLeafBrushes; lbi++)
			{
				const uint16_t inBrushIndex = bsp.m_leafBrushes[lbi + firstLeafBrush].Get();

				RKIT_ASSERT(brushUsedBits[inBrushIndex]);

				const rkit::endian::LittleUInt16_t outBrushIndex(static_cast<uint16_t>(inBrushToOutBrush[inBrushIndex]));
				RKIT_CHECK(bspOutput.m_leafBrushes.Append(outBrushIndex));
			}

			// Emit geometry indexes
			const uint16_t firstLeafFace = inLeaf.m_firstLeafFace.Get();
			const uint16_t numLeafFaces = inLeaf.m_numLeafFaces.Get();

			rkit::Vector<size_t> realFaces;

			for (size_t lfi = 0; lfi < numLeafFaces; lfi++)
			{
				size_t leafFaceIndex = firstLeafFace + lfi;

				const uint16_t inFaceIndex = bsp.m_leafFaces[leafFaceIndex].Get();

				RKIT_ASSERT(faceUsedBits[inFaceIndex]);

				RKIT_CHECK(realFaces.Append(inFaceToOutFace[inFaceIndex]));
			}

			rkit::QuickSort(realFaces.begin(), realFaces.end());

			for (size_t rfi = 1; rfi < realFaces.Count(); )
			{
				if (realFaces[rfi] == realFaces[rfi - 1])
					realFaces.RemoveRange(rfi, 1);
				else
					rfi++;
			}

			for (size_t realFaceIndex : realFaces)
			{
				RKIT_CHECK(bspOutput.m_leafFaces.Append(rkit::endian::LittleUInt16_t(static_cast<uint16_t>(realFaceIndex))));
			}

			outLeaf.m_numLeafDrawSurfaces = static_cast<uint16_t>(realFaces.Count());
		}

		// Emit brushes
		for (size_t outBrushIndex = 0; outBrushIndex < brushOrder.Count(); outBrushIndex++)
		{
			size_t inBrushIndex = brushOrder[outBrushIndex];

			const BSPBrush &inBrush = bsp.m_brushes[inBrushIndex];
			data::BSPBrush &outBrush = bspOutput.m_brushes[outBrushIndex];

			RKIT_ASSERT(brushUsedBits[inBrushIndex]);

			outBrush.m_contents = inBrush.m_contents;
			outBrush.m_numBrushSides = inBrush.m_numBrushSides;

			const uint32_t numBrushSides = inBrush.m_numBrushSides.Get();
			const uint32_t firstBrushSide = inBrush.m_firstBrushSide.Get();

			for (size_t bsi = 0; bsi < numBrushSides; bsi++)
			{
				const BSPBrushSide &inBrushSide = bsp.m_brushSides[bsi + firstBrushSide];
				data::BSPBrushSide outBrushSide = {};

				const uint16_t texInfoIndex = inBrushSide.m_texInfo.Get();

				if (texInfoIndex == 0xffffu)
				{
					outBrushSide.m_materialFlags = 0;
					outBrushSide.m_material = static_cast<uint32_t>(-1);
				}
				else
				{
					if (texInfoIndex >= bsp.m_texInfos.Count())
					{
						rkit::log::Error("Invalid texinfo index");
						return rkit::ResultCode::kDataError;
					}

					outBrushSide.m_materialFlags = bsp.m_texInfos[texInfoIndex].m_flags;
					outBrushSide.m_material = static_cast<uint32_t>(texInfoToUniqueTexIndex[texInfoIndex]);
				}

				uint32_t planeIndex = 0;
				RKIT_CHECK(IndexPlane(planeIndex, planeLookup, normalLookup, bsp.m_planes[inBrushSide.m_plane.Get()]));

				outBrushSide.m_plane = planeIndex;

				RKIT_CHECK(bspOutput.m_brushSides.Append(outBrushSide));
			}
		}

		rkit::Vector<rkit::Vector<data::BSPModelDrawCluster>> modelDrawClusters;

		RKIT_CHECK(modelDrawClusters.Resize(bsp.m_models.Count()));

		// Emit geometry
		for (size_t geoClusterIndex = 0; geoClusterIndex < geoClusters.Count(); geoClusterIndex++)
		{
			const BSPGeometryCluster &geoCluster = geoClusters[geoClusterIndex];

			data::BSPDrawMaterialGroup *matGroup = nullptr;
			data::BSPDrawLightmapGroup *lmGroup = nullptr;
			data::BSPDrawModelGroup *modelGroup = nullptr;

			data::BSPDrawCluster drawCluster = {};

			for (const BSPRenderableFace &face : geoCluster.m_faces)
			{
				if (modelGroup == nullptr || face.m_modelIndex != modelGroup->m_modelIndex.Get())
				{
					{
						data::BSPModelDrawCluster modelDrawCluster = {};
						modelDrawCluster.m_drawClusterIndex = static_cast<uint32_t>(geoClusterIndex);
						modelDrawCluster.m_modelGroupIndex = drawCluster.m_numModelGroups;

						RKIT_CHECK(modelDrawClusters[face.m_modelIndex].Append(modelDrawCluster));
					}


					RKIT_CHECK(bspOutput.m_modelGroups.Append(data::BSPDrawModelGroup()));
					modelGroup = &bspOutput.m_modelGroups[bspOutput.m_modelGroups.Count() - 1];
					modelGroup->m_modelIndex = face.m_modelIndex;

					drawCluster.m_numModelGroups = drawCluster.m_numModelGroups.Get() + 1;

					lmGroup = nullptr;
					matGroup = nullptr;
				}

				if (lmGroup == nullptr || face.m_atlasIndex != lmGroup->m_atlasIndex.Get())
				{
					RKIT_CHECK(bspOutput.m_lightmapGroups.Append(data::BSPDrawLightmapGroup()));
					lmGroup = &bspOutput.m_lightmapGroups[bspOutput.m_lightmapGroups.Count() - 1];
					lmGroup->m_atlasIndex = face.m_atlasIndex;

					modelGroup->m_numLightmapGroups = modelGroup->m_numLightmapGroups.Get() + 1;

					matGroup = nullptr;
				}

				if (matGroup == nullptr || face.m_materialIndex != matGroup->m_materialIndex.Get())
				{
					RKIT_CHECK(bspOutput.m_materialGroups.Append(data::BSPDrawMaterialGroup()));
					matGroup = &bspOutput.m_materialGroups[bspOutput.m_materialGroups.Count() - 1];
					matGroup->m_materialIndex = face.m_materialIndex;

					lmGroup->m_numMaterialGroups = lmGroup->m_numMaterialGroups.Get() + 1;
				}

				matGroup->m_numFaces = matGroup->m_numFaces.Get() + 1;

				data::BSPDrawFace drawFace = {};
				drawFace.m_numTris = face.m_numTris;

				RKIT_CHECK(bspOutput.m_drawFaces.Append(drawFace));

				for (uint16_t index : geoCluster.m_indexes.ToSpan().SubSpan(face.m_firstTri * 3, face.m_numTris * 3))
				{
					RKIT_CHECK(bspOutput.m_triIndexes.Append(rkit::endian::LittleUInt16_t(index)));
				}
			}

			drawCluster.m_numVerts = static_cast<uint16_t>(geoCluster.m_verts.Count());

			for (const BSPGeometryVertex_t &bspVert : geoCluster.m_verts)
			{
				data::BSPDrawVertex outVert = {};
				outVert.m_xyz[0] = rkit::endian::LittleFloat32_t::FromBits(bspVert.m_values[0]);
				outVert.m_xyz[1] = rkit::endian::LittleFloat32_t::FromBits(bspVert.m_values[1]);
				outVert.m_xyz[2] = rkit::endian::LittleFloat32_t::FromBits(bspVert.m_values[2]);
				outVert.m_uv[0] = rkit::endian::LittleFloat32_t::FromBits(bspVert.m_values[3]);
				outVert.m_uv[1] = rkit::endian::LittleFloat32_t::FromBits(bspVert.m_values[4]);
				outVert.m_lightUV[2] = rkit::endian::LittleFloat32_t::FromBits(bspVert.m_values[5]);
				outVert.m_lightUV[0] = rkit::endian::LittleFloat32_t::FromBits(bspVert.m_values[6]);
				outVert.m_normal = bspVert.m_values[7];

				RKIT_CHECK(bspOutput.m_drawVerts.Append(outVert));
			}

			RKIT_CHECK(bspOutput.m_drawClusters.Append(drawCluster));
		}

		RKIT_CHECK(bspOutput.m_models.Resize(bsp.m_models.Count()));

		for (size_t modelIndex = 0; modelIndex < bsp.m_models.Count(); modelIndex++)
		{
			const BSPModel &inModel = bsp.m_models[modelIndex];
			data::BSPModel &outModel = bspOutput.m_models[modelIndex];

			const rkit::Vector<data::BSPModelDrawCluster> &drawClusters = modelDrawClusters[modelIndex];

			for (int axis = 0; axis < 3; axis++)
			{
				outModel.m_maxs[axis] = inModel.m_maxs[axis];
				outModel.m_mins[axis] = inModel.m_mins[axis];
				outModel.m_origin[axis] = inModel.m_origin[axis];
			}

			outModel.m_numModelDrawClusters = static_cast<uint32_t>(drawClusters.Count());
			outModel.m_rootIsLeaf = (inModel.m_headNode.Get() < 0) ? 1 : 0;

			RKIT_CHECK(bspOutput.m_modelDrawClusters.Append(drawClusters.ToSpan()));
		}

		// Delta compress tri indexes
		{
			uint16_t prevIndex = bspOutput.m_triIndexes[0].Get();
			for (size_t i = 1; i < bspOutput.m_triIndexes.Count(); i++)
			{
				uint16_t thisIndex = bspOutput.m_triIndexes[i].Get();
				bspOutput.m_triIndexes[i] = static_cast<uint16_t>((thisIndex - prevIndex) & 0xffff);
				prevIndex = thisIndex;
			}
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result BSPMapCompilerBase2::BuildMaterials(BSPOutput &bspOutput, rkit::ConstSpan<rkit::CIPath> paths)
	{
		RKIT_CHECK(bspOutput.m_materials.Resize(paths.Count()));

		for (size_t materialIndex = 0; materialIndex < paths.Count(); materialIndex++)
		{
			data::BSPMaterial &bspMat = bspOutput.m_materials[materialIndex];
			bspMat = {};

			const rkit::CIPath &path = paths[materialIndex];

			if (path.Length() > 256 || path.Length() == 0)
			{
				rkit::log::Error("Material name is invalid");
				return rkit::ResultCode::kDataError;
			}

			bspMat.m_nameLengthMinusOne = static_cast<uint8_t>(path.Length() - 1);

			RKIT_CHECK(bspOutput.m_materialNameChars.Append(path.ToString().ToSpan()));
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result BSPMapCompilerBase2::LoadBSPData(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, BSPDataCollection &bsp, rkit::Vector<LumpLoader> &loaders)
	{
		const rkit::StringView identifier = depsNode->GetIdentifier();

		rkit::CIPath ciPath;
		RKIT_CHECK(ciPath.Set(identifier));

		rkit::UniquePtr<rkit::ISeekableReadStream> bspStream;
		RKIT_CHECK(feedback->TryOpenInput(rkit::buildsystem::BuildFileLocation::kSourceDir, ciPath, bspStream));

		if (!bspStream.IsValid())
		{
			rkit::log::ErrorFmt("Failed to open '{}'", identifier.GetChars());
			return rkit::ResultCode::kFileOpenError;
		}

		BSPHeader bspHeader;
		RKIT_CHECK(bspStream->ReadAll(&bspHeader, sizeof(bspHeader)));

		rkit::QuickSort(loaders.begin(), loaders.end(), LumpLoaderSortPred(bspHeader));

		for (const LumpLoader &loader : loaders)
		{
			RKIT_CHECK(loader.m_loadLump(loader.m_arr, *bspStream, bspHeader, loader.m_lumpIndex));
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result BSPMapCompilerBase2::FormatLightmapPath(rkit::String &path, const rkit::StringView &identifier, size_t lightmapIndex)
	{
		return path.Format("ax_bsp/{}_lm{}.dds", identifier.GetChars(), lightmapIndex);
	}

	rkit::Result BSPMapCompilerBase2::FormatModelPath(rkit::String &path, const rkit::StringView &identifier)
	{
		return path.Format("ax_bsp/{}.bspmodel", identifier.GetChars());
	}

	rkit::Result BSPMapCompilerBase2::FormatFaceStatsPath(rkit::String &path, const rkit::StringView &identifier)
	{
		return path.Format("ax_bsp/{}.facestats", identifier.GetChars());
	}

	rkit::Result BSPMapCompilerBase2::WriteBSPModel(const BSPOutput &bspOutput, rkit::IWriteStream &outStream)
	{
		const VectorSerializer serializers[] =
		{
			CreateVectorSerializer(bspOutput.m_materials),
			CreateVectorSerializer(bspOutput.m_materialNameChars),
			CreateVectorSerializer(bspOutput.m_lightmaps),
			CreateVectorSerializer(bspOutput.m_normals),
			CreateVectorSerializer(bspOutput.m_planes),
			CreateVectorSerializer(bspOutput.m_treeNodes),
			CreateVectorSerializer(bspOutput.m_treeNodeSplitBits),
			CreateVectorSerializer(bspOutput.m_leafs),
			CreateVectorSerializer(bspOutput.m_brushes),
			CreateVectorSerializer(bspOutput.m_brushSides),
			CreateVectorSerializer(bspOutput.m_drawVerts),
			CreateVectorSerializer(bspOutput.m_drawFaces),
			CreateVectorSerializer(bspOutput.m_materialGroups),
			CreateVectorSerializer(bspOutput.m_lightmapGroups),
			CreateVectorSerializer(bspOutput.m_modelGroups),
			CreateVectorSerializer(bspOutput.m_drawClusters),
			CreateVectorSerializer(bspOutput.m_models),
			CreateVectorSerializer(bspOutput.m_modelDrawClusters),
			CreateVectorSerializer(bspOutput.m_leafBrushes),
			CreateVectorSerializer(bspOutput.m_leafFaces),
			CreateVectorSerializer(bspOutput.m_triIndexes),
		};

		const size_t kNumSerializers = sizeof(serializers) / sizeof(serializers[0]);

		data::BSPFile bspFile = {};
		bspFile.m_fourCC = data::BSPFile::kFourCC;
		bspFile.m_version = data::BSPFile::kVersion;

		rkit::endian::LittleUInt32_t counts[kNumSerializers] = {};

		for (size_t i = 0; i < kNumSerializers; i++)
		{
			const VectorSerializer &serializer = serializers[i];

			const size_t count = serializer.m_getSizeCallback(serializer.m_vectorPtr);

			if (count >= std::numeric_limits<uint32_t>::max())
			{
				rkit::log::Error("Numeric overflow");
				return rkit::ResultCode::kIntegerOverflow;
			}

			counts[i] = static_cast<uint32_t>(count);
		}

		RKIT_CHECK(outStream.WriteAll(&bspFile, sizeof(bspFile)));
		RKIT_CHECK(outStream.WriteAll(counts, sizeof(counts)));

		for (const VectorSerializer &serializer : serializers)
		{
			RKIT_CHECK(serializer.m_writerCallback(serializer.m_vectorPtr, outStream));
		}

		return rkit::ResultCode::kOK;
	}

	template<class T>
	BSPMapCompiler::VectorSerializer BSPMapCompilerBase2::CreateVectorSerializer(const rkit::Vector<T> &arr)
	{
		VectorSerializer result = {};
		result.m_vectorPtr = &arr;
		result.m_getSizeCallback = GetVectorSizeCB<T>;
		result.m_writerCallback = WriteVectorCB<T>;

		return result;
	}

	template<class T>
	rkit::Result BSPMapCompilerBase2::WriteVectorCB(const void *vectorPtr, rkit::IWriteStream &stream)
	{
		const rkit::Vector<T> &vec = *static_cast<const rkit::Vector<T> *>(vectorPtr);
		return stream.WriteAll(vec.GetBuffer(), vec.Count() * sizeof(T));
	}

	template<class T>
	size_t BSPMapCompilerBase2::GetVectorSizeCB(const void *vectorPtr)
	{
		const rkit::Vector<T> &vec = *static_cast<const rkit::Vector<T> *>(vectorPtr);
		return vec.Count();
	}

	rkit::Result BSPMapCompiler::RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		BSPDataCollection bsp;

		rkit::Vector<LumpLoader> loaders;
		RKIT_CHECK(loaders.Append(LumpLoader(BSPLumpIndex::kModels, bsp.m_models)));
		RKIT_CHECK(loaders.Append(LumpLoader(BSPLumpIndex::kNodes, bsp.m_nodes)));
		RKIT_CHECK(loaders.Append(LumpLoader(BSPLumpIndex::kLeafs, bsp.m_leafs)));
		RKIT_CHECK(loaders.Append(LumpLoader(BSPLumpIndex::kLeafFaces, bsp.m_leafFaces)));
		RKIT_CHECK(loaders.Append(LumpLoader(BSPLumpIndex::kLeafBrushes, bsp.m_leafBrushes)));
		RKIT_CHECK(loaders.Append(LumpLoader(BSPLumpIndex::kTexInfo, bsp.m_texInfos)));
		RKIT_CHECK(loaders.Append(LumpLoader(BSPLumpIndex::kFaceEdges, bsp.m_faceEdges)));
		RKIT_CHECK(loaders.Append(LumpLoader(BSPLumpIndex::kFaces, bsp.m_faces)));
		RKIT_CHECK(loaders.Append(LumpLoader(BSPLumpIndex::kPlanes, bsp.m_planes)));
		RKIT_CHECK(loaders.Append(LumpLoader(BSPLumpIndex::kEdges, bsp.m_edges)));
		RKIT_CHECK(loaders.Append(LumpLoader(BSPLumpIndex::kVerts, bsp.m_verts)));
		RKIT_CHECK(loaders.Append(LumpLoader(BSPLumpIndex::kLightmaps, bsp.m_lightMapData)));
		RKIT_CHECK(loaders.Append(LumpLoader(BSPLumpIndex::kBrushes, bsp.m_brushes)));
		RKIT_CHECK(loaders.Append(LumpLoader(BSPLumpIndex::kBrushSides, bsp.m_brushSides)));

		RKIT_CHECK(LoadBSPData(depsNode, feedback, bsp, loaders));

		rkit::Vector<BSPFaceStats> faceStats;
		rkit::Vector<rkit::UniquePtr<priv::LightmapTree>> lightmapTrees;
		RKIT_CHECK(LoadFaceStats(depsNode, feedback, faceStats, lightmapTrees));

		rkit::Vector<size_t> texInfoToUniqueTexture;
		rkit::Vector<rkit::CIPath> uniqueTextures;

		RKIT_CHECK(texInfoToUniqueTexture.Resize(bsp.m_texInfos.Count()));

		{
			rkit::HashMap<rkit::CIPath, size_t> textureNameToUniqueTexture;

			for (size_t texInfoIndex = 0; texInfoIndex < bsp.m_texInfos.Count(); texInfoIndex++)
			{
				const BSPTexInfo &texInfo = bsp.m_texInfos[texInfoIndex];

				char fixedName[33] = {};
				static_assert(sizeof(fixedName) == sizeof(texInfo.m_textureName) + 1, "Wrong texture name size");

				size_t nameLength = 0;
				while (nameLength < sizeof(fixedName) - 1u)
				{
					char nameChar = static_cast<uint8_t>(texInfo.m_textureName[nameLength]);
					if (nameChar == 0)
						break;

					nameChar = rkit::InvariantCharCaseAdjuster<char>::ToLower(nameChar);
					if (nameChar == '\\')
						nameChar = '/';

					fixedName[nameLength++] = nameChar;
				}

				fixedName[nameLength] = 0;

				rkit::CIPath texName;
				RKIT_CHECK(texName.Set(rkit::StringView(fixedName, nameLength)));

				const rkit::HashMapConstIterator it = textureNameToUniqueTexture.Find(texName);

				size_t uniqueTexIndex = 0;
				if (it == textureNameToUniqueTexture.end())
				{
					uniqueTexIndex = uniqueTextures.Count();

					RKIT_CHECK(textureNameToUniqueTexture.Set(texName, uniqueTexIndex));
					RKIT_CHECK(uniqueTextures.Append(texName));
				}
				else
					uniqueTexIndex = it.Value();

				texInfoToUniqueTexture[texInfoIndex] = uniqueTexIndex;
			}
		}

		rkit::Vector<rkit::Pair<uint16_t, uint16_t>> lightmapDimensions;
		rkit::Vector<rkit::data::ContentID> lightmapContentIDs;

		{
			RKIT_CHECK(lightmapDimensions.Resize(lightmapTrees.Count()));
			RKIT_CHECK(lightmapContentIDs.Resize(lightmapTrees.Count()));

			for (size_t lightmapIndex = 0; lightmapIndex < lightmapTrees.Count(); lightmapIndex++)
			{
				const priv::LightmapTree &tree = *lightmapTrees[lightmapIndex];

				for (const priv::LightmapTreeNode &node : tree.m_nodes)
				{
					if (node.m_isOccupied)
					{
						BSPFaceStats &specificFaceStats = faceStats[node.m_ident.m_faceIndex];
						specificFaceStats.m_atlasIndex = lightmapIndex;
						specificFaceStats.m_lightmapPos[0] = node.m_x;
						specificFaceStats.m_lightmapPos[1] = node.m_y;
					}
				}

				lightmapDimensions[lightmapIndex] = rkit::Pair<uint16_t, uint16_t>(tree.m_nodes[0].m_width, tree.m_nodes[0].m_height);
			}

			RKIT_CHECK(ExportLightmaps(depsNode, feedback, lightmapContentIDs, bsp, faceStats, lightmapTrees));

			lightmapTrees.Reset();
		}

		rkit::Vector<size_t> faceModelIndex;
		RKIT_CHECK(faceModelIndex.Resize(bsp.m_faces.Count()));

		BSPOutput bspOutput;
		RKIT_CHECK(BuildGeometry(bspOutput, bsp, faceStats.ToSpan(), faceModelIndex.ToSpan(), texInfoToUniqueTexture.ToSpan(), lightmapDimensions.ToSpan()));

		RKIT_CHECK(BuildMaterials(bspOutput, uniqueTextures.ToSpan()));

		rkit::String outPathStr;
		RKIT_CHECK(FormatModelPath(outPathStr, depsNode->GetIdentifier()));

		rkit::CIPath outPath;
		RKIT_CHECK(outPath.Set(outPathStr));

		rkit::UniquePtr<rkit::ISeekableReadWriteStream> outStream;
		RKIT_CHECK(feedback->OpenOutput(rkit::buildsystem::BuildFileLocation::kOutputFiles, outPath, outStream));

		bspOutput.m_lightmaps = std::move(lightmapContentIDs);

		RKIT_CHECK(WriteBSPModel(bspOutput, *outStream));

		return rkit::ResultCode::kOK;
	}

	rkit::Result BSPMapCompilerBase2::ExportLightmaps(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback,
		rkit::Vector<rkit::data::ContentID> &outContentIDs, const BSPDataCollection &bsp, const rkit::Vector<BSPFaceStats> &faceStats, const rkit::Vector<rkit::UniquePtr<priv::LightmapTree>> &lightmapTrees)
	{
		size_t numLightmaps = lightmapTrees.Count();

		for (size_t lmi = 0; lmi < numLightmaps; lmi++)
		{
			const priv::LightmapTree &lightmapTree = *lightmapTrees[lmi];

			const uint16_t atlasWidth = lightmapTree.m_nodes[0].m_width;
			const uint16_t atlasHeight = lightmapTree.m_nodes[0].m_height;

			const size_t atlasPitch = static_cast<size_t>(atlasWidth) * 4;
			const size_t atlasNumPixels = static_cast<size_t>(atlasWidth) * atlasHeight;

			rkit::Vector<uint8_t> atlasBytes;
			RKIT_CHECK(atlasBytes.Resize(atlasNumPixels * 4));

			// Clear the lightmap data
			memset(atlasBytes.GetBuffer(), 255, atlasBytes.Count());
			for (size_t px = 0; px < atlasNumPixels; px++)
				atlasBytes[px * 4 + 1] = 0;

			// Fill it up
			for (const priv::LightmapTreeNode &treeNode : lightmapTree.m_nodes)
			{
				if (!treeNode.m_isOccupied)
					continue;

				const size_t faceIndex = treeNode.m_ident.m_faceIndex;
				const BSPFace &face = bsp.m_faces[faceIndex];
				const uint8_t firstUniqueStyle = treeNode.m_ident.m_uniqueStyleIndex;

				const BSPFaceStats &stats = faceStats[faceIndex];

#if ANOX_CLUSTER_LIGHTMAPS
				const uint8_t numUniqueStylesToCopy = stats.m_numUniqueStyles;
#else
				const uint8_t numUniqueStylesToCopy = 1;
#endif

				const uint16_t inLightmapWidth = stats.m_lightmapDimensions[0];
				const uint16_t outLightmapWidth = inLightmapWidth;
				const uint16_t inLightmapHeight = stats.m_lightmapDimensions[1];
				const uint16_t outLightmapHeight = inLightmapHeight * numUniqueStylesToCopy;
				const uint32_t inOneLightmapsSizeBytes = static_cast<uint32_t>(inLightmapHeight) * inLightmapWidth * 3;

				RKIT_ASSERT(outLightmapWidth == treeNode.m_width);
				RKIT_ASSERT(outLightmapHeight == treeNode.m_height);

				const size_t lightmapDataOffs = static_cast<uint64_t>(face.m_lightmapDataPos.Get());

				if (lightmapDataOffs > bsp.m_lightMapData.Count() || bsp.m_lightMapData.Count() - static_cast<size_t>(lightmapDataOffs) < inOneLightmapsSizeBytes * stats.m_numInputStyles)
				{
					rkit::log::Error("Lightmap data out of range");
					return rkit::ResultCode::kDataError;
				}

				for (uint8_t usi = 0; usi < numUniqueStylesToCopy; usi++)
				{
					const uint8_t style = face.m_lmStyles[usi];
					rkit::StaticArray<rkit::ConstSpan<uint8_t>, 4> inStyleLightmaps;

					uint8_t numLightmapsWithThisStyle = 0;
					for (size_t lmsi = 0; lmsi < 4; lmsi++)
					{
						if (face.m_lmStyles[lmsi] == 0xff)
							break;

						if (face.m_lmStyles[lmsi] == style)
						{
							const size_t lightmapStyleOffs = lightmapDataOffs + lmsi * inLightmapWidth * 3;
							inStyleLightmaps[numLightmapsWithThisStyle++] = bsp.m_lightMapData.ToSpan().SubSpan(lightmapStyleOffs, inOneLightmapsSizeBytes);
						}
					}

					RKIT_ASSERT(numLightmapsWithThisStyle > 0);

					for (size_t lmy = 0; lmy < inLightmapHeight; lmy++)
					{
						const rkit::Span<uint8_t> outScanline = atlasBytes.ToSpan().SubSpan((lmy + treeNode.m_y) * atlasPitch + static_cast<size_t>(treeNode.m_x) * 4, static_cast<size_t>(treeNode.m_width) * 4);

						rkit::StaticArray<rkit::ConstSpan<uint8_t>, 4> inScanlines;
						for (size_t inStyleIndex = 0; inStyleIndex < numLightmapsWithThisStyle; inStyleIndex++)
							inScanlines[inStyleIndex] = inStyleLightmaps[inStyleIndex].SubSpan(lmy * inLightmapWidth * 3, static_cast<size_t>(inLightmapWidth) * 3);

						for (size_t lmx = 0; lmx < inLightmapWidth; lmx++)
						{
							const rkit::Span<uint8_t> outTexel = outScanline.SubSpan(lmx * 4, 4);

							uint16_t rgb[3] = { 0, 0, 0 };
							for (uint8_t lmsi = 0; lmsi < numLightmapsWithThisStyle; lmsi++)
							{
								const rkit::ConstSpan<uint8_t> inTexel = inScanlines[lmsi].SubSpan(lmx * 3, 3);

								for (size_t channel = 0; channel < 3; channel++)
									rgb[channel] += inTexel[channel];
							}

							uint16_t maxMagnitude = std::max(rgb[0], std::max(rgb[1], rgb[2]));
							uint8_t alpha = 255;

							if (maxMagnitude > 255)
							{
								alpha -= (maxMagnitude - 255) / 3;
								for (size_t channel = 0; channel < 3; channel++)
									outTexel[channel] = rgb[channel] * 255u / maxMagnitude;
								outTexel[3] = 255 - ((maxMagnitude - 255) / 3);
							}
							else
							{
								for (size_t channel = 0; channel < 3; channel++)
									outTexel[channel] = static_cast<uint8_t>(rgb[channel]);
								outTexel[3] = 255;
							}
						}
					}
				}
			}

			rkit::String outPathStr;
			RKIT_CHECK(FormatLightmapPath(outPathStr, depsNode->GetIdentifier(), lmi));

			rkit::CIPath outPath;
			RKIT_CHECK(outPath.Set(outPathStr));

			const rkit::buildsystem::BuildFileLocation outLocation = rkit::buildsystem::BuildFileLocation::kIntermediateDir;

			rkit::UniquePtr<rkit::ISeekableReadWriteStream> outStream;
			RKIT_CHECK(feedback->OpenOutput(outLocation, outPath, outStream));

			rkit::data::DDSHeader ddsHeader = {};

			uint32_t ddsFlags = 0;
			ddsFlags |= rkit::data::DDSFlags::kCaps;
			ddsFlags |= rkit::data::DDSFlags::kHeight;
			ddsFlags |= rkit::data::DDSFlags::kWidth;
			ddsFlags |= rkit::data::DDSFlags::kPixelFormat;
			ddsFlags |= rkit::data::DDSFlags::kPitch;

			uint32_t pixelFormatFlags = 0;
			pixelFormatFlags |= rkit::data::DDSPixelFormatFlags::kRGB;
			pixelFormatFlags |= rkit::data::DDSPixelFormatFlags::kAlphaPixels;

			ddsHeader.m_magic = rkit::data::DDSHeader::kExpectedMagic;
			ddsHeader.m_headerSizeMinus4 = sizeof(ddsHeader) - 4;

			ddsHeader.m_ddsFlags = ddsFlags;
			ddsHeader.m_height = atlasHeight;
			ddsHeader.m_width = atlasWidth;
			ddsHeader.m_pitchOrLinearSize = static_cast<uint32_t>(atlasPitch);
			ddsHeader.m_depth = 1;
			ddsHeader.m_mipMapCount = 1;

			ddsHeader.m_pixelFormat.m_pixelFormatSize = sizeof(ddsHeader.m_pixelFormat);
			ddsHeader.m_pixelFormat.m_pixelFormatFlags = pixelFormatFlags;
			ddsHeader.m_pixelFormat.m_rgbBitCount = 32;
			ddsHeader.m_pixelFormat.m_rBitMask = 0x000000ff;
			ddsHeader.m_pixelFormat.m_gBitMask = 0x0000ff00;
			ddsHeader.m_pixelFormat.m_bBitMask = 0x00ff0000;
			ddsHeader.m_pixelFormat.m_aBitMask = 0xff000000;
			ddsHeader.m_caps = rkit::data::DDSCaps::kTexture;

			RKIT_CHECK(outStream->WriteAll(&ddsHeader, sizeof(ddsHeader)));
			RKIT_CHECK(outStream->WriteAll(atlasBytes.GetBuffer(), atlasBytes.Count()));

			outStream.Reset();

			RKIT_CHECK(feedback->IndexCAS(outLocation, outPath, outContentIDs[lmi]));
		}

		return rkit::ResultCode::kOK;
	}

	bool BSPMapCompilerBase2::LightmapFitsInNode(const priv::LightmapTreeNode &node, uint16_t width, uint16_t height)
	{
		const uint16_t nodeDimensions[] = { node.m_width, node.m_height };
		const uint16_t candidateDimensions[] = { width, height };

		const uint16_t minimumMargin = 2;

		for (int i = 0; i < 2; i++)
		{
			const uint16_t nodeDimension = nodeDimensions[i];
			const uint16_t candidateDimension = candidateDimensions[i];

			if (candidateDimension > nodeDimension)
				return false;

			if (candidateDimension < nodeDimension && (nodeDimension - candidateDimension) < minimumMargin)
				return false;
		}

		return true;
	}

	uint32_t BSPMapCompiler::GetVersion() const
	{
		return 3;
	}

	template<class TStructure>
	BSPMapCompiler::LumpLoader::LumpLoader(BSPLumpIndex lumpIndex, rkit::Vector<TStructure> &arr)
		: m_lumpIndex(lumpIndex)
		, m_arr(&arr)
		, m_loadLump(LoadLumpThunk<TStructure>)
	{
	}

	template<class TStructure>
	rkit::Result BSPMapCompiler::LumpLoader::LoadLumpThunk(void *arr, rkit::ISeekableReadStream &stream, const BSPHeader &bspHeader, BSPLumpIndex lumpIndex)
	{
		return LoadLump(*static_cast<rkit::Vector<TStructure>*>(arr), stream, bspHeader, lumpIndex);
	}


	BSPMapCompiler::LumpLoaderSortPred::LumpLoaderSortPred(const BSPHeader &bspHeader)
		: m_header(bspHeader)
	{
	}

	bool BSPMapCompiler::LumpLoaderSortPred::operator()(const LumpLoader &a, const LumpLoader &b) const
	{
		return m_header.m_lumps[static_cast<size_t>(a.m_lumpIndex)].m_offset.Get() < m_header.m_lumps[static_cast<size_t>(b.m_lumpIndex)].m_offset.Get();
	}

	rkit::Result BSPMapCompilerBase::CreateMapCompiler(rkit::UniquePtr<BSPMapCompilerBase> &outCompiler)
	{
		rkit::UniquePtr<BSPMapCompiler> bspMapCompiler;
		RKIT_CHECK(rkit::New<BSPMapCompiler>(bspMapCompiler));

		outCompiler = std::move(bspMapCompiler);

		return rkit::ResultCode::kOK;
	}

	rkit::Result BSPMapCompilerBase::CreateLightingCompiler(rkit::UniquePtr<BSPMapCompilerBase> &outCompiler)
	{
		rkit::UniquePtr<BSPLightingCompiler> bspMapCompiler;
		RKIT_CHECK(rkit::New<BSPLightingCompiler>(bspMapCompiler));

		outCompiler = std::move(bspMapCompiler);

		return rkit::ResultCode::kOK;
	}

	rkit::Result BSPMapCompilerBase::CreateGeometryCompiler(rkit::UniquePtr<BSPMapCompilerBase> &outCompiler)
	{
		rkit::UniquePtr<BSPGeometryCompiler> bspMapCompiler;
		RKIT_CHECK(rkit::New<BSPGeometryCompiler>(bspMapCompiler));

		outCompiler = std::move(bspMapCompiler);

		return rkit::ResultCode::kOK;
	}
} } // anox::buildsystem

namespace rkit
{
	template<size_t TSize>
	struct rkit::Hasher<anox::buildsystem::U32Cluster<TSize>>
		: public BinaryHasher<anox::buildsystem::U32Cluster<TSize>>
	{
	};
}
