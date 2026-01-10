#pragma once

#include "rkit/Core/StaticArray.h"

namespace rkit { namespace render
{
	enum class RenderDeviceBoolCap
	{
		kIndependentBlend,

		kCount,
	};

	enum class RenderDeviceUInt32Cap
	{
		kMaxTexture1DSize,
		kMaxTexture2DSize,
		kMaxTexture3DSize,
		kMaxTextureCubeSize,
		kMaxTextureArrayLayers,

		kCount,
	};

	struct IRenderDeviceCaps
	{
		virtual bool GetBoolCap(RenderDeviceBoolCap cap) const = 0;
		virtual uint32_t GetUInt32Cap(RenderDeviceUInt32Cap cap) const = 0;
	};

	class RenderDeviceCaps final : public IRenderDeviceCaps
	{
	public:
		RenderDeviceCaps();
		RenderDeviceCaps(const RenderDeviceCaps &other) = default;

		void SetBoolCap(RenderDeviceBoolCap cap, bool value);
		bool GetBoolCap(RenderDeviceBoolCap cap) const override;

		void SetUInt32Cap(RenderDeviceUInt32Cap cap, uint32_t value);
		uint32_t GetUInt32Cap(RenderDeviceUInt32Cap cap) const override;

		void RaiseTo(const IRenderDeviceCaps &otherCaps);
		bool MeetsOrExceeds(const IRenderDeviceCaps &otherCaps) const;

	private:
		static const size_t kNumBoolCaps = static_cast<size_t>(RenderDeviceBoolCap::kCount);
		static const size_t kNumUInt32Caps = static_cast<size_t>(RenderDeviceUInt32Cap::kCount);

		StaticArray<bool, kNumBoolCaps> m_boolCaps;
		StaticArray<uint32_t, kNumUInt32Caps> m_uint32Caps;
	};


	enum class RenderDeviceUInt32Requirement
	{
		kDataBufferOffsetAlignment,
		kConstantBufferOffsetAlignment,
		kTexelBufferOffsetAlignment,

		kCount,
	};

	struct IRenderDeviceRequirements
	{
		virtual uint32_t GetUInt32Req(RenderDeviceUInt32Requirement cap) const = 0;
	};

	class RenderDeviceRequirements final : public IRenderDeviceRequirements
	{
	public:
		RenderDeviceRequirements();
		RenderDeviceRequirements(const RenderDeviceRequirements &other) = default;

		void SetUInt32Req(RenderDeviceUInt32Requirement cap, uint32_t value);
		uint32_t GetUInt32Req(RenderDeviceUInt32Requirement cap) const override;

	private:
		static const size_t kNumUInt32Requirements = static_cast<size_t>(RenderDeviceUInt32Requirement::kCount);

		StaticArray<uint32_t, kNumUInt32Requirements> m_uint32Reqs;
	};
} } // rkit::render

#include "rkit/Core/Algorithm.h"

namespace rkit { namespace render
{
	inline RenderDeviceCaps::RenderDeviceCaps()
	{
		for (bool &v : m_boolCaps)
			v = false;
		for (uint32_t &v : m_uint32Caps)
			v = 0;
	}

	inline RenderDeviceRequirements::RenderDeviceRequirements()
	{
		for (uint32_t &v : m_uint32Reqs)
			v = 0;
	}

	inline void RenderDeviceRequirements::SetUInt32Req(RenderDeviceUInt32Requirement req, uint32_t value)
	{
		m_uint32Reqs[static_cast<size_t>(req)] = value;
	}

	inline uint32_t RenderDeviceRequirements::GetUInt32Req(RenderDeviceUInt32Requirement req) const
	{
		return m_uint32Reqs[static_cast<size_t>(req)];
	}

	inline void RenderDeviceCaps::SetBoolCap(RenderDeviceBoolCap cap, bool value)
	{
		m_boolCaps[static_cast<size_t>(cap)] = value;
	}

	inline bool RenderDeviceCaps::GetBoolCap(RenderDeviceBoolCap cap) const
	{
		return m_boolCaps[static_cast<size_t>(cap)];
	}

	inline void RenderDeviceCaps::SetUInt32Cap(RenderDeviceUInt32Cap cap, uint32_t value)
	{
		m_uint32Caps[static_cast<size_t>(cap)] = value;
	}

	inline uint32_t RenderDeviceCaps::GetUInt32Cap(RenderDeviceUInt32Cap cap) const
	{
		return m_uint32Caps[static_cast<size_t>(cap)];
	}

	inline void RenderDeviceCaps::RaiseTo(const IRenderDeviceCaps &otherCaps)
	{
		for (size_t i = 0; i < kNumBoolCaps; i++)
			m_boolCaps[i] = (m_boolCaps[i] || otherCaps.GetBoolCap(static_cast<RenderDeviceBoolCap>(i)));

		for (size_t i = 0; i < kNumUInt32Caps; i++)
			m_uint32Caps[i] = rkit::Max(m_uint32Caps[i], otherCaps.GetUInt32Cap(static_cast<RenderDeviceUInt32Cap>(i)));
	}

	inline bool RenderDeviceCaps::MeetsOrExceeds(const IRenderDeviceCaps &otherCaps) const
	{
		for (size_t i = 0; i < kNumBoolCaps; i++)
		{
			if (m_boolCaps[i] == false && otherCaps.GetBoolCap(static_cast<RenderDeviceBoolCap>(i)))
				return false;
		}

		for (size_t i = 0; i < kNumUInt32Caps; i++)
		{
			if (m_uint32Caps[i] < otherCaps.GetUInt32Cap(static_cast<RenderDeviceUInt32Cap>(i)))
				return false;
		}

		return true;
	}
} } // rkit::render
