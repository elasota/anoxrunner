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
