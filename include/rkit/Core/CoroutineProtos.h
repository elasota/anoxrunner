namespace rkit
{
	namespace coro
	{
		template<class TSignature>
		class MethodStarter;

		template<class TSignature>
		class FunctionStarter;

#if RKIT_IS_DEBUG
		struct InspectableStackFrameBase;
		typedef InspectableStackFrameBase StackFrameBase;
#else
		struct StackFrameBase;
#endif
	}
}

#define CORO_DECL_METHOD_BASE(name, prefix, suffix, ...)	\
	typedef void CoroSignature_ ## name(__VA_ARGS__);\
	prefix ::rkit::coro::MethodStarter<CoroSignature_ ## name> Async ## name() suffix;\
	struct CoroFunction_ ##name

#define CORO_DECL_METHOD(name, ...)	CORO_DECL_METHOD_BASE(name, , , __VA_ARGS__)
#define CORO_DECL_METHOD_VIRTUAL(name, ...)	CORO_DECL_METHOD_BASE(name, virtual, , __VA_ARGS__)
#define CORO_DECL_METHOD_ABSTRACT(name, ...)	CORO_DECL_METHOD_BASE(name, virtual, = 0, __VA_ARGS__)

#define CORO_DECL_METHOD_OVERRIDE(name, ...) \
	::rkit::coro::MethodStarter<CoroSignature_ ## name> Async ## name() override; \
	struct CoroFunction_ ## name
