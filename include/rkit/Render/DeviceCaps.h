#pragma once

#include "rkit/Core/StaticArray.h"

namespace rkit::render
{
	enum class RenderDeviceBoolCap
	{
		kIndependentBlend,

		kCount,
	};

	struct IRenderDeviceCaps
	{
		virtual bool GetBoolCap(RenderDeviceBoolCap cap) const = 0;
	};

	class RenderDeviceCaps final : public IRenderDeviceCaps
	{
	public:
		RenderDeviceCaps();
		RenderDeviceCaps(const RenderDeviceCaps &other) = default;

		void SetBoolCap(RenderDeviceBoolCap cap, bool value);
		bool GetBoolCap(RenderDeviceBoolCap cap) const override;

		void RaiseTo(const IRenderDeviceCaps &otherCaps);
		bool MeetsOrExceeds(const IRenderDeviceCaps &otherCaps) const;

	private:
		static const size_t kNumBoolCaps = static_cast<size_t>(RenderDeviceBoolCap::kCount);

		StaticArray<bool, kNumBoolCaps> m_boolCaps;
	};
}

namespace rkit::render
{
	inline RenderDeviceCaps::RenderDeviceCaps()
	{
		for (bool &v : m_boolCaps)
			v = false;
	}

	inline void RenderDeviceCaps::SetBoolCap(RenderDeviceBoolCap cap, bool value)
	{
		m_boolCaps[static_cast<size_t>(cap)] = value;
	}

	inline bool RenderDeviceCaps::GetBoolCap(RenderDeviceBoolCap cap) const
	{
		return m_boolCaps[static_cast<size_t>(cap)];
	}

	inline void RenderDeviceCaps::RaiseTo(const IRenderDeviceCaps &otherCaps)
	{
		for (size_t i = 0; i < kNumBoolCaps; i++)
			m_boolCaps[i] = (m_boolCaps[i] || otherCaps.GetBoolCap(static_cast<RenderDeviceBoolCap>(i)));
	}

	inline bool RenderDeviceCaps::MeetsOrExceeds(const IRenderDeviceCaps &otherCaps) const
	{
		for (size_t i = 0; i < kNumBoolCaps; i++)
		{
			if (m_boolCaps[i] == false && otherCaps.GetBoolCap(static_cast<RenderDeviceBoolCap>(i)))
				return false;
		}

		return true;
	}
}
