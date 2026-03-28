#include "AnoxSpawnDefsResource.h"

#include "AnoxEntityDefResource.h"
#include "AnoxAbstractSingleFileResource.h"

#include "rkit/Data/ContentID.h"
#include "rkit/Core/Sanitizers.h"

#include "anox/CoreUtils/CoreUtils.h"
#include "anox/Data/EntitySpawnData.h"
#include "anox/Data/EntityStructs.h"

#include "anox/AnoxUtilitiesDriver.h"
#include "anox/AnoxModule.h"


namespace anox
{
	class AnoxSpawnDefsResource;
	struct AnoxSpawnDefsResourceLoaderState;

	struct AnoxSpawnDefsResourceLoaderState final : public AnoxAbstractSingleFileResourceLoaderState
	{
		rkit::Vector<rkit::Future<AnoxResourceRetrieveResult>> m_edefResources;
	};

	struct AnoxSpawnDefsLoaderInfo
	{
		typedef AnoxSpawnDefsResourceLoaderBase LoaderBase_t;
		typedef AnoxSpawnDefsResource Resource_t;
		typedef AnoxSpawnDefsResourceLoaderState State_t;

		static constexpr size_t kNumPhases = 2;

		class SpawnDataChunkVisitor
		{
		public:
			SpawnDataChunkVisitor(rkit::IReadStream &stream, AnoxSpawnDefsResourceLoaderState &state);

			template<class T>
			rkit::Result VisitMember(rkit::Vector<T> &array) const;

		private:
			rkit::IReadStream &m_stream;
			AnoxSpawnDefsResourceLoaderState &m_state;
		};

		static rkit::Result LoadHeaderAndQueueDependencies(State_t &state, Resource_t &resource, rkit::traits::TraitRef<rkit::VectorTrait<rkit::RCPtr<rkit::Job>>> outDeps);
		static rkit::Result LoadContents(State_t &state, Resource_t &resource);

		static bool PhaseHasDependencies(size_t phase)
		{
			switch (phase)
			{
			case 0:
				return true;
			default:
				return false;
			}
		}

		static rkit::Result LoadPhase(State_t &state, Resource_t &resource, size_t phase, rkit::traits::TraitRef<rkit::VectorTrait<rkit::RCPtr<rkit::Job>>> outDeps)
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

		static rkit::Result ParseEntity(void *data, const AnoxSpawnDefsResource &resource, rkit::Span<const uint8_t> entityData, const data::EntityClassDef *eclass);

		template<size_t TComponents>
		static void ParseFloatArray(void *outData, const void *inData);
	};

	class AnoxSpawnDefsResource final : public AnoxSpawnDefsResourceBase
	{
	public:
		friend struct AnoxSpawnDefsLoaderInfo;

		const data::EntitySpawnDataChunks &GetChunks() const override;
		rkit::CallbackSpan<AnoxEntityDefResourceBase *, const AnoxSpawnDefsResourceBase *> GetUserEntityDefs() const override;

	private:
		static AnoxEntityDefResourceBase *StaticGetUserEntityDef(AnoxSpawnDefsResourceBase const * const& selfPtr, size_t index);

		rkit::Vector<rkit::RCPtr<AnoxEntityDefResourceBase>> m_userEntityDefs;

		data::EntitySpawnDataChunks m_chunks;
	};

	const data::EntitySpawnDataChunks &AnoxSpawnDefsResource::GetChunks() const
	{
		return m_chunks;
	}

	rkit::CallbackSpan<AnoxEntityDefResourceBase *, const AnoxSpawnDefsResourceBase *> AnoxSpawnDefsResource::GetUserEntityDefs() const
	{
		return rkit::CallbackSpan<AnoxEntityDefResourceBase *, const AnoxSpawnDefsResourceBase *>(StaticGetUserEntityDef, this, m_userEntityDefs.Count());
	}

	AnoxEntityDefResourceBase *AnoxSpawnDefsResource::StaticGetUserEntityDef(AnoxSpawnDefsResourceBase const *const &selfPtr, size_t index)
	{
		return static_cast<const AnoxSpawnDefsResource *>(selfPtr)->m_userEntityDefs[index].Get();
	}

