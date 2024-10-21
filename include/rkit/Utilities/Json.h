#include "rkit/Core/CoreDefs.h"
#include "rkit/Core/String.h"

#include <cstring>

namespace rkit
{
	template<class T>
	class Span;

	namespace utils
	{
		struct JsonValue;

		enum class JsonElementType
		{
			kNull,
			kBool,
			kObject,
			kArray,
			kString,
			kNumber,
		};

		typedef Result(*JsonObjectIteratorCallback_t)(void *userdata, const char *keyCharPtr, size_t keyLength, const JsonValue &jsonValue, bool &shouldContinue);

		struct JsonValueVFTable
		{
			Result(*m_toBool)(const void *jsonValue, bool &outBool);
			Result(*m_toString)(const void *jsonValue, const char *&outCharPtr, size_t &outLength);
			Result(*m_toNumber)(const void *jsonValue, double &outNumber);
			Result(*m_getArraySize)(const void *jsonValue, size_t &outSize);
			Result(*m_getArrayElement)(const void *jsonValue, size_t index, JsonValue &outJsonValue);
			Result(*m_iterateObject)(const void *jsonValue, void *userdata, JsonObjectIteratorCallback_t callback);
			Result(*m_objectHasElement)(const void *jsonValue, const char *keyChars, size_t keyLength, bool &outHasElement);
			Result(*m_getObjectElement)(const void *jsonValue, const char *keyChars, size_t keyLength, JsonValue &outJsonValue);
		};

		struct JsonValue
		{
		public:
			JsonValue();
			JsonValue(JsonElementType type, const JsonValueVFTable *vptr, const void *jsonValuePtr);

			JsonElementType GetType() const;

			inline Result ToBool(bool &outBool) const
			{
				return m_vptr->m_toBool(m_jsonValuePtr, outBool);
			}

			Result ToString(StringView &outStrView) const;

			inline Result ToNumber(double &outNumber) const
			{
				return m_vptr->m_toNumber(m_jsonValuePtr, outNumber);
			}

			inline Result GetArraySize(size_t &outSize) const
			{
				return m_vptr->m_getArraySize(m_jsonValuePtr, outSize);
			}

			inline Result GetArrayElement(size_t index, JsonValue &outJsonValue) const
			{
				return m_vptr->m_getArrayElement(m_jsonValuePtr, index, outJsonValue);
			}

			inline Result IterateObject(void *userdata, JsonObjectIteratorCallback_t callback) const
			{
				return m_vptr->m_iterateObject(m_jsonValuePtr, userdata, callback);
			}

			template<class TCallable>
			Result IterateObjectWithCallable(const TCallable &callable) const;

			template<class TClass>
			Result IterateObjectWithMethod(TClass *obj, Result (TClass::*method)(const StringView &, const JsonValue &, bool &)) const;

			Result ObjectHasElement(const StringView &key, bool &outHasElement) const;

			Result GetObjectElement(const StringView &key, JsonValue &outJsonValue) const;

		private:
			template<class TCallable>
			class IterateObjectWithCallableHelper
			{
			public:
				explicit IterateObjectWithCallableHelper(const TCallable &callable);

				static Result Thunk(void *userdata, const char *keyCharPtr, size_t keyLength, const JsonValue &jsonValue, bool &shouldContinue);

			private:
				const TCallable &m_callable;
			};

			template<class TClass>
			class IterateObjectWithMethodHelper
			{
			public:
				typedef Result(TClass:: *MethodPtrType_t)(const StringView &, const JsonValue &, bool &);

				explicit IterateObjectWithMethodHelper(TClass *obj, MethodPtrType_t method);

				static Result Thunk(void *userdata, const char *keyCharPtr, size_t keyLength, const JsonValue &jsonValue, bool &shouldContinue);

			public:
				TClass *m_obj;
				MethodPtrType_t m_method;
			};

