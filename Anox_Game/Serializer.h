#pragma once

#include "rkit/Core/StringProto.h"
#include "rkit/Core/Result.h"

#include "rkit/Math/BBoxProto.h"
#include "rkit/Math/VecProto.h"

#include <stdint.h>
#include <type_traits>

namespace rkit
{
	template<class T>
	class EnumMask;
}

namespace anox
{
	class Label;
}

namespace anox::game
{
	struct ISerializer
	{
		template<class TType>
		rkit::Result Serialize(TType &value);

	protected:
		virtual rkit::Result SerializeInternal(uint8_t &value) = 0;
		virtual rkit::Result SerializeInternal(uint16_t &value) = 0;
		virtual rkit::Result SerializeInternal(uint32_t &value) = 0;
		virtual rkit::Result SerializeInternal(uint64_t &value) = 0;
		virtual rkit::Result SerializeInternal(int8_t &value) = 0;
		virtual rkit::Result SerializeInternal(int16_t &value) = 0;
		virtual rkit::Result SerializeInternal(int32_t &value) = 0;
		virtual rkit::Result SerializeInternal(int64_t &value) = 0;
		virtual rkit::Result SerializeInternal(bool &value) = 0;
		virtual rkit::Result SerializeInternal(float &value) = 0;
		virtual rkit::Result SerializeInternal(rkit::math::Vec2 &value) = 0;
		virtual rkit::Result SerializeInternal(rkit::math::Vec3 &value) = 0;
		virtual rkit::Result SerializeInternal(rkit::math::Vec4 &value) = 0;
		virtual rkit::Result SerializeInternal(rkit::math::BBox3 &value) = 0;
		virtual rkit::Result SerializeInternal(rkit::ByteString &value) = 0;
		virtual rkit::Result SerializeInternal(Label &value) = 0;

		virtual bool IsWriting() const = 0;
		bool IsReading() const;

		template<class TEnumType>
		rkit::Result SerializeInternal(rkit::EnumMask<TEnumType> &value);

	private:
		template<class TEnum, class TUnderlying>
		rkit::Result SerializeEnumAs(TEnum &value);
	};
}

#include "rkit/Core/EnumMask.h"

namespace anox::game
{
	template<class TType>
	rkit::Result ISerializer::Serialize(TType &value)
	{
		if constexpr (std::is_enum<TType>::value)
		{
			return this->template SerializeEnumAs<TType, typename std::underlying_type<TType>::type>(value);
		}
		else
		{
			return this->SerializeInternal(value);
		}
	}

	inline bool ISerializer::IsReading() const
	{
		return !this->IsWriting();
	}

	template<class TEnum, class TUnderlying>
	rkit::Result ISerializer::SerializeEnumAs(TEnum &value)
	{
		if (this->IsWriting())
		{
			TUnderlying underlying = static_cast<TUnderlying>(value);
			return this->Serialize(underlying);
		}
		else
		{
			TUnderlying underlying = static_cast<TUnderlying>(0);
			RKIT_CHECK(this->Serialize(underlying));
			value = static_cast<TEnum>(underlying);
			RKIT_RETURN_OK;
		}
	}

	template<class TEnumType>
	rkit::Result ISerializer::SerializeInternal(rkit::EnumMask<TEnumType> &value)
	{
		for (auto &subValue : value.AsBoolVector().Chunks())
		{
			RKIT_CHECK(this->SerializeInternal(subValue));
		}
		RKIT_RETURN_OK;
	}
}
