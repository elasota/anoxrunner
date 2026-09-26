#pragma once

#include "rkit/Core/Vector.h"

#include <stdint.h>
#include <compare>

namespace rkit::data
{
	struct ClusterRef;
}

namespace rkit::data::priv
{
	class ClusterSpanIteratorBase
	{
	public:
		ClusterSpanIteratorBase() = default;
		explicit ClusterSpanIteratorBase(const ClusterRef *cluster, uint_fast8_t searchStartBit);

		void Increment();
		uint_fast8_t ResolveRealBit() const;
		size_t ResolveOffset() const;

		std::strong_ordering operator<=>(const ClusterSpanIteratorBase &other) const;
		bool operator==(const ClusterSpanIteratorBase &other) const;

	private:
		const ClusterRef *m_cluster = nullptr;
		uint_fast8_t m_searchStartBit = 0;
	};
}

namespace rkit::data
{
	struct ClusterRef
	{
		uint32_t m_baseIndex = 0;
		uint32_t m_clusters = 0;
	};

	template<class T>
	class ClusterSpan;

	template<class T>
	class ClusterSpanIterator
	{
	public:
		friend class ClusterSpan<T>;

		ClusterSpanIterator() = default;
		explicit ClusterSpanIterator(const T *instances, const ClusterRef *cluster, uint_fast8_t searchStartBit);

		std::strong_ordering operator<=>(const ClusterSpanIterator<T> &other) const;
		bool operator==(const ClusterSpanIterator<T> &other) const;

		ClusterSpanIterator<T> &operator++();
		ClusterSpanIterator<T> operator++(int);

		const T *operator *() const;
		const T *operator->() const;

	private:
		const T *Resolve() const;

		const T *m_instances = nullptr;
		priv::ClusterSpanIteratorBase m_base;
	};

	template<class T>
	class ClusterSpan
	{
	public:
		using Iterator_t = ClusterSpanIterator<T>;

		ClusterSpan() = default;
		explicit ClusterSpan(const T *instances, rkit::Span<const ClusterRef> clusters);

		ClusterSpanIterator<T> begin() const;
		ClusterSpanIterator<T> end() const;

	private:
		const T *m_instances = nullptr;
		rkit::Span<const ClusterRef> m_clusters;
	};

	template<class T>
	class ClusterRefVector
	{
	public:
		void SetInstances(rkit::Span<T> instances);
		rkit::Vector<ClusterRef> &ModifyClusterVector();
		const rkit::Vector<ClusterRef> &GetClusterVector() const;

	private:
		rkit::Span<T> m_instances;
		rkit::Vector<ClusterRef> m_clusters;
	};
}

#include "rkit/Core/Algorithm.h"

namespace rkit::data::priv
{
	inline ClusterSpanIteratorBase::ClusterSpanIteratorBase(const ClusterRef *cluster, uint_fast8_t searchStartBit)
		: m_cluster(cluster)
		, m_searchStartBit(searchStartBit)
	{
	}

	inline void ClusterSpanIteratorBase::Increment()
	{
		uint_fast8_t nextBit = ResolveRealBit() + 1;

		if (nextBit == 32 || m_cluster->m_clusters < static_cast<uint32_t>(static_cast<uint32_t>(1) << nextBit))
		{
			++m_cluster;
			m_searchStartBit = 0;
		}
		else
			m_searchStartBit = nextBit;
	}

	inline uint_fast8_t ClusterSpanIteratorBase::ResolveRealBit() const
	{
		uint32_t mask = 1;
		mask <<= m_searchStartBit;
		--mask;
		mask = ~mask;

		const uint32_t maskedHighBits = (m_cluster->m_clusters & mask);
		return FindLowestSetBit(maskedHighBits);
	}

	inline size_t ClusterSpanIteratorBase::ResolveOffset() const
	{
		return static_cast<size_t>(m_cluster->m_baseIndex) * 32u + ResolveRealBit();
	}

	inline std::strong_ordering ClusterSpanIteratorBase::operator<=>(const ClusterSpanIteratorBase &other) const
	{
		if (m_cluster == other.m_cluster)
			return m_searchStartBit <=> other.m_searchStartBit;

		return m_cluster <=> other.m_cluster;
	}

	inline bool ClusterSpanIteratorBase::operator==(const ClusterSpanIteratorBase &other) const
	{
		return m_cluster == other.m_cluster;
	}
}

namespace rkit::data
{
	template<class T>
	ClusterSpanIterator<T>::ClusterSpanIterator(const T *instances, const ClusterRef *cluster, uint_fast8_t searchStartBit)
		: m_instances(instances)
		, m_base(cluster, searchStartBit)
	{
	}

	template<class T>
	std::strong_ordering ClusterSpanIterator<T>::operator<=>(const ClusterSpanIterator<T> &other) const
	{
		return m_base <=> other.m_base;
	}

	template<class T>
	bool ClusterSpanIterator<T>::operator==(const ClusterSpanIterator<T> &other) const
	{
		return m_base == other.m_base;
	}

	template<class T>
	ClusterSpanIterator<T> &ClusterSpanIterator<T>::operator++()
	{
		m_base.Increment();
		return *this;
	}

	template<class T>
	ClusterSpanIterator<T> ClusterSpanIterator<T>::operator++(int)
	{
		ClusterSpanIterator<T> clone = (*this);
		return ++clone;
	}

	template<class T>
	const T *ClusterSpanIterator<T>::operator *() const
	{
		return Resolve();
	}

	template<class T>
	const T *ClusterSpanIterator<T>::operator->() const
	{
		return Resolve();
	}

	template<class T>
	const T *ClusterSpanIterator<T>::Resolve() const
	{
		return m_instances + m_base.ResolveOffset();
	}

	template<class T>
	ClusterSpan<T>::ClusterSpan(const T *instances, rkit::Span<const ClusterRef> clusters)
		: m_instances(instances)
		, m_clusters(clusters)
	{
	}

	template<class T>
	ClusterSpanIterator<T> ClusterSpan<T>::begin() const
	{
		return ClusterSpanIterator(m_instances, m_clusters.Ptr(), 0);
	}

	template<class T>
	ClusterSpanIterator<T> ClusterSpan<T>::end() const
	{
		return ClusterSpanIterator(m_instances, m_clusters.Ptr() + m_clusters.Count(), 0);
	}

	template<class T>
	void ClusterRefVector<T>::SetInstances(rkit::Span<T> instances)
	{
		m_instances = instances;
	}

	template<class T>
	rkit::Vector<ClusterRef> &ClusterRefVector<T>::ModifyClusterVector()
	{
		return m_clusters;
	}

	template<class T>
	const rkit::Vector<ClusterRef> &ClusterRefVector<T>::GetClusterVector() const
	{
		return m_clusters;
	}
}

