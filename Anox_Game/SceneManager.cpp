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
#include "ScenePackage.h"
#include "World.h"

namespace anox::game
{
	template<class T>
	class ObjRef;

	class Scene;

	class ScenePackageImpl final: public rkit::OpaqueImplementation<ScenePackage>
	{
		friend class ScenePackage;
		friend class SceneManagerImpl;

	private:
		uint32_t m_cineID = 0;
		bool m_isInterrupt = false;

		rkit::Vector<rkit::ByteString> m_strings;
		rkit::Vector<ScenePackage::Block> m_blocks;
		rkit::Vector<ScenePackage::Path> m_paths;

		rkit::Vector<ScenePackage::CubicNode> m_cubic;
		rkit::Vector<ScenePackage::FocusNode> m_focus;
		rkit::Vector<ScenePackage::CommandNode> m_command;
		rkit::Vector<ScenePackage::ScaleNode> m_scale;
		rkit::Vector<ScenePackage::RollNode> m_roll;
		rkit::Vector<ScenePackage::FOVNode> m_fov;

		rkit::Vector<data::SceneCommandOpcode> m_commandOpcodes;
		rkit::Vector<uint32_t> m_commandParamDWords;

		rkit::StaticArray<rkit::Vector<ScenePackage::Resource>, ScenePackage::kNumContentTypes> m_content;

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

		rkit::ResultCoroutine LoadScenePackageFromContentID(rkit::ICoroThread &thread, rkit::RCPtr<ScenePackage> &outPackage, rkit::data::ContentID cid);
		static rkit::Result LoadScenePackage(rkit::RCPtr<ScenePackage> &outPackage, rkit::ConstSpan<uint8_t> data);
		static rkit::math::Vec3 LoadVec3Data(const rkit::endian::LittleFloat32_t(&data)[3]);

		static rkit::Result NormalizeName(rkit::ByteStringSliceView &view, rkit::ByteString &tempStorage);

		// SAVEGAME TODO
		struct ScenePackageRefAndFreeID
		{
			ScenePackage *m_package = nullptr;
			size_t m_freeID = 0;
		};

		rkit::Vector<ScenePackageRefAndFreeID> m_scenePackageList;
		size_t m_numFreeIDs = 0;

		rkit::HashMap<rkit::data::ContentID, size_t> m_scenePackageMap;

		rkit::HashMap<rkit::ByteString, ObjRef<Scene>> m_scenes;

		World &m_world;
	};

	void ScenePackage::RCTrackerZero()
	{
		if (Impl().m_scenePackageID != 0)
			Impl().m_sceneManager->ReleaseScenePackage(Impl().m_scenePackageID);

		RefCounted::RCTrackerZero();
	}

	SceneManagerImpl::SceneManagerImpl(World &world)
		: m_world(world)
	{
	}

	rkit::ResultCoroutine SceneManagerImpl::LoadScenePackageFromContentID(rkit::ICoroThread &thread, rkit::RCPtr<ScenePackage> &outPackage, rkit::data::ContentID cid)
	{
		rkit::HashValue_t cidHash = rkit::Hasher<rkit::data::ContentID>::ComputeHash(0, cid);

		rkit::HashMap<rkit::data::ContentID, size_t>::ConstIterator_t it = m_scenePackageMap.FindPrehashed(cidHash, cid);
		if (it != m_scenePackageMap.end())
		{
			outPackage = m_scenePackageList[it.Value()].m_package;
			CORO_RETURN_OK;
		}

		SandboxResourceRequestHandle sceneReqHandle;
		CORO_CHECK(SandboxResourceLoader::LoadContentKeyedResource(sceneReqHandle, resloaders::kContentIDRawFileResourceTypeCode, cid));

		SandboxResourceHandle sceneResHandle;
		CORO_CHECK(co_await sceneReqHandle.WaitForLoaded(thread, sceneResHandle));

		SandboxResourceDataBlob blob;
		CORO_CHECK(SandboxResourceLoader::GetFileResourceContents(blob, sceneResHandle));

		rkit::RCPtr<ScenePackage> scenePackage;
		CORO_CHECK(LoadScenePackage(scenePackage, blob.GetContents()));

		if (m_numFreeIDs == 0)
		{
			CORO_CHECK(m_scenePackageList.Append(ScenePackageRefAndFreeID()));

			m_numFreeIDs = 1;
			m_scenePackageList[0].m_freeID = m_scenePackageList.Count();
		}

		const size_t packageID = m_scenePackageList[--m_numFreeIDs].m_freeID;

		CORO_CHECK(m_scenePackageMap.SetPrehashed(cidHash, cid, packageID));

		m_scenePackageList[packageID - 1].m_package = scenePackage.Get();

		scenePackage->Impl().m_scenePackageID = packageID;
		scenePackage->Impl().m_sceneManager = this;

		outPackage = std::move(scenePackage);

		CORO_RETURN_OK;
	}