	AnoxSpawnDefsLoaderInfo::SpawnDataChunkVisitor::SpawnDataChunkVisitor(rkit::IReadStream &stream, AnoxSpawnDefsResourceLoaderState &state)
		: m_stream(stream)
		, m_state(state)
	{
	}

	template<class T>
	rkit::Result AnoxSpawnDefsLoaderInfo::SpawnDataChunkVisitor::VisitMember(rkit::Vector<T> &array) const
	{
		rkit::endian::LittleUInt32_t countData;
		RKIT_CHECK(m_stream.ReadOneBinary(countData));

		const uint32_t count = countData.Get();
		RKIT_CHECK(array.Resize(count));

		if (count > 0)
		{
			RKIT_CHECK(m_stream.ReadAllSpan(array.ToSpan()));
		}

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxSpawnDefsLoaderInfo::LoadHeaderAndQueueDependencies(State_t &state, Resource_t &resource, rkit::traits::TraitRef<rkit::VectorTrait<rkit::RCPtr<rkit::Job>>> outDeps)
	{
		{
			rkit::ReadOnlyMemoryStream stream(state.m_fileContents.ToSpan());

			data::EntitySpawnDataFile spawnDataFile;
			RKIT_CHECK(stream.ReadOneBinary(spawnDataFile));

			if (spawnDataFile.m_fourCC.Get() != data::EntitySpawnDataFile::kFourCC || spawnDataFile.m_version.Get() != data::EntitySpawnDataFile::kVersion)
				RKIT_THROW(rkit::ResultCode::kDataError);

			RKIT_CHECK(resource.m_chunks.VisitAllChunks(SpawnDataChunkVisitor(stream, state)));
		}

		state.m_fileContents.Reset();

		RKIT_CHECK(state.m_edefResources.Resize(resource.m_chunks.m_entityDefContentIDs.Count()));

		AnoxResourceManagerBase *resManager = state.m_systems.m_resManager;

		RKIT_CHECK((rkit::CheckedProcessParallelSpans(state.m_edefResources.ToSpan(), resource.m_chunks.m_entityDefContentIDs.ToSpan(),
			[resManager, &outDeps]
			(rkit::Future<AnoxResourceRetrieveResult> &outResourceFuture, const rkit::data::ContentID &inContentID)
			-> rkit::Result
			{
				rkit::RCPtr<rkit::Job> depsJob;
				RKIT_CHECK(resManager->GetContentIDKeyedResource(&depsJob, outResourceFuture, resloaders::kEntityDefTypeCode, inContentID));
				RKIT_CHECK(outDeps.AppendRValue(std::move(depsJob)));

				RKIT_RETURN_OK;
			})));

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxSpawnDefsLoaderInfo::LoadContents(State_t &state, Resource_t &resource)
	{
		const size_t numEntityDefs = state.m_edefResources.Count();
		RKIT_CHECK(resource.m_userEntityDefs.Resize(numEntityDefs));
		rkit::ProcessParallelSpans(resource.m_userEntityDefs.ToSpan(), state.m_edefResources.ToSpan(),
			[](rkit::RCPtr<AnoxEntityDefResourceBase> &outEDef, const rkit::Future<AnoxResourceRetrieveResult> &inFuture)
			{
				outEDef = inFuture.GetResult().m_resourceHandle.StaticCast<AnoxEntityDefResourceBase>();
			});

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxSpawnDefsLoaderInfo::ParseEntity(void *data, const AnoxSpawnDefsResource &resource, rkit::Span<const uint8_t> entityData, const data::EntityClassDef *eclass)
	{
		const data::EntityClassDef *parent = eclass->m_parent;

		if (parent)
		{
			RKIT_CHECK(ParseEntity(data, resource, entityData.SubSpan(0, parent->m_dataSize), parent));
		}

		const rkit::ConstSpan<data::EntityFieldDef> fieldDefs(eclass->m_fields, eclass->m_numFields);

		for (const data::EntityFieldDef &fieldDef : fieldDefs)
		{
			void *fieldDataPos = static_cast<void *>(static_cast<uint8_t *>(data) + fieldDef.m_structOffset);
			const void *inDataPos = entityData.Ptr() + fieldDef.m_dataOffset;

			switch (fieldDef.m_fieldType)
			{
			case data::EntityFieldType::kLabel:
				*static_cast<Label *>(fieldDataPos) = Label::FromRawValue(static_cast<const rkit::endian::LittleUInt32_t *>(inDataPos)->Get());
				break;
			case data::EntityFieldType::kBool:
			case data::EntityFieldType::kBoolOnOff:
				*static_cast<bool *>(fieldDataPos) = ((*static_cast<const uint8_t *>(inDataPos)) != 0);
				break;
			case data::EntityFieldType::kUInt:
				*static_cast<uint32_t *>(fieldDataPos) = static_cast<const rkit::endian::LittleUInt32_t *>(inDataPos)->Get();
				break;
			case data::EntityFieldType::kFloat:
				*static_cast<float *>(fieldDataPos) = rkit::sanitizers::SanitizeClampFloat(*static_cast<const rkit::endian::LittleFloat32_t *>(inDataPos), 40);
				break;
			case data::EntityFieldType::kVec2:
				ParseFloatArray<2>(fieldDataPos, inDataPos);
				break;
			case data::EntityFieldType::kVec3:
				ParseFloatArray<3>(fieldDataPos, inDataPos);
				break;
			case data::EntityFieldType::kVec4:
				ParseFloatArray<4>(fieldDataPos, inDataPos);
				break;
			case data::EntityFieldType::kString:
				{
					uint32_t stringIndex = static_cast<const rkit::endian::LittleUInt32_t *>(inDataPos)->Get();

					if (stringIndex > resource.m_chunks.m_entityStringLengths.Count())
						RKIT_THROW(rkit::ResultCode::kDataError);

					if (stringIndex == 0)
						new (fieldDataPos) rkit::Optional<uint32_t>();
					else
						new (fieldDataPos) rkit::Optional<uint32_t>(stringIndex - 1);
				}
				break;
			case data::EntityFieldType::kEntityDef:
				{
					uint32_t edefIndex = static_cast<const rkit::endian::LittleUInt32_t *>(inDataPos)->Get();
					if (edefIndex >= resource.m_userEntityDefs.Count())
						RKIT_THROW(rkit::ResultCode::kDataError);

					*static_cast<uint32_t *>(fieldDataPos) = edefIndex;
				}
				break;
			case data::EntityFieldType::kBSPModel:
				// uint32
				RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
			case data::EntityFieldType::kComponent:
				{
					const data::EntityClassDef *componentClass = fieldDef.m_classDef;
					RKIT_ASSERT(componentClass != nullptr);
					RKIT_CHECK(ParseEntity(fieldDataPos, resource, entityData.SubSpan(fieldDef.m_dataOffset, componentClass->m_dataSize), componentClass));
				}
				break;
			default:
				RKIT_THROW(rkit::ResultCode::kDataError);
			};
		}

		RKIT_RETURN_OK;		
	}

	template<size_t TComponents>
	void AnoxSpawnDefsLoaderInfo::ParseFloatArray(void *outData, const void *inData)
	{
		const rkit::endian::LittleFloat32_t *inFloats = static_cast<const rkit::endian::LittleFloat32_t *>(inData);

		rkit::StaticArray<float, TComponents> &outFloats = *static_cast<rkit::StaticArray<float, TComponents> *>(outData);

		for (size_t i = 0; i < TComponents; i++)
			outFloats[i] = rkit::sanitizers::SanitizeClampFloat(inFloats[i], 40);
	}

	rkit::Result AnoxSpawnDefsResourceLoaderBase::Create(rkit::RCPtr<AnoxSpawnDefsResourceLoaderBase> &outLoader)
	{
		typedef AnoxAbstractSingleFileResourceLoader<AnoxSpawnDefsLoaderInfo> Loader_t;

		rkit::RCPtr<Loader_t> loader;
		RKIT_CHECK(rkit::New<Loader_t>(loader));

		outLoader = std::move(loader);

		RKIT_RETURN_OK;
	}
}
