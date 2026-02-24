#pragma once

#include "CoroutineProtos.h"
#include "Result.h"
#include "StringProto.h"

#include <coroutine>
#include <cstddef>

#include <exception>

namespace rkit::coro::priv
{
	template<class TReturnType>
	class CoroutineAwaiterBase
	{
	public:
		CoroutineAwaiterBase() = delete;
		CoroutineAwaiterBase(const CoroutineAwaiterBase &) = delete;
		CoroutineAwaiterBase &operator=(const CoroutineAwaiterBase &) = delete;

		explicit CoroutineAwaiterBase(std::coroutine_handle<Promise<TReturnType>> coroHandle);

		bool await_ready() const;
		void await_suspend(std::coroutine_handle<> continuation) const;

	protected:
		std::coroutine_handle<Promise<TReturnType>> m_coroHandle;
	};

	template<class TReturnType>
	class CoroutineAwaiter : public CoroutineAwaiterBase<TReturnType>
	{
	public:
		explicit CoroutineAwaiter(std::coroutine_handle<Promise<TReturnType>> coroHandle);

		TReturnType await_resume();
	};

	template<class TReturnType>
	class CoroutineAwaiter<TReturnType &> : public CoroutineAwaiterBase<TReturnType &>
	{
	public:
		explicit CoroutineAwaiter(std::coroutine_handle<Promise<TReturnType &>> coroHandle);

		TReturnType &await_resume();
	};

	template<class TReturnType>
	class CoroutineAwaiter<TReturnType &&> : public CoroutineAwaiterBase<TReturnType &&>
	{
	public:
		explicit CoroutineAwaiter(std::coroutine_handle<Promise<TReturnType &&>> coroHandle);

		TReturnType &&await_resume();
	};

	template<>
	class CoroutineAwaiter<void> : public CoroutineAwaiterBase<void>
	{
	public:
		explicit CoroutineAwaiter(std::coroutine_handle<Promise<void>> coroHandle);

		void await_resume();
	};

	template<class TReturnType>
	class Returner
	{
	public:
		template<class TReturnArg>
		void return_value(TReturnArg &&returnArg);

		TReturnType *GetRVStorage();

	private:
		alignas(TReturnType) char m_storage[sizeof(TReturnType)];
	};

	template<class TReturnType>
	class Returner<TReturnType &>
	{
	public:
		void return_value(TReturnType &returnArg);

		TReturnType *GetRV();

	private:
		TReturnType *m_returnValue;
	};

	template<class TReturnType>
	class Returner<TReturnType &&>
	{
	public:
		void return_value(TReturnType &&returnArg);

		TReturnType *GetRV();

	private:
		TReturnType *m_returnValue;
	};

	template<>
	class Returner<void>
	{
	public:
		static void return_void();
	};

	class CoroutineFinalSuspender
	{
	public:
		explicit CoroutineFinalSuspender(std::coroutine_handle<> continuation);

		static bool await_ready() noexcept;
		std::coroutine_handle<> await_suspend(std::coroutine_handle<> suspended) const noexcept;
		static void await_resume() noexcept;

	private:
		CoroutineFinalSuspender(const CoroutineFinalSuspender &) = delete;
		CoroutineFinalSuspender &operator=(const CoroutineFinalSuspender &) = delete;

		std::coroutine_handle<> m_continuation;
	};

	struct PromiseAllocatorHeader
	{
		void *m_context;
		void (*m_deleter)(void *context, void *mem);

		static size_t ComputeSize();
		static void *AllocateUsingThread(ICoroThread &thread, size_t size);

	private:
		static void DeallocateUsingThread(void *context, void *mem);
	};

	template<class TReturnType>
	class Promise : public Returner<TReturnType>
	{
	private:
		template<class TReturnType>
		friend class Returner;

		template<class TReturnType>
		friend class CoroutineAwaiter;

		template<class TReturnType>
		friend class CoroutineAwaiterBase;

	public:
#ifdef __INTELLISENSE__
		// Hack for broken IntelliSense
		Promise();
#endif

		template<class... TArgs>
		Promise(ICoroThread &thread, TArgs&&...);

		template<class TObject, class... TArgs>
		Promise(TObject &obj, ICoroThread &thread, TArgs&&...);

		~Promise();

		Coroutine<TReturnType> get_return_object();
		std::suspend_always initial_suspend() noexcept;
		CoroutineFinalSuspender final_suspend() noexcept;

		void unhandled_exception();

		void operator delete(void *ptr);

#ifndef __INTELLISENSE__
		// Hack for IntelliSense not recognizing this entire thing
		static Coroutine<TReturnType> get_return_object_on_allocation_failure();
#endif

