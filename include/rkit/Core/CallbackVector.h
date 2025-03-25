#pragma once

#include "RKitAssert.h"

#include <cstddef>
#include <limits>

namespace rkit
{
	template<class T>
	class Span;

	template<class T>
	class SpanIterator;

	template<class T>
	class ICallbackConstVector
	{
	public:
		typedef SpanIterator<T> Iterator_t;
		typedef SpanIterator<const T> ConstIterator_t;

		virtual const T *GetBuffer() const = 0;

		virtual size_t Count() const = 0;

		virtual Span<const T> ToSpan() const = 0;

		virtual const T &operator[](size_t index) const = 0;

		virtual ConstIterator_t begin() const = 0;

		virtual ConstIterator_t end() const = 0;
	};

	template<class T>
	class ICallbackVector : public ICallbackConstVector<T>
	{
	public:
		virtual void RemoveRange(size_t firstArg, size_t numArgs) = 0;
		virtual Result Resize(size_t size) = 0;
		virtual Result Reserve(size_t size) = 0;
		virtual void ShrinkToSize(size_t size) = 0;

		virtual Result Append(const T &item) = 0;
		virtual Result Append(T &&item) = 0;
		virtual Result Append(const Span<const T> &items) = 0;

		virtual void Reset() = 0;

		virtual T *GetBuffer() = 0;

		virtual Span<T> ToSpan() = 0;

		virtual T &operator[](size_t index) = 0;

		virtual Iterator_t begin() = 0;

		virtual Iterator_t end() = 0;
	};

	template<class T, class TVector>
	class CallbackConstVectorWrapper : public virtual ICallbackConstVector<T>
	{
	public:
		CallbackConstVectorWrapper() = delete;
		explicit CallbackConstVectorWrapper(const TVector &vector);

		const T *GetBuffer() const override;

		size_t Count() const override;

		Span<const T> ToSpan() const override;

		const T &operator[](size_t index) const override;

		ConstIterator_t begin() const override;

		ConstIterator_t end() const override;

	private:
		TVector &m_vector;
	};

	template<class T, class TVector>
	class CallbackVectorWrapper : public ICallbackVector<T>
	{
	public:
		explicit CallbackVectorWrapper(TVector &vector);

		void RemoveRange(size_t firstArg, size_t numArgs) override;
		Result Resize(size_t size) override;
		Result Reserve(size_t size) override;
		void ShrinkToSize(size_t size) override;

		Result Append(const T &item) override;
		Result Append(T &&item) override;
		Result Append(const Span<const T> &items) override;

		void Reset() override;

		T *GetBuffer() override;
		const T *GetBuffer() const override;

		size_t Count() const override;

		Span<T> ToSpan() override;
		Span<const T> ToSpan() const override;

		T &operator[](size_t index) override;
		const T &operator[](size_t index) const override;

		Iterator_t begin() override;
		ConstIterator_t begin() const override;

		Iterator_t end() override;
		ConstIterator_t end() const override;

	private:
		TVector &m_vector;
	};
}

namespace rkit
{
	template<class T, class TVector>
	CallbackConstVectorWrapper<T, TVector>::CallbackConstVectorWrapper(const TVector &vector)
		: m_vector(vector)
	{
	}

	template<class T, class TVector>
	const T *CallbackConstVectorWrapper<T, TVector>::GetBuffer() const
	{
		return m_vector.GetBuffer();
	}

	template<class T, class TVector>
	size_t CallbackConstVectorWrapper<T, TVector>::Count() const
	{
		return m_vector.Count();
	}

	template<class T, class TVector>
	Span<const T> CallbackConstVectorWrapper<T, TVector>::ToSpan() const
	{
		return m_vector.ToSpan();
	}

	template<class T, class TVector>
	const T &CallbackConstVectorWrapper<T, TVector>::operator[](size_t index) const
	{
		return m_vector[index];
	}

	template<class T, class TVector>
	typename CallbackVectorWrapper<T, TVector>::ConstIterator_t CallbackConstVectorWrapper<T, TVector>::begin() const
	{
		return m_vector.begin();
	}

	template<class T, class TVector>
	typename CallbackVectorWrapper<T, TVector>::ConstIterator_t CallbackConstVectorWrapper<T, TVector>::end() const
	{
		return m_vector.end();
	}

	template<class T, class TVector>
	CallbackVectorWrapper<T, TVector>::CallbackVectorWrapper(TVector &vector)
		: m_vector(vector)
	{
	}

	template<class T, class TVector>
	void CallbackVectorWrapper<T, TVector>::RemoveRange(size_t firstArg, size_t numArgs)
	{
		return m_vector.RemoveRange(firstArg, numArgs);
	}

	template<class T, class TVector>
	Result CallbackVectorWrapper<T, TVector>::Resize(size_t size)
	{
		return m_vector.Resize(size);
	}

	template<class T, class TVector>
	Result CallbackVectorWrapper<T, TVector>::Reserve(size_t size)
	{
		return m_vector.Reserve(size);
	}

	template<class T, class TVector>
	void CallbackVectorWrapper<T, TVector>::ShrinkToSize(size_t size)
	{
		m_vector.ShrinkToSize(size);
	}

	template<class T, class TVector>
	Result CallbackVectorWrapper<T, TVector>::Append(const T &item)
	{
		return m_vector.Append(item);
	}

	template<class T, class TVector>
	Result CallbackVectorWrapper<T, TVector>::Append(T &&item)
	{
		return m_vector.Append(std::move(item));
	}

	template<class T, class TVector>
	Result CallbackVectorWrapper<T, TVector>::Append(const Span<const T> &items)
	{
		return m_vector.Append(items);
	}

	template<class T, class TVector>
	void CallbackVectorWrapper<T, TVector>::Reset()
	{
		m_vector.Reset();
	}

	template<class T, class TVector>
	T *CallbackVectorWrapper<T, TVector>::GetBuffer()
	{
		return m_vector.GetBuffer();
	}

	template<class T, class TVector>
	const T *CallbackVectorWrapper<T, TVector>::GetBuffer() const
	{
		return m_vector.GetBuffer();
	}

	template<class T, class TVector>
	size_t CallbackVectorWrapper<T, TVector>::Count() const
	{
		return m_vector.Count();
	}

	template<class T, class TVector>
	Span<T> CallbackVectorWrapper<T, TVector>::ToSpan()
	{
		return m_vector.ToSpan();
	}

	template<class T, class TVector>
	Span<const T> CallbackVectorWrapper<T, TVector>::ToSpan() const
	{
		return m_vector.ToSpan();
	}

	template<class T, class TVector>
	T &CallbackVectorWrapper<T, TVector>::operator[](size_t index)
	{
		return m_vector[index];
	}

	template<class T, class TVector>
	const T &CallbackVectorWrapper<T, TVector>::operator[](size_t index) const override;
	{
		return m_vector[index];
	}

	template<class T, class TVector>
	typename CallbackVectorWrapper<T, TVector>::Iterator_t CallbackVectorWrapper<T, TVector>::begin()
	{
		return m_vector.begin();
	}

	template<class T, class TVector>
	typename CallbackVectorWrapper<T, TVector>::ConstIterator_t begin() const
	{
		return m_vector.begin();
	}

	template<class T, class TVector>
	CallbackVectorWrapper<T, TVector>::Iterator_t CallbackVectorWrapper<T, TVector>::end()
	{
		return m_vector.end();
	}

	template<class T, class TVector>
	CallbackVectorWrapper<T, TVector>::ConstIterator_t CallbackVectorWrapper<T, TVector>::end() const
	{
		return m_vector.end();
	}
}
