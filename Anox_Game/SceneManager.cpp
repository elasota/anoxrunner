#include "SceneManager.h"

#include "rkit/Core/Coroutine.h"
#include "rkit/Core/HashTable.h"
#include "rkit/Core/MemoryStream.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/RefCounted.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/String.h"
#include "rkit/Core/Vector.h"

#include "rkit/Math/Vec.h"

#include "rkit/Data/ContentID.h"

#include "anox/Data/ResourceTypeCodes.h"
#include "anox/Data/Scene.h"
#include "anox/Data/SceneCommandOpcodes.generated.h"

#include "GameObjects/Scene.h"

#include "AnoxWorldObjectFactory.h"
#include "SandboxResourceLoader.h"
#include "World.h"

namespace anox::game
{
	enum class SceneFocusType : uint8_t
	{
		kNone,
		kScaledSource,
		kScaledTarget,
		kInscribed,
		kCircumscribed,
		kFixedSource,
		kFixedTarget,
		kFixedInscribed,

		kCount,
	};

	class ScenePackage final: public rkit::RefCounted
	{
	public:
		void RCTrackerZero() override;

		static constexpr size_t kNumPathTypes = static_cast<size_t>(data::ScenePathType::kCount);
		static constexpr size_t kNumContentTypes = static_cast<size_t>(data::SceneContentRefType::kCount);

		struct Path
		{
			data::ScenePathType m_pathType = data::ScenePathType::kCount;

			const void *m_firstNode = nullptr;

			uint32_t m_timeOffs = 0;
			uint32_t m_maxLen = 0;
			size_t m_numNodes = 0;
		};

		struct Block
		{
			rkit::ConstSpan<Path> m_paths;
			uint32_t m_flags = 0;
		};

		struct Resource
		{
			rkit::data::ContentID m_contentID;
			SandboxResourceHandle m_resHandle;
		};

		struct NodeBase
		{
			uint32_t m_flags = 0;
			uint32_t m_timeLen = 0;
		};

		struct CubicNode : public NodeBase
		{
			rkit::math::Vec3 m_position;
			rkit::math::Vec3 m_velocity;
			SceneFocusType m_relativeMode = SceneFocusType::kCount;
		};

		struct FocusNode : public NodeBase
		{
			uint32_t m_focusTarget = 0;
		};

		struct CommandNode : public NodeBase
		{
			rkit::ConstSpan<data::SceneCommandOpcode> m_commandOpcodes;
			rkit::ConstSpan<uint32_t> m_commandParamDWords;
		};

		struct ScaleNode : public NodeBase
		{
			rkit::math::Vec3 m_scale;
			rkit::math::Vec3 m_delta;
		};

		struct RollNode : public NodeBase
		{
			float m_value = 0.f;
			float m_rate = 0.f;
		};

		struct FOVNode : public NodeBase
		{
			float m_value = 0.f;
			float m_rate = 0.f;
		};

		uint32_t m_cineID = 0;
		bool m_isInterrupt = false;

		rkit::Vector<rkit::ByteString> m_strings;
		rkit::Vector<Block> m_blocks;
		rkit::Vector<Path> m_paths;

		rkit::Vector<CubicNode> m_cubic;
		rkit::Vector<FocusNode> m_focus;
		rkit::Vector<CommandNode> m_command;
		rkit::Vector<ScaleNode> m_scale;
		rkit::Vector<RollNode> m_roll;
		rkit::Vector<FOVNode> m_fov;

		rkit::Vector<data::SceneCommandOpcode> m_commandOpcodes;
		rkit::Vector<uint32_t> m_commandParamDWords;

		rkit::StaticArray<rkit::Vector<Resource>, kNumContentTypes> m_content;

		size_t m_scenePackageID = 0;
		SceneManagerImpl *m_sceneManager = nullptr;
	};

	class SceneManagerImpl final : public rkit::OpaqueImplementation<SceneManager>
	{
		friend class SceneManager;

	public:
		explicit SceneManagerImpl(World &world);

		rkit::ResultCoroutine RunScene(rkit::ICoroThread &thread, rkit::ByteStringSliceView name, rkit::data::ContentID cid, bool loop);
		rkit::ResultCoroutine RunFrame(rkit::ICoroThread &thread);

		void ReleaseScenePackage(size_t scenePackageID);

	private:
		struct ActiveScene : public rkit::RefCounted
		{
			rkit::ByteString m_name;
			rkit::data::ContentID m_contentID;
		};

