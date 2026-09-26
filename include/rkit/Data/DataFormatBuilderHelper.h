#pragma once

#include <stddef.h>

namespace rkit
{
	template<class T>
	class RCPtr;

	template<class T>
	class Vector;

	template<class T>
	class Span;

	template<class T, size_t TSize>
	class StaticArray;
}

namespace rkit::data::builder
{
	template<class T>
	class InvertibleBuilder;
}

namespace rkit::data
{
	template<class T>
	class Invertible;

	template<class T>
	class ClusterSpan;

	class DataFormatBuilderHelper
	{
	public:
		template<class T>
		static void InitRCPtrVector(Vector<RCPtr<T>> &vector, size_t count);

		template<class TOut, class TIn, class TFunc>
		static void ConvertInvertible(builder::InvertibleBuilder<TOut> &outValue, const Invertible<TIn> &inValue, const TFunc &func);

		template<class TOut, class TIn, size_t TSize, class TFunc>
		static void ConvertFixedArray(StaticArray<TOut, TSize> &outArray, const StaticArray<TIn, TSize> &inArray, const TFunc &func);

		template<class TOut, class TIn, class TFunc>
		static void ConvertClusterArray(Vector<TOut> &outArray, const ClusterSpan<TIn> &inArray, const TFunc &func);

		template<class TOut, class TIn, class TFunc>
		static void ConvertVector(Vector<TOut> &outVector, const Span<const TIn> inSpan, const TFunc &func);
	};
}

#include "rkit/Core/RefCounted.h"
#include "rkit/Core/NewDelete.h"

#include "rkit/Data/Invertible.h"
#include "rkit/Data/ClusterRefVector.h"

namespace rkit::data
{
	template<class T>
	void DataFormatBuilderHelper::InitRCPtrVector(Vector<::rkit::RCPtr<T>> &vector, size_t count)
	{
		vector.Resize(count);
		for (RCPtr<T> &rcPtr : vector)
			rcPtr = MakeRC(::rkit::New<T>());
	}

	template<class TOut, class TIn, class TFunc>
	void DataFormatBuilderHelper::ConvertInvertible(builder::InvertibleBuilder<TOut> &outValue, const Invertible<TIn> &inValue, const TFunc &func)
	{
		outValue.SetInverted(inValue.IsInverted());
		func(outValue.ModifyValue(), inValue.GetValue());
	}

	template<class TOut, class TIn, size_t TSize, class TFunc>
	void DataFormatBuilderHelper::ConvertFixedArray(StaticArray<TOut, TSize> &outArray, const StaticArray<TIn, TSize> &inArray, const TFunc &func)
	{
		for (size_t i = 0; i < TSize; i++)
			func(outArray[i], inArray[i]);
	}

	template<class TOut, class TIn, class TFunc>
	void DataFormatBuilderHelper::ConvertClusterArray(Vector<TOut> &outArray, const ClusterSpan<TIn> &inArray, const TFunc &func)
	{
		for (const TIn *inValue : inArray)
		{
			TOut outElement;
			func(outElement, inValue);
			outArray.Append(std::move(outElement));
		}
	}

	template<class TOut, class TIn, class TFunc>
	void DataFormatBuilderHelper::ConvertVector(Vector<TOut> &outVector, const Span<const TIn> inSpan, const TFunc &func)
	{
		const size_t count = inSpan.Count();

		outVector.Resize(inSpan.Count());

		const TIn *inPtr = inSpan.Ptr();
		TOut *outPtr = outVector.GetBuffer();

		for (size_t i = 0; i < count; i++)
			func(outPtr[i], inPtr[i]);
	}
}
