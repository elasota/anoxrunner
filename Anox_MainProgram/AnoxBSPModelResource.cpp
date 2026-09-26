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
		rkit::RCPtr<IBuffer> m_vertexBuffer;
		rkit::RCPtr<IBuffer> m_indexBuffer;
		rkit::RCPtr<IBuffer> m_normalsBuffer;
	};

	struct AnoxBSPModelResourceGPUResources final : public rkit::RefCounted
	{
		struct SurfaceSpec
		{
			float m_lightStyleVOffset;
			float m_normal[3];
		};

		struct DrawVert
		{
			float m_xyz[3];
			uint32_t m_surfaceSpec;
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
		data2::BSPFile_Instance m_instance;
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

			data2::loader::BSPFile_Loader loader;
			if (!loader.Load(state.m_instance, stream))
				RKIT_THROW(rkit::ResultCode::kDataError);
		}


		anox::AnoxResourceManagerBase &resManager = *state.m_systems.m_resManager;

		// No SafeAdd since we don't really care about overflow here
		outDeps.Reserve(state.m_instance.m_instancesOf_BSPMaterial.Count() + state.m_instance.m_instancesOf_BSPGeometryLightmap.Count());

		for (const data2::BSPMaterial &material : state.m_instance.m_instancesOf_BSPMaterial)
		{
			rkit::RCPtr<rkit::Job> job;
			rkit::Future<AnoxResourceRetrieveResult> result;
			resManager.GetContentIDKeyedResource(&job, result, resloaders::kWorldMaterialTypeCode, material.m_contentID);

			outDeps.Append(job);
		}

		for (const data2::BSPGeometryLightmap &lightmap : state.m_instance.m_instancesOf_BSPGeometryLightmap)
		{
			rkit::RCPtr<rkit::Job> job;
			rkit::Future<AnoxResourceRetrieveResult> result;
			resManager.GetContentIDKeyedResource(&job, result, resloaders::kTextureResourceTypeCode, lightmap.m_contentID);

			outDeps.Append(job);
		}

		// Validate draw clusters
		for (const data2::BSPDrawCluster &drawCluster : state.m_instance.m_dynArraysOf_BSPDrawCluster)
		{
			const size_t numVerts = drawCluster.m_drawVerts.Count();

			for (const data2::BSPDrawMaterialGroup &materialGroup : drawCluster.m_materialGroups)
			{
				for (const data2::BSPLightmapGroup &lightmapGroup : materialGroup.m_lightmapGroup)
				{
					for (const data2::BSPDrawSurface *surf : lightmapGroup.m_surfaces)
					{
						for (const data2::BSPTri &tri : surf->m_tris)
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

		// Validate tree
		for (const data2::BSPModel &model : state.m_instance.m_dynArraysOf_BSPModel)
		{
			if (model.m_rootIsLeaf)
			{
				if (model.m_treeLeafs.Count() != 1)
					RKIT_THROW(rkit::ResultCode::kDataError);
			}
			else
			{
				rkit::Span<const data2::BSPTreeNode> treeNodes = model.m_treeNodes;
				rkit::Span<const data2::BSPTreeLeaf> treeLeafs = model.m_treeLeafs;

				if (treeNodes.Count() == 0)
					RKIT_THROW(rkit::ResultCode::kDataError);

				if (treeLeafs.Count() >= 0x80000000u)
					RKIT_THROW(rkit::ResultCode::kDataError);

				const data2::BSPTreeNode &firstNode = model.m_treeNodes[0];

				if (!totalEquals(firstNode.m_numFrontNodes, firstNode.m_numBackNodes, treeNodes.Count() - 1))
					RKIT_THROW(rkit::ResultCode::kDataError);

				// Check that all split bits and offsets are correct
				uint32_t leafOffset = 0;
				for (size_t i = 0; i < treeNodes.Count(); i++)
				{
					const data2::BSPTreeNode &node = treeNodes[i];

					const bool frontIsLeaf = (node.m_numFrontNodes == 0);
					const bool backIsLeaf = (node.m_numBackNodes == 0);

					// Do back first since we're going to modify numFrontNodes if it's a leaf
					if (backIsLeaf)
					{
						if (leafOffset == treeLeafs.Count())
							RKIT_THROW(rkit::ResultCode::kDataError);

						const_cast<data2::BSPTreeNode &>(node).m_numBackNodes = (0x80000000u | (leafOffset++));
					}
					else
					{
						const data2::BSPTreeNode &backNode = treeNodes[i + node.m_numFrontNodes + 1];
						if (node.m_numBackNodes == 0 || !totalEquals(backNode.m_numFrontNodes, backNode.m_numBackNodes, node.m_numBackNodes - 1))
							RKIT_THROW(rkit::ResultCode::kDataError);
					}

					if (frontIsLeaf)
					{
						if (leafOffset == treeLeafs.Count())
							RKIT_THROW(rkit::ResultCode::kDataError);

						const_cast<data2::BSPTreeNode &>(node).m_numFrontNodes = (0x80000000u | (leafOffset++));
					}
					else
					{
						const data2::BSPTreeNode &frontNode = treeNodes[i + 1];
						if (node.m_numFrontNodes == 0 || !totalEquals(frontNode.m_numFrontNodes, frontNode.m_numBackNodes, node.m_numFrontNodes - 1))
							RKIT_THROW(rkit::ResultCode::kDataError);
					}
				}

				if (leafOffset != treeLeafs.Count())
					RKIT_THROW(rkit::ResultCode::kDataError);
			}
		}

		rkit::RCPtr<AnoxBSPModelResourceGPUResources> gpuResources = rkit::NewRC<AnoxBSPModelResourceGPUResources>();

		// Post vertex buffer upload
#if 0
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

			copyOp.m_data = gpuResources->m_triIndexes.ToSpan().ReinterpretCast<const uint8_t>();
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

			AnoxBSPModelResourceGPUResources::InitCopyOp &indexInitCopy = gpuResources->m_normalsInitCopy;
			BufferInitializer::CopyOperation &copyOp = indexInitCopy.m_copyOp;
			BufferInitializer &initializer = indexInitCopy.m_initializer;

			copyOp.m_data = gpuResources->m_normals.ToSpan().ReinterpretCast<const uint8_t>();
			copyOp.m_offset = 0;
			initializer.m_copyOperations = rkit::ConstSpan<BufferInitializer::CopyOperation>(&copyOp, 1);
			initializer.m_spec.m_size = copyOp.m_data.Count();
			initializer.m_resSpec.m_usage.Add({ rkit::render::BufferUsageFlag::kStorageBuffer });

			state.m_systems.m_graphicsSystem->CreateAsyncCreateAndFillBufferJob(&uploadJob,
				resource.m_vertexBuffer,
				gpuResources.FieldRef(&AnoxBSPModelResourceGPUResources::m_normalsInitCopy).FieldRef(&AnoxBSPModelResourceGPUResources::InitCopyOp::m_initializer),
				nullptr);
			outDeps.Append(uploadJob);
		}
#endif

		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
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
