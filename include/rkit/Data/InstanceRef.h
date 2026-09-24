#pragma once

#include <utility>

namespace rkit::data
{
	template<class T>
	class InstanceRef
	{
	public:
		InstanceRef() = default;
		InstanceRef(const InstanceRef &other) = default;
		InstanceRef(InstanceRef && other) = default;
		~InstanceRef() = default;

		explicit InstanceRef(uint32_t index);

		uint32_t GetIndex() const;
		void SetIndex(uint32_t index);

	private:
		uint32_t m_index = 0;
	};
}

namespace rkit::data
{
	template<class T>
	InstanceRef<T>::InstanceRef(uint32_t index)
		: m_index(index)
	{
	}

	template<class T>
	uint32_t InstanceRef<T>::GetIndex() const
	{
		return m_index;
	}

	template<class T>
	void InstanceRef<T>::SetIndex(uint32_t index)
	{
		m_index = index;
	}
}
