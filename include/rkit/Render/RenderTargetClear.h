#pragma once

#include "rkit/Render/ImagePlane.h"

#include "rkit/Core/EnumMask.h"
#include "rkit/Core/Optional.h"

#include <cstdint>
#include <cstddef>

namespace rkit::render
{
	union RenderTargetClearValue
	{
		double m_float64[2];
		int64_t m_sint64[2];
		uint64_t m_uint64[2];

		float m_float32[4];
		int32_t m_sint32[4];
		uint32_t m_uint32[4];

		uint8_t m_rawBytes[16];

		static RenderTargetClearValue FromFloat64(double v0, double v1);
		static RenderTargetClearValue FromSInt64(int64_t v0, int64_t v1);
		static RenderTargetClearValue FromUInt64(uint64_t v0, uint64_t v1);
		static RenderTargetClearValue FromFloat32(float v0, float v1, float v2, float v3);
		static RenderTargetClearValue FromSInt32(int32_t v0, int32_t v1, int32_t v2, int32_t v3);
		static RenderTargetClearValue FromUInt32(uint32_t v0, uint32_t v1, uint32_t v2, uint32_t v3);
		static RenderTargetClearValue Zero();
	};

	struct RenderTargetClear
	{
		uint32_t m_renderTargetIndex = 0;
		RenderTargetClearValue m_clearValue = RenderTargetClearValue::Zero();
	};
}

namespace rkit::render::priv
{
	template<class T>
	struct RenderTargetInitHelper
	{
		template<T(RenderTargetClearValue:: *TField)[2]>
		static RenderTargetClearValue From2(T v0, T v1);

		template<T(RenderTargetClearValue:: *TField)[4]>
		static RenderTargetClearValue From4(T v0, T v1, T v2, T v3);
	};
}

namespace rkit::render
{
	inline RenderTargetClearValue RenderTargetClearValue::FromFloat64(double v0, double v1)
	{
		return priv::RenderTargetInitHelper<double>::From2<&RenderTargetClearValue::m_float64>(v0, v1);
	}

	inline RenderTargetClearValue RenderTargetClearValue::FromSInt64(int64_t v0, int64_t v1)
	{
		return priv::RenderTargetInitHelper<int64_t>::From2<&RenderTargetClearValue::m_sint64>(v0, v1);
	}

	inline RenderTargetClearValue RenderTargetClearValue::FromUInt64(uint64_t v0, uint64_t v1)
	{
		return priv::RenderTargetInitHelper<uint64_t>::From2<&RenderTargetClearValue::m_uint64>(v0, v1);
	}

	inline RenderTargetClearValue RenderTargetClearValue::FromFloat32(float v0, float v1, float v2, float v3)
	{
		return priv::RenderTargetInitHelper<float>::From4<&RenderTargetClearValue::m_float32>(v0, v1, v2, v3);
	}

	inline RenderTargetClearValue RenderTargetClearValue::FromSInt32(int32_t v0, int32_t v1, int32_t v2, int32_t v3)
	{
		return priv::RenderTargetInitHelper<int32_t>::From4<&RenderTargetClearValue::m_sint32>(v0, v1, v2, v3);
	}

	inline RenderTargetClearValue RenderTargetClearValue::FromUInt32(uint32_t v0, uint32_t v1, uint32_t v2, uint32_t v3)
	{
		return priv::RenderTargetInitHelper<uint32_t>::From4<&RenderTargetClearValue::m_uint32>(v0, v1, v2, v3);
	}

	inline RenderTargetClearValue RenderTargetClearValue::Zero()
	{
		RenderTargetClearValue result;
		for (size_t i = 0; i < sizeof(result.m_rawBytes); i++)
			result.m_rawBytes[i] = 0;

		return result;
	}
}

namespace rkit::render::priv
{
	template<class T>
	template<T(RenderTargetClearValue:: *TField)[2]>
	inline RenderTargetClearValue RenderTargetInitHelper<T>::From2(T v0, T v1)
	{
		RenderTargetClearValue result;
		(result.*TField)[0] = v0;
		(result.*TField)[1] = v1;
		return result;
	}

	template<class T>
	template<T(RenderTargetClearValue:: *TField)[4]>
	inline RenderTargetClearValue RenderTargetInitHelper<T>::From4(T v0, T v1, T v2, T v3)
	{
		RenderTargetClearValue result;
		(result.*TField)[0] = v0;
		(result.*TField)[1] = v1;
		(result.*TField)[2] = v2;
		(result.*TField)[3] = v3;
		return result;
	}
}
