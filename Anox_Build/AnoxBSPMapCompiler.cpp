#include "AnoxBSPMapCompiler.h"

#include "AnoxMaterialCompiler.h"

#include "anox/AnoxModule.h"

#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Endian.h"
#include "rkit/Core/Optional.h"
#include "rkit/Core/QuickSort.h"
#include "rkit/Core/Stream.h"
#include "rkit/Math/SoftFloat.h"

#include "rkit/Data/DDSFile.h"

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
			size_t m_children[2];	// 0 = Leaf

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
			size_t m_expansionLevel = 0;

			static const size_t kMaxExpansionLevel = 24;	// (1 << 12) = 4096
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

	typedef rkit::endian::LittleInt32_t BSPFaceEdge_t;

	struct BSPDataCollection
	{
		rkit::Vector<BSPTexInfo> m_texInfos;
		rkit::Vector<BSPFaceEdge_t> m_faceEdges;
		rkit::Vector<BSPFace> m_faces;
		rkit::Vector<BSPEdge> m_edges;
		rkit::Vector<BSPVertex> m_verts;
		rkit::Vector<uint8_t> m_lightMapData;
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
		const BSPTexInfo *m_texture = nullptr;

		float m_uvMin[2] = {};
		uint16_t m_lightmapDimensions[2] = {};
		uint8_t m_numUniqueStyles = 0;
		uint8_t m_numInputStyles = 0;
		uint8_t m_uniqueStyles[4] = {};
	};

	class BSPMapCompiler final : public BSPMapCompilerBase
	{
	public:
		bool HasAnalysisStage() const override;

		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;

		uint32_t GetVersion() const override;

	private:
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

		template<class TStructure>
		static rkit::Result LoadLump(rkit::Vector<TStructure> &arr, rkit::ISeekableReadStream &stream, const BSPHeader &bspHeader, BSPLumpIndex lumpIndex);

		template<class TStructure>
		static rkit::Result LoadLumpThunk(void *arr, rkit::ISeekableReadStream &stream, const BSPHeader &bspHeader, BSPLumpIndex lumpIndex);

		static rkit::Result ComputeSurfaceExtents(BSPDataCollection &bsp, rkit::Vector<BSPFaceStats> &faceStats);
		static rkit::Result BuildLightmapTrees(BSPDataCollection &bsp, const rkit::Vector<BSPFaceStats> &faceStats, rkit::Vector<rkit::UniquePtr<priv::LightmapTree>> &lightmapTrees);
		static rkit::Result InsertNodeIntoLightmapTreeList(const priv::LightmapIdentifier &ident, uint16_t width, uint16_t height, bool xAxisDominant, rkit::Vector<size_t> &stack, rkit::Vector<rkit::UniquePtr<priv::LightmapTree>> &lightmapTrees);
		static rkit::Result InsertNodeIntoExpandableLightmapTree(uint16_t width, uint16_t height, bool xAxisDominant, rkit::Vector<size_t> &stack, priv::LightmapTree &tree, rkit::Optional<size_t> &outInsertedIndex);
		static rkit::Result InsertNodeIntoLightmapTree(uint16_t width, uint16_t height, rkit::Vector<size_t> &stack, priv::LightmapTree &tree, rkit::Optional<size_t> &outInsertedIndex);
		static rkit::Result InitLightmapTree(priv::LightmapTree &tree, size_t expansionLevel, bool xAxisDominant);
		static rkit::Result CopyLightmapTree(priv::LightmapTree &newTree, const priv::LightmapTree &oldTree, rkit::Vector<size_t> &stack, bool &outCopiedOK);
		static rkit::Result ExportLightmaps(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback,
			BSPDataCollection &bsp, const rkit::Vector<BSPFaceStats> &faceStats, const rkit::Vector<rkit::UniquePtr<priv::LightmapTree>> &lightmapTrees);
		static bool LightmapFitsInNode(const priv::LightmapTreeNode &node, uint16_t width, uint16_t height);


		static rkit::Result LoadBSPData(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, BSPDataCollection &bsp, rkit::Vector<LumpLoader> &loaders);

		static rkit::Result FormatLightmapPath(rkit::String &path, const rkit::StringView &identifier, size_t lightmapIndex);
	};

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

			if (!rkit::CharacterEncodingValidator<rkit::CharacterEncoding::kASCII>::ValidateSpan(span))
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

		return rkit::ResultCode::kOK;
	}

	template<class TStructure>
	rkit::Result BSPMapCompiler::LoadLump(rkit::Vector<TStructure> &arr, rkit::ISeekableReadStream &stream, const BSPHeader &bspHeader, BSPLumpIndex lumpIndex)
	{
		const BSPLump &lump = bspHeader.m_lumps[static_cast<size_t>(lumpIndex)];
		const uint32_t lumpSize = lump.m_length.Get();
		const uint32_t lumpPos = lump.m_offset.Get();
		const size_t structureSize = sizeof(TStructure);

		if (lumpSize % structureSize != 0)
		{
			rkit::log::ErrorFmt("Size of lump %i was invalid", static_cast<int>(lumpIndex));
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

	rkit::Result BSPMapCompiler::ComputeSurfaceExtents(BSPDataCollection &bsp, rkit::Vector<BSPFaceStats> &faceStatsVector)
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

			faceStats.m_texture = &texInfo;

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
					const float vert0 = vert.x.Get();
					const float vert1 = vert.y.Get();
					const float vert2 = vert.z.Get();

					const float vec0 = texInfo.m_uvVectors[axis][0].Get();
					const float vec1 = texInfo.m_uvVectors[axis][1].Get();
					const float vec2 = texInfo.m_uvVectors[axis][2].Get();
					const float vec3 = texInfo.m_uvVectors[axis][3].Get();

					const rkit::math::SoftFloat32 v = (rkit::math::SoftFloat80(vert0) * vec0
						+ rkit::math::SoftFloat80(vert1) * vec1
						+ rkit::math::SoftFloat80(vert2) * vec2
						+ vec3).ToFloat32();

					rkit::math::SoftFloat80 termA = rkit::math::SoftFloat80(vert0) * vec0;
					rkit::math::SoftFloat80 termB = rkit::math::SoftFloat80(vert1) * vec1;
					rkit::math::SoftFloat80 termC = rkit::math::SoftFloat80(vert2) * vec2;
					rkit::math::SoftFloat80 termD = vec3;

					rkit::math::SoftFloat80 sum = termA + termB + termC + termD;

					if (edgeIndex == 0 || v < extMin[axis])
						extMin[axis] = v;
					if (edgeIndex == 0 || v > extMax[axis])
						extMax[axis] = v;
				}
			}

			uint16_t lightmapSize[2] = {};

			for (int axis = 0; axis < 2; axis++)
			{
				const int64_t intMin = (extMin[axis].ToFloat80() * rcp16).Floor().ToInt64();
				const int64_t intMax = (extMax[axis].ToFloat80() * rcp16).Ceil().ToInt64();

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
				faceStats.m_uvMin[axis] = extMin[axis].ToSingle();
			}
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result BSPMapCompiler::BuildLightmapTrees(BSPDataCollection &bsp, const rkit::Vector<BSPFaceStats> &faceStats, rkit::Vector<rkit::UniquePtr<priv::LightmapTree>> &lightmapTrees)
	{
		uint64_t xAxisTotal = 0;
		uint64_t yAxisTotal = 0;

		rkit::Vector<size_t> stack;

		const size_t kMaxLightmapSizeIncrement = 12;	// 1 << 6 == 4096

		for (const BSPFaceStats &stats : faceStats)
		{
			xAxisTotal += stats.m_lightmapDimensions[0] * stats.m_numUniqueStyles;
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

	rkit::Result BSPMapCompiler::InsertNodeIntoLightmapTreeList(const priv::LightmapIdentifier &ident, uint16_t width, uint16_t height, bool xAxisDominant, rkit::Vector<size_t> &stack, rkit::Vector<rkit::UniquePtr<priv::LightmapTree>> &lightmapTrees)
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

	rkit::Result BSPMapCompiler::InitLightmapTree(priv::LightmapTree &tree, size_t expansionLevel, bool xAxisDominant)
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

	rkit::Result BSPMapCompiler::CopyLightmapTree(priv::LightmapTree &newTree, const priv::LightmapTree &oldTree, rkit::Vector<size_t> &stack, bool &outCopiedOK)
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

	rkit::Result BSPMapCompiler::InsertNodeIntoExpandableLightmapTree(uint16_t width, uint16_t height, bool xAxisDominant, rkit::Vector<size_t> &stack, priv::LightmapTree &tree, rkit::Optional<size_t> &outInsertedIndex)
	{
		outInsertedIndex.Reset();

		uint16_t requiredExpansionLevel = 0;

		size_t tryExpansionLevel = tree.m_expansionLevel;

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

	rkit::Result BSPMapCompiler::InsertNodeIntoLightmapTree(uint16_t width, uint16_t height, rkit::Vector<size_t> &stack, priv::LightmapTree &tree, rkit::Optional<size_t> &outInsertedIndex)
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

			node.m_children[0] = child0Index;
			node.m_children[1] = child1Index;

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

	rkit::Result BSPMapCompiler::LoadBSPData(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, BSPDataCollection &bsp, rkit::Vector<LumpLoader> &loaders)
	{
		const rkit::StringView identifier = depsNode->GetIdentifier();

		rkit::CIPath ciPath;
		RKIT_CHECK(ciPath.Set(identifier));

		rkit::UniquePtr<rkit::ISeekableReadStream> bspStream;
		RKIT_CHECK(feedback->TryOpenInput(rkit::buildsystem::BuildFileLocation::kSourceDir, ciPath, bspStream));

		if (!bspStream.IsValid())
		{
			rkit::log::ErrorFmt("Failed to open '%s'", identifier.GetChars());
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

	rkit::Result BSPMapCompiler::FormatLightmapPath(rkit::String &path, const rkit::StringView &identifier, size_t lightmapIndex)
	{
		return path.Format("ax_bsp/%s_lm%zu.dds", identifier.GetChars(), lightmapIndex);
	}

	rkit::Result BSPMapCompiler::RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		BSPDataCollection bsp;

		rkit::Vector<LumpLoader> loaders;
		RKIT_CHECK(loaders.Append(LumpLoader(BSPLumpIndex::kTexInfo, bsp.m_texInfos)));
		RKIT_CHECK(loaders.Append(LumpLoader(BSPLumpIndex::kFaceEdges, bsp.m_faceEdges)));
		RKIT_CHECK(loaders.Append(LumpLoader(BSPLumpIndex::kFaces, bsp.m_faces)));
		RKIT_CHECK(loaders.Append(LumpLoader(BSPLumpIndex::kEdges, bsp.m_edges)));
		RKIT_CHECK(loaders.Append(LumpLoader(BSPLumpIndex::kVerts, bsp.m_verts)));
		RKIT_CHECK(loaders.Append(LumpLoader(BSPLumpIndex::kLightmaps, bsp.m_lightMapData)));

		RKIT_CHECK(LoadBSPData(depsNode, feedback, bsp, loaders));

		rkit::Vector<BSPFaceStats> faceStats;
		RKIT_CHECK(ComputeSurfaceExtents(bsp, faceStats));

		rkit::Vector<rkit::UniquePtr<priv::LightmapTree>> lightmapTrees;
		RKIT_CHECK(BuildLightmapTrees(bsp, faceStats, lightmapTrees));

		RKIT_CHECK(ExportLightmaps(depsNode, feedback, bsp, faceStats, lightmapTrees));

		return rkit::ResultCode::kOK;
	}

	rkit::Result BSPMapCompiler::ExportLightmaps(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, BSPDataCollection &bsp, const rkit::Vector<BSPFaceStats> &faceStats, const rkit::Vector<rkit::UniquePtr<priv::LightmapTree>> &lightmapTrees)
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

			rkit::UniquePtr<rkit::ISeekableReadWriteStream> outStream;
			RKIT_CHECK(feedback->OpenOutput(rkit::buildsystem::BuildFileLocation::kIntermediateDir, outPath, outStream));

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
		}

		return rkit::ResultCode::kOK;
	}

	bool BSPMapCompiler::LightmapFitsInNode(const priv::LightmapTreeNode &node, uint16_t width, uint16_t height)
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
		return 4;
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

	rkit::Result BSPMapCompilerBase::Create(rkit::UniquePtr<BSPMapCompilerBase> &outCompiler)
	{
		rkit::UniquePtr<BSPMapCompiler> bspMapCompiler;
		RKIT_CHECK(rkit::New<BSPMapCompiler>(bspMapCompiler));

		outCompiler = std::move(bspMapCompiler);

		return rkit::ResultCode::kOK;
	}
} } // anox::buildsystem
