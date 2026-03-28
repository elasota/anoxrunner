#pragma once

#include "rkit/Core/Opaque.h"
#include "rkit/Core/Result.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	template<class T>
	class Span;

	struct IMallocDriver;
}

namespace anox::game
{
	class SessionImpl;
	class World;
	struct SpawnDef;
	struct UserEntityDefValues;

	class Session final : public rkit::Opaque<SessionImpl>
	{
	public:
		World &GetWorld() const;

		rkit::Result AsyncSpawnInitialEntities(World &world,
			rkit::Span<const rkit::endian::LittleUInt32_t> entityTypes,
			rkit::Span<const uint8_t> spawnData,
			rkit::Span<const rkit::ByteStringView> spawnDefStrings, rkit::Span<const game::UserEntityDefValues> udefs,
			rkit::Span<const rkit::ByteString> udefDescriptions);
		rkit::Result AsyncPostSpawnInitialEntities(World &world);
		rkit::Result AsyncRunFrame(World &world);
		rkit::Result AsyncEnterGameSession(World &world);
		rkit::Result WaitForMainThreadCoro(bool &outIsFinished);

		static rkit::Result Create(rkit::UniquePtr<Session> &outSession, rkit::IMallocDriver *alloc);
	};
}