	rkit::ResultCoroutine SceneManagerImpl::RunScene(rkit::ICoroThread &thread, rkit::ByteStringSliceView name, rkit::data::ContentID cid, bool loop)
	{
		rkit::ByteString tempName;

		CORO_CHECK(NormalizeName(name, tempName));

		const rkit::HashValue_t nameHash = rkit::Hasher<rkit::ByteStringSliceView>::ComputeHash(0, name);
		const rkit::HashMap<rkit::ByteString, ObjRef<Scene>>::ConstIterator_t sceneIt = m_scenes.FindPrehashed(nameHash, name);

		if (sceneIt != m_scenes.end())
			CORO_RETURN_OK;

		rkit::RCPtr<ScenePackage> package;
		CORO_CHECK(co_await LoadScenePackageFromContentID(thread, package, cid));

		Scene *scene = nullptr;
		CORO_CHECK((WorldObjectFactory::CreateDynamic<Scene>(m_world, scene)));

		if (tempName.Length() == 0)
		{
			CORO_CHECK(tempName.Set(name));
		}

		CORO_CHECK(m_scenes.SetPrehashed(nameHash, std::move(tempName), ObjRef<Scene>(scene)));

		scene->Initialize(SceneHandle(package));

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

		ScenePackageImpl &packageImpl = package->Impl();

		packageImpl.m_cineID = header.m_cineID.Get();
		packageImpl.m_isInterrupt = (header.m_isInterrupt != 0);

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

			RKIT_CHECK(packageImpl.m_strings.Resize(stringCBufs.Count()));
			rkit::ProcessParallelSpans(packageImpl.m_strings.ToSpan(), stringCBufs.ToSpan(), [](rkit::ByteString &outString, rkit::ByteStringConstructionBuffer &inString)
				{
					outString = rkit::ByteString(std::move(inString));
				});
		}

		RKIT_CHECK(packageImpl.m_blocks.Resize(header.m_numBlocks.Get()));
		RKIT_CHECK(packageImpl.m_paths.Resize(header.m_numPaths.Get()));

