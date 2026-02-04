#include "AnoxBSPModelResource.h"
#include "AnoxMaterialResource.h"
#include "AnoxAbstractSingleFileResource.h"
#include "anox/AnoxGraphicsSubsystem.h"
#include "AnoxGameFileSystem.h"

#include "anox/Data/CompressedNormal.h"
#include "anox/Data/BSPModel.h"

#include "anox/Data/EntityStructs.h"

#include "rkit/Data/ContentID.h"

#include "rkit/Math/Vec.h"

#include "rkit/Core/Future.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/JobQueue.h"
#include "rkit/Core/MemoryStream.h"
#include "rkit/Core/Path.h"
#include "rkit/Core/Sanitizers.h"
#include "rkit/Core/String.h"
#include "rkit/Core/Vector.h"

namespace anox
{
	struct AnoxBSPModelLoaderInfo;

	class AnoxBSPModelResource final : public AnoxBSPModelResourceBase
	{
	public:
		friend struct AnoxBSPModelLoaderInfo;

	private:
		rkit::Vector<rkit::math::Vec3> m_normals;
		rkit::Vector<Plane> m_planes;
		rkit::Vector<TreeNode> m_treeNodes;
		rkit::Vector<Model> m_models;
		rkit::Vector<Leaf> m_leafs;
		rkit::Vector<uint16_t> m_leafBrushes;
		rkit::Vector<Brush> m_brushes;
		rkit::Vector<BrushSide> m_brushSides;
		rkit::Vector<DrawSurfaceLocator> m_model0LeafDrawSurfaceLocators;
		rkit::Vector<DrawSurface> m_drawSurfaces;
		rkit::Vector<DrawLightmapGroup> m_drawLightmapGroups;
		rkit::Vector<DrawMaterialGroup> m_drawMaterialGroups;
		rkit::Vector<DrawModelGroup> m_drawModelGroups;
		rkit::Vector<DrawCluster> m_drawClusters;
		rkit::Vector<DrawClusterModelGroupRef> m_drawClusterModelGroupRefs;

		rkit::RCPtr<IBuffer> m_vertexBuffer;
		rkit::RCPtr<IBuffer> m_indexBuffer;
		rkit::RCPtr<IBuffer> m_normalsBuffer;
	};

	struct AnoxBSPModelResourceGPUResources final : public rkit::RefCounted
	{
		struct DrawVert
		{
			float m_xyz[3];
			uint32_t m_flippableNormalIndex;
			float m_uv[2];
			float m_lightUV[2];
		};

		struct DrawNormal
		{
			uint32_t m_part0;
			uint32_t m_part1;
		};

		struct InitCopyOp
		{
			BufferInitializer m_initializer;
			BufferInitializer::CopyOperation m_copyOp;
		};

		rkit::Vector<DrawVert> m_verts;
		rkit::Vector<DrawNormal> m_normals;
		rkit::Vector<uint16_t> m_triIndexes;

		InitCopyOp m_vertsInitCopy;
		InitCopyOp m_normalsInitCopy;
		InitCopyOp m_indexesInitCopy;

		// Keepalive for buffer upload tasks, since it stores the destination buffers
		rkit::RCPtr<AnoxResourceBase> m_resourceKeepAlive;
	};

	struct AnoxBSPModelResourceLoaderState final : public AnoxAbstractSingleFileResourceLoaderState
	{
		data::BSPDataChunksSpans m_chunks;
	};

	struct AnoxBSPModelLoaderInfo
	{
		typedef AnoxBSPModelResourceLoaderBase LoaderBase_t;
		typedef AnoxBSPModelResource Resource_t;
		typedef AnoxBSPModelResourceLoaderState State_t;

		static constexpr size_t kNumPhases = 2;

		static rkit::Result LoadHeaderAndQueueDependencies(State_t &state, Resource_t &resource, rkit::traits::TraitRef<rkit::VectorTrait<rkit::RCPtr<rkit::Job>>> outDeps);
		static rkit::Result LoadContents(State_t &state, Resource_t &resource);

		static bool PhaseHasDependencies(size_t phase);
		static rkit::Result LoadPhase(State_t &state, Resource_t &resource, size_t phase, rkit::traits::TraitRef<rkit::VectorTrait<rkit::RCPtr<rkit::Job>>> outDeps);

		class ChunkReader
		{
		public:
			explicit ChunkReader(rkit::FixedSizeMemoryStream &readStream);

			template<class T>
			rkit::Result VisitMember(rkit::Span<T> &span) const;

		private:
			rkit::FixedSizeMemoryStream &m_readStream;
		};
	};

