#pragma once

#if defined(_M_X64) || defined(__x86_64__)
#include "FPControlSSE.h"
#else
#error "Unknown platform for FPControl"
#endif

#include "FPRounding.h"

namespace rkit
{
	struct FloatingPointState
	{
	public:
		FloatingPointState(const FloatingPointState &) = default;
		FloatingPointState &operator=(const FloatingPointState &) = default;

		void Commit() const;

		static FloatingPointState GetCurrent();
		static FloatingPointState GetDefault();

		FloatingPointState AppendIEEEStrict() const;
		FloatingPointState AppendFast() const;

		FloatingPointState AppendRoundingMode(FloatingPointRoundingMode roundingMode) const;

	private:
		explicit FloatingPointState(const priv::FloatingPointStateInternal &privState);

		FloatingPointState() = delete;

		priv::FloatingPointStateInternal m_internal;
	};

	class FloatingPointStateScope
	{
	public:
		explicit FloatingPointStateScope(const FloatingPointState &newFPState);
		~FloatingPointStateScope();

	private:
		FloatingPointStateScope() = delete;
		FloatingPointStateScope(const FloatingPointStateScope &) = delete;

		FloatingPointStateScope &operator=(const FloatingPointStateScope &) = delete;

		FloatingPointState m_oldState;
	};
}

namespace rkit
{
	inline FloatingPointStateScope::FloatingPointStateScope(const FloatingPointState &newFPState)
		: m_oldState(FloatingPointState::GetCurrent())
	{
		newFPState.Commit();
	}

	inline FloatingPointStateScope::~FloatingPointStateScope()
	{
		m_oldState.Commit();
	}

	inline void FloatingPointState::Commit() const
	{
		priv::fpcontrol::Commit(m_internal);
	}

	inline FloatingPointState FloatingPointState::GetCurrent()
	{
		return FloatingPointState(priv::fpcontrol::GetCurrent());
	}

	inline FloatingPointState FloatingPointState::GetDefault()
	{
		return FloatingPointState(priv::fpcontrol::GetDefault());
	}

	inline FloatingPointState FloatingPointState::AppendIEEEStrict() const
	{
		priv::FloatingPointStateInternal fpStateInternal(m_internal);
		priv::fpcontrol::SetIEEEStrict(fpStateInternal);
		return FloatingPointState(fpStateInternal);
	}

	inline FloatingPointState FloatingPointState::AppendFast() const
	{
		priv::FloatingPointStateInternal fpStateInternal(m_internal);
		priv::fpcontrol::SetFast(fpStateInternal);
		return FloatingPointState(fpStateInternal);
	}

	inline FloatingPointState FloatingPointState::AppendRoundingMode(FloatingPointRoundingMode roundingMode) const
	{
		priv::FloatingPointStateInternal fpStateInternal(m_internal);
		priv::fpcontrol::SetRoundingMode(fpStateInternal, roundingMode);
		return FloatingPointState(fpStateInternal);
	}

	inline FloatingPointState::FloatingPointState(const priv::FloatingPointStateInternal &privState)
		: m_internal(privState)
	{
	}
}