		{
			size_t pathOffset = 0;

			for (ScenePackage::Block &outBlock : packageImpl.m_blocks)
			{
				data::SceneBlock inBlock;
				RKIT_CHECK(stream.ReadOneBinary(inBlock));

				const uint32_t numPaths = inBlock.m_numPaths.Get();

				if (packageImpl.m_paths.Count() - pathOffset < numPaths)
					RKIT_THROW(rkit::ResultCode::kDataError);

				outBlock.m_paths = packageImpl.m_paths.ToSpan().SubSpan(pathOffset, numPaths);
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
		deserializers[static_cast<size_t>(data::ScenePathType::kCubic)] = CreateSceneDeserializer<data::SceneCubicNode>(*package, packageImpl.m_cubic, processOneCubic);
		deserializers[static_cast<size_t>(data::ScenePathType::kFocus)] = CreateSceneDeserializer<data::SceneFocusNode>(*package, packageImpl.m_focus, processOneFocus);
		deserializers[static_cast<size_t>(data::ScenePathType::kCommand)] = CreateSceneDeserializer<data::SceneCommandNode>(*package, packageImpl.m_command, processOneCommand);
		deserializers[static_cast<size_t>(data::ScenePathType::kScale)] = CreateSceneDeserializer<data::SceneScaleNode>(*package, packageImpl.m_scale, processOneScale);
		deserializers[static_cast<size_t>(data::ScenePathType::kRoll)] = CreateSceneDeserializer<data::SceneRollNode>(*package, packageImpl.m_roll, processOneRoll);
		deserializers[static_cast<size_t>(data::ScenePathType::kFOV)] = CreateSceneDeserializer<data::SceneFOVNode>(*package, packageImpl.m_fov, processOneFOV);

		for (size_t i = 0; i < kNumPathTypes; i++)
		{
			SceneObjectDeserializer &deserializer = deserializers[i];

			RKIT_ASSERT(deserializer.m_isValid);

			RKIT_CHECK(deserializer.m_resizeOutVectorFunc(deserializer.m_outVector, header.m_nodeCounts[i].Get()));
		}

		for (ScenePackage::Path &outPath : packageImpl.m_paths)
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

			if (!inPath.m_isGlobal)
				outPath.m_group = 0;	// Temporary, will be reassigned later

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
		for (const ScenePackage::CommandNode &cmd : packageImpl.m_command)
		{
			RKIT_CHECK(rkit::SafeAdd<size_t>(numCommandOpcodes, numCommandOpcodes, cmd.m_commandOpcodes.Count()));
			RKIT_CHECK(rkit::SafeAdd<size_t>(numCommandParamDWords, numCommandParamDWords, cmd.m_commandParamDWords.Count()));
		}

		RKIT_CHECK(packageImpl.m_commandOpcodes.Resize(numCommandOpcodes));
		RKIT_CHECK(packageImpl.m_commandParamDWords.Resize(numCommandParamDWords));

		{
			size_t opcodeOffset = 0;
			size_t paramDWordOffset = 0;
			for (ScenePackage::CommandNode &cmd : packageImpl.m_command)
			{
				cmd.m_commandOpcodes = packageImpl.m_commandOpcodes.ToSpan().SubSpan(opcodeOffset, cmd.m_commandOpcodes.Count());
				cmd.m_commandParamDWords = packageImpl.m_commandParamDWords.ToSpan().SubSpan(paramDWordOffset, cmd.m_commandParamDWords.Count());
				opcodeOffset += cmd.m_commandOpcodes.Count();
				paramDWordOffset += cmd.m_commandParamDWords.Count();
			}
		}

		RKIT_CHECK(stream.ReadAllSpan(packageImpl.m_commandOpcodes.ToSpan()));
		RKIT_CHECK(stream.ReadAllSpan(packageImpl.m_commandParamDWords.ToSpan()));

		for (uint32_t &dword : packageImpl.m_commandParamDWords)
			rkit::endian::LittleUInt32_t::StaticConvertToHostOrderInPlace(dword);

		{
			size_t contentTypeIndex = 0;
			for (const rkit::endian::LittleUInt32_t &contentCount : header.m_contentCounts)
			{
				RKIT_CHECK(packageImpl.m_content[contentTypeIndex++].Resize(contentCount.Get()));
			}
		}

		for (rkit::Vector<ScenePackage::Resource> &resourceVector : packageImpl.m_content)
		{
			for (ScenePackage::Resource &resource : resourceVector)
			{
				RKIT_CHECK(stream.ReadOneBinary(resource.m_contentID));
			}
		}

		// Index groups
		for (ScenePackage::Block &block : packageImpl.m_blocks)
		{
			size_t numGroups = 0;
			for (const ScenePackage::Path &constPath : block.m_paths)
			{
				if (!constPath.m_group.IsSet())
					continue;

				if (constPath.m_pathType == data::ScenePathType::kCubic)
					numGroups++;

				if (numGroups == 0)
					RKIT_THROW(rkit::ResultCode::kDataError);

				const_cast<ScenePackage::Path &>(constPath).m_group = numGroups - 1;
			}

			block.m_numGroups = numGroups;
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


	rkit::Result SceneManagerImpl::NormalizeName(rkit::ByteStringSliceView &view, rkit::ByteString &tempStorage)
	{
		bool isAlreadyLowercase = true;
		for (uint8_t ch : view)
		{
			if (rkit::InvariantCharCaseAdjuster<uint8_t>::ToLower(ch) != ch)
			{
				isAlreadyLowercase = false;
				break;
			}
		}

		if (isAlreadyLowercase)
			RKIT_RETURN_OK;

		rkit::ByteStringConstructionBuffer cbuf;
		RKIT_CHECK(cbuf.Allocate(view.Length()));

		rkit::ProcessParallelSpans(cbuf.GetSpan(), view.ToSpan(), [](uint8_t &outCh, uint8_t inCh)
			{
				outCh = rkit::InvariantCharCaseAdjuster<uint8_t>::ToLower(inCh);
			});

		tempStorage = std::move(cbuf);

		view = tempStorage;

		RKIT_RETURN_OK;
	}

	SceneManager::SceneManager(World &world)
		: rkit::Opaque<SceneManagerImpl>(world)
	{
	}

	rkit::Result SceneManager::Create(rkit::UniquePtr<SceneManager> &outManager, World &world)
	{
		return rkit::New<SceneManager>(outManager, world);
	}

	rkit::ResultCoroutine SceneManager::RunScene(rkit::ICoroThread &thread, rkit::ByteStringSliceView name, const rkit::data::ContentID &cid, bool loop)
	{
		return Impl().RunScene(thread, name, cid, loop);
	}

	rkit::ResultCoroutine SceneManager::OnFrame(rkit::ICoroThread &thread)
	{
		CORO_RETURN_OK;
	}


	uint32_t ScenePackage::GetCineID() const
	{
		return Impl().m_cineID;
	}

	bool ScenePackage::IsInterrupt() const
	{
		return Impl().m_isInterrupt;
	}

	rkit::ConstSpan<ScenePackage::Block> ScenePackage::GetBlocks() const
	{
		return Impl().m_blocks.ToSpan();
	}
}

RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(anox::game::SceneManagerImpl)
RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(anox::game::ScenePackageImpl)