		struct SceneObjectDeserializer
		{
			void *m_outVector = nullptr;
			const void *m_userData = nullptr;
			rkit::Result (*m_resizeOutVectorFunc)(void *outVector, size_t size);
			void* (*m_getVectorBufferFunc)(void *outVector);
			rkit::Result (*m_deserializeOneFunc)(const void *userdata, void *outItem, rkit::ReadOnlyMemoryStream &stream);
			bool m_isValid = false;
			size_t m_nodeStride = 0;

			size_t m_currentNodeOffset = 0;
		};

		template<class TDataType, class TVectorItem, class TFunc>
		static SceneObjectDeserializer CreateSceneDeserializer(ScenePackage& package, rkit::Vector<TVectorItem> &outVector, const TFunc &func);

		static rkit::Result LoadScenePackage(rkit::RCPtr<ScenePackage> &outPackage, rkit::ConstSpan<uint8_t> data);
		static rkit::math::Vec3 LoadVec3Data(const rkit::endian::LittleFloat32_t(&data)[3]);

		// SAVEGAME TODO
		struct ScenePackageRefAndFreeID
		{
			ScenePackage *m_package = nullptr;
			size_t m_freeID = 0;
		};

		rkit::Vector<ScenePackageRefAndFreeID> m_scenePackageList;
		size_t m_numFreeIDs = 0;

		rkit::HashMap<rkit::data::ContentID, ScenePackage *> m_scenePackageMap;

		World &m_world;
	};

	void ScenePackage::RCTrackerZero()
	{
		if (m_scenePackageID != 0)
			m_sceneManager->ReleaseScenePackage(m_scenePackageID);

		RefCounted::RCTrackerZero();
	}

	SceneManagerImpl::SceneManagerImpl(World &world)
		: m_world(world)
	{
	}

	rkit::ResultCoroutine SceneManagerImpl::RunScene(rkit::ICoroThread &thread, rkit::ByteStringSliceView name, rkit::data::ContentID cid, bool loop)
	{
		SandboxResourceRequestHandle sceneReqHandle;
		CORO_CHECK(SandboxResourceLoader::LoadContentKeyedResource(sceneReqHandle, resloaders::kContentIDRawFileResourceTypeCode, cid));

		SandboxResourceHandle sceneResHandle;
		CORO_CHECK(co_await sceneReqHandle.WaitForLoaded(thread, sceneResHandle));

		SandboxResourceDataBlob blob;
		CORO_CHECK(SandboxResourceLoader::GetFileResourceContents(blob, sceneResHandle));

		rkit::RCPtr<ScenePackage> scenePackage;

		rkit::HashValue_t cidHash = rkit::Hasher<rkit::data::ContentID>::ComputeHash(0, cid);

		rkit::HashMap<rkit::data::ContentID, ScenePackage *>::ConstIterator_t it = m_scenePackageMap.FindPrehashed(cidHash, cid);

		if (it != m_scenePackageMap.end())
			scenePackage = it.Value();
		else
		{
			CORO_CHECK(LoadScenePackage(scenePackage, blob.GetContents()));

			if (m_numFreeIDs == 0)
			{
				CORO_CHECK(m_scenePackageList.Append(ScenePackageRefAndFreeID()));

				m_numFreeIDs = 1;
				m_scenePackageList[0].m_freeID = m_scenePackageList.Count();
			}

			CORO_CHECK(m_scenePackageMap.SetPrehashed(cidHash, cid, scenePackage.Get()));


			const size_t packageID = m_scenePackageList[--m_numFreeIDs].m_freeID;
			m_scenePackageList[packageID - 1].m_package = scenePackage.Get();

			scenePackage->m_scenePackageID = packageID;
			scenePackage->m_sceneManager = this;
		}

		CORO_RETURN_OK;
	}


	void SceneManagerImpl::ReleaseScenePackage(size_t scenePackageID)
	{
		RKIT_ASSERT(scenePackageID != 0);

		m_scenePackageList[scenePackageID - 1].m_package = nullptr;
		m_scenePackageList[m_numFreeIDs++].m_freeID = scenePackageID;
	}

