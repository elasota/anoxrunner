#pragma once

#include "rkit/Core/Endian.h"
#include "rkit/Core/StringProto.h"
#include "rkit/Math/VecProto.h"

namespace rkit
{
	template<class T>
	class Optional;
}

namespace anox
{
	class Label;
}

namespace anox::game
{
	struct WorldObjectSpawnParams;
	struct UserEntityDef;

	class EntityLevelLoader
	{
	public:
		static void LoadFloat(float &value, const void *source);
		static void LoadUInt(uint32_t &value, const void *source);
		static void LoadBool(bool &value, const void *source);
		static void LoadLabel(Label &label, const void *source);
		static void LoadVec2(rkit::math::Vec2 &vec, const void *source);
		static void LoadVec3(rkit::math::Vec3 &vec, const void *source);
		static void LoadVec4(rkit::math::Vec4 &vec, const void *source);
		static rkit::Result LoadByteString(rkit::ByteString &str, const void *source, const WorldObjectSpawnParams &spawnParams);
		static rkit::Result LoadEDef(UserEntityDef &edef, const void *source, const WorldObjectSpawnParams &spawnParams);
	};
}

#include "rkit/Math/Vec.h"
#include "anox/Label.h"

namespace anox::game
{
	inline void EntityLevelLoader::LoadFloat(float &value, const void *source)
	{
		value = static_cast<const rkit::endian::LittleFloat32_t *>(source)->Get();
	}

	inline void EntityLevelLoader::LoadUInt(uint32_t &value, const void *source)
	{
		value = static_cast<const rkit::endian::LittleUInt32_t *>(source)->Get();
	}

	inline void EntityLevelLoader::LoadBool(bool &value, const void *source)
	{
		value = ((*static_cast<const uint8_t *>(source)) != 0);
	}

	inline void EntityLevelLoader::LoadLabel(Label &label, const void *source)
	{
		label = Label::FromRawValue(static_cast<const rkit::endian::LittleUInt32_t *>(source)->Get());
	}

	inline void EntityLevelLoader::LoadVec2(rkit::math::Vec2 &vec, const void *source)
	{
		const rkit::endian::LittleFloat32_t *floats = static_cast<const rkit::endian::LittleFloat32_t *>(source);
		vec = rkit::math::Vec2(floats[0].Get(), floats[1].Get());
	}

	inline void EntityLevelLoader::LoadVec3(rkit::math::Vec3 &vec, const void *source)
	{
		const rkit::endian::LittleFloat32_t *floats = static_cast<const rkit::endian::LittleFloat32_t *>(source);
		vec = rkit::math::Vec3(floats[0].Get(), floats[1].Get(), floats[2].Get());
	}

	inline void EntityLevelLoader::LoadVec4(rkit::math::Vec4 &vec, const void *source)
	{
		const rkit::endian::LittleFloat32_t *floats = static_cast<const rkit::endian::LittleFloat32_t *>(source);
		vec = rkit::math::Vec4(floats[0].Get(), floats[1].Get(), floats[2].Get(), floats[3].Get());
	}
}
