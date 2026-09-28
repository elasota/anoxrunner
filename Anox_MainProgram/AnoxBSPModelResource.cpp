#include "AnoxBSPModelResource.h"
#include "AnoxMaterialResource.h"
#include "AnoxAbstractSingleFileResource.h"
#include "AnoxGameFileSystem.h"

#include "anox/Data/CompressedNormal.h"
#include "anox/Data/AnoxBSPFileLoader.generated.h"
#include "anox/Data/AnoxBSPFileLoader.loader.generated.h"

#include "anox/Data/EntityStructs.h"
#include "anox/Data/ResourceTypeCodes.h"

#include "AnoxGraphicsSubsystem.h"
#include "AnoxMaterialResource.h"
#include "AnoxTextureResource.h"

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
		rkit::Vector<DrawClusterVertRange> m_drawClusterVertRanges;
		rkit::Vector<DrawSurfTriRange> m_drawSurfTriRanges;

		data::BSPFile_Instance m_instance;

		rkit::Vector<rkit::RCPtr<AnoxTextureResourceBase>> m_lightMaps;
		rkit::Vector<rkit::RCPtr<AnoxMaterialResource>> m_materials;

		rkit::RCPtr<IBuffer> m_vertexBuffer;
		rkit::RCPtr<IBuffer> m_indexBuffer;
		rkit::RCPtr<IBuffer> m_normalsBuffer;
	};

	struct AnoxBSPModelResourceGPUResources final : public rkit::RefCounted
	{
		struct DrawVert
		{
			float m_xyz[3];
			uint32_t m_surfaceSpec;
			float m_uv[2];
			float m_lightUV[2];
		};

		struct SurfaceSpec
		{
			float m_lightStyleVOffset;
			float m_normal[3];
		};

		struct Tri
		{
			uint32_t m_indexes[3];
		};

		struct InitCopyOp
		{
			BufferInitializer m_initializer;
			BufferInitializer::CopyOperation m_copyOp;
		};

		rkit::Vector<SurfaceSpec> m_surfaceSpecs;
		rkit::Vector<DrawVert> m_verts;
		rkit::Vector<Tri> m_tris;

		InitCopyOp m_surfaceSpecInitCopy;
		InitCopyOp m_vertsInitCopy;
		InitCopyOp m_indexesInitCopy;

		// Keepalive for buffer upload tasks, since it stores the destination buffers
		rkit::RCPtr<AnoxResourceBase> m_resourceKeepAlive;
	};

	struct AnoxBSPModelResourceLoaderState final : public AnoxAbstractSingleFileResourceLoaderState
	{
		rkit::Vector<rkit::Future<AnoxResourceRetrieveResult>> m_materials;
		rkit::Vector<rkit::Future<AnoxResourceRetrieveResult>> m_lightMaps;
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
		rkit::IUtilitiesDriver &utils = *rkit::GetDrivers().m_utilitiesDriver;

		{
			rkit::FixedSizeMemoryStream stream(state.m_fileContents.GetBuffer(), state.m_fileContents.Count());

			data::loader::BSPFile_Loader loader;
			if (!loader.Load(resource.m_instance, stream))
				RKIT_THROW(rkit::ResultCode::kDataError);
		}

		anox::AnoxResourceManagerBase &resManager = *state.m_systems.m_resManager;

		// No SafeAdd since we don't really care about overflow here
		outDeps.Reserve(resource.m_instance.m_instancesOf_BSPMaterial.Count() + resource.m_instance.m_instancesOf_BSPGeometryLightmap.Count());

		for (const data::BSPMaterial &material : resource.m_instance.m_instancesOf_BSPMaterial)
		{
			rkit::RCPtr<rkit::Job> job;
			rkit::Future<AnoxResourceRetrieveResult> result;
			resManager.GetContentIDKeyedResource(&job, result, resloaders::kWorldMaterialTypeCode, material.m_contentID);

			state.m_materials.Append(result);

			outDeps.Append(job);
		}

		for (const data::BSPGeometryLightmap &lightmap : resource.m_instance.m_instancesOf_BSPGeometryLightmap)
		{
			rkit::RCPtr<rkit::Job> job;
			rkit::Future<AnoxResourceRetrieveResult> result;
			resManager.GetContentIDKeyedResource(&job, result, resloaders::kTextureResourceTypeCode, lightmap.m_contentID);

			state.m_lightMaps.Append(result);

			outDeps.Append(job);
		}

		// Validate draw clusters
		for (const data::BSPDrawCluster &drawCluster : resource.m_instance.m_dynArraysOf_BSPDrawCluster)
		{
			const size_t numVerts = drawCluster.m_drawVerts.Count();

			for (const data::BSPDrawMaterialGroup &materialGroup : drawCluster.m_materialGroups)
			{
				for (const data::BSPLightmapGroup &lightmapGroup : materialGroup.m_lightmapGroup)
				{
					for (const data::BSPDrawSurface *surf : lightmapGroup.m_surfaces)
					{
						for (const data::BSPTri &tri : surf->m_tris)
						{
							for (uint16_t triIndex : tri.m_indexes)
							{
								if (triIndex >= numVerts)
									RKIT_THROW(rkit::ResultCode::kDataError);
							}
						}
					}
				}
			}
		}

		auto totalEquals = [](const size_t a, const size_t b, const size_t expected) -> bool
			{
				if (a > expected)
					return false;

				return (expected - a) == b;
			};

		// Validate tree and convert leafs into tagged offsets
		for (const data::BSPModel &model : resource.m_instance.m_dynArraysOf_BSPModel)
		{
			if (model.m_rootIsLeaf)
			{
				if (model.m_treeLeafs.Count() != 1 || model.m_treeNodes.Count() != 0)
					RKIT_THROW(rkit::ResultCode::kDataError);
			}
			else
			{
				rkit::Span<const data::BSPTreeNode> treeNodes = model.m_treeNodes;
				rkit::Span<const data::BSPTreeLeaf> treeLeafs = model.m_treeLeafs;

				if (treeNodes.Count() == 0)
					RKIT_THROW(rkit::ResultCode::kDataError);

				if (treeLeafs.Count() >= 0x80000000u)
					RKIT_THROW(rkit::ResultCode::kDataError);

				const data::BSPTreeNode &firstNode = model.m_treeNodes[0];

				if (!totalEquals(firstNode.m_numFrontNodes, firstNode.m_numBackNodes, treeNodes.Count() - 1))
					RKIT_THROW(rkit::ResultCode::kDataError);

				// Check that all split bits and offsets are correct
				uint32_t leafOffset = 0;
				for (size_t i = 0; i < treeNodes.Count(); i++)
				{
					const data::BSPTreeNode &node = treeNodes[i];

					const bool frontIsLeaf = (node.m_numFrontNodes == 0);
					const bool backIsLeaf = (node.m_numBackNodes == 0);

					// Do back first since we're going to modify numFrontNodes if it's a leaf
					if (backIsLeaf)
					{
						if (leafOffset == treeLeafs.Count())
							RKIT_THROW(rkit::ResultCode::kDataError);

						const_cast<data::BSPTreeNode &>(node).m_numBackNodes = (0x80000000u | (leafOffset++));
					}
					else
					{
						const data::BSPTreeNode &backNode = treeNodes[i + node.m_numFrontNodes + 1];
						if (node.m_numBackNodes == 0 || !totalEquals(backNode.m_numFrontNodes, backNode.m_numBackNodes, node.m_numBackNodes - 1))
							RKIT_THROW(rkit::ResultCode::kDataError);
					}

					if (frontIsLeaf)
					{
						if (leafOffset == treeLeafs.Count())
							RKIT_THROW(rkit::ResultCode::kDataError);

						const_cast<data::BSPTreeNode &>(node).m_numFrontNodes = (0x80000000u | (leafOffset++));
					}
					else
					{
						const data::BSPTreeNode &frontNode = treeNodes[i + 1];
						if (node.m_numFrontNodes == 0 || !totalEquals(frontNode.m_numFrontNodes, frontNode.m_numBackNodes, node.m_numFrontNodes - 1))
							RKIT_THROW(rkit::ResultCode::kDataError);
					}
				}

				if (leafOffset != treeLeafs.Count())
					RKIT_THROW(rkit::ResultCode::kDataError);
			}
		}

		rkit::RCPtr<AnoxBSPModelResourceGPUResources> gpuResources = rkit::NewRC<AnoxBSPModelResourceGPUResources>();

		rkit::Span<const data::BSPSurfaceSpec> inSurfaceSpecs = resource.m_instance.m_instancesOf_BSPSurfaceSpec.ToSpan();
		rkit::Span<const data::BSPDrawVertex> inVerts = resource.m_instance.m_dynArraysOf_BSPDrawVertex.ToSpan();
		rkit::Span<const data::BSPTri> inTris = resource.m_instance.m_dynArraysOf_BSPTri.ToSpan();

		gpuResources->m_tris.Resize(inTris.Count());
		gpuResources->m_surfaceSpecs.Resize(inSurfaceSpecs.Count());
		gpuResources->m_verts.Resize(inVerts.Count());

		rkit::ProcessParallelSpans(gpuResources->m_tris.ToSpan(), inTris,
			[](AnoxBSPModelResourceGPUResources::Tri &outTri, const data::BSPTri &inTri)
			{
				for (size_t i = 0; i < 3; i++)
					outTri.m_indexes[i] = inTri.m_indexes[i];
			});

		rkit::ProcessParallelSpans(gpuResources->m_surfaceSpecs.ToSpan(), inSurfaceSpecs,
			[](AnoxBSPModelResourceGPUResources::SurfaceSpec &outSpec, const data::BSPSurfaceSpec &inSpec)
			{
				outSpec.m_lightStyleVOffset = inSpec.m_lightStyleVOffset;
				for (size_t i = 0; i < 3; i++)
					outSpec.m_normal[i] = inSpec.m_normal[i];
			});

		rkit::ProcessParallelSpans(gpuResources->m_verts.ToSpan(), inVerts,
			[inSurfaceSpecs](AnoxBSPModelResourceGPUResources::DrawVert &outVert, const data::BSPDrawVertex &inVert)
			{
				outVert.m_surfaceSpec = inVert.m_surfaceSpec - inSurfaceSpecs.Ptr();
				for (size_t i = 0; i < 3; i++)
					outVert.m_xyz[i] = inVert.m_xyz[i];
				for (size_t i = 0; i < 2; i++)
				{
					outVert.m_lightUV[i] = inVert.m_lightUV[i];
					outVert.m_uv[i] = inVert.m_uv[i];
				}
			});

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

			state.m_systems.m_graphicsSystem->CreateAsyncCreateAndFillBufferJob(&uploadJob,
				resource.m_vertexBuffer,
				gpuResources.FieldRef(&AnoxBSPModelResourceGPUResources::m_vertsInitCopy).FieldRef(&AnoxBSPModelResourceGPUResources::InitCopyOp::m_initializer),
				nullptr);
			outDeps.Append(uploadJob);
		}

		// Post index buffer upload
		{
			rkit::RCPtr<rkit::Job> uploadJob;

			AnoxBSPModelResourceGPUResources::InitCopyOp &indexInitCopy = gpuResources->m_indexesInitCopy;
			BufferInitializer::CopyOperation &copyOp = indexInitCopy.m_copyOp;
			BufferInitializer &initializer = indexInitCopy.m_initializer;

			copyOp.m_data = gpuResources->m_tris.ToSpan().ReinterpretCast<const uint8_t>();
			copyOp.m_offset = 0;
			initializer.m_copyOperations = rkit::ConstSpan<BufferInitializer::CopyOperation>(&copyOp, 1);
			initializer.m_spec.m_size = copyOp.m_data.Count();
			initializer.m_resSpec.m_usage.Add({ rkit::render::BufferUsageFlag::kStorageBuffer });

			state.m_systems.m_graphicsSystem->CreateAsyncCreateAndFillBufferJob(&uploadJob,
				resource.m_vertexBuffer,
				gpuResources.FieldRef(&AnoxBSPModelResourceGPUResources::m_indexesInitCopy).FieldRef(&AnoxBSPModelResourceGPUResources::InitCopyOp::m_initializer),
				nullptr);
			outDeps.Append(uploadJob);
		}

		// Post normal buffer upload
		{
			rkit::RCPtr<rkit::Job> uploadJob;

			AnoxBSPModelResourceGPUResources::InitCopyOp &surfaceSpecInitCopy = gpuResources->m_surfaceSpecInitCopy;
			BufferInitializer::CopyOperation &copyOp = surfaceSpecInitCopy.m_copyOp;
			BufferInitializer &initializer = surfaceSpecInitCopy.m_initializer;

			copyOp.m_data = gpuResources->m_surfaceSpecs.ToSpan().ReinterpretCast<const uint8_t>();
			copyOp.m_offset = 0;
			initializer.m_copyOperations = rkit::ConstSpan<BufferInitializer::CopyOperation>(&copyOp, 1);
			initializer.m_spec.m_size = copyOp.m_data.Count();
			initializer.m_resSpec.m_usage.Add({ rkit::render::BufferUsageFlag::kStorageBuffer });

			state.m_systems.m_graphicsSystem->CreateAsyncCreateAndFillBufferJob(&uploadJob,
				resource.m_vertexBuffer,
				gpuResources.FieldRef(&AnoxBSPModelResourceGPUResources::m_surfaceSpecInitCopy).FieldRef(&AnoxBSPModelResourceGPUResources::InitCopyOp::m_initializer),
				nullptr);
			outDeps.Append(uploadJob);
		}

		// Clear temp resources
		resource.m_drawClusterVertRanges.Resize(resource.m_instance.m_dynArraysOf_BSPDrawCluster.Count());
		resource.m_drawSurfTriRanges.Resize(resource.m_instance.m_instancesOf_BSPDrawSurface.Count());

		rkit::ProcessParallelSpans(resource.m_drawClusterVertRanges.ToSpan(), resource.m_instance.m_dynArraysOf_BSPDrawCluster.ToSpan(),
			[inVerts](AnoxBSPModelResource::DrawClusterVertRange &vertRange, data::BSPDrawCluster &drawCluster)
			{
				vertRange.m_firstVert = drawCluster.m_drawVerts.Ptr() - inVerts.Ptr();
				vertRange.m_numVerts = drawCluster.m_drawVerts.Count();

				drawCluster.m_drawVerts = rkit::Span<const data::BSPDrawVertex>();
			});

		rkit::ProcessParallelSpans(resource.m_drawSurfTriRanges.ToSpan(), resource.m_instance.m_instancesOf_BSPDrawSurface.ToSpan(),
			[inTris](AnoxBSPModelResource::DrawSurfTriRange &triRange, data::BSPDrawSurface &drawSurf)
			{
				triRange.m_firstTri = drawSurf.m_tris.Ptr() - inTris.Ptr();
				triRange.m_numTris = drawSurf.m_tris.Count();

				drawSurf.m_tris = rkit::Span<const data::BSPTri>();
			});

		rkit::Vector<AnoxBSPModelResourceBase::DrawSurfTriRange> m_drawSurfTriRanges;

		resource.m_instance.m_dynArraysOf_BSPTri.Reset();
		resource.m_instance.m_dynArraysOf_BSPDrawVertex.Reset();
		resource.m_instance.m_instancesOf_BSPSurfaceSpec.Reset();	// Surface specs are only referenced by verts
	}

	rkit::Result AnoxBSPModelLoaderInfo::LoadContents(State_t &state, Resource_t &resource)
	{
		resource.m_lightMaps.Resize(state.m_lightMaps.Count());
		resource.m_materials.Resize(state.m_materials.Count());

		rkit::ProcessParallelSpans(resource.m_lightMaps.ToSpan(), state.m_lightMaps.ToSpan(),
			[](rkit::RCPtr<AnoxTextureResourceBase> &outTexture, const rkit::Future<AnoxResourceRetrieveResult> &resourceResult)
			{
				outTexture = resourceResult.GetResult().m_resourceHandle.StaticCast<AnoxTextureResourceBase>();
			});

		rkit::ProcessParallelSpans(resource.m_materials.ToSpan(), state.m_materials.ToSpan(),
			[](rkit::RCPtr<AnoxMaterialResource> &outMaterial, const rkit::Future<AnoxResourceRetrieveResult> &resourceResult)
			{
				outMaterial = resourceResult.GetResult().m_resourceHandle.StaticCast<AnoxMaterialResource>();
			});


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
		m_readStream.ReadOneBinary(countData);

		uint32_t count = countData.Get();

		if (count == 0)
		{
			span = rkit::Span<T>();
		}
		else
		{
			m_readStream.ExtractSpan(span, count);
		}

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxBSPModelResourceLoaderBase::Create(rkit::RCPtr<AnoxBSPModelResourceLoaderBase> &outLoader)
	{
		typedef AnoxAbstractSingleFileResourceLoader<AnoxBSPModelLoaderInfo> Loader_t;

		rkit::RCPtr<Loader_t> loader = rkit::NewRC<Loader_t>();

		outLoader = std::move(loader);

		RKIT_RETURN_OK;
	}
}