	rkit::Result AnoxBSPModelLoaderInfo::LoadHeaderAndQueueDependencies(State_t &state, Resource_t &resource, rkit::traits::TraitRef<rkit::VectorTrait<rkit::RCPtr<rkit::Job>>> outDeps)
	{
		rkit::IUtilitiesDriver *utils = rkit::GetDrivers().m_utilitiesDriver;

		{
			rkit::FixedSizeMemoryStream stream(state.m_fileContents.GetBuffer(), state.m_fileContents.Count());

			data::BSPFile bspFile;
			RKIT_CHECK(stream.ReadAll(&bspFile, sizeof(bspFile)));

			if (bspFile.m_fourCC.Get() != data::BSPFile::kFourCC
				|| bspFile.m_version.Get() != data::BSPFile::kVersion)
				RKIT_THROW(rkit::ResultCode::kDataError);

			RKIT_CHECK(data::BSPDataChunksProcessor::VisitAllChunks(state.m_chunks, ChunkReader(stream)));
		}

		const data::BSPDataChunksSpans &chunks = state.m_chunks;
		anox::AnoxResourceManagerBase &resManager = *state.m_systems.m_resManager;

		// No SafeAdd since we don't really care about overflow here
		RKIT_CHECK(outDeps.Reserve(chunks.m_materials.Count() + chunks.m_lightmaps.Count()));

		for (const rkit::data::ContentID &materialContentID : chunks.m_materials)
		{
			rkit::RCPtr<rkit::Job> job;
			rkit::Future<AnoxResourceRetrieveResult> result;
			RKIT_CHECK(resManager.GetContentIDKeyedResource(&job, result, resloaders::kWorldMaterialTypeCode, materialContentID));

			RKIT_CHECK(outDeps.Append(job));
		}

		for (const rkit::data::ContentID &lightmapContentID : chunks.m_lightmaps)
		{
			rkit::RCPtr<rkit::Job> job;
			rkit::Future<AnoxResourceRetrieveResult> result;
			RKIT_CHECK(resManager.GetContentIDKeyedResource(&job, result, resloaders::kTextureResourceTypeCode, lightmapContentID));

			RKIT_CHECK(outDeps.Append(job));
		}

		const size_t numLightmaps = chunks.m_lightmaps.Count();
		const size_t numMaterials = chunks.m_materials.Count();
		const uint32_t numNormals = static_cast<uint32_t>(chunks.m_normals.Count());
		const uint32_t numDrawVerts = static_cast<uint32_t>(chunks.m_drawVerts.Count());
		const uint32_t numDrawTriIndexes = static_cast<uint32_t>(chunks.m_drawTriIndexes.Count());
		const uint32_t numPlanes = static_cast<uint32_t>(chunks.m_planes.Count());
		const uint32_t numLeafs = static_cast<uint32_t>(chunks.m_leafs.Count());
		const uint32_t numTreeNodes = static_cast<uint32_t>(chunks.m_treeNodes.Count());
		const uint32_t numModels = static_cast<uint32_t>(chunks.m_models.Count());
		const uint32_t numDrawSurfaces = static_cast<uint32_t>(chunks.m_drawSurfaces.Count());
		const uint32_t numBrushes = static_cast<uint32_t>(chunks.m_brushes.Count());
		const uint32_t numBrushSides = static_cast<uint32_t>(chunks.m_brushSides.Count());
		const uint32_t numLeafBrushes = static_cast<uint32_t>(chunks.m_leafBrushes.Count());
		const uint32_t numModelDrawClusterModelGroupRefs = static_cast<uint32_t>(chunks.m_modelDrawClusterModelGroupRefs.Count());
		const uint32_t numModel0LeafDrawSurfaceLocators = static_cast<uint32_t>(chunks.m_model0LeafDrawSurfaceLocators.Count());
		const uint32_t numModel0LeafDrawSurfaceLocatorCounts = static_cast<uint32_t>(chunks.m_model0LeafDrawSurfaceLocatorCounts.Count());
		const uint32_t numDrawLightmapGroups = static_cast<uint32_t>(chunks.m_lightmapGroups.Count());
		const uint32_t numDrawMaterialGroups = static_cast<uint32_t>(chunks.m_materialGroups.Count());
		const uint32_t numDrawModelGroups = static_cast<uint32_t>(chunks.m_drawModelGroups.Count());
		const uint32_t numDrawClusters = static_cast<uint32_t>(chunks.m_drawClusters.Count());

		if (numDrawTriIndexes % 3u != 0)
			RKIT_THROW(rkit::ResultCode::kDataError);

		const uint32_t numDrawTris = numDrawTriIndexes / 3u;

		rkit::Vector<rkit::math::Vec3> &outNormals = resource.m_normals;
		RKIT_CHECK(outNormals.Resize(numNormals));

		rkit::ProcessParallelSpans(outNormals.ToSpan(), chunks.m_normals,
			[](rkit::math::Vec3 &outNormal, const data::BSPNormal &inNormal)
			{
				data::DecompressNormal64NoNegate(inNormal.m_normal.m_part0.Get(), inNormal.m_normal.m_part1.Get(), outNormal[0], outNormal[1], outNormal[2]);
			});

		if (numPlanes >= 0x80000000u || numLeafs >= 0x80000000u || numTreeNodes >= 0x80000000u)
			RKIT_THROW(rkit::ResultCode::kDataError);

		if (numPlanes == 0 || numNormals == 0)
			RKIT_THROW(rkit::ResultCode::kDataError);

		const uint32_t maxNormalIndex = numNormals - 1;

		RKIT_CHECK(resource.m_planes.Resize(numPlanes));
		rkit::ProcessParallelSpans(resource.m_planes.ToSpan(), chunks.m_planes,
			[maxNormalIndex](AnoxBSPModelResourceBase::Plane &outPlane, const data::BSPPlane &inPlane)
			{
				outPlane.m_dist = rkit::sanitizers::SanitizeClampFloat(inPlane.m_dist, 20);
				outPlane.m_normalIndex = rkit::sanitizers::SanitizeClampUInt(inPlane.m_normal.Get(), maxNormalIndex);
			});


		rkit::Vector<AnoxBSPModelResource::TreeNode> &outTreeNodes = resource.m_treeNodes;
		rkit::Vector<AnoxBSPModelResource::Model> &outModels = resource.m_models;
		rkit::Vector<AnoxBSPModelResource::Leaf> &outLeafs = resource.m_leafs;

		{
			// Parse BSP tree
			const uint32_t numSplitBytes = static_cast<uint32_t>(chunks.m_treeNodeSplitBits.Count());
			const uint64_t numSplitBitPairs = static_cast<uint64_t>(numSplitBytes) * 4;

			if (numSplitBitPairs < numTreeNodes)
				RKIT_THROW(rkit::ResultCode::kDataError);

			if (numLeafs == 0 || numTreeNodes == 0 || numModels == 0)
				RKIT_THROW(rkit::ResultCode::kDataError);

			rkit::Vector<uint32_t> backNodeStack;

			RKIT_CHECK(outTreeNodes.Reserve(numTreeNodes));
			RKIT_CHECK(outModels.Reserve(numModels));
			RKIT_CHECK(outLeafs.Reserve(numLeafs));

			uint32_t leafIndex = 0;
			uint32_t nodeIndex = 0;
			uint32_t modelDrawClusterIndex = 0;
			size_t splitBitIndex = 0;

			const uint8_t *splitBytes = chunks.m_treeNodeSplitBits.Ptr();

			for (const data::BSPModel &inModel : chunks.m_models)
			{
				AnoxBSPModelResource::Model outModel = {};
				outModel.m_numModelDrawClusterModelGroups = inModel.m_numModelDrawClusterModelGroups.Get();
				for (size_t axis = 0; axis < 3; axis++)
				{
					outModel.m_mins[axis] = rkit::sanitizers::SanitizeClampFloat(inModel.m_mins[axis], 20);
					outModel.m_maxs[axis] = rkit::sanitizers::SanitizeClampFloat(inModel.m_maxs[axis], 20);
					outModel.m_origin[axis] = rkit::sanitizers::SanitizeClampFloat(inModel.m_origin[axis], 20);

					if (!(outModel.m_mins[axis] < outModel.m_maxs[axis]))
						RKIT_THROW(rkit::ResultCode::kDataError);
				}

				outModel.m_firstLeaf = leafIndex;
				outModel.m_numDrawSurfaces = inModel.m_numDrawSurfaces.Get();

				if (outModel.m_rootIsLeaf)
				{
					outModel.m_rootIsLeaf = 1;

					if (leafIndex == numLeafs)
						RKIT_THROW(rkit::ResultCode::kDataError);

					outModel.m_rootIndex = leafIndex++;
				}
				else
				{
					outModel.m_rootIsLeaf = 0;
					outModel.m_rootIndex = nodeIndex;

					for (;;)
					{
						if (nodeIndex == numTreeNodes)
							RKIT_THROW(rkit::ResultCode::kDataError);

						AnoxBSPModelResource::TreeNode outNode = {};
						const data::BSPTreeNode &inNode = chunks.m_treeNodes[nodeIndex];

						for (size_t axis = 0; axis < 3; axis++)
						{
							outNode.m_minBounds[axis] = inNode.m_minBounds[axis].Get();
							outNode.m_maxBounds[axis] = inNode.m_maxBounds[axis].Get();

							if (outNode.m_minBounds[axis] > outNode.m_maxBounds[axis])
								RKIT_THROW(rkit::ResultCode::kDataError);
						}

						uint32_t planeValue = inNode.m_plane.Get();
						outNode.m_planeIndex = rkit::sanitizers::SanitizeClampUInt(planeValue >> 1, numPlanes - 1);
						outNode.m_planeFlip = (planeValue & 1);

						const uint8_t splitByte = (splitBytes[nodeIndex >> 2] >> ((nodeIndex & 3) * 2)) & 3;

						bool recurse = false;
						if (splitByte == 0)
						{
							if (leafIndex == numLeafs)
								RKIT_THROW(rkit::ResultCode::kDataError);

							outNode.m_frontNode = leafIndex++;
							outNode.m_frontNodeIsLeaf = 1;

							if (leafIndex == numLeafs)
								RKIT_THROW(rkit::ResultCode::kDataError);

							outNode.m_backNode = leafIndex++;
							outNode.m_backNodeIsLeaf = 1;
						}
						else if (splitByte == 1)
						{
							recurse = true;

							if (nodeIndex == numTreeNodes)
								RKIT_THROW(rkit::ResultCode::kDataError);

							outNode.m_frontNode = nodeIndex + 1;

							if (leafIndex == numLeafs)
								RKIT_THROW(rkit::ResultCode::kDataError);

							outNode.m_backNode = leafIndex++;
							outNode.m_backNodeIsLeaf = 1;
						}
						else if (splitByte == 2)
						{
							recurse = true;

							if (leafIndex == numLeafs)
								RKIT_THROW(rkit::ResultCode::kDataError);

							outNode.m_frontNode = leafIndex++;
							outNode.m_frontNodeIsLeaf = 1;

							if (nodeIndex == numTreeNodes)
								RKIT_THROW(rkit::ResultCode::kDataError);

							outNode.m_backNode = nodeIndex + 1;
						}
						else // if (splitByte == 3)
						{
							recurse = true;
							RKIT_CHECK(backNodeStack.Append(nodeIndex));

							if (nodeIndex == numTreeNodes)
								RKIT_THROW(rkit::ResultCode::kDataError);

							outNode.m_frontNode = nodeIndex + 1;

							// Back node to be evaluated later
						}

						RKIT_CHECK(outTreeNodes.Append(outNode));
						nodeIndex++;

						if (!recurse)
						{
							if (backNodeStack.Count() == 0)
								break;

							const uint32_t poppedNodeIndex = backNodeStack[backNodeStack.Count() - 1];
							backNodeStack.ShrinkToSize(backNodeStack.Count() - 1);

							outTreeNodes[poppedNodeIndex].m_backNode = nodeIndex;
						}
					}
				}

				outModel.m_numLeafs = leafIndex - outModel.m_firstLeaf;
				RKIT_CHECK(outModels.Append(outModel));
			}

			if (leafIndex != numLeafs || nodeIndex != numTreeNodes || outModels[0].m_numLeafs != numModel0LeafDrawSurfaceLocatorCounts)
				RKIT_THROW(rkit::ResultCode::kDataError);
		}

		rkit::Vector<uint16_t> &outLeafBrushes = resource.m_leafBrushes;

		RKIT_CHECK(outLeafBrushes.Resize(numLeafBrushes));
		RKIT_CHECK(outLeafs.Resize(numLeafs));

		if (numLeafBrushes > 0)
		{
			if (numBrushes == 0)
				RKIT_THROW(rkit::ResultCode::kDataError);

			if (numBrushes > 0x10000)
				RKIT_THROW(rkit::ResultCode::kDataError);

			utils->SanitizeClampUInt16s(outLeafBrushes.ToSpan(), chunks.m_leafBrushes, static_cast<uint16_t>(numBrushes - 1));
		}

		{
			uint32_t leafBrushIndex = 0;

			const rkit::Span<const uint16_t> leafBrushesSpan = outLeafBrushes.ToSpan();

			RKIT_CHECK((rkit::CheckedProcessParallelSpans(resource.m_leafs.ToSpan(), chunks.m_leafs,
				[&leafBrushIndex, leafBrushesSpan, numLeafBrushes]
				(AnoxBSPModelResourceBase::Leaf &outLeaf, const data::BSPTreeLeaf &inLeaf)
				-> rkit::Result
				{
					outLeaf.m_contentFlags = inLeaf.m_contentFlags.Get();

					// FIXME: Validate this
					outLeaf.m_cluster = inLeaf.m_cluster.Get();
					outLeaf.m_area = inLeaf.m_area.Get();

					for (size_t axis = 0; axis < 3; axis++)
					{
						outLeaf.m_minBounds[axis] = inLeaf.m_minBounds[axis].Get();
						outLeaf.m_maxBounds[axis] = inLeaf.m_maxBounds[axis].Get();

						if (outLeaf.m_minBounds[axis] > outLeaf.m_maxBounds[axis])
							RKIT_THROW(rkit::ResultCode::kDataError);
					}

					const uint32_t leafBrushCount = inLeaf.m_numLeafBrushes.Get();

					const uint32_t leafBrushesAvailable = numLeafBrushes - leafBrushIndex;

					if (leafBrushesAvailable < leafBrushCount)
						RKIT_THROW(rkit::ResultCode::kDataError);

					outLeaf.m_brushes = leafBrushesSpan.SubSpan(leafBrushIndex, leafBrushCount);

					leafBrushIndex += leafBrushCount;

					RKIT_RETURN_OK;
				}
			)));

			if (leafBrushIndex != numLeafBrushes)
				RKIT_THROW(rkit::ResultCode::kDataError);
		}

		rkit::Vector<AnoxBSPModelResource::Brush> &outBrushes = resource.m_brushes;
		rkit::Vector<AnoxBSPModelResource::BrushSide> &outBrushSides = resource.m_brushSides;
		RKIT_CHECK(outBrushes.Resize(numBrushes));
		RKIT_CHECK(outBrushSides.Resize(numBrushSides));

		{
			uint32_t brushSideIndex = 0;
			rkit::Span<const AnoxBSPModelResource::BrushSide> brushSidesSpan = outBrushSides.ToSpan();

			RKIT_CHECK((rkit::CheckedProcessParallelSpans(outBrushes.ToSpan(), chunks.m_brushes,
				[numBrushSides, brushSidesSpan, &brushSideIndex](AnoxBSPModelResource::Brush &outBrush, const data::BSPBrush &inBrush)
				->rkit::Result
				{
					outBrush.m_contents = inBrush.m_contents.Get();

					const uint32_t brushSidesAvailable = numBrushSides - brushSideIndex;
					const uint32_t brushSideCount = inBrush.m_numBrushSides.Get();

					if (brushSideCount > brushSidesAvailable)
						RKIT_THROW(rkit::ResultCode::kDataError);

					outBrush.m_sides = brushSidesSpan.SubSpan(brushSideIndex, brushSideCount);
					brushSideIndex += brushSideCount;

					RKIT_RETURN_OK;
				})));;

			if (brushSideIndex != numBrushSides)
				RKIT_THROW(rkit::ResultCode::kDataError);
		}

		if (numBrushSides > 0)
		{
			if (numPlanes == 0 || numPlanes >= 0x80000000u)
				RKIT_THROW(rkit::ResultCode::kDataError);

			const uint32_t maxPlaneValue = numPlanes - 1u;

			rkit::ProcessParallelSpans(outBrushSides.ToSpan(), chunks.m_brushSides,
				[maxPlaneValue](AnoxBSPModelResource::BrushSide &outBrushSide, const data::BSPBrushSide &inBrushSide)
				{
					const uint32_t planeValue = inBrushSide.m_plane.Get();

					outBrushSide.m_planeFlipBit = (planeValue & 1);
					outBrushSide.m_planeIndex = rkit::sanitizers::SanitizeClampUInt(planeValue >> 1, maxPlaneValue);
					outBrushSide.m_material = inBrushSide.m_material.Get();
					outBrushSide.m_materialFlags = inBrushSide.m_materialFlags.Get();
				});
		}

		const size_t model0LeafCount = resource.m_models[0].m_numLeafs;

		if (chunks.m_model0LeafDrawSurfaceLocatorCounts.Count() != model0LeafCount)
			RKIT_THROW(rkit::ResultCode::kDataError);

		rkit::Vector<AnoxBSPModelResource::DrawSurfaceLocator> &outModel0LeafDrawSurfaceLocators = resource.m_model0LeafDrawSurfaceLocators;
		RKIT_CHECK(outModel0LeafDrawSurfaceLocators.Resize(numModel0LeafDrawSurfaceLocators));

		{
			const uint32_t numModel0DrawSurfaces = outModels[0].m_numDrawSurfaces;

			if (numModel0DrawSurfaces > 0x7fffffffu || numModel0DrawSurfaces == 0)
				RKIT_THROW(rkit::ResultCode::kDataError);

			const uint32_t numBitChunks = (numModel0DrawSurfaces + 31u) / 32u;
			const uint32_t numTrailingBits = numModel0DrawSurfaces % 32u;
			const uint32_t partialChunkBitMask = ~static_cast<uint32_t>((0xffffffffu << numTrailingBits) & 0xffffffffu);

			const uint32_t partialChunkIndex = numModel0DrawSurfaces / 32u;

			uint32_t locatorStartIndex = 0;

			const rkit::Span<AnoxBSPModelResource::DrawSurfaceLocator> locatorsSpan = outModel0LeafDrawSurfaceLocators.ToSpan();

			RKIT_CHECK((rkit::CheckedProcessParallelSpans(
				outLeafs.ToSpan().SubSpan(0, outModels[0].m_numLeafs), chunks.m_model0LeafDrawSurfaceLocatorCounts,
				[&locatorStartIndex, numModel0LeafDrawSurfaceLocators, locatorsSpan]
				(AnoxBSPModelResource::Leaf &outLeaf, const rkit::endian::LittleUInt16_t &inCount)
				-> rkit::Result
				{
					const uint32_t availableLocators = numModel0LeafDrawSurfaceLocators - locatorStartIndex;
					const uint32_t count = inCount.Get();

					if (count > availableLocators)
						RKIT_THROW(rkit::ResultCode::kDataError);

					outLeaf.m_drawLocators = locatorsSpan.SubSpan(locatorStartIndex, count);

					locatorStartIndex += count;

					RKIT_RETURN_OK;
				}
			)));

			rkit::ProcessParallelSpans(
				outModel0LeafDrawSurfaceLocators.ToSpan(), chunks.m_model0LeafDrawSurfaceLocators,
				[numBitChunks, partialChunkIndex, partialChunkBitMask]
				(AnoxBSPModelResource::DrawSurfaceLocator &outLocator, const data::BSPLeafDrawSurfaceLocator &inLocator)
				{
					const uint32_t drawSurfChunkIndex = rkit::Min<uint32_t>(inLocator.m_drawSurfChunkIndex.Get(), numBitChunks - 1);

					outLocator.m_drawSurfChunkIndex = drawSurfChunkIndex;
					outLocator.m_bits = inLocator.m_bits.Get();
					if (drawSurfChunkIndex == partialChunkIndex)
						outLocator.m_bits &= partialChunkBitMask;

				});
		}


		rkit::RCPtr<AnoxBSPModelResourceGPUResources> gpuResources;
		RKIT_CHECK(rkit::New<AnoxBSPModelResourceGPUResources>(gpuResources));

		gpuResources->m_resourceKeepAlive = state.m_resource;

		RKIT_CHECK(gpuResources->m_normals.Resize(numNormals));

		{
			rkit::Vector<AnoxBSPModelResourceGPUResources::DrawNormal> &outDrawNormals = gpuResources->m_normals;
			RKIT_CHECK(outDrawNormals.Resize(numNormals));

			rkit::ProcessParallelSpans(outDrawNormals.ToSpan(), chunks.m_normals,
				[](AnoxBSPModelResourceGPUResources::DrawNormal &outNormal, const data::BSPNormal &inNormal)
				{
					const uint32_t part0 = inNormal.m_normal.m_part0.Get();
					const uint32_t part1 = inNormal.m_normal.m_part1.Get();

					outNormal.m_part0 = part0;
					outNormal.m_part1 = part1;
				});
		}

		if (numDrawVerts > 0)
		{
			rkit::Vector<AnoxBSPModelResourceGPUResources::DrawVert> &outDrawVerts = gpuResources->m_verts;
			RKIT_CHECK(gpuResources->m_verts.Resize(numDrawVerts));

			if (numNormals == 0 || numNormals >= 0x80000000u)
				RKIT_THROW(rkit::ResultCode::kDataError);

			uint32_t maxFlippableNormalValue = (numNormals * 2u) - 1u;

			rkit::ProcessParallelSpans(outDrawVerts.ToSpan(), chunks.m_drawVerts,
				[maxFlippableNormalValue, utils]
				(AnoxBSPModelResourceGPUResources::DrawVert &outVert, const data::BSPDrawVertex &inVert)
				{
					utils->SanitizeClampFloats(rkit::Span<float>(outVert.m_xyz), rkit::Span<const rkit::endian::LittleFloat32_t>(inVert.m_xyz), 20);
					outVert.m_flippableNormalIndex = rkit::Min(maxFlippableNormalValue, inVert.m_normal.Get());
					utils->SanitizeClampFloats(rkit::Span<float>(outVert.m_lightUV), rkit::Span<const rkit::endian::LittleFloat32_t>(inVert.m_lightUV), 1);
					utils->SanitizeClampFloats(rkit::Span<float>(outVert.m_uv), rkit::Span<const rkit::endian::LittleFloat32_t>(inVert.m_uv), 20);
				});
		}

		rkit::Vector<AnoxBSPModelResource::DrawSurface> &outDrawSurfaces = resource.m_drawSurfaces;

		if (numDrawVerts > 0)
		{
			uint32_t firstTriIndex = 0;
			RKIT_CHECK(outDrawSurfaces.Resize(numDrawSurfaces));

			RKIT_CHECK((rkit::CheckedProcessParallelSpans(outDrawSurfaces.ToSpan(), chunks.m_drawSurfaces,
				[numDrawTris, &firstTriIndex](AnoxBSPModelResource::DrawSurface &outSurf, const data::BSPDrawSurface &inSurf)
				-> rkit::Result
				{
					uint32_t availableTris = numDrawTris - firstTriIndex;

					const uint32_t numTrisForSurf = inSurf.m_numTris.Get();
					if (numTrisForSurf > availableTris)
						RKIT_THROW(rkit::ResultCode::kDataError);

					outSurf.m_firstTriIndex = firstTriIndex;
					outSurf.m_numTris = numTrisForSurf;

					firstTriIndex += numTrisForSurf;

					RKIT_RETURN_OK;
				}
			)));

			rkit::Vector<uint16_t> &outTriIndexes = gpuResources->m_triIndexes;
			RKIT_CHECK(outTriIndexes.Resize(numDrawTriIndexes));

			if (firstTriIndex != numDrawTris)
				RKIT_THROW(rkit::ResultCode::kDataError);
		}

		{
			uint32_t firstDrawSurfaceIndex = 0;

			rkit::Vector<AnoxBSPModelResource::DrawLightmapGroup> &outLightmapGroups = resource.m_drawLightmapGroups;
			RKIT_CHECK(outLightmapGroups.Resize(numDrawLightmapGroups));

			const rkit::Span<AnoxBSPModelResource::DrawSurface> drawSurfacesSpan = outDrawSurfaces.ToSpan();

			RKIT_CHECK((rkit::CheckedProcessParallelSpans(outLightmapGroups.ToSpan(), chunks.m_lightmapGroups,
				[&firstDrawSurfaceIndex, numDrawSurfaces, numLightmaps, drawSurfacesSpan]
				(AnoxBSPModelResource::DrawLightmapGroup &outLightmapGroup, const data::BSPDrawLightmapGroup &inLightmapGroup)
				-> rkit::Result
			{
				const uint32_t availableDrawSurfaces = numDrawSurfaces - firstDrawSurfaceIndex;
				const uint32_t lightmapIndex = inLightmapGroup.m_atlasIndex.Get();

				const uint32_t numDrawSurfacesForLMG = inLightmapGroup.m_numSurfaces.Get();

				if (lightmapIndex >= numLightmaps || numDrawSurfacesForLMG > availableDrawSurfaces || numDrawSurfacesForLMG == 0)
					RKIT_THROW(rkit::ResultCode::kDataError);

				outLightmapGroup.m_lightMapAtlasIndex = lightmapIndex;
				outLightmapGroup.m_drawSurfaces = drawSurfacesSpan.SubSpan(firstDrawSurfaceIndex, numDrawSurfacesForLMG);

				outLightmapGroup.m_combinedSurface.m_numTris = 0;
				outLightmapGroup.m_combinedSurface.m_firstTriIndex = outLightmapGroup.m_drawSurfaces[0].m_firstTriIndex;

				for (const AnoxBSPModelResource::DrawSurface &surf : outLightmapGroup.m_drawSurfaces)
					outLightmapGroup.m_combinedSurface.m_numTris += surf.m_numTris;

				firstDrawSurfaceIndex += numDrawSurfacesForLMG;

				RKIT_RETURN_OK;
			}
			)));

			if (firstDrawSurfaceIndex != numDrawSurfaces)
				RKIT_THROW(rkit::ResultCode::kDataError);
		}

		{
			uint32_t firstLightmapGroupIndex = 0;

			rkit::Vector<AnoxBSPModelResource::DrawMaterialGroup> &outMaterialGroups = resource.m_drawMaterialGroups;
			RKIT_CHECK(outMaterialGroups.Resize(numDrawMaterialGroups));

			const rkit::Span<AnoxBSPModelResource::DrawLightmapGroup> drawLightmapGroupSpan = resource.m_drawLightmapGroups.ToSpan();

			RKIT_CHECK((rkit::CheckedProcessParallelSpans(outMaterialGroups.ToSpan(), chunks.m_materialGroups,
				[&firstLightmapGroupIndex, numDrawLightmapGroups, numMaterials, drawLightmapGroupSpan]
				(AnoxBSPModelResource::DrawMaterialGroup &outMaterialGroup, const data::BSPDrawMaterialGroup &inMaterialGroup)
				-> rkit::Result
				{
					const uint32_t availableLightmapGroups = numDrawLightmapGroups - firstLightmapGroupIndex;
					const uint32_t materialIndex = inMaterialGroup.m_materialIndex.Get();

					const uint32_t numLightmapGroupsForMG = inMaterialGroup.m_numLightmapGroups.Get();

					if (materialIndex >= numMaterials || numLightmapGroupsForMG > availableLightmapGroups || numLightmapGroupsForMG == 0)
						RKIT_THROW(rkit::ResultCode::kDataError);

					outMaterialGroup.m_materialIndex = materialIndex;
					outMaterialGroup.m_lightmapGroups = drawLightmapGroupSpan.SubSpan(firstLightmapGroupIndex, numLightmapGroupsForMG);

					outMaterialGroup.m_combinedSurface.m_numTris = 0;
					outMaterialGroup.m_combinedSurface.m_firstTriIndex = outMaterialGroup.m_lightmapGroups[0].m_combinedSurface.m_firstTriIndex;

					for (const AnoxBSPModelResource::DrawLightmapGroup &lmg : outMaterialGroup.m_lightmapGroups)
						outMaterialGroup.m_combinedSurface.m_numTris += lmg.m_combinedSurface.m_numTris;

					firstLightmapGroupIndex += numLightmapGroupsForMG;

					RKIT_RETURN_OK;
				}
			)));

			if (firstLightmapGroupIndex != numDrawLightmapGroups)
				RKIT_THROW(rkit::ResultCode::kDataError);
		}

		{
			uint32_t firstMaterialGroupIndex = 0;

			rkit::Vector<AnoxBSPModelResource::DrawModelGroup> &outModelGroups = resource.m_drawModelGroups;
			RKIT_CHECK(outModelGroups.Resize(numDrawModelGroups));

			const rkit::Span<AnoxBSPModelResource::DrawMaterialGroup> materialGroupsSpan = resource.m_drawMaterialGroups.ToSpan();

			RKIT_CHECK((rkit::CheckedProcessParallelSpans(outModelGroups.ToSpan(), chunks.m_drawModelGroups,
				[&firstMaterialGroupIndex, numDrawMaterialGroups, materialGroupsSpan]
				(AnoxBSPModelResource::DrawModelGroup &outModelGroup, const data::BSPDrawModelGroup& inModelGroup)
				-> rkit::Result
				{
					const uint32_t availableMaterialGroups = numDrawMaterialGroups - firstMaterialGroupIndex;

					const uint32_t numMaterialGroupsForModelGroup = inModelGroup.m_numMaterialGroups.Get();

					if (numMaterialGroupsForModelGroup > availableMaterialGroups || numMaterialGroupsForModelGroup == 0)
						RKIT_THROW(rkit::ResultCode::kDataError);

					outModelGroup.m_materialGroups = materialGroupsSpan.SubSpan(firstMaterialGroupIndex, numMaterialGroupsForModelGroup);

					outModelGroup.m_combinedSurface.m_numTris = 0;
					outModelGroup.m_combinedSurface.m_firstTriIndex = outModelGroup.m_materialGroups[0].m_combinedSurface.m_firstTriIndex;

					for (const AnoxBSPModelResource::DrawMaterialGroup &mg : outModelGroup.m_materialGroups)
						outModelGroup.m_combinedSurface.m_numTris += mg.m_combinedSurface.m_numTris;

					firstMaterialGroupIndex += numMaterialGroupsForModelGroup;

					RKIT_RETURN_OK;
				}
			)));

			if (firstMaterialGroupIndex != numDrawMaterialGroups)
				RKIT_THROW(rkit::ResultCode::kDataError);
		}

		{
			uint32_t firstModelGroupIndex = 0;

			rkit::Vector<AnoxBSPModelResource::DrawCluster> &outClusters = resource.m_drawClusters;
			RKIT_CHECK(outClusters.Resize(numDrawClusters));

			const rkit::Span<AnoxBSPModelResource::DrawModelGroup> modelGroupsSpan = resource.m_drawModelGroups.ToSpan();

			RKIT_CHECK((rkit::CheckedProcessParallelSpans(outClusters.ToSpan(), chunks.m_drawClusters,
				[&firstModelGroupIndex, numDrawModelGroups, modelGroupsSpan]
				(AnoxBSPModelResource::DrawCluster &outCluster, const data::BSPDrawCluster &inCluster)
				-> rkit::Result
				{
					const uint32_t availableModelGroups = numDrawModelGroups - firstModelGroupIndex;

					const uint32_t numModelGroupsForCluster = inCluster.m_numModelGroups.Get();

					if (numModelGroupsForCluster > availableModelGroups || numModelGroupsForCluster == 0)
						RKIT_THROW(rkit::ResultCode::kDataError);

					outCluster.m_numVerts = static_cast<uint32_t>(inCluster.m_numVertsMinusOne.Get()) + 1u;
					outCluster.m_modelGroups = modelGroupsSpan.SubSpan(firstModelGroupIndex, numModelGroupsForCluster);

					outCluster.m_combinedSurface.m_numTris = 0;
					outCluster.m_combinedSurface.m_firstTriIndex = outCluster.m_modelGroups[0].m_combinedSurface.m_firstTriIndex;

					for (const AnoxBSPModelResource::DrawModelGroup &mg : outCluster.m_modelGroups)
						outCluster.m_combinedSurface.m_numTris += mg.m_combinedSurface.m_numTris;

					firstModelGroupIndex += numModelGroupsForCluster;

					RKIT_RETURN_OK;
				}
			)));

			if (firstModelGroupIndex != numDrawModelGroups)
				RKIT_THROW(rkit::ResultCode::kDataError);
		}

		{
			uint32_t firstVertIndex = 0;

			for (AnoxBSPModelResource::DrawCluster &cluster : resource.m_drawClusters.ToSpan())
			{
				const uint32_t numAvailableVerts = numDrawVerts - firstVertIndex;
				const uint32_t clusterNumVerts = cluster.m_numVerts;

				if (clusterNumVerts > numAvailableVerts)
					RKIT_THROW(rkit::ResultCode::kDataError);

				cluster.m_firstVertIndex = firstVertIndex;

				firstVertIndex += clusterNumVerts;

				const size_t firstTriIndex = cluster.m_combinedSurface.m_firstTriIndex * 3u;
				const size_t numTriIndexes = cluster.m_combinedSurface.m_numTris * 3u;

				const uint16_t maxVertIndex = static_cast<uint16_t>(clusterNumVerts - 1u);

				const rkit::Span<uint16_t> outTriIndexSpan = gpuResources->m_triIndexes.ToSpan().SubSpan(firstTriIndex, numTriIndexes);
				const rkit::Span<const rkit::endian::LittleUInt16_t> inTriIndexSpan = chunks.m_drawTriIndexes.SubSpan(firstTriIndex, numTriIndexes);

				utils->SanitizeClampUInt16s(outTriIndexSpan, inTriIndexSpan, maxVertIndex);
			}

			if (firstVertIndex != numDrawVerts)
				RKIT_THROW(rkit::ResultCode::kDataError);
		}

		// Compute model-draw-cluster and draw cluster ranges
		{
			uint32_t firstDrawSurfaceIndex = 0;
			uint32_t firstModelDrawClusterModelGroupRefIndex = 0;

			for (AnoxBSPModelResource::Model &model : resource.m_models)
			{
				const uint32_t availableDrawSurfaces = numDrawSurfaces - firstDrawSurfaceIndex;
				const uint32_t availableModelDrawClusterModelGroupRefs = numModelDrawClusterModelGroupRefs - firstModelDrawClusterModelGroupRefIndex;

				if (model.m_numModelDrawClusterModelGroups > availableModelDrawClusterModelGroupRefs || model.m_numDrawSurfaces > availableDrawSurfaces)
					RKIT_THROW(rkit::ResultCode::kDataError);

				model.m_firstDrawSurfaceIndex = firstDrawSurfaceIndex;
				model.m_firstModelDrawClusterModelGroupRefIndex = firstModelDrawClusterModelGroupRefIndex;


				firstDrawSurfaceIndex += model.m_numDrawSurfaces;
				firstModelDrawClusterModelGroupRefIndex += model.m_numModelDrawClusterModelGroups;
			}

			if (firstDrawSurfaceIndex != numDrawSurfaces || firstModelDrawClusterModelGroupRefIndex != numModelDrawClusterModelGroupRefs)
				RKIT_THROW(rkit::ResultCode::kDataError);

			rkit::Vector<AnoxBSPModelResource::DrawClusterModelGroupRef> &outRefs = resource.m_drawClusterModelGroupRefs;

			RKIT_CHECK(outRefs.Resize(numModelDrawClusterModelGroupRefs));

			rkit::ProcessParallelSpans(outRefs.ToSpan(), chunks.m_modelDrawClusterModelGroupRefs,
				[](AnoxBSPModelResource::DrawClusterModelGroupRef &outRef, const data::BSPModelDrawClusterModelGroupRef &inRef)
				{
					outRef.m_clusterIndex = inRef.m_drawClusterIndex.Get();
					outRef.m_modelGroupIndex = inRef.m_modelGroupIndex.Get();
				});

			const AnoxBSPModelResource::DrawSurface *drawSurfacesPtr = outDrawSurfaces.GetBuffer();

			const rkit::Span<const AnoxBSPModelResource::DrawCluster> clustersSpan = resource.m_drawClusters.ToSpan();
			const rkit::Span<const AnoxBSPModelResource::DrawClusterModelGroupRef> allRefsSpan = outRefs.ToSpan();

			for (const AnoxBSPModelResource::Model &model : resource.m_models)
			{
				const rkit::Span<const AnoxBSPModelResource::DrawClusterModelGroupRef> refs = allRefsSpan.SubSpan(model.m_firstModelDrawClusterModelGroupRefIndex, model.m_numModelDrawClusterModelGroups);

				const size_t boundsStart = model.m_firstDrawSurfaceIndex;
				const size_t boundsEnd = model.m_firstDrawSurfaceIndex + model.m_numDrawSurfaces;

				for (const AnoxBSPModelResource::DrawClusterModelGroupRef &ref : refs)
				{
					if (ref.m_clusterIndex >= clustersSpan.Count())
						RKIT_THROW(rkit::ResultCode::kDataError);

					const AnoxBSPModelResource::DrawCluster &cluster = clustersSpan[ref.m_clusterIndex];
					if (ref.m_modelGroupIndex >= cluster.m_modelGroups.Count())
						RKIT_THROW(rkit::ResultCode::kDataError);

					const AnoxBSPModelResource::DrawSurface *surfStart = cluster.m_modelGroups[0].m_materialGroups[0].m_lightmapGroups[0].m_drawSurfaces.Ptr();

					rkit::ConstSpan<AnoxBSPModelResource::DrawSurface> lastSpan = cluster.m_modelGroups.Last().m_materialGroups.Last().m_lightmapGroups.Last().m_drawSurfaces;
					const AnoxBSPModelResource::DrawSurface *surfEnd = lastSpan.Ptr() + lastSpan.Count();

					const size_t surfStartIndex = surfStart - drawSurfacesPtr;
					const size_t surfEndIndex = surfEnd - drawSurfacesPtr;

					if (surfStartIndex < boundsStart || surfEndIndex > boundsEnd)
						RKIT_THROW(rkit::ResultCode::kDataError);
				}
			}
		}

		// Post vertex buffer upload
		{
			rkit::RCPtr<rkit::Job> uploadJob;

			AnoxBSPModelResourceGPUResources::InitCopyOp &vertexInitCopy = gpuResources->m_vertsInitCopy;
			BufferInitializer::CopyOperation &copyOp = vertexInitCopy.m_copyOp;
			BufferInitializer &initializer = vertexInitCopy.m_initializer;

			copyOp.m_data = gpuResources->m_verts.ToSpan().ReinterpretCast<const uint8_t>();
			copyOp.m_offset = 0;
			initializer.m_copyOperations = rkit::ConstSpan<BufferInitializer::CopyOperation>(&copyOp, 1);
			initializer.m_spec.m_size = copyOp.m_data.Count();
			initializer.m_resSpec.m_usage.Add({ rkit::render::BufferUsageFlag::kVertexBuffer });

			RKIT_CHECK(state.m_systems.m_graphicsSystem->CreateAsyncCreateAndFillBufferJob(&uploadJob,
				resource.m_vertexBuffer,
				gpuResources.FieldRef(&AnoxBSPModelResourceGPUResources::m_vertsInitCopy).FieldRef(&AnoxBSPModelResourceGPUResources::InitCopyOp::m_initializer),
				nullptr));
			RKIT_CHECK(outDeps.Append(uploadJob));
		}

		// Post index buffer upload
		{
			rkit::RCPtr<rkit::Job> uploadJob;

			AnoxBSPModelResourceGPUResources::InitCopyOp &indexInitCopy = gpuResources->m_indexesInitCopy;
			BufferInitializer::CopyOperation &copyOp = indexInitCopy.m_copyOp;
			BufferInitializer &initializer = indexInitCopy.m_initializer;

			copyOp.m_data = gpuResources->m_triIndexes.ToSpan().ReinterpretCast<const uint8_t>();
			copyOp.m_offset = 0;
			initializer.m_copyOperations = rkit::ConstSpan<BufferInitializer::CopyOperation>(&copyOp, 1);
			initializer.m_spec.m_size = copyOp.m_data.Count();
			initializer.m_resSpec.m_usage.Add({ rkit::render::BufferUsageFlag::kStorageBuffer });

			RKIT_CHECK(state.m_systems.m_graphicsSystem->CreateAsyncCreateAndFillBufferJob(&uploadJob,
				resource.m_vertexBuffer,
				gpuResources.FieldRef(&AnoxBSPModelResourceGPUResources::m_indexesInitCopy).FieldRef(&AnoxBSPModelResourceGPUResources::InitCopyOp::m_initializer),
				nullptr));
			RKIT_CHECK(outDeps.Append(uploadJob));
		}

		// Post normal buffer upload
		{
			rkit::RCPtr<rkit::Job> uploadJob;

			AnoxBSPModelResourceGPUResources::InitCopyOp &indexInitCopy = gpuResources->m_normalsInitCopy;
			BufferInitializer::CopyOperation &copyOp = indexInitCopy.m_copyOp;
			BufferInitializer &initializer = indexInitCopy.m_initializer;

			copyOp.m_data = gpuResources->m_normals.ToSpan().ReinterpretCast<const uint8_t>();
			copyOp.m_offset = 0;
			initializer.m_copyOperations = rkit::ConstSpan<BufferInitializer::CopyOperation>(&copyOp, 1);
			initializer.m_spec.m_size = copyOp.m_data.Count();
			initializer.m_resSpec.m_usage.Add({ rkit::render::BufferUsageFlag::kStorageBuffer });

			RKIT_CHECK(state.m_systems.m_graphicsSystem->CreateAsyncCreateAndFillBufferJob(&uploadJob,
				resource.m_vertexBuffer,
				gpuResources.FieldRef(&AnoxBSPModelResourceGPUResources::m_normalsInitCopy).FieldRef(&AnoxBSPModelResourceGPUResources::InitCopyOp::m_initializer),
				nullptr));
			RKIT_CHECK(outDeps.Append(uploadJob));
		}

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxBSPModelLoaderInfo::LoadContents(State_t &state, Resource_t &resource)
	{
		RKIT_RETURN_OK;
	}

	bool AnoxBSPModelLoaderInfo::PhaseHasDependencies(size_t phase)
	{
		switch (phase)
		{
		case 0:
			return true;
		default:
			return false;
		}
	}

	rkit::Result AnoxBSPModelLoaderInfo::LoadPhase(State_t &state, Resource_t &resource, size_t phase, rkit::traits::TraitRef<rkit::VectorTrait<rkit::RCPtr<rkit::Job>>> outDeps)
	{
		switch (phase)
		{
		case 0:
			return LoadHeaderAndQueueDependencies(state, resource, outDeps);
		case 1:
			return LoadContents(state, resource);
		default:
			RKIT_THROW(rkit::ResultCode::kInternalError);
		}
	}

	AnoxBSPModelLoaderInfo::ChunkReader::ChunkReader(rkit::FixedSizeMemoryStream &readStream)
		: m_readStream(readStream)
	{
	}

	template<class T>
	rkit::Result AnoxBSPModelLoaderInfo::ChunkReader::VisitMember(rkit::Span<T> &span) const
	{
		rkit::endian::LittleUInt32_t countData;
		RKIT_CHECK(m_readStream.ReadOneBinary(countData));

		uint32_t count = countData.Get();

		if (count == 0)
		{
			span = rkit::Span<T>();
		}
		else
		{
			RKIT_CHECK(m_readStream.ExtractSpan(span, count));
		}

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxBSPModelResourceLoaderBase::Create(rkit::RCPtr<AnoxBSPModelResourceLoaderBase> &outLoader)
	{
		typedef AnoxAbstractSingleFileResourceLoader<AnoxBSPModelLoaderInfo> Loader_t;

		rkit::RCPtr<Loader_t> loader;
		RKIT_CHECK(rkit::New<Loader_t>(loader));

		outLoader = std::move(loader);

		RKIT_RETURN_OK;
	}
}
