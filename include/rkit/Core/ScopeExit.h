#pragma once

namespace rkit
{
	template<class TExitFunc>
	class ScopeExit
	{
	public:
		ScopeExit() = delete;
		ScopeExit(const TExitFunc &exitFunc);
		ScopeExit(const ScopeExit &) = delete;
		~ScopeExit();

		ScopeExit &operator=(const ScopeExit &) = delete;

	private:
		const TExitFunc &m_exitFunc;
	};
}

namespace rkit
{
	template<class TExitFunc>
	ScopeExit<TExitFunc>::ScopeExit(const TExitFunc &exitFunc)
		: m_exitFunc(exitFunc)
	{
	}

	template<class TExitFunc>
	ScopeExit<TExitFunc>::~ScopeExit()
	{
		m_exitFunc();
	}
}