		Returner<TReturnType> &GetReturnValueStorage();

#ifdef __INTELLISENSE__
		// Hack for IntelliSense not recognizing overloaded allocators
		void *operator new(std::size_t n);
#endif

		template<class... TArgs>
		void *operator new(std::size_t n, ICoroThread &allocator, TArgs&&... args) noexcept;

		template<class TObject, class... TArgs>
		void *operator new(std::size_t n, TObject &obj, ICoroThread &allocator, TArgs&&... args) noexcept;

	private:
		std::coroutine_handle<> m_continuation;

		std::exception_ptr m_exception;
	};
}

namespace rkit::coro
{
	template<class TReturnType = rkit::Result>
	class Coroutine
	{
	public:
		using promise_type = priv::Promise<TReturnType>;

		explicit Coroutine(std::coroutine_handle<promise_type> handle);
		Coroutine(Coroutine &&) = default;
		~Coroutine();

		Coroutine &operator=(Coroutine &&other) = default;

		priv::CoroutineAwaiter<TReturnType> operator co_await();

		std::coroutine_handle<promise_type> GetCoroutineHandle() const;

	private:
		Coroutine() = delete;
		Coroutine(const Coroutine &) = delete;
		Coroutine &operator=(const Coroutine &) = delete;

		std::coroutine_handle<promise_type> m_coroHandle;
	};
}

#include "CoroThread.h"

#if RKIT_RESULT_BEHAVIOR != RKIT_RESULT_BEHAVIOR_EXCEPTION
#include "RKitAssert.h"
#endif

#include <new>
#include <limits>

// Implementations
namespace rkit::coro::priv
{
	template<class TReturnType>
	inline CoroutineAwaiterBase<TReturnType>::CoroutineAwaiterBase(std::coroutine_handle<Promise<TReturnType>> coroHandle)
		: m_coroHandle(coroHandle)
	{
		if (coroHandle)
			coroHandle.resume();
	}

	template<class TReturnType>
	bool CoroutineAwaiterBase<TReturnType>::await_ready() const
	{
		return (!m_coroHandle) || m_coroHandle.done();
	}

	template<class TReturnType>
	void CoroutineAwaiterBase<TReturnType>::await_suspend(std::coroutine_handle<> continuation) const
	{
		m_coroHandle.promise().m_continuation = continuation;
	}

	template<class TReturnType>
	CoroutineAwaiter<TReturnType>::CoroutineAwaiter(std::coroutine_handle<Promise<TReturnType>> coroHandle)
		: CoroutineAwaiterBase<TReturnType>(coroHandle)
	{
	}

	template<class TReturnType>
	TReturnType CoroutineAwaiter<TReturnType>::await_resume()
	{
		if (!this->m_coroHandle)
		{
			RKIT_THROW(rkit::ResultCode::kCoroStackOverflow);
		}
		else
		{
			Promise<TReturnType> &promise = this->m_coroHandle.promise();

			if (promise.m_exception)
			{
				std::exception_ptr exPtr = promise.m_exception;
				this->m_coroHandle.destroy();
				std::rethrow_exception(exPtr);
			}

			Returner<TReturnType> &returner = this->m_coroHandle.promise();
			TReturnType *storedRV = returner.GetRVStorage();

			TReturnType temp(static_cast<TReturnType &&>(*storedRV));
			storedRV->~TReturnType();

			this->m_coroHandle.destroy();

			return static_cast<TReturnType &&>(temp);
		}
	}

	template<class TReturnType>
	CoroutineAwaiter<TReturnType &>::CoroutineAwaiter(std::coroutine_handle<Promise<TReturnType &>> coroHandle)
		: CoroutineAwaiterBase<TReturnType &>(coroHandle)
	{
	}

	template<class TReturnType>
	TReturnType &CoroutineAwaiter<TReturnType &>::await_resume()
	{
		if (!this->m_coroHandle)
		{
#if RKIT_RESULT_BEHAVIOR == RKIT_RESULT_BEHAVIOR_EXCEPTION
			RKIT_THROW(rkit::ResultCode::kCoroStackOverflow);
#else
			RKIT_ASSERT(false);
			std::terminate();
#endif
		}
		Promise<TReturnType> &promise = this->m_coroHandle.promise();

		if (promise.m_exception)
		{
			std::exception_ptr exPtr = promise.m_exception;
			this->m_coroHandle.destroy();
			std::rethrow_exception(exPtr);
		}

		Returner<TReturnType &> &returner = promise.GetReturnValueStorage();
		TReturnType *temp = returner.GetRV();

		this->m_coroHandle.destroy();

		return *temp;
	}