	template<class TDataType, class TVectorItem, class TFunc>
	SceneManagerImpl::SceneObjectDeserializer SceneManagerImpl::CreateSceneDeserializer(ScenePackage &package, rkit::Vector<TVectorItem> &outVector, const TFunc &func)
	{
		SceneObjectDeserializer result = {};
		result.m_outVector = &outVector;
		result.m_userData = &func;
		result.m_resizeOutVectorFunc = [](void *outVectorPtr, size_t size) -> rkit::Result
			{
				return static_cast<rkit::Vector<TVectorItem>*>(outVectorPtr)->Resize(size);
			};
		result.m_getVectorBufferFunc = [](void *outVectorPtr) -> void*
			{
				return static_cast<rkit::Vector<TVectorItem>*>(outVectorPtr)->GetBuffer();
			};

		result.m_deserializeOneFunc = [](const void *userdata, void *outItem, rkit::ReadOnlyMemoryStream &stream) -> rkit::Result
			{
				const TFunc &funcRef = *static_cast<const TFunc *>(userdata);
				TVectorItem *outItemTyped = static_cast<TVectorItem *>(outItem);

				TDataType data = {};
				RKIT_CHECK(stream.ReadOneBinary(data));

				const data::SceneNodeCommon &inCommon = data.m_common;
				ScenePackage::NodeBase &outCommon = *outItemTyped;

				outCommon.m_flags = inCommon.m_flags.Get();
				outCommon.m_timeLen = inCommon.m_timeLen.Get();

				RKIT_CHECK(funcRef(*outItemTyped, data));

				RKIT_RETURN_OK;
			};

		result.m_isValid = true;
		result.m_nodeStride = sizeof(TVectorItem);
		result.m_currentNodeOffset = 0;

		return result;
	}

