#include "BBoxProto.h"
#include "Vec.h"

namespace rkit::math
{
	template<class TComponent, size_t TSize>
	class BBox final
	{
	public:
		BBox() = default;
		BBox(const BBox &other) = default;
		BBox(const Vec<TComponent, TSize> &min, const Vec<TComponent, TSize> &max);

		const Vec<TComponent, TSize> &GetMin() const;
		const Vec<TComponent, TSize> &GetMax() const;

		Vec<TComponent, TSize> &ModifyMin();
		Vec<TComponent, TSize> &ModifyMax();

		void SetMin(const Vec<TComponent, TSize> &min);
		void SetMax(const Vec<TComponent, TSize> &max);

	private:
		Vec<TComponent, TSize> m_min;
		Vec<TComponent, TSize> m_max;
	};
}


namespace rkit::math
{
	template<class TComponent, size_t TSize>
	BBox<TComponent, TSize>::BBox(const Vec<TComponent, TSize> &min, const Vec<TComponent, TSize> &max)
		: m_min(min)
		, m_max(max)
	{
	}

	template<class TComponent, size_t TSize>
	const Vec<TComponent, TSize> &BBox<TComponent, TSize>::GetMin() const
	{
		return m_min;
	}

	template<class TComponent, size_t TSize>
	const Vec<TComponent, TSize> &BBox<TComponent, TSize>::GetMax() const
	{
		return m_max;
	}

	template<class TComponent, size_t TSize>
	Vec<TComponent, TSize> &BBox<TComponent, TSize>::ModifyMin()
	{
		return m_min;
	}

	template<class TComponent, size_t TSize>
	Vec<TComponent, TSize> &BBox<TComponent, TSize>::ModifyMax()
	{
		return m_max;
	}

	template<class TComponent, size_t TSize>
	void BBox<TComponent, TSize>::SetMin(const Vec<TComponent, TSize> &min)
	{
		return m_min;
	}

	template<class TComponent, size_t TSize>
	void BBox<TComponent, TSize>::SetMax(const Vec<TComponent, TSize> &max)
	{
		return m_max;
	}
}