	template<class TReturnType>
	CoroutineAwaiter<TReturnType &&>::CoroutineAwaiter(std::coroutine_handle<Promise<TReturnType &&>> coroHandle)
		: CoroutineAwaiterBase<TReturnType &&>(coroHandle)
	{
	}

	template<class TReturnType>
	TReturnType &&CoroutineAwaiter<TReturnType &&>::await_resume()
	{
		if (!this->m_coroHandle)
		{
#if RKIT_RESULT_BEHAVIOR == RKIT_RESULT_BEHAVIOR_EXCEPTION
			RKIT_THROW(rkit::ResultCode::kCoroStackOverflow);
#else
			RKIT_ASSERT(false);
			std::terminate();
#endif
		}
		Promise<TReturnType> &promise = this->m_coroHandle.promise();

		if (promise.m_exception)
		{
			std::exception_ptr exPtr = promise.m_exception;
			this->m_coroHandle.destroy();
			std::rethrow_exception(exPtr);
		}

		Returner<TReturnType &> &returner = promise.GetReturnValueStorage();
		TReturnType *temp = returner.GetRV();

		this->m_coroHandle.destroy();

		return static_cast<TReturnType &&>(*temp);
	}

	inline CoroutineAwaiter<void>::CoroutineAwaiter(std::coroutine_handle<Promise<void>> coroHandle)
		: CoroutineAwaiterBase<void>(coroHandle)
	{
	}

	inline void CoroutineAwaiter<void>::await_resume()
	{
		if (!this->m_coroHandle)
		{
#if RKIT_RESULT_BEHAVIOR == RKIT_RESULT_BEHAVIOR_EXCEPTION
			RKIT_THROW(rkit::ResultCode::kCoroStackOverflow);
#else
			RKIT_ASSERT(false);
			std::terminate();
#endif
		}

		Promise<void> &promise = this->m_coroHandle.promise();

		if (promise.m_exception)
		{
			std::exception_ptr exPtr = promise.m_exception;
			this->m_coroHandle.destroy();
			std::rethrow_exception(exPtr);
		}

		this->m_coroHandle.destroy();
	}

	template<class TReturnType>
	template<class TReturnArg>
	void Returner<TReturnType>::return_value(TReturnArg &&returnArg)
	{
		new (m_storage) TReturnType(std::forward<TReturnArg>(returnArg));
	}

	template<class TReturnType>
	TReturnType *Returner<TReturnType>::GetRVStorage()
	{
		return reinterpret_cast<TReturnType *>(m_storage);
	}

	template<class TReturnType>
	void Returner<TReturnType &>::return_value(TReturnType &returnArg)
	{
		m_returnValue = std::addressof(returnArg);
	}

	template<class TReturnType>
	TReturnType *Returner<TReturnType &>::GetRV()
	{
		return m_returnValue;
	}

	template<class TReturnType>
	void Returner<TReturnType &&>::return_value(TReturnType &&returnArg)
	{
		m_returnValue = std::addressof(returnArg);
	}

	template<class TReturnType>
	TReturnType *Returner<TReturnType &&>::GetRV()
	{
		return m_returnValue;
	}

	inline void Returner<void>::return_void()
	{
	}

	inline CoroutineFinalSuspender::CoroutineFinalSuspender(std::coroutine_handle<> continuation)
		: m_continuation(continuation)
	{
	}

	inline bool CoroutineFinalSuspender::await_ready() noexcept
	{
		return false;
	}

	inline std::coroutine_handle<> CoroutineFinalSuspender::await_suspend(std::coroutine_handle<> suspended) const noexcept
	{
		if (m_continuation)
			return m_continuation;
		else
			return std::noop_coroutine();
	}

	inline void CoroutineFinalSuspender::await_resume() noexcept
	{
	}

	inline size_t PromiseAllocatorHeader::ComputeSize()
	{
		const size_t alignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__;
		const size_t size = sizeof(PromiseAllocatorHeader);
		const size_t remainder = (size % alignment);
		if (remainder == 0)
			return size;
		else
			return size + (alignment - remainder);
	}

	inline void *PromiseAllocatorHeader::AllocateUsingThread(ICoroThread &thread, size_t size)
	{
		const size_t headerSize = ComputeSize();

		if (std::numeric_limits<size_t>::max() - headerSize < size)
			return nullptr;

		const size_t expandedSize = size + headerSize;
		uint8_t *mem = static_cast<uint8_t *>(thread.AllocFrame(expandedSize));
		if (!mem)
			return nullptr;

		PromiseAllocatorHeader *header = reinterpret_cast<PromiseAllocatorHeader *>(mem);
		header->m_context = &thread;
		header->m_deleter = DeallocateUsingThread;

		return mem + headerSize;
	}