			JsonElementType m_type;
			const JsonValueVFTable *m_vptr;
			const void *m_jsonValuePtr;
		};

		struct IJsonDocument
		{
			virtual ~IJsonDocument() {}

			virtual Result ToJsonValue(JsonValue &outValue) = 0;
		};
	}
}

#include "rkit/Core/Span.h"

inline rkit::utils::JsonValue::JsonValue()
	: m_jsonValuePtr(nullptr)
	, m_type(JsonElementType::kNull)
	, m_vptr(nullptr)
{
}

inline rkit::utils::JsonValue::JsonValue(JsonElementType type, const JsonValueVFTable *vptr, const void *jsonValuePtr)
	: m_type(type)
	, m_vptr(vptr)
	, m_jsonValuePtr(jsonValuePtr)
{
}

inline rkit::Result rkit::utils::JsonValue::ToString(StringView &outStrView) const
{
	const char *charPtr = nullptr;
	size_t length = 0;
	RKIT_CHECK(m_vptr->m_toString(m_jsonValuePtr, charPtr, length));

	outStrView = StringView(charPtr, length);

	return ResultCode::kOK;
}

inline rkit::utils::JsonElementType rkit::utils::JsonValue::GetType() const
{
	return m_type;
}


template<class TCallable>
inline rkit::Result rkit::utils::JsonValue::IterateObjectWithCallable(const TCallable &callable) const
{
	IterateObjectWithCallableHelper<TCallable> helper(callable);

	return IterateObject(&helper, IterateObjectWithCallableHelper<TCallable>::Thunk);
}

template<class TClass>
inline rkit::Result rkit::utils::JsonValue::IterateObjectWithMethod(TClass *obj, Result(TClass:: *method)(const StringView &, const JsonValue &, bool &)) const
{
	IterateObjectWithMethodHelper<TClass> helper(obj, method);

	return IterateObject(&helper, IterateObjectWithMethodHelper<TClass>::Thunk);
}

inline rkit::Result rkit::utils::JsonValue::ObjectHasElement(const StringView &key, bool &outHasElement) const
{
	return m_vptr->m_objectHasElement(m_jsonValuePtr, key.GetChars(), key.Length(), outHasElement);
}

inline rkit::Result rkit::utils::JsonValue::GetObjectElement(const StringView &key, JsonValue &outJsonValue) const
{
	return m_vptr->m_getObjectElement(m_jsonValuePtr, key.GetChars(), key.Length(), outJsonValue);
}


template<class TCallable>
rkit::utils::JsonValue::IterateObjectWithCallableHelper<TCallable>::IterateObjectWithCallableHelper(const TCallable &callable)
	: m_callable(callable)
{
}

template<class TCallable>
rkit::Result rkit::utils::JsonValue::IterateObjectWithCallableHelper<TCallable>::Thunk(void *userdata, const char *keyCharPtr, size_t keyLength, const JsonValue &jsonValue, bool &shouldContinue)
{
	const TCallable &callable = static_cast<IterateObjectWithCallableHelper<TCallable>*>(userdata)->m_callable;

	return callable(StringView(keyCharPtr, keyLength), jsonValue, shouldContinue);
}

template<class TClass>
rkit::utils::JsonValue::IterateObjectWithMethodHelper<TClass>::IterateObjectWithMethodHelper(TClass *obj, MethodPtrType_t method)
	: m_obj(obj)
	, m_method(method)
{
}

template<class TClass>
rkit::Result rkit::utils::JsonValue::IterateObjectWithMethodHelper<TClass>::Thunk(void *userdata, const char *keyCharPtr, size_t keyLength, const JsonValue &jsonValue, bool &shouldContinue)
{
	IterateObjectWithMethodHelper<TClass> *self = static_cast<IterateObjectWithMethodHelper<TClass> *>(userdata);

	return self->m_obj->*(self->m_method)(StringView(keyCharPtr, keyLength), jsonValue, shouldContinue);
}
