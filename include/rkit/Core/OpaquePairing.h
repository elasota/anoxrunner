#pragma once

namespace rkit
{
	namespace priv
	{
		template<class TBase, class TImpl>
		struct OpaquePairing
		{
			TBase m_base;
			TImpl m_impl;
		};
	}
}