	inline void PromiseAllocatorHeader::DeallocateUsingThread(void *context, void *mem)
	{
		static_cast<ICoroThread *>(context)->DeallocFrame(mem);
	}

	template<class TReturnType>
	template<class... TArgs>
	Promise<TReturnType>::Promise(ICoroThread &thread, TArgs&&...)
	{
	}

	template<class TReturnType>
	template<class TObject, class... TArgs>
	Promise<TReturnType>::Promise(TObject &object, ICoroThread &thread, TArgs&&...)
	{
	}

	template<class TReturnType>
	Promise<TReturnType>::~Promise()
	{
		if (m_continuation)
			m_continuation.destroy();
	}

	template<class TReturnType>
	Coroutine<TReturnType> Promise<TReturnType>::get_return_object()
	{
		return Coroutine<TReturnType>(std::coroutine_handle<Promise>::from_promise(*this));
	}

	template<class TReturnType>
	std::suspend_always Promise<TReturnType>::initial_suspend() noexcept
	{
		return std::suspend_always();
	}

	template<class TReturnType>
	CoroutineFinalSuspender Promise<TReturnType>::final_suspend() noexcept
	{
		std::coroutine_handle<> continuation = m_continuation;
		m_continuation = std::coroutine_handle<>();
		return CoroutineFinalSuspender(continuation);
	}

	template<class TReturnType>
	void Promise<TReturnType>::unhandled_exception()
	{
		m_exception = std::current_exception();
	}

	template<class TReturnType>
	void Promise<TReturnType>::operator delete(void *ptr)
	{
		uint8_t *mem = static_cast<uint8_t *>(ptr);

		PromiseAllocatorHeader *header = reinterpret_cast<PromiseAllocatorHeader *>(mem - PromiseAllocatorHeader::ComputeSize());

		header->m_deleter(header->m_context, header);
	}

#ifndef __INTELLISENSE__
	template<class TReturnType>
	Coroutine<TReturnType> Promise<TReturnType>::get_return_object_on_allocation_failure()
	{
		return Coroutine<TReturnType>(std::coroutine_handle<Promise>());
	}
#endif

	template<class TReturnType>
	Returner<TReturnType> &Promise<TReturnType>::GetReturnValueStorage()
	{
		return *this;
	}

	template<class TReturnType>
	template<class... TArgs>
	void *Promise<TReturnType>::operator new(std::size_t size, ICoroThread &thread, TArgs&&... args) noexcept
	{
		return PromiseAllocatorHeader::AllocateUsingThread(thread, size);
	}

	template<class TReturnType>
	template<class TObject, class... TArgs>
	void *Promise<TReturnType>::operator new(std::size_t size, TObject &obj, ICoroThread &thread, TArgs&&... args) noexcept
	{
		return PromiseAllocatorHeader::AllocateUsingThread(thread, size);
	}
}



namespace rkit::coro
{
	template<class TReturnType>
	Coroutine<TReturnType>::Coroutine(std::coroutine_handle<promise_type> handle)
		: m_coroHandle(handle)
	{
	}

	template<class TReturnType>
	Coroutine<TReturnType>::~Coroutine()
	{
	}

	template<class TReturnType>
	priv::CoroutineAwaiter<TReturnType> Coroutine<TReturnType>::operator co_await()
	{
		return priv::CoroutineAwaiter<TReturnType>(m_coroHandle);
	}

	template<class TReturnType>
	std::coroutine_handle<typename Coroutine<TReturnType>::promise_type> Coroutine<TReturnType>::GetCoroutineHandle() const
	{
		return m_coroHandle;
	}
}


#if RKIT_RESULT_BEHAVIOR == RKIT_RESULT_BEHAVIOR_ENUM || RKIT_RESULT_BEHAVIOR == RKIT_RESULT_BEHAVIOR_CLASS

#define CORO_THROW(expr)	co_return (::rkit::priv::ThrowResult(expr));

#define CORO_RETURN_OK co_return (static_cast<::rkit::Result>(::rkit::ResultCode::kOK))

#define CORO_CHECK(expr) do {\
	::rkit::Result RKIT_PP_CONCAT(exprResult_, __LINE__) = (expr);\
	if (static_cast<uint64_t>(RKIT_PP_CONCAT(exprResult_, __LINE__)) != 0)\
		co_return RKIT_PP_CONCAT(exprResult_, __LINE__);\
} while (false)


#elif RKIT_RESULT_BEHAVIOR == RKIT_RESULT_BEHAVIOR_EXCEPTION

#define CORO_THROW(expr)	RKIT_THROW(expr)

#define CORO_CHECK(expr) RKIT_CHECK(expr)

#define CORO_RETURN_OK co_return

#endif
