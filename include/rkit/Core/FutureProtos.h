#pragma once

namespace rkit
{
	template<class T>
	struct FutureContainer;

	template<class T>
	class RCPtr;

	template<class T>
	using FutureContainerPtr = RCPtr<FutureContainer<T>>;
}