	rkit::Result SceneManagerImpl::LoadScenePackage(rkit::RCPtr<ScenePackage> &outPackage, rkit::ConstSpan<uint8_t> data)
	{
		rkit::ReadOnlyMemoryStream stream(data);

		data::SceneHeader header;
		RKIT_CHECK(stream.ReadOneBinary(header));

		if (header.m_magic.Get() != data::SceneHeader::kExpectedMagic)
			RKIT_THROW(rkit::ResultCode::kDataError);

		rkit::RCPtr<ScenePackage> package;
		RKIT_CHECK(rkit::New<ScenePackage>(package));

		package->m_cineID = header.m_cineID.Get();
		package->m_isInterrupt = (header.m_isInterrupt != 0);

		{
			rkit::Vector<rkit::ByteStringConstructionBuffer> stringCBufs;
			RKIT_CHECK(stringCBufs.Resize(header.m_numStrings.Get()));

			for (rkit::ByteStringConstructionBuffer &cbuf : stringCBufs)
			{
				rkit::endian::LittleUInt32_t lengthData = {};
				RKIT_CHECK(stream.ReadOneBinary(lengthData));

				RKIT_CHECK(cbuf.Allocate(lengthData.Get()));
			}

			for (rkit::ByteStringConstructionBuffer &cbuf : stringCBufs)
			{
				RKIT_CHECK(stream.ReadAllSpan(cbuf.GetSpan()));
			}

			RKIT_CHECK(package->m_strings.Resize(stringCBufs.Count()));
			rkit::ProcessParallelSpans(package->m_strings.ToSpan(), stringCBufs.ToSpan(), [](rkit::ByteString &outString, rkit::ByteStringConstructionBuffer &inString)
				{
					outString = rkit::ByteString(std::move(inString));
				});
		}

		RKIT_CHECK(package->m_blocks.Resize(header.m_numBlocks.Get()));
		RKIT_CHECK(package->m_paths.Resize(header.m_numPaths.Get()));


		{
			size_t pathOffset = 0;

			for (ScenePackage::Block &outBlock : package->m_blocks)
			{
				data::SceneBlock inBlock;
				RKIT_CHECK(stream.ReadOneBinary(inBlock));

				const uint32_t numPaths = inBlock.m_numPaths.Get();

				if (package->m_paths.Count() - pathOffset < numPaths)
					RKIT_THROW(rkit::ResultCode::kDataError);

				outBlock.m_paths = package->m_paths.ToSpan().SubSpan(pathOffset, numPaths);
				pathOffset += numPaths;
			}
		}

		constexpr size_t kNumPathTypes = static_cast<size_t>(data::ScenePathType::kCount);

		const auto processOneCubic = [](ScenePackage::CubicNode &outNode, data::SceneCubicNode &inNode) -> rkit::Result
			{
				const uint32_t relMode = inNode.m_relativeMode.Get();

				if (relMode > static_cast<size_t>(SceneFocusType::kCount))
					RKIT_THROW(rkit::ResultCode::kDataError);

				outNode.m_position = LoadVec3Data(inNode.m_position);
				outNode.m_velocity = LoadVec3Data(inNode.m_velocity);
				outNode.m_relativeMode = static_cast<SceneFocusType>(inNode.m_relativeMode.Get());

				RKIT_RETURN_OK;
			};

		const auto processOneFocus = [](ScenePackage::FocusNode &outNode, data::SceneFocusNode &inNode) -> rkit::Result
			{
				outNode.m_focusTarget = inNode.m_focusTarget.Get();

				RKIT_RETURN_OK;
			};

		const auto processOneCommand = [](ScenePackage::CommandNode &outNode, data::SceneCommandNode &inNode) -> rkit::Result
			{
				outNode.m_commandOpcodes = rkit::ConstSpan<data::SceneCommandOpcode>(nullptr, inNode.m_numCommands.Get());
				outNode.m_commandParamDWords = rkit::ConstSpan<uint32_t>(nullptr, inNode.m_numParamDWords.Get());

				RKIT_RETURN_OK;
			};

		const auto processOneScale = [](ScenePackage::ScaleNode &outNode, data::SceneScaleNode &inNode) -> rkit::Result
			{
				outNode.m_scale = LoadVec3Data(inNode.m_scale);
				outNode.m_delta = LoadVec3Data(inNode.m_delta);

				RKIT_RETURN_OK;
			};

		const auto processOneRoll = [](ScenePackage::RollNode &outNode, data::SceneRollNode &inNode) -> rkit::Result
			{
				outNode.m_value = inNode.m_value.Get();
				outNode.m_rate = inNode.m_rate.Get();

				RKIT_RETURN_OK;
			};

		const auto processOneFOV = [](ScenePackage::FOVNode &outNode, data::SceneFOVNode &inNode) -> rkit::Result
			{
				outNode.m_value = inNode.m_value.Get();
				outNode.m_rate = inNode.m_rate.Get();

				RKIT_RETURN_OK;
			};

		rkit::StaticArray<SceneObjectDeserializer, kNumPathTypes> deserializers;
		deserializers[static_cast<size_t>(data::ScenePathType::kCubic)] = CreateSceneDeserializer<data::SceneCubicNode>(*package, package->m_cubic, processOneCubic);
		deserializers[static_cast<size_t>(data::ScenePathType::kFocus)] = CreateSceneDeserializer<data::SceneFocusNode>(*package, package->m_focus, processOneFocus);
		deserializers[static_cast<size_t>(data::ScenePathType::kCommand)] = CreateSceneDeserializer<data::SceneCommandNode>(*package, package->m_command, processOneCommand);
		deserializers[static_cast<size_t>(data::ScenePathType::kScale)] = CreateSceneDeserializer<data::SceneScaleNode>(*package, package->m_scale, processOneScale);
		deserializers[static_cast<size_t>(data::ScenePathType::kRoll)] = CreateSceneDeserializer<data::SceneRollNode>(*package, package->m_roll, processOneRoll);
		deserializers[static_cast<size_t>(data::ScenePathType::kFOV)] = CreateSceneDeserializer<data::SceneFOVNode>(*package, package->m_fov, processOneFOV);

		for (size_t i = 0; i < kNumPathTypes; i++)
		{
			SceneObjectDeserializer &deserializer = deserializers[i];

			RKIT_ASSERT(deserializer.m_isValid);

			RKIT_CHECK(deserializer.m_resizeOutVectorFunc(deserializer.m_outVector, header.m_nodeCounts[i].Get()));
		}

		for (ScenePackage::Path &outPath : package->m_paths)
		{
			data::ScenePath inPath;
			RKIT_CHECK(stream.ReadOneBinary(inPath));

			const size_t typeIndex = static_cast<size_t>(inPath.m_pathType);

			if (typeIndex >= kNumPathTypes)
				RKIT_THROW(rkit::ResultCode::kDataError);

			SceneObjectDeserializer &deserializer = deserializers[typeIndex];

			const void *outVectorBuffer = deserializer.m_getVectorBufferFunc(deserializer.m_outVector);

			const uint32_t numNodes = inPath.m_numNodes.Get();

			if (header.m_nodeCounts[typeIndex].Get() - deserializer.m_currentNodeOffset < numNodes)
				RKIT_THROW(rkit::ResultCode::kDataError);

			outPath.m_firstNode = static_cast<const uint8_t *>(outVectorBuffer) + deserializer.m_nodeStride * deserializer.m_currentNodeOffset;
			outPath.m_numNodes = numNodes;
			outPath.m_timeOffs = inPath.m_timeOffs.Get();
			outPath.m_pathType = inPath.m_pathType;
			outPath.m_maxLen = inPath.m_maxLen.Get();

			deserializer.m_currentNodeOffset += numNodes;
		}

		for (size_t typeIndex = 0; typeIndex < kNumPathTypes; typeIndex++)
		{
			SceneObjectDeserializer &deserializer = deserializers[typeIndex];

			const uint32_t numNodes = header.m_nodeCounts[typeIndex].Get();

			uint8_t *currentOutNodeAddr = static_cast<uint8_t *>(deserializer.m_getVectorBufferFunc(deserializer.m_outVector));

			for (size_t nodeIndex = 0; nodeIndex < numNodes; nodeIndex++)
			{
				RKIT_CHECK(deserializer.m_deserializeOneFunc(deserializer.m_userData, currentOutNodeAddr, stream));
				currentOutNodeAddr += deserializer.m_nodeStride;
			}
		}

		size_t numCommandOpcodes = 0;
		size_t numCommandParamDWords = 0;
		for (const ScenePackage::CommandNode &cmd : package->m_command)
		{
			RKIT_CHECK(rkit::SafeAdd<size_t>(numCommandOpcodes, numCommandOpcodes, cmd.m_commandOpcodes.Count()));
			RKIT_CHECK(rkit::SafeAdd<size_t>(numCommandParamDWords, numCommandParamDWords, cmd.m_commandParamDWords.Count()));
		}

		RKIT_CHECK(package->m_commandOpcodes.Resize(numCommandOpcodes));
		RKIT_CHECK(package->m_commandParamDWords.Resize(numCommandParamDWords));

		{
			size_t opcodeOffset = 0;
			size_t paramDWordOffset = 0;
			for (ScenePackage::CommandNode &cmd : package->m_command)
			{
				cmd.m_commandOpcodes = package->m_commandOpcodes.ToSpan().SubSpan(opcodeOffset, cmd.m_commandOpcodes.Count());
				cmd.m_commandParamDWords = package->m_commandParamDWords.ToSpan().SubSpan(paramDWordOffset, cmd.m_commandParamDWords.Count());
				opcodeOffset += cmd.m_commandOpcodes.Count();
				paramDWordOffset += cmd.m_commandParamDWords.Count();
			}
		}

		RKIT_CHECK(stream.ReadAllSpan(package->m_commandOpcodes.ToSpan()));
		RKIT_CHECK(stream.ReadAllSpan(package->m_commandParamDWords.ToSpan()));

		for (uint32_t &dword : package->m_commandParamDWords)
			rkit::endian::LittleUInt32_t::StaticConvertToHostOrderInPlace(dword);

		{
			size_t contentTypeIndex = 0;
			for (const rkit::endian::LittleUInt32_t &contentCount : header.m_contentCounts)
			{
				RKIT_CHECK(package->m_content[contentTypeIndex++].Resize(contentCount.Get()));
			}
		}

		for (rkit::Vector<ScenePackage::Resource> &resourceVector : package->m_content)
		{
			for (ScenePackage::Resource &resource : resourceVector)
			{
				RKIT_CHECK(stream.ReadOneBinary(resource.m_contentID));
			}
		}

		if (stream.Tell() != stream.GetSize())
			RKIT_THROW(rkit::ResultCode::kDataError);

		outPackage = std::move(package);

		RKIT_RETURN_OK;
	}

	rkit::math::Vec3 SceneManagerImpl::LoadVec3Data(const rkit::endian::LittleFloat32_t(&data)[3])
	{
		float dataFloats[3] = { data[0].Get(), data[1].Get(), data[2].Get() };
		return rkit::math::Vec3::FromArray(dataFloats);
	}


	SceneManager::SceneManager(World &world)
		: rkit::Opaque<SceneManagerImpl>(world)
	{
	}

	rkit::Result SceneManager::Create(rkit::UniquePtr<SceneManager> &outManager, World &world)
	{
		return rkit::New<SceneManager>(outManager, world);
	}

	rkit::ResultCoroutine SceneManager::RunScene(rkit::ICoroThread &thread, const rkit::ByteStringSliceView &name, const rkit::data::ContentID &cid, bool loop)
	{
		return Impl().RunScene(thread, name, cid, loop);
	}

	rkit::ResultCoroutine SceneManager::OnFrame(rkit::ICoroThread &thread)
	{
		CORO_RETURN_OK;
	}
}

RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(anox::game::SceneManagerImpl)
