#ifndef SRC_NAPI_INL_H_
#define SRC_NAPI_INL_H_

////////////////////////////////////////////////////////////////////////////////
// N-API C++ Wrapper Classes
//
// Inline header-only implementations for "N-API" ABI-stable C APIs for Node.js.
////////////////////////////////////////////////////////////////////////////////

// Note: Do not include this file directly! Include "napi.h" instead.

#include <cassert>
#include <cstring>

namespace Napi {

// For use in JS to C++ callback wrappers to catch any Napi::Error exceptions
// and rethrow them as JavaScript exceptions before returning from the callback.
#define NAPI_RETHROW_JS_ERROR(env)               \
  catch (const Error& e) {                       \
    e.ThrowAsJavaScriptException();              \
    return nullptr;                              \
  }

// Helpers to handle functions exposed from C++.
namespace details {

template <typename Callable, typename Return>
struct CallbackData {
  static inline
  napi_value Wrapper(napi_env env, napi_callback_info info) {
    try {
      CallbackInfo callbackInfo(env, info);
      CallbackData* callbackData =
        static_cast<CallbackData*>(callbackInfo.Data());
      callbackInfo.SetData(callbackData->data);
      return callbackData->callback(callbackInfo);
    }
    NAPI_RETHROW_JS_ERROR(env)
  }

  Callable callback;
  void* data;
};

template <typename Callable>
struct CallbackData<Callable, void> {
  static inline
  napi_value Wrapper(napi_env env, napi_callback_info info) {
    try {
      CallbackInfo callbackInfo(env, info);
      CallbackData* callbackData =
        static_cast<CallbackData*>(callbackInfo.Data());
      callbackInfo.SetData(callbackData->data);
      callbackData->callback(callbackInfo);
      return nullptr;
    }
    NAPI_RETHROW_JS_ERROR(env)
  }

  Callable callback;
  void* data;
};

template <typename T, typename Finalizer, typename Hint = void>
struct FinalizeData {
  static inline
  void Wrapper(napi_env env, void* data, void* finalizeHint) {
    FinalizeData* finalizeData = static_cast<FinalizeData*>(finalizeHint);
    finalizeData->callback(Env(env), static_cast<T*>(data));
    delete finalizeData;
  }

  static inline
  void WrapperWithHint(napi_env env, void* data, void* finalizeHint) {
    FinalizeData* finalizeData = static_cast<FinalizeData*>(finalizeHint);
    finalizeData->callback(Env(env), static_cast<T*>(data), finalizeData->hint);
    delete finalizeData;
  }

  Finalizer callback;
  Hint* hint;
};

template <typename Getter, typename Setter>
struct AccessorCallbackData {
  static inline
  napi_value GetterWrapper(napi_env env, napi_callback_info info) {
    try {
      CallbackInfo callbackInfo(env, info);
      AccessorCallbackData* callbackData =
        static_cast<AccessorCallbackData*>(callbackInfo.Data());
      return callbackData->getterCallback(callbackInfo);
    }
    NAPI_RETHROW_JS_ERROR(env)
  }

  static inline
  napi_value SetterWrapper(napi_env env, napi_callback_info info) {
    try {
      CallbackInfo callbackInfo(env, info);
      AccessorCallbackData* callbackData =
        static_cast<AccessorCallbackData*>(callbackInfo.Data());
      callbackData->setterCallback(callbackInfo);
      return nullptr;
    }
    NAPI_RETHROW_JS_ERROR(env)
  }

  Getter getterCallback;
  Setter setterCallback;
};

}  // namespace details

////////////////////////////////////////////////////////////////////////////////
// Module registration
////////////////////////////////////////////////////////////////////////////////

#define NODE_API_MODULE(modname, regfunc)                 \
  void __napi_ ## regfunc(napi_env env,                   \
                          napi_value exports,             \
                          napi_value module,              \
                          void* priv) {                   \
    Napi::RegisterModule(env, exports, module, regfunc);  \
  }                                                       \
  NAPI_MODULE(modname, __napi_ ## regfunc);

// Adapt the NAPI_MODULE registration function:
//  - Wrap the arguments in NAPI wrappers.
//  - Catch any NAPI errors and rethrow as JS exceptions.
inline void RegisterModule(napi_env env,
                           napi_value exports,
                           napi_value module,
                           ModuleRegisterCallback registerCallback) {
  try {
      registerCallback(Napi::Env(env),
                       Napi::Object(env, exports),
                       Napi::Object(env, module));
  }
  catch (const Error& e) {
    e.ThrowAsJavaScriptException();
  }
}

////////////////////////////////////////////////////////////////////////////////
// Env class
////////////////////////////////////////////////////////////////////////////////

inline Env::Env(napi_env env) : _env(env) {
}

inline Env::operator napi_env() const {
  return _env;
}

inline Object Env::Global() const {
  napi_value value;
  napi_status status = napi_get_global(*this, &value);
  if (status != napi_ok) throw Error::New(*this);
  return Object(*this, value);
}

inline Value Env::Undefined() const {
  napi_value value;
  napi_status status = napi_get_undefined(*this, &value);
  if (status != napi_ok) throw Error::New(*this);
  return Value(*this, value);
}

inline Value Env::Null() const {
  napi_value value;
  napi_status status = napi_get_null(*this, &value);
  if (status != napi_ok) throw Error::New(*this);
  return Value(*this, value);
}

inline bool Env::IsExceptionPending() const {
  bool result;
  napi_status status = napi_is_exception_pending(_env, &result);
  if (status != napi_ok) result = false; // Checking for a pending exception shouldn't throw.
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// Value class
////////////////////////////////////////////////////////////////////////////////

inline Value::Value() : _env(nullptr), _value(nullptr) {
}

inline Value::Value(napi_env env, napi_value value) : _env(env), _value(value) {
}

inline Value::operator napi_value() const {
  return _value;
}

inline bool Value::operator ==(const Value& other) const {
  return StrictEquals(other);
}

inline bool Value::operator !=(const Value& other) const {
  return !this->operator ==(other);
}

inline bool Value::StrictEquals(const Value& other) const {
  bool result;
  napi_status status = napi_strict_equals(_env, *this, other, &result);
  if (status != napi_ok) throw Error::New(_env);
  return result;
}

inline Napi::Env Value::Env() const {
  return Napi::Env(_env);
}

inline napi_valuetype Value::Type() const {
  if (_value == nullptr) {
    return napi_undefined;
  }

  napi_valuetype type;
  napi_status status = napi_typeof(_env, _value, &type);
  if (status != napi_ok) throw Error::New(_env);
  return type;
}

inline bool Value::IsUndefined() const {
  return Type() == napi_undefined;
}

inline bool Value::IsNull() const {
  return Type() == napi_null;
}

inline bool Value::IsBoolean() const {
  return Type() == napi_boolean;
}

inline bool Value::IsNumber() const {
  return Type() == napi_number;
}

inline bool Value::IsString() const {
  return Type() == napi_string;
}

inline bool Value::IsSymbol() const {
  return Type() == napi_symbol;
}

inline bool Value::IsArray() const {
  if (_value == nullptr) {
    return false;
  }

  bool result;
  napi_status status = napi_is_array(_env, _value, &result);
  if (status != napi_ok) throw Error::New(_env);
  return result;
}

inline bool Value::IsArrayBuffer() const {
  if (_value == nullptr) {
    return false;
  }

  bool result;
  napi_status status = napi_is_arraybuffer(_env, _value, &result);
  if (status != napi_ok) throw Error::New(_env);
  return result;
}

inline bool Value::IsTypedArray() const {
  if (_value == nullptr) {
    return false;
  }

  bool result;
  napi_status status = napi_is_typedarray(_env, _value, &result);
  if (status != napi_ok) throw Error::New(_env);
  return result;
}

inline bool Value::IsObject() const {
  return Type() == napi_object;
}

inline bool Value::IsFunction() const {
  return Type() == napi_function;
}

inline bool Value::IsBuffer() const {
  if (_value == nullptr) {
    return false;
  }

  bool result;
  napi_status status = napi_is_buffer(_env, _value, &result);
  if (status != napi_ok) throw Error::New(_env);
  return result;
}

template <typename T>
inline T Value::As() const {
  return T(_env, _value);
}

inline Boolean Value::ToBoolean() const {
  napi_value result;
  napi_status status = napi_coerce_to_bool(_env, _value, &result);
  if (status != napi_ok) throw Error::New(_env);
  return Boolean(_env, result);
}

inline Number Value::ToNumber() const {
  napi_value result;
  napi_status status = napi_coerce_to_number(_env, _value, &result);
  if (status != napi_ok) throw Error::New(_env);
  return Number(_env, result);
}

inline String Value::ToString() const {
  napi_value result;
  napi_status status = napi_coerce_to_string(_env, _value, &result);
  if (status != napi_ok) throw Error::New(_env);
  return String(_env, result);
}

inline Object Value::ToObject() const {
  napi_value result;
  napi_status status = napi_coerce_to_object(_env, _value, &result);
  if (status != napi_ok) throw Error::New(_env);
  return Object(_env, result);
}

////////////////////////////////////////////////////////////////////////////////
// Boolean class
////////////////////////////////////////////////////////////////////////////////

inline Boolean Boolean::New(napi_env env, bool val) {
  napi_value value;
  napi_status status = napi_get_boolean(env, val, &value);
  if (status != napi_ok) throw Error::New(env);
  return Boolean(env, value);
}

inline Boolean::Boolean() : Napi::Value() {
}

inline Boolean::Boolean(napi_env env, napi_value value) : Napi::Value(env, value) {
}

inline Boolean::operator bool() const {
  return Value();
}

inline bool Boolean::Value() const {
  bool result;
  napi_status status = napi_get_value_bool(_env, _value, &result);
  if (status != napi_ok) throw Error::New(_env);
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// Number class
////////////////////////////////////////////////////////////////////////////////

inline Number Number::New(napi_env env, double val) {
  napi_value value;
  napi_status status = napi_create_number(env, val, &value);
  if (status != napi_ok) throw Error::New(env);
  return Number(env, value);
}

inline Number::Number() : Value() {
}

inline Number::Number(napi_env env, napi_value value) : Value(env, value) {
}

inline Number::operator int32_t() const {
  return Int32Value();
}

inline Number::operator uint32_t() const {
  return Uint32Value();
}

inline Number::operator int64_t() const {
  return Int64Value();
}

inline Number::operator float() const {
  return FloatValue();
}

inline Number::operator double() const {
  return DoubleValue();
}

inline int32_t Number::Int32Value() const {
  int32_t result;
  napi_status status = napi_get_value_int32(_env, _value, &result);
  if (status != napi_ok) throw Error::New(_env);
  return result;
}

inline uint32_t Number::Uint32Value() const {
  uint32_t result;
  napi_status status = napi_get_value_uint32(_env, _value, &result);
  if (status != napi_ok) throw Error::New(_env);
  return result;
}

inline int64_t Number::Int64Value() const {
  int64_t result;
  napi_status status = napi_get_value_int64(_env, _value, &result);
  if (status != napi_ok) throw Error::New(_env);
  return result;
}

inline float Number::FloatValue() const {
  return static_cast<float>(DoubleValue());
}

inline double Number::DoubleValue() const {
  double result;
  napi_status status = napi_get_value_double(_env, _value, &result);
  if (status != napi_ok) throw Error::New(_env);
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// Name class
////////////////////////////////////////////////////////////////////////////////

inline Name::Name() : Value() {
}

inline Name::Name(napi_env env, napi_value value) : Value(env, value) {
}

////////////////////////////////////////////////////////////////////////////////
// String class
////////////////////////////////////////////////////////////////////////////////

inline String String::New(napi_env env, const std::string& val) {
  return String::New(env, val.c_str(), val.size());
}

inline String String::New(napi_env env, const std::u16string& val) {
  return String::New(env, val.c_str(), val.size());
}

inline String String::New(napi_env env, const char* val) {
  napi_value value;
  napi_status status = napi_create_string_utf8(env, val, std::strlen(val), &value);
  if (status != napi_ok) throw Error::New(env);
  return String(env, value);
}

inline String String::New(napi_env env, const char16_t* val) {
  napi_value value;
  napi_status status = napi_create_string_utf16(env, val, std::u16string(val).size(), &value);
  if (status != napi_ok) throw Error::New(env);
  return String(env, value);
}

inline String String::New(napi_env env, const char* val, size_t length) {
  napi_value value;
  napi_status status = napi_create_string_utf8(env, val, length, &value);
  if (status != napi_ok) throw Error::New(env);
  return String(env, value);
}

inline String String::New(napi_env env, const char16_t* val, size_t length) {
  napi_value value;
  napi_status status = napi_create_string_utf16(env, val, length, &value);
  if (status != napi_ok) throw Error::New(env);
  return String(env, value);
}

inline String::String() : Name() {
}

inline String::String(napi_env env, napi_value value) : Name(env, value) {
}

inline String::operator std::string() const {
  return Utf8Value();
}

inline String::operator std::u16string() const {
  return Utf16Value();
}

inline std::string String::Utf8Value() const {
  size_t length;
  napi_status status = napi_get_value_string_utf8(_env, _value, nullptr, 0, &length);
  if (status != napi_ok) throw Error::New(_env);

  std::string value;
  value.reserve(length + 1);
  value.resize(length);
  status = napi_get_value_string_utf8(_env, _value, &value[0], value.capacity(), nullptr);
  if (status != napi_ok) throw Error::New(_env);
  return value;
}

inline std::u16string String::Utf16Value() const {
  size_t length;
  napi_status status = napi_get_value_string_utf16(_env, _value, nullptr, 0, &length);
  if (status != napi_ok) throw Error::New(_env);

  std::u16string value;
  value.reserve(length + 1);
  value.resize(length);
  status = napi_get_value_string_utf16(_env, _value, &value[0], value.capacity(), nullptr);
  if (status != napi_ok) throw Error::New(_env);
  return value;
}

////////////////////////////////////////////////////////////////////////////////
// Symbol class
////////////////////////////////////////////////////////////////////////////////

inline Symbol Symbol::New(napi_env env, const char* description) {
  napi_value descriptionValue = description != nullptr ?
    String::New(env, description) : static_cast<napi_value>(nullptr);
  return Symbol::New(env, descriptionValue);
}

inline Symbol Symbol::New(napi_env env, const std::string& description) {
  napi_value descriptionValue = String::New(env, description);
  return Symbol::New(env, descriptionValue);
}

inline Symbol Symbol::New(napi_env env, String description) {
  napi_value descriptionValue = description;
  return Symbol::New(env, descriptionValue);
}

inline Symbol Symbol::New(napi_env env, napi_value description) {
  napi_value value;
  napi_status status = napi_create_symbol(env, description, &value);
  if (status != napi_ok) throw Error::New(env);
  return Symbol(env, value);
}

inline Symbol::Symbol() : Name() {
}

inline Symbol::Symbol(napi_env env, napi_value value) : Name(env, value) {
}

////////////////////////////////////////////////////////////////////////////////
// Object class
////////////////////////////////////////////////////////////////////////////////

template <typename Key>
inline PropertyLValue<Key>::operator Value() const {
  return _object.Get(_key);
}

template <typename Key> template <typename ValueType>
inline PropertyLValue<Key>& PropertyLValue<Key>::operator =(ValueType value) {
  _object.Set(_key, value);
  return *this;
}

template <typename Key>
inline PropertyLValue<Key>::PropertyLValue(Object object, Key key)
  : _object(object), _key(key) {}

inline Object Object::New(napi_env env) {
  napi_value value;
  napi_status status = napi_create_object(env, &value);
  if (status != napi_ok) throw Error::New(env);
  return Object(env, value);
}

inline Object::Object() : Value() {
}

inline Object::Object(napi_env env, napi_value value) : Value(env, value) {
}

inline PropertyLValue<std::string> Object::operator [](const char* name) {
  return PropertyLValue<std::string>(*this, name);
}

inline PropertyLValue<std::string> Object::operator [](const std::string& name) {
  return PropertyLValue<std::string>(*this, name);
}

inline PropertyLValue<uint32_t> Object::operator [](uint32_t index) {
  return PropertyLValue<uint32_t>(*this, index);
}

inline Value Object::operator [](const char* name) const {
  return Get(name);
}

inline Value Object::operator [](const std::string& name) const {
  return Get(name);
}

inline Value Object::operator [](uint32_t index) const {
  return Get(index);
}

inline bool Object::Has(napi_value name) const {
  bool result;
  napi_status status = napi_has_property(_env, _value, name, &result);
  if (status != napi_ok) throw Error::New(_env);
  return result;
}

inline bool Object::Has(Value name) const {
  bool result;
  napi_status status = napi_has_property(_env, _value, name, &result);
  if (status != napi_ok) throw Error::New(_env);
  return result;
}

inline bool Object::Has(const char* utf8name) const {
  bool result;
  napi_status status = napi_has_named_property(_env, _value, utf8name, &result);
  if (status != napi_ok) throw Error::New(Env());
  return result;
}

inline bool Object::Has(const std::string& utf8name) const {
  return Has(utf8name.c_str());
}

inline Value Object::Get(napi_value name) const {
  napi_value result;
  napi_status status = napi_get_property(_env, _value, name, &result);
  if (status != napi_ok) throw Error::New(_env);
  return Value(_env, result);
}

inline Value Object::Get(Value name) const {
  napi_value result;
  napi_status status = napi_get_property(_env, _value, name, &result);
  if (status != napi_ok) throw Error::New(_env);
  return Value(_env, result);
}

inline Value Object::Get(const char* utf8name) const {
  napi_value result;
  napi_status status = napi_get_named_property(_env, _value, utf8name, &result);
  if (status != napi_ok) throw Error::New(Env());
  return Value(_env, result);
}

inline Value Object::Get(const std::string& utf8name) const {
  return Get(utf8name.c_str());
}

inline void Object::Set(napi_value name, napi_value value) {
  napi_status status = napi_set_property(_env, _value, name, value);
  if (status != napi_ok) throw Error::New(_env);
}

inline void Object::Set(const char* utf8name, Value value) {
  napi_status status = napi_set_named_property(_env, _value, utf8name, value);
  if (status != napi_ok) throw Error::New(Env());
}

inline void Object::Set(const char* utf8name, napi_value value) {
  napi_status status = napi_set_named_property(_env, _value, utf8name, value);
  if (status != napi_ok) throw Error::New(Env());
}

inline void Object::Set(const char* utf8name, const char* utf8value) {
  Set(utf8name, String::New(Env(), utf8value));
}

inline void Object::Set(const char* utf8name, bool boolValue) {
  Set(utf8name, Boolean::New(Env(), boolValue));
}

inline void Object::Set(const char* utf8name, double numberValue) {
  Set(utf8name, Number::New(Env(), numberValue));
}

inline void Object::Set(const std::string& utf8name, napi_value value) {
  Set(utf8name.c_str(), value);
}

inline void Object::Set(const std::string& utf8name, Value value) {
  Set(utf8name.c_str(), value);
}

inline void Object::Set(const std::string& utf8name, std::string& utf8value) {
  Set(utf8name.c_str(), String::New(Env(), utf8value));
}

inline void Object::Set(const std::string& utf8name, bool boolValue) {
  Set(utf8name.c_str(), Boolean::New(Env(), boolValue));
}

inline void Object::Set(const std::string& utf8name, double numberValue) {
  Set(utf8name.c_str(), Number::New(Env(), numberValue));
}

inline bool Object::Has(uint32_t index) const {
  bool result;
  napi_status status = napi_has_element(_env, _value, index, &result);
  if (status != napi_ok) throw Error::New(_env);
  return result;
}

inline Value Object::Get(uint32_t index) const {
  napi_value value;
  napi_status status = napi_get_element(_env, _value, index, &value);
  if (status != napi_ok) throw Error::New(_env);
  return Value(_env, value);
}

inline void Object::Set(uint32_t index, napi_value value) {
  napi_status status = napi_set_element(_env, _value, index, value);
  if (status != napi_ok) throw Error::New(_env);
}

inline void Object::Set(uint32_t index, Value value) {
  napi_status status = napi_set_element(_env, _value, index, value);
  if (status != napi_ok) throw Error::New(_env);
}

inline void Object::Set(uint32_t index, const char* utf8value) {
  Set(index, static_cast<napi_value>(String::New(Env(), utf8value)));
}

inline void Object::Set(uint32_t index, const std::string& utf8value) {
  Set(index, static_cast<napi_value>(String::New(Env(), utf8value)));
}

inline void Object::Set(uint32_t index, bool boolValue) {
  Set(index, static_cast<napi_value>(Boolean::New(Env(), boolValue)));
}

inline void Object::Set(uint32_t index, double numberValue) {
  Set(index, static_cast<napi_value>(Number::New(Env(), numberValue)));
}

inline void Object::DefineProperty(const PropertyDescriptor& property) {
  napi_status status = napi_define_properties(_env, _value, 1,
    reinterpret_cast<const napi_property_descriptor*>(&property));
  if (status != napi_ok) throw Error::New(_env);
}

inline void Object::DefineProperties(const std::initializer_list<PropertyDescriptor>& properties) {
  napi_status status = napi_define_properties(_env, _value, properties.size(),
    reinterpret_cast<const napi_property_descriptor*>(properties.begin()));
  if (status != napi_ok) throw Error::New(_env);
}

inline void Object::DefineProperties(const std::vector<PropertyDescriptor>& properties) {
  napi_status status = napi_define_properties(_env, _value, properties.size(),
    reinterpret_cast<const napi_property_descriptor*>(properties.data()));
  if (status != napi_ok) throw Error::New(_env);
}

inline bool Object::InstanceOf(const Function& constructor) const {
  bool result;
  napi_status status = napi_instanceof(_env, _value, constructor, &result);
  if (status != napi_ok) throw Error::New(_env);
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// External class
////////////////////////////////////////////////////////////////////////////////

template <typename T>
inline External<T> External<T>::New(napi_env env, T* data) {
  napi_value value;
  napi_status status = napi_create_external(env, data, nullptr, nullptr, &value);
  if (status != napi_ok) throw Error::New(env);
  return External(env, value);
}

template <typename T>
template <typename Finalizer>
inline External<T> External<T>::New(napi_env env,
                                    T* data,
                                    Finalizer finalizeCallback) {
  napi_value value;
  details::FinalizeData<T, Finalizer>* finalizeData =
    new details::FinalizeData<T, Finalizer>({ finalizeCallback, nullptr });
  napi_status status = napi_create_external(
    env,
    data,
    details::FinalizeData<T, Finalizer>::Wrapper,
    finalizeData,
    &value);
  if (status != napi_ok) {
    delete finalizeData;
    throw Error::New(env);
  }
  return External(env, value);
}

template <typename T>
template <typename Finalizer, typename Hint>
inline External<T> External<T>::New(napi_env env,
                                    T* data,
                                    Finalizer finalizeCallback,
                                    Hint* finalizeHint) {
  napi_value value;
  details::FinalizeData<T, Finalizer, Hint>* finalizeData =
    new details::FinalizeData<T, Finalizer, Hint>({ finalizeCallback, finalizeHint });
  napi_status status = napi_create_external(
    env,
    data,
    details::FinalizeData<T, Finalizer, Hint>::WrapperWithHint,
    finalizeData,
    &value);
  if (status != napi_ok) {
    delete finalizeData;
    throw Error::New(env);
  }
  return External(env, value);
}

template <typename T>
inline External<T>::External() : Value() {
}

template <typename T>
inline External<T>::External(napi_env env, napi_value value) : Value(env, value) {
}

template <typename T>
inline T* External<T>::Data() const {
  void* data;
  napi_status status = napi_get_value_external(_env, _value, &data);
  if (status != napi_ok) throw Error::New(_env);
  return reinterpret_cast<T*>(data);
}

////////////////////////////////////////////////////////////////////////////////
// Array class
////////////////////////////////////////////////////////////////////////////////

inline Array Array::New(napi_env env) {
  napi_value value;
  napi_status status = napi_create_array(env, &value);
  if (status != napi_ok) throw Error::New(env);
  return Array(env, value);
}

inline Array Array::New(napi_env env, size_t length) {
  napi_value value;
  napi_status status = napi_create_array_with_length(env, length, &value);
  if (status != napi_ok) throw Error::New(env);
  return Array(env, value);
}

inline Array::Array() : Object() {
}

inline Array::Array(napi_env env, napi_value value) : Object(env, value) {
}

inline uint32_t Array::Length() const {
  uint32_t result;
  napi_status status = napi_get_array_length(_env, _value, &result);
  if (status != napi_ok) throw Error::New(_env);
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// ArrayBuffer class
////////////////////////////////////////////////////////////////////////////////

inline ArrayBuffer ArrayBuffer::New(napi_env env, size_t byteLength) {
  napi_value value;
  void* data;
  napi_status status = napi_create_arraybuffer(env, byteLength, &data, &value);
  if (status != napi_ok) throw Error::New(env);

  return ArrayBuffer(env, value, data, byteLength);
}

inline ArrayBuffer ArrayBuffer::New(napi_env env,
                                    void* externalData,
                                    size_t byteLength) {
  napi_value value;
  napi_status status = napi_create_external_arraybuffer(
    env, externalData, byteLength, nullptr, nullptr, &value);
  if (status != napi_ok) throw Error::New(env);

  return ArrayBuffer(env, value, externalData, byteLength);
}

template <typename Finalizer>
inline ArrayBuffer ArrayBuffer::New(napi_env env,
                                    void* externalData,
                                    size_t byteLength,
                                    Finalizer finalizeCallback) {
  napi_value value;
  details::FinalizeData<void, Finalizer>* finalizeData =
    new details::FinalizeData<void, Finalizer>({ finalizeCallback, nullptr });
  napi_status status = napi_create_external_arraybuffer(
    env,
    externalData,
    byteLength,
    details::FinalizeData<void, Finalizer>::Wrapper,
    finalizeData,
    &value);
  if (status != napi_ok) {
    delete finalizeData;
    throw Error::New(env);
  }

  return ArrayBuffer(env, value, externalData, byteLength);
}

template <typename Finalizer, typename Hint>
inline ArrayBuffer ArrayBuffer::New(napi_env env,
                                    void* externalData,
                                    size_t byteLength,
                                    Finalizer finalizeCallback,
                                    Hint* finalizeHint) {
  napi_value value;
  details::FinalizeData<void, Finalizer, Hint>* finalizeData =
    new details::FinalizeData<void, Finalizer, Hint>({ finalizeCallback, finalizeHint });
  napi_status status = napi_create_external_arraybuffer(
    env,
    externalData,
    byteLength,
    details::FinalizeData<void, Finalizer, Hint>::WrapperWithHint,
    finalizeData,
    &value);
  if (status != napi_ok) {
    delete finalizeData;
    throw Error::New(env);
  }

  return ArrayBuffer(env, value, externalData, byteLength);
}

inline ArrayBuffer::ArrayBuffer() : Object(), _data(nullptr), _length(0) {
}

inline ArrayBuffer::ArrayBuffer(napi_env env, napi_value value)
  : Object(env, value), _data(nullptr), _length(0) {
}

inline ArrayBuffer::ArrayBuffer(napi_env env, napi_value value, void* data, size_t length)
  : Object(env, value), _data(data), _length(length) {
}

inline void* ArrayBuffer::Data() {
  EnsureInfo();
  return _data;
}

inline size_t ArrayBuffer::ByteLength() {
  EnsureInfo();
  return _length;
}

inline void ArrayBuffer::EnsureInfo() const {
  // The ArrayBuffer instance may have been constructed from a napi_value whose
  // length/data are not yet known. Fetch and cache these values just once,
  // since they can never change during the lifetime of the ArrayBuffer.
  if (_data == nullptr) {
    napi_status status = napi_get_arraybuffer_info(_env, _value, &_data, &_length);
    if (status != napi_ok) throw Error::New(_env);
  }
}

////////////////////////////////////////////////////////////////////////////////
// TypedArray class
////////////////////////////////////////////////////////////////////////////////

inline TypedArray::TypedArray()
  : Object(), _type(TypedArray::unknown_array_type), _length(0) {
}

inline TypedArray::TypedArray(napi_env env, napi_value value)
  : Object(env, value), _type(TypedArray::unknown_array_type), _length(0) {
}

inline TypedArray::TypedArray(napi_env env,
                              napi_value value,
                              napi_typedarray_type type,
                              size_t length)
  : Object(env, value), _type(type), _length(length) {
}

inline napi_typedarray_type TypedArray::TypedArrayType() const {
  if (_type == TypedArray::unknown_array_type) {
    napi_status status = napi_get_typedarray_info(_env, _value,
      &const_cast<TypedArray*>(this)->_type, &const_cast<TypedArray*>(this)->_length,
      nullptr, nullptr, nullptr);
    if (status != napi_ok) throw Error::New(_env);
  }

  return _type;
}

inline uint8_t TypedArray::ElementSize() const {
  switch (TypedArrayType()) {
    case napi_int8_array:
    case napi_uint8_array:
    case napi_uint8_clamped_array:
      return 1;
    case napi_int16_array:
    case napi_uint16_array:
      return 2;
    case napi_int32_array:
    case napi_uint32_array:
    case napi_float32_array:
      return 4;
    case napi_float64_array:
      return 8;
    default:
      return 0;
  }
}

inline size_t TypedArray::ElementLength() const {
  if (_type == TypedArray::unknown_array_type) {
    napi_status status = napi_get_typedarray_info(_env, _value,
      &const_cast<TypedArray*>(this)->_type, &const_cast<TypedArray*>(this)->_length,
      nullptr, nullptr, nullptr);
    if (status != napi_ok) throw Error::New(_env);
  }

  return _length;
}

inline size_t TypedArray::ByteOffset() const {
  size_t byteOffset;
  napi_status status = napi_get_typedarray_info(
    _env, _value, nullptr, nullptr, nullptr, nullptr, &byteOffset);
  if (status != napi_ok) throw Error::New(_env);
  return byteOffset;
}

inline size_t TypedArray::ByteLength() const {
  return ElementSize() * ElementLength();
}

inline Napi::ArrayBuffer TypedArray::ArrayBuffer() const {
  napi_value arrayBuffer;
  napi_status status = napi_get_typedarray_info(
    _env, _value, nullptr, nullptr, nullptr, &arrayBuffer, nullptr);
  if (status != napi_ok) throw Error::New(_env);
  return Napi::ArrayBuffer(_env, arrayBuffer);
}

////////////////////////////////////////////////////////////////////////////////
// TypedArrayOf<T> class
////////////////////////////////////////////////////////////////////////////////

template <typename T>
inline TypedArrayOf<T> TypedArrayOf<T>::New(napi_env env,
                                            size_t elementLength,
                                            napi_typedarray_type type) {
  Napi::ArrayBuffer arrayBuffer = Napi::ArrayBuffer::New(env, elementLength * sizeof (T));
  return New(env, elementLength, arrayBuffer, 0, type);
}

template <typename T>
inline TypedArrayOf<T> TypedArrayOf<T>::New(napi_env env,
                                            size_t elementLength,
                                            Napi::ArrayBuffer arrayBuffer,
                                            size_t bufferOffset,
                                            napi_typedarray_type type) {
  napi_value value;
  napi_status status = napi_create_typedarray(
    env, type, elementLength, arrayBuffer, bufferOffset, &value);
  if (status != napi_ok) throw Error::New(env);

  return TypedArrayOf<T>(
    env, value, type, elementLength,
    reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(arrayBuffer.Data()) + bufferOffset));
}

template <typename T>
inline TypedArrayOf<T>::TypedArrayOf() : TypedArray(), _data(nullptr) {
}

template <typename T>
inline TypedArrayOf<T>::TypedArrayOf(napi_env env, napi_value value)
  : TypedArray(env, value), _data(nullptr) {
  napi_status status = napi_get_typedarray_info(
    _env, _value, &_type, &_length, reinterpret_cast<void**>(&_data), nullptr, nullptr);
  if (status != napi_ok) throw Error::New(_env);
}

template <typename T>
inline TypedArrayOf<T>::TypedArrayOf(napi_env env,
                                     napi_value value,
                                     napi_typedarray_type type,
                                     size_t length,
                                     T* data)
  : TypedArray(env, value, type, length), _data(data) {
  if (!(type == TypedArrayTypeForPrimitiveType<T>() ||
      (type == napi_uint8_clamped_array && std::is_same<T, uint8_t>::value))) {
    throw TypeError::New(env, "Array type must match the template parameter. "
      "(Uint8 arrays may optionally have the \"clamped\" array type.)");
  }
}

template <typename T>
inline T& TypedArrayOf<T>::operator [](size_t index) {
  return _data[index];
}

template <typename T>
inline const T& TypedArrayOf<T>::operator [](size_t index) const {
  return _data[index];
}

template <typename T>
inline T* TypedArrayOf<T>::Data() {
  return _data;
}

template <typename T>
inline const T* TypedArrayOf<T>::Data() const {
  return _data;
}

////////////////////////////////////////////////////////////////////////////////
// Function class
////////////////////////////////////////////////////////////////////////////////

template <typename Callable>
inline Function Function::New(napi_env env,
                              Callable cb,
                              const char* utf8name,
                              void* data) {
  typedef decltype(cb(CallbackInfo(nullptr, nullptr))) ReturnType;
  typedef details::CallbackData<Callable, ReturnType> CbData;
  // TODO: Delete when the function is destroyed
  auto callbackData = new CbData({ cb, data });

  napi_value value;
  napi_status status = napi_create_function(
    env, utf8name, CbData::Wrapper, callbackData, &value);
  if (status != napi_ok) throw Error::New(env);
  return Function(env, value);
}

template <typename Callable>
inline Function Function::New(napi_env env,
                              Callable cb,
                              const std::string& utf8name,
                              void* data) {
  return New(env, cb, utf8name.c_str(), data);
}

inline Function::Function() : Object() {
}

inline Function::Function(napi_env env, napi_value value) : Object(env, value) {
}

inline Value Function::operator ()(const std::initializer_list<napi_value>& args) const {
  return Call(Env().Undefined(), args);
}

inline Value Function::Call(const std::initializer_list<napi_value>& args) const {
  return Call(Env().Undefined(), args);
}

inline Value Function::Call(const std::vector<napi_value>& args) const {
  return Call(Env().Undefined(), args);
}

inline Value Function::Call(napi_value recv, const std::initializer_list<napi_value>& args) const {
  napi_value result;
  napi_status status = napi_call_function(
    _env, recv, _value, args.size(), args.begin(), &result);
  if (status != napi_ok) throw Error::New(_env);
  return Value(_env, result);
}

inline Value Function::Call(napi_value recv, const std::vector<napi_value>& args) const {
  napi_value result;
  napi_status status = napi_call_function(
    _env, recv, _value, args.size(), args.data(), &result);
  if (status != napi_ok) throw Error::New(_env);
  return Value(_env, result);
}

inline Value Function::MakeCallback(
    napi_value recv, const std::initializer_list<napi_value>& args) const {
  napi_value result;
  napi_status status = napi_make_callback(
    _env, recv, _value, args.size(), args.begin(), &result);
  if (status != napi_ok) throw Error::New(_env);
  return Value(_env, result);
}

inline Value Function::MakeCallback(
    napi_value recv, const std::vector<napi_value>& args) const {
  napi_value result;
  napi_status status = napi_make_callback(
    _env, recv, _value, args.size(), args.data(), &result);
  if (status != napi_ok) throw Error::New(_env);
  return Value(_env, result);
}

inline Object Function::New(const std::initializer_list<napi_value>& args) const {
  napi_value result;
  napi_status status = napi_new_instance(
    _env, _value, args.size(), args.begin(), &result);
  if (status != napi_ok) throw Error::New(_env);
  return Object(_env, result);
}

inline Object Function::New(const std::vector<napi_value>& args) const {
  napi_value result;
  napi_status status = napi_new_instance(
    _env, _value, args.size(), args.data(), &result);
  if (status != napi_ok) throw Error::New(_env);
  return Object(_env, result);
}

////////////////////////////////////////////////////////////////////////////////
// Buffer<T> class
////////////////////////////////////////////////////////////////////////////////

template <typename T>
inline Buffer<T> Buffer<T>::New(napi_env env, size_t length) {
  napi_value value;
  void* data;
  napi_status status = napi_create_buffer(env, length * sizeof (T), &data, &value);
  if (status != napi_ok) throw Error::New(env);
  return Buffer(env, value, length, static_cast<T*>(data));
}

template <typename T>
inline Buffer<T> Buffer<T>::New(napi_env env, T* data, size_t length) {
  napi_value value;
  napi_status status = napi_create_external_buffer(
    env, length * sizeof (T), data, nullptr, nullptr, &value);
  if (status != napi_ok) throw Error::New(env);
  return Buffer(env, value, length, data);
}

template <typename T>
template <typename Finalizer>
inline Buffer<T> Buffer<T>::New(napi_env env,
                                T* data,
                                size_t length,
                                Finalizer finalizeCallback) {
  napi_value value;
  details::FinalizeData<T, Finalizer>* finalizeData =
    new details::FinalizeData<T, Finalizer>({ finalizeCallback, nullptr });
  napi_status status = napi_create_external_buffer(
    env,
    length * sizeof (T),
    data,
    details::FinalizeData<T, Finalizer>::Wrapper,
    finalizeData,
    &value);
  if (status != napi_ok) {
    delete finalizeData;
    throw Error::New(env);
  }
  return Buffer(env, value, length, data);
}

template <typename T>
template <typename Finalizer, typename Hint>
inline Buffer<T> Buffer<T>::New(napi_env env,
                                T* data,
                                size_t length,
                                Finalizer finalizeCallback,
                                Hint* finalizeHint) {
  napi_value value;
  details::FinalizeData<T, Finalizer, Hint>* finalizeData =
    new details::FinalizeData<T, Finalizer, Hint>({ finalizeCallback, finalizeHint });
  napi_status status = napi_create_external_buffer(
    env,
    length * sizeof (T),
    data,
    details::FinalizeData<T, Finalizer, Hint>::WrapperWithHint,
    finalizeData,
    &value);
  if (status != napi_ok) {
    delete finalizeData;
    throw Error::New(env);
  }
  return Buffer(env, value, length, data);
}

template <typename T>
inline Buffer<T> Buffer<T>::Copy(napi_env env, const T* data, size_t length) {
  napi_value value;
  napi_status status = napi_create_buffer_copy(
    env, length * sizeof (T), data, nullptr, &value);
  if (status != napi_ok) throw Error::New(env);
  return Buffer<T>(env, value);
}

template <typename T>
inline Buffer<T>::Buffer() : Object(), _length(0), _data(nullptr) {
}

template <typename T>
inline Buffer<T>::Buffer(napi_env env, napi_value value)
  : Object(env, value), _length(0), _data(nullptr) {
}

template <typename T>
inline Buffer<T>::Buffer(napi_env env, napi_value value, size_t length, T* data)
  : Object(env, value), _length(length), _data(data) {
}

template <typename T>
inline size_t Buffer<T>::Length() const {
  EnsureInfo();
  return _length;
}

template <typename T>
inline T* Buffer<T>::Data() const {
  EnsureInfo();
  return _data;
}

template <typename T>
inline void Buffer<T>::EnsureInfo() const {
  // The Buffer instance may have been constructed from a napi_value whose
  // length/data are not yet known. Fetch and cache these values just once,
  // since they can never change during the lifetime of the Buffer.
  if (_data == nullptr) {
    size_t byteLength;
    void* voidData;
    napi_status status = napi_get_buffer_info(_env, _value, &voidData, &byteLength);
    if (status != napi_ok) throw Error::New(_env);
    _length = byteLength / sizeof (T);
    _data = static_cast<T*>(voidData);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Error class
////////////////////////////////////////////////////////////////////////////////

inline Error Error::New(napi_env env) {
  napi_status status;
  napi_value error = nullptr;

  const napi_extended_error_info* info;
  status = napi_get_last_error_info(env, &info);
  assert(status == napi_ok);

  if (status == napi_ok) {
    if (info->error_code == napi_pending_exception) {
      status = napi_get_and_clear_last_exception(env, &error);
      assert(status == napi_ok);
    }
    else {
      const char* error_message = info->error_message != nullptr ?
        info->error_message : "Error in native callback";
      napi_value message;
      status = napi_create_string_utf8(
        env,
        error_message,
        std::strlen(error_message),
        &message);
      assert(status == napi_ok);

      if (status == napi_ok) {
        switch (info->error_code) {
        case napi_object_expected:
        case napi_string_expected:
        case napi_boolean_expected:
        case napi_number_expected:
          status = napi_create_type_error(env, message, &error);
          break;
        default:
          status = napi_create_error(env, message, &error);
          break;
        }
        assert(status == napi_ok);
      }
    }
  }

  return Error(env, error);
}

inline Error Error::New(napi_env env, const char* message) {
  return Error::New<Error>(env, message, std::strlen(message), napi_create_error);
}

inline Error Error::New(napi_env env, const std::string& message) {
  return Error::New<Error>(env, message.c_str(), message.size(), napi_create_error);
}

inline Error::Error() : ObjectReference(), _message(nullptr) {
}

inline Error::Error(napi_env env, napi_value value) : ObjectReference(env, nullptr) {
  if (value != nullptr) {
    napi_status status = napi_create_reference(env, value, 1, &_ref);

    // Avoid infinite recursion in the failure case.
    // Don't try to construct & throw another Error instance.
    assert(status == napi_ok);
  }
}

inline Error::Error(Error&& other) : ObjectReference(std::move(other)) {
}

inline Error& Error::operator =(Error&& other) {
  static_cast<Reference<Object>*>(this)->operator=(std::move(other));
  return *this;
}

inline Error::Error(const Error& other) : Error(other.Env(), other.Value()) {
}

inline Error& Error::operator =(Error& other) {
  Reset();

  _env = other.Env();
  HandleScope scope(_env);

  napi_value value = other.Value();
  if (value != nullptr) {
    napi_status status = napi_create_reference(_env, value, 1, &_ref);
    if (status != napi_ok) throw Error::New(_env);
  }

  return *this;
}

inline const std::string& Error::Message() const NAPI_NOEXCEPT {
  if (_message.size() == 0 && _env != nullptr) {
    try {
      _message = Get("message").As<String>();
    }
    catch (...) {
      // Catch all errors here, to include e.g. a std::bad_alloc from
      // the std::string::operator=, because this is used by the
      // Error::what() noexcept method.
    }
  }
  return _message;
}

inline void Error::ThrowAsJavaScriptException() const {
  HandleScope scope(_env);
  if (!IsEmpty()) {
    napi_status status = napi_throw(_env, Value());
    if (status != napi_ok) throw Error::New(_env);
  }
}

inline const char* Error::what() const NAPI_NOEXCEPT {
  return Message().c_str();
}

template <typename TError>
inline TError Error::New(napi_env env,
                         const char* message,
                         size_t length,
                         create_error_fn create_error) {
  napi_value str;
  napi_status status = napi_create_string_utf8(env, message, length, &str);
  if (status != napi_ok) throw Error::New(env);

  napi_value error;
  status = create_error(env, str, &error);
  if (status != napi_ok) throw Error::New(env);

  return TError(env, error);
}

inline TypeError TypeError::New(napi_env env, const char* message) {
  return Error::New<TypeError>(env, message, std::strlen(message), napi_create_type_error);
}

inline TypeError TypeError::New(napi_env env, const std::string& message) {
  return Error::New<TypeError>(env, message.c_str(), message.size(), napi_create_type_error);
}

inline TypeError::TypeError() : Error() {
}

inline TypeError::TypeError(napi_env env, napi_value value) : Error(env, value) {
}

inline RangeError RangeError::New(napi_env env, const char* message) {
  return Error::New<RangeError>(env, message, std::strlen(message), napi_create_range_error);
}

inline RangeError RangeError::New(napi_env env, const std::string& message) {
  return Error::New<RangeError>(env, message.c_str(), message.size(), napi_create_range_error);
}

inline RangeError::RangeError() : Error() {
}

inline RangeError::RangeError(napi_env env, napi_value value) : Error(env, value) {
}

////////////////////////////////////////////////////////////////////////////////
// Reference<T> class
////////////////////////////////////////////////////////////////////////////////

template <typename T>
inline Reference<T> Reference<T>::New(const T& value, uint32_t initialRefcount) {
  napi_env env = value.Env();
  napi_value val = value;

  if (val == nullptr) {
    return Reference<T>(env, nullptr);
  }

  napi_ref ref;
  napi_status status = napi_create_reference(env, value, initialRefcount, &ref);
  if (status != napi_ok) throw Error::New(napi_env(env));

  return Reference<T>(env, ref);
}


template <typename T>
inline Reference<T>::Reference() : _env(nullptr), _ref(nullptr), _suppressDestruct(false) {
}

template <typename T>
inline Reference<T>::Reference(napi_env env, napi_ref ref)
  : _env(env), _ref(ref) {
}

template <typename T>
inline Reference<T>::~Reference() {
  if (_ref != nullptr) {
    if (!_suppressDestruct) {
      napi_delete_reference(_env, _ref);
    }

    _ref = nullptr;
  }
}

template <typename T>
inline Reference<T>::Reference(Reference<T>&& other) {
  _env = other._env;
  _ref = other._ref;
  other._env = nullptr;
  other._ref = nullptr;
}

template <typename T>
inline Reference<T>& Reference<T>::operator =(Reference<T>&& other) {
  Reset();
  _env = other._env;
  _ref = other._ref;
  other._env = nullptr;
  other._ref = nullptr;
  return *this;
}

template <typename T>
inline Reference<T>::operator napi_ref() const {
  return _ref;
}

template <typename T>
inline bool Reference<T>::operator ==(const Reference<T> &other) const {
  HandleScope scope(_env);
  return this->Value().StrictEquals(other.Value());
}

template <typename T>
inline bool Reference<T>::operator !=(const Reference<T> &other) const {
  return !this->operator ==(other);
}

template <typename T>
inline Napi::Env Reference<T>::Env() const {
  return Napi::Env(_env);
}

template <typename T>
inline bool Reference<T>::IsEmpty() const {
  return _ref == nullptr;
}

template <typename T>
inline T Reference<T>::Value() const {
  if (_ref == nullptr) {
    return T(_env, nullptr);
  }

  napi_value value;
  napi_status status = napi_get_reference_value(_env, _ref, &value);
  if (status != napi_ok) throw Error::New(_env);
  return T(_env, value);
}

template <typename T>
inline uint32_t Reference<T>::Ref() {
  uint32_t result;
  napi_status status = napi_reference_ref(_env, _ref, &result);
  if (status != napi_ok) throw Error::New(_env);
  return result;
}

template <typename T>
inline uint32_t Reference<T>::Unref() {
  uint32_t result;
  napi_status status = napi_reference_unref(_env, _ref, &result);
  if (status != napi_ok) throw Error::New(_env);
  return result;
}

template <typename T>
inline void Reference<T>::Reset() {
  if (_ref != nullptr) {
    napi_status status = napi_delete_reference(_env, _ref);
    if (status != napi_ok) throw Error::New(_env);
    _ref = nullptr;
  }
}

template <typename T>
inline void Reference<T>::Reset(const T& value, uint32_t refcount) {
  Reset();
  _env = value.Env();

  napi_value val = value;
  if (val != nullptr) {
    napi_status status = napi_create_reference(_env, value, refcount, &_ref);
    if (status != napi_ok) throw Error::New(_env);
  }
}

template <typename T>
inline void Reference<T>::SuppressDestruct() {
  _suppressDestruct = true;
}

template <typename T>
inline Reference<T> Weak(T value) {
  return Reference<T>::New(value, 0);
}

inline ObjectReference Weak(Object value) {
  return Reference<Object>::New(value, 0);
}

inline FunctionReference Weak(Function value) {
  return Reference<Function>::New(value, 0);
}

template <typename T>
inline Reference<T> Persistent(T value) {
  return Reference<T>::New(value, 1);
}

inline ObjectReference Persistent(Object value) {
  return Reference<Object>::New(value, 1);
}

inline FunctionReference Persistent(Function value) {
  return Reference<Function>::New(value, 1);
}

////////////////////////////////////////////////////////////////////////////////
// ObjectReference class
////////////////////////////////////////////////////////////////////////////////

inline ObjectReference::ObjectReference(): Reference<Object>() {
}

inline ObjectReference::ObjectReference(napi_env env, napi_ref ref): Reference<Object>(env, ref) {
}

inline ObjectReference::ObjectReference(Reference<Object>&& other)
  : Reference<Object>(std::move(other)) {
}

inline ObjectReference& ObjectReference::operator =(Reference<Object>&& other) {
  static_cast<Reference<Object>*>(this)->operator=(std::move(other));
  return *this;
}

inline ObjectReference::ObjectReference(ObjectReference&& other)
  : Reference<Object>(std::move(other)) {
}

inline ObjectReference& ObjectReference::operator =(ObjectReference&& other) {
  static_cast<Reference<Object>*>(this)->operator=(std::move(other));
  return *this;
}

inline Napi::Value ObjectReference::Get(const char* utf8name) const {
  EscapableHandleScope scope(_env);
  return scope.Escape(Value().Get(utf8name));
}

inline Napi::Value ObjectReference::Get(const std::string& utf8name) const {
  EscapableHandleScope scope(_env);
  return scope.Escape(Value().Get(utf8name));
}

inline void ObjectReference::Set(const char* utf8name, napi_value value) {
  HandleScope scope(_env);
  Value().Set(utf8name, value);
}

inline void ObjectReference::Set(const char* utf8name, Napi::Value value) {
  HandleScope scope(_env);
  Value().Set(utf8name, value);
}

inline void ObjectReference::Set(const char* utf8name, const char* utf8value) {
  HandleScope scope(_env);
  Value().Set(utf8name, utf8value);
}

inline void ObjectReference::Set(const char* utf8name, bool boolValue) {
  HandleScope scope(_env);
  Value().Set(utf8name, boolValue);
}

inline void ObjectReference::Set(const char* utf8name, double numberValue) {
  HandleScope scope(_env);
  Value().Set(utf8name, numberValue);
}

inline void ObjectReference::Set(const std::string& utf8name, napi_value value) {
  HandleScope scope(_env);
  Value().Set(utf8name, value);
}

inline void ObjectReference::Set(const std::string& utf8name, Napi::Value value) {
  HandleScope scope(_env);
  Value().Set(utf8name, value);
}

inline void ObjectReference::Set(const std::string& utf8name, std::string& utf8value) {
  HandleScope scope(_env);
  Value().Set(utf8name, utf8value);
}

inline void ObjectReference::Set(const std::string& utf8name, bool boolValue) {
  HandleScope scope(_env);
  Value().Set(utf8name, boolValue);
}

inline void ObjectReference::Set(const std::string& utf8name, double numberValue) {
  HandleScope scope(_env);
  Value().Set(utf8name, numberValue);
}

inline Napi::Value ObjectReference::Get(uint32_t index) const {
  EscapableHandleScope scope(_env);
  return scope.Escape(Value().Get(index));
}

inline void ObjectReference::Set(uint32_t index, napi_value value) {
  HandleScope scope(_env);
  Value().Set(index, value);
}

inline void ObjectReference::Set(uint32_t index, Napi::Value value) {
  HandleScope scope(_env);
  Value().Set(index, value);
}

inline void ObjectReference::Set(uint32_t index, const char* utf8value) {
  HandleScope scope(_env);
  Value().Set(index, utf8value);
}

inline void ObjectReference::Set(uint32_t index, const std::string& utf8value) {
  HandleScope scope(_env);
  Value().Set(index, utf8value);
}

inline void ObjectReference::Set(uint32_t index, bool boolValue) {
  HandleScope scope(_env);
  Value().Set(index, boolValue);
}

inline void ObjectReference::Set(uint32_t index, double numberValue) {
  HandleScope scope(_env);
  Value().Set(index, numberValue);
}

////////////////////////////////////////////////////////////////////////////////
// FunctionReference class
////////////////////////////////////////////////////////////////////////////////

inline FunctionReference::FunctionReference(): Reference<Function>() {
}

inline FunctionReference::FunctionReference(napi_env env, napi_ref ref)
  : Reference<Function>(env, ref) {
}

inline FunctionReference::FunctionReference(Reference<Function>&& other)
  : Reference<Function>(std::move(other)) {
}

inline FunctionReference& FunctionReference::operator =(Reference<Function>&& other) {
  static_cast<Reference<Function>*>(this)->operator=(std::move(other));
  return *this;
}

inline FunctionReference::FunctionReference(FunctionReference&& other)
  : Reference<Function>(std::move(other)) {
}

inline FunctionReference& FunctionReference::operator =(FunctionReference&& other) {
  static_cast<Reference<Function>*>(this)->operator=(std::move(other));
  return *this;
}

inline Napi::Value FunctionReference::operator ()(
    const std::initializer_list<napi_value>& args) const {
  EscapableHandleScope scope(_env);
  return scope.Escape(Value()(args));
}

inline Napi::Value FunctionReference::Call(const std::initializer_list<napi_value>& args) const {
  EscapableHandleScope scope(_env);
  return scope.Escape(Value().Call(args));
}

inline Napi::Value FunctionReference::Call(const std::vector<napi_value>& args) const {
  EscapableHandleScope scope(_env);
  return scope.Escape(Value().Call(args));
}

inline Napi::Value FunctionReference::Call(
    napi_value recv, const std::initializer_list<napi_value>& args) const {
  EscapableHandleScope scope(_env);
  return scope.Escape(Value().Call(recv, args));
}

inline Napi::Value FunctionReference::Call(
    napi_value recv, const std::vector<napi_value>& args) const {
  EscapableHandleScope scope(_env);
  return scope.Escape(Value().Call(recv, args));
}

inline Napi::Value FunctionReference::MakeCallback(
    napi_value recv, const std::initializer_list<napi_value>& args) const {
  EscapableHandleScope scope(_env);
  return scope.Escape(Value().MakeCallback(recv, args));
}

inline Napi::Value FunctionReference::MakeCallback(
    napi_value recv, const std::vector<napi_value>& args) const {
  EscapableHandleScope scope(_env);
  return scope.Escape(Value().MakeCallback(recv, args));
}

inline Object FunctionReference::New(const std::initializer_list<napi_value>& args) const {
  EscapableHandleScope scope(_env);
  return scope.Escape(Value().New(args)).As<Object>();
}

inline Object FunctionReference::New(const std::vector<napi_value>& args) const {
  EscapableHandleScope scope(_env);
  return scope.Escape(Value().New(args)).As<Object>();
}

////////////////////////////////////////////////////////////////////////////////
// CallbackInfo class
////////////////////////////////////////////////////////////////////////////////

inline CallbackInfo::CallbackInfo(napi_env env, napi_callback_info info)
    : _env(env), _this(nullptr), _dynamicArgs(nullptr), _data(nullptr) {
  _argc = _staticArgCount;
  _argv = _staticArgs;
  napi_status status = napi_get_cb_info(env, info, &_argc, _argv, &_this, &_data);
  if (status != napi_ok) throw Error::New(_env);

  if (_argc > _staticArgCount) {
    // Use either a fixed-size array (on the stack) or a dynamically-allocated
    // array (on the heap) depending on the number of args.
    _dynamicArgs = new napi_value[_argc];
    _argv = _dynamicArgs;

    status = napi_get_cb_info(env, info, &_argc, _argv, nullptr, nullptr);
    if (status != napi_ok) throw Error::New(_env);
  }
}

inline CallbackInfo::~CallbackInfo() {
  if (_dynamicArgs != nullptr) {
    delete[] _dynamicArgs;
  }
}

inline Napi::Env CallbackInfo::Env() const {
  return Napi::Env(_env);
}

inline size_t CallbackInfo::Length() const {
  return _argc;
}

inline const Value CallbackInfo::operator [](size_t index) const {
  return index < _argc ? Value(_env, _argv[index]) : Env().Undefined();
}

inline Value CallbackInfo::This() const {
  if (_this == nullptr) {
    return Env().Undefined();
  }
  return Object(_env, _this);
}

inline void* CallbackInfo::Data() const {
  return _data;
}

inline void CallbackInfo::SetData(void* data) {
  _data = data;
}

////////////////////////////////////////////////////////////////////////////////
// PropertyDescriptor class
////////////////////////////////////////////////////////////////////////////////

template <typename Getter>
inline PropertyDescriptor
PropertyDescriptor::Accessor(const char* utf8name,
                             Getter getter,
                             napi_property_attributes attributes,
                             void* data) {
  typedef details::CallbackData<Getter, Napi::Value> CbData;
  // TODO: Delete when the function is destroyed
  auto callbackData = new CbData({ getter, nullptr });

  return PropertyDescriptor({
    utf8name,
    nullptr,
    nullptr,
    CbData::Wrapper,
    nullptr,
    nullptr,
    attributes,
    callbackData
  });
}

template <typename Getter>
inline PropertyDescriptor PropertyDescriptor::Accessor(const std::string& utf8name,
                                                       Getter getter,
                                                       napi_property_attributes attributes,
                                                       void* data) {
  return Accessor(utf8name.c_str(), getter, attributes, data);
}

template <typename Getter>
inline PropertyDescriptor PropertyDescriptor::Accessor(napi_value name,
                                                       Getter getter,
                                                       napi_property_attributes attributes,
                                                       void* data) {
  typedef details::CallbackData<Getter, Napi::Value> CbData;
  // TODO: Delete when the function is destroyed
  auto callbackData = new CbData({ getter, nullptr });

  return PropertyDescriptor({
    nullptr,
    name,
    nullptr,
    CbData::Wrapper,
    nullptr,
    nullptr,
    attributes,
    callbackData
  });
}

template <typename Getter>
inline PropertyDescriptor PropertyDescriptor::Accessor(Name name,
                                                       Getter getter,
                                                       napi_property_attributes attributes,
                                                       void* data) {
  napi_value nameValue = name;
  return PropertyDescriptor::Accessor(nameValue, getter, attributes, data);
}

template <typename Getter, typename Setter>
inline PropertyDescriptor PropertyDescriptor::Accessor(const char* utf8name,
                                                       Getter getter,
                                                       Setter setter,
                                                       napi_property_attributes attributes,
                                                       void* data) {
  typedef details::AccessorCallbackData<Getter, Setter> CbData;
  // TODO: Delete when the function is destroyed
  auto callbackData = new CbData({ getter, setter });

  return PropertyDescriptor({
    utf8name,
    nullptr,
    nullptr,
    CbData::GetterWrapper,
    CbData::SetterWrapper,
    nullptr,
    attributes,
    callbackData
  });
}

template <typename Getter, typename Setter>
inline PropertyDescriptor PropertyDescriptor::Accessor(const std::string& utf8name,
                                                       Getter getter,
                                                       Setter setter,
                                                       napi_property_attributes attributes,
                                                       void* data) {
  return Accessor(utf8name.c_str(), getter, setter, attributes, data);
}

template <typename Getter, typename Setter>
inline PropertyDescriptor PropertyDescriptor::Accessor(napi_value name,
                                                       Getter getter,
                                                       Setter setter,
                                                       napi_property_attributes attributes,
                                                       void* data) {
  typedef details::AccessorCallbackData<Getter, Setter> CbData;
  // TODO: Delete when the function is destroyed
  auto callbackData = new CbData({ getter, setter });

  return PropertyDescriptor({
    nullptr,
    name,
    nullptr,
    CbData::GetterWrapper,
    CbData::SetterWrapper,
    nullptr,
    attributes,
    callbackData
  });
}

template <typename Getter, typename Setter>
inline PropertyDescriptor PropertyDescriptor::Accessor(Name name,
                                                       Getter getter,
                                                       Setter setter,
                                                       napi_property_attributes attributes,
                                                       void* data) {
  napi_value nameValue = name;
  return PropertyDescriptor::Accessor(nameValue, getter, setter, attributes, data);
}

template <typename Callable>
inline PropertyDescriptor PropertyDescriptor::Function(const char* utf8name,
                                                       Callable cb,
                                                       napi_property_attributes attributes,
                                                       void* data) {
  typedef decltype(cb(CallbackInfo(nullptr, nullptr))) ReturnType;
  typedef details::CallbackData<Callable, ReturnType> CbData;
  // TODO: Delete when the function is destroyed
  auto callbackData = new CbData({ cb, nullptr });

  return PropertyDescriptor({
    utf8name,
    nullptr,
    CbData::Wrapper,
    nullptr,
    nullptr,
    nullptr,
    attributes,
    callbackData
  });
}

template <typename Callable>
inline PropertyDescriptor PropertyDescriptor::Function(const std::string& utf8name,
                                                       Callable cb,
                                                       napi_property_attributes attributes,
                                                       void* data) {
  return Function(utf8name.c_str(), cb, attributes, data);
}

template <typename Callable>
inline PropertyDescriptor PropertyDescriptor::Function(napi_value name,
                                                       Callable cb,
                                                       napi_property_attributes attributes,
                                                       void* data) {
  typedef decltype(cb(CallbackInfo(nullptr, nullptr))) ReturnType;
  typedef details::CallbackData<Callable, ReturnType> CbData;
  // TODO: Delete when the function is destroyed
  auto callbackData = new CbData({ cb, nullptr });

  return PropertyDescriptor({
    nullptr,
    name,
    CbData::Wrapper,
    nullptr,
    nullptr,
    nullptr,
    attributes,
    callbackData
  });
}

template <typename Callable>
inline PropertyDescriptor PropertyDescriptor::Function(Name name,
                                                       Callable cb,
                                                       napi_property_attributes attributes,
                                                       void* data) {
  napi_value nameValue = name;
  return PropertyDescriptor::Function(nameValue, cb, attributes, data);
}

inline PropertyDescriptor PropertyDescriptor::Value(const char* utf8name,
                                                    napi_value value,
                                                    napi_property_attributes attributes) {
  return PropertyDescriptor({
    utf8name, nullptr, nullptr, nullptr, nullptr, value, attributes, nullptr
  });
}

inline PropertyDescriptor PropertyDescriptor::Value(const std::string& utf8name,
                                                    napi_value value,
                                                    napi_property_attributes attributes) {
  return Value(utf8name.c_str(), value, attributes);
}

inline PropertyDescriptor PropertyDescriptor::Value(napi_value name,
                                                    napi_value value,
                                                    napi_property_attributes attributes) {
  return PropertyDescriptor({
    nullptr, name, nullptr, nullptr, nullptr, value, attributes, nullptr
  });
}

inline PropertyDescriptor PropertyDescriptor::Value(Name name,
                                                    Napi::Value value,
                                                    napi_property_attributes attributes) {
  napi_value nameValue = name;
  napi_value valueValue = value;
  return PropertyDescriptor::Value(nameValue, valueValue, attributes);
}

inline PropertyDescriptor::PropertyDescriptor(napi_property_descriptor desc)
  : _desc(desc) {
}

inline PropertyDescriptor::operator napi_property_descriptor&() {
  return _desc;
}

inline PropertyDescriptor::operator const napi_property_descriptor&() const {
  return _desc;
}

////////////////////////////////////////////////////////////////////////////////
// ObjectWrap<T> class
////////////////////////////////////////////////////////////////////////////////

template <typename T>
inline ObjectWrap<T>::ObjectWrap() {
}

template<typename T>
inline T* ObjectWrap<T>::Unwrap(Object wrapper) {
  T* unwrapped;
  napi_status status = napi_unwrap(wrapper.Env(), wrapper, reinterpret_cast<void**>(&unwrapped));
  if (status != napi_ok) throw Error::New(wrapper.Env());
  return unwrapped;
}

template <typename T>
inline Function ObjectWrap<T>::DefineClass(
    Napi::Env env,
    const char* utf8name,
    const std::initializer_list<ClassPropertyDescriptor<T>>& properties,
    void* data) {
  napi_value value;
  napi_status status = napi_define_class(
    env, utf8name, T::ConstructorCallbackWrapper, data, properties.size(),
    reinterpret_cast<const napi_property_descriptor*>(properties.begin()), &value);
  if (status != napi_ok) throw Error::New(env);

  return Function(env, value);
}

template <typename T>
inline Function ObjectWrap<T>::DefineClass(
    Napi::Env env,
    const char* utf8name,
    const std::vector<ClassPropertyDescriptor<T>>& properties,
    void* data) {
  napi_value value;
  napi_status status = napi_define_class(
    env, utf8name, T::ConstructorCallbackWrapper, data, properties.size(),
    reinterpret_cast<const napi_property_descriptor*>(properties.data()), &value);
  if (status != napi_ok) throw Error::New(env);

  return Function(env, value);
}

template <typename T>
inline ClassPropertyDescriptor<T> ObjectWrap<T>::StaticMethod(
    const char* utf8name,
    StaticVoidMethodCallback method,
    napi_property_attributes attributes,
    void* data) {
  // TODO: Delete when the class is destroyed
  StaticVoidMethodCallbackData* callbackData = new StaticVoidMethodCallbackData({ method, data });

  napi_property_descriptor desc = {};
  desc.utf8name = utf8name;
  desc.method = T::StaticVoidMethodCallbackWrapper;
  desc.data = callbackData;
  desc.attributes = static_cast<napi_property_attributes>(attributes | napi_static);
  return desc;
}

template <typename T>
inline ClassPropertyDescriptor<T> ObjectWrap<T>::StaticMethod(
    const char* utf8name,
    StaticMethodCallback method,
    napi_property_attributes attributes,
    void* data) {
  // TODO: Delete when the class is destroyed
  StaticMethodCallbackData* callbackData = new StaticMethodCallbackData({ method, data });

  napi_property_descriptor desc = {};
  desc.utf8name = utf8name;
  desc.method = T::StaticMethodCallbackWrapper;
  desc.data = callbackData;
  desc.attributes = static_cast<napi_property_attributes>(attributes | napi_static);
  return desc;
}

template <typename T>
inline ClassPropertyDescriptor<T> ObjectWrap<T>::StaticAccessor(
    const char* utf8name,
    StaticGetterCallback getter,
    StaticSetterCallback setter,
    napi_property_attributes attributes,
    void* data) {
  // TODO: Delete when the class is destroyed
  StaticAccessorCallbackData* callbackData =
    new StaticAccessorCallbackData({ getter, setter, data });

  napi_property_descriptor desc = {};
  desc.utf8name = utf8name;
  desc.getter = getter != nullptr ? T::StaticGetterCallbackWrapper : nullptr;
  desc.setter = setter != nullptr ? T::StaticSetterCallbackWrapper : nullptr;
  desc.data = callbackData;
  desc.attributes = static_cast<napi_property_attributes>(attributes | napi_static);
  return desc;
}

template <typename T>
inline ClassPropertyDescriptor<T> ObjectWrap<T>::InstanceMethod(
    const char* utf8name,
    InstanceVoidMethodCallback method,
    napi_property_attributes attributes,
    void* data) {
  // TODO: Delete when the class is destroyed
  InstanceVoidMethodCallbackData* callbackData =
    new InstanceVoidMethodCallbackData({ method, data});

  napi_property_descriptor desc = {};
  desc.utf8name = utf8name;
  desc.method = T::InstanceVoidMethodCallbackWrapper;
  desc.data = callbackData;
  desc.attributes = attributes;
  return desc;
}

template <typename T>
inline ClassPropertyDescriptor<T> ObjectWrap<T>::InstanceMethod(
    const char* utf8name,
    InstanceMethodCallback method,
    napi_property_attributes attributes,
    void* data) {
  // TODO: Delete when the class is destroyed
  InstanceMethodCallbackData* callbackData = new InstanceMethodCallbackData({ method, data });

  napi_property_descriptor desc = {};
  desc.utf8name = utf8name;
  desc.method = T::InstanceMethodCallbackWrapper;
  desc.data = callbackData;
  desc.attributes = attributes;
  return desc;
}

template <typename T>
inline ClassPropertyDescriptor<T> ObjectWrap<T>::InstanceAccessor(
    const char* utf8name,
    InstanceGetterCallback getter,
    InstanceSetterCallback setter,
    napi_property_attributes attributes,
    void* data) {
  // TODO: Delete when the class is destroyed
  InstanceAccessorCallbackData* callbackData =
    new InstanceAccessorCallbackData({ getter, setter, data });

  napi_property_descriptor desc = {};
  desc.utf8name = utf8name;
  desc.getter = T::InstanceGetterCallbackWrapper;
  desc.setter = T::InstanceSetterCallbackWrapper;
  desc.data = callbackData;
  desc.attributes = attributes;
  return desc;
}

template <typename T>
inline ClassPropertyDescriptor<T> ObjectWrap<T>::StaticValue(const char* utf8name,
    Napi::Value value, napi_property_attributes attributes) {
  napi_property_descriptor desc = {};
  desc.utf8name = utf8name;
  desc.value = value;
  desc.attributes = static_cast<napi_property_attributes>(attributes | napi_static);
  return desc;
}

template <typename T>
inline ClassPropertyDescriptor<T> ObjectWrap<T>::InstanceValue(
    const char* utf8name,
    Napi::Value value,
    napi_property_attributes attributes) {
  napi_property_descriptor desc = {};
  desc.utf8name = utf8name;
  desc.value = value;
  desc.attributes = attributes;
  return desc;
}

template <typename T>
inline napi_value ObjectWrap<T>::ConstructorCallbackWrapper(
    napi_env env,
    napi_callback_info info) {
  bool isConstructCall;
  napi_status status = napi_is_construct_call(env, info, &isConstructCall);
  if (status != napi_ok) return nullptr;

  if (!isConstructCall) {
    napi_throw_type_error(env, "Class constructors cannot be invoked without 'new'");
    return nullptr;
  }

  T* instance;
  napi_value wrapper;
  try {
    CallbackInfo callbackInfo(env, info);
    instance = new T(callbackInfo);
    wrapper = callbackInfo.This();
  }
  NAPI_RETHROW_JS_ERROR(env)

  napi_ref ref;
  status = napi_wrap(env, wrapper, instance, FinalizeCallback, nullptr, &ref);
  if (status != napi_ok) return nullptr;

  Reference<Object>* instanceRef = instance;
  *instanceRef = Reference<Object>(env, ref);

  return wrapper;
}

template <typename T>
inline napi_value ObjectWrap<T>::StaticVoidMethodCallbackWrapper(
    napi_env env,
    napi_callback_info info) {
  try {
    CallbackInfo callbackInfo(env, info);
    StaticVoidMethodCallbackData* callbackData =
      reinterpret_cast<StaticVoidMethodCallbackData*>(callbackInfo.Data());
    callbackInfo.SetData(callbackData->data);
    callbackData->callback(callbackInfo);
    return nullptr;
  }
  NAPI_RETHROW_JS_ERROR(env)
}

template <typename T>
inline napi_value ObjectWrap<T>::StaticMethodCallbackWrapper(
    napi_env env,
    napi_callback_info info) {
  try {
    CallbackInfo callbackInfo(env, info);
    StaticMethodCallbackData* callbackData =
      reinterpret_cast<StaticMethodCallbackData*>(callbackInfo.Data());
    callbackInfo.SetData(callbackData->data);
    return callbackData->callback(callbackInfo);
  }
  NAPI_RETHROW_JS_ERROR(env)
}

template <typename T>
inline napi_value ObjectWrap<T>::StaticGetterCallbackWrapper(
    napi_env env,
    napi_callback_info info) {
  try {
    CallbackInfo callbackInfo(env, info);
    StaticAccessorCallbackData* callbackData =
      reinterpret_cast<StaticAccessorCallbackData*>(callbackInfo.Data());
    callbackInfo.SetData(callbackData->data);
    return callbackData->getterCallback(callbackInfo);
  }
  NAPI_RETHROW_JS_ERROR(env)
}

template <typename T>
inline napi_value ObjectWrap<T>::StaticSetterCallbackWrapper(
    napi_env env,
    napi_callback_info info) {
  try {
    CallbackInfo callbackInfo(env, info);
    StaticAccessorCallbackData* callbackData =
      reinterpret_cast<StaticAccessorCallbackData*>(callbackInfo.Data());
    callbackInfo.SetData(callbackData->data);
    callbackData->setterCallback(callbackInfo, callbackInfo[0]);
    return nullptr;
  }
  NAPI_RETHROW_JS_ERROR(env)
}

template <typename T>
inline napi_value ObjectWrap<T>::InstanceVoidMethodCallbackWrapper(
    napi_env env,
    napi_callback_info info) {
  try {
    CallbackInfo callbackInfo(env, info);
    InstanceVoidMethodCallbackData* callbackData =
      reinterpret_cast<InstanceVoidMethodCallbackData*>(callbackInfo.Data());
    callbackInfo.SetData(callbackData->data);
    T* instance = Unwrap(callbackInfo.This().As<Object>());
    auto cb = callbackData->callback;
    (instance->*cb)(callbackInfo);
    return nullptr;
  }
  NAPI_RETHROW_JS_ERROR(env)
}

template <typename T>
inline napi_value ObjectWrap<T>::InstanceMethodCallbackWrapper(
    napi_env env,
    napi_callback_info info) {
  try {
    CallbackInfo callbackInfo(env, info);
    InstanceMethodCallbackData* callbackData =
      reinterpret_cast<InstanceMethodCallbackData*>(callbackInfo.Data());
    callbackInfo.SetData(callbackData->data);
    T* instance = Unwrap(callbackInfo.This().As<Object>());
    auto cb = callbackData->callback;
    return (instance->*cb)(callbackInfo);
  }
  NAPI_RETHROW_JS_ERROR(env)
}

template <typename T>
inline napi_value ObjectWrap<T>::InstanceGetterCallbackWrapper(
    napi_env env,
    napi_callback_info info) {
  try {
    CallbackInfo callbackInfo(env, info);
    InstanceAccessorCallbackData* callbackData =
      reinterpret_cast<InstanceAccessorCallbackData*>(callbackInfo.Data());
    callbackInfo.SetData(callbackData->data);
    T* instance = Unwrap(callbackInfo.This().As<Object>());
    auto cb = callbackData->getterCallback;
    return (instance->*cb)(callbackInfo);
  }
  NAPI_RETHROW_JS_ERROR(env)
}

template <typename T>
inline napi_value ObjectWrap<T>::InstanceSetterCallbackWrapper(
    napi_env env,
    napi_callback_info info) {
  try {
    CallbackInfo callbackInfo(env, info);
    InstanceAccessorCallbackData* callbackData =
      reinterpret_cast<InstanceAccessorCallbackData*>(callbackInfo.Data());
    callbackInfo.SetData(callbackData->data);
    T* instance = Unwrap(callbackInfo.This().As<Object>());
    auto cb = callbackData->setterCallback;
    (instance->*cb)(callbackInfo, callbackInfo[0]);
    return nullptr;
  }
  NAPI_RETHROW_JS_ERROR(env)
}

template <typename T>
inline void ObjectWrap<T>::FinalizeCallback(napi_env /*env*/, void* data, void* /*hint*/) {
  T* instance = reinterpret_cast<T*>(data);
  delete instance;
}

////////////////////////////////////////////////////////////////////////////////
// HandleScope class
////////////////////////////////////////////////////////////////////////////////

inline HandleScope::HandleScope(napi_env env, napi_handle_scope scope)
    : _env(env), _scope(scope) {
}

inline HandleScope::HandleScope(Napi::Env env) : _env(env) {
  napi_status status = napi_open_handle_scope(_env, &_scope);
  if (status != napi_ok) throw Error::New(_env);
}

inline HandleScope::~HandleScope() {
  napi_close_handle_scope(_env, _scope);
}

inline HandleScope::operator napi_handle_scope() const {
  return _scope;
}

inline Napi::Env HandleScope::Env() const {
  return Napi::Env(_env);
}

////////////////////////////////////////////////////////////////////////////////
// EscapableHandleScope class
////////////////////////////////////////////////////////////////////////////////

inline EscapableHandleScope::EscapableHandleScope(
  napi_env env, napi_escapable_handle_scope scope) : _env(env), _scope(scope) {
}

inline EscapableHandleScope::EscapableHandleScope(Napi::Env env) : _env(env) {
  napi_status status = napi_open_escapable_handle_scope(_env, &_scope);
  if (status != napi_ok) throw Error::New(_env);
}

inline EscapableHandleScope::~EscapableHandleScope() {
  napi_close_escapable_handle_scope(_env, _scope);
}

inline EscapableHandleScope::operator napi_escapable_handle_scope() const {
  return _scope;
}

inline Napi::Env EscapableHandleScope::Env() const {
  return Napi::Env(_env);
}

inline Value EscapableHandleScope::Escape(napi_value escapee) {
  napi_value result;
  napi_status status = napi_escape_handle(_env, _scope, escapee, &result);
  if (status != napi_ok) throw Error::New(_env);
  return Value(_env, result);
}

////////////////////////////////////////////////////////////////////////////////
// AsyncWorker class
////////////////////////////////////////////////////////////////////////////////

inline AsyncWorker::AsyncWorker(const Function& callback)
  : AsyncWorker(Object::New(callback.Env()), callback) {
}

inline AsyncWorker::AsyncWorker(const Object& receiver, const Function& callback)
  : _env(callback.Env()),
    _receiver(Napi::Persistent(receiver)),
    _callback(Napi::Persistent(callback)) {
  napi_status status = napi_create_async_work(
    _env, OnExecute, OnWorkComplete, this, &_work);
  if (status != napi_ok) throw Error::New(_env);
}

inline AsyncWorker::~AsyncWorker() {
  if (_work != nullptr) {
    napi_delete_async_work(_env, _work);
    _work = nullptr;
  }
}

inline AsyncWorker::AsyncWorker(AsyncWorker&& other) {
  _env = other._env;
  other._env = nullptr;
  _work = other._work;
  other._work = nullptr;
  _receiver = std::move(other._receiver);
  _callback = std::move(other._callback);
  _error = std::move(other._error);
}

inline AsyncWorker& AsyncWorker::operator =(AsyncWorker&& other) {
  _env = other._env;
  other._env = nullptr;
  _work = other._work;
  other._work = nullptr;
  _receiver = std::move(other._receiver);
  _callback = std::move(other._callback);
  _error = std::move(other._error);
  return *this;
}

inline AsyncWorker::operator napi_async_work() const {
  return _work;
}

inline Napi::Env AsyncWorker::Env() const {
  return Napi::Env(_env);
}

inline void AsyncWorker::Queue() {
  napi_status status = napi_queue_async_work(_env, _work);
  if (status != napi_ok) throw Error::New(_env);
}

inline void AsyncWorker::Cancel() {
  napi_status status = napi_cancel_async_work(_env, _work);
  if (status != napi_ok) throw Error::New(_env);
}

inline ObjectReference& AsyncWorker::Receiver() {
  return _receiver;
}

inline FunctionReference& AsyncWorker::Callback() {
  return _callback;
}

inline void AsyncWorker::OnOK() {
  _callback.MakeCallback(_receiver.Value(), {});
}

inline void AsyncWorker::OnError(const Error& e) {
  _callback.MakeCallback(_receiver.Value(), { e.Value() });
}

inline void AsyncWorker::SetError(const std::string& error) {
  _error = error;
}

inline void AsyncWorker::OnExecute(napi_env env, void* this_pointer) {
  AsyncWorker* self = static_cast<AsyncWorker*>(this_pointer);
  try {
    self->Execute();
  } catch (const std::exception& e) {
    self->SetError(e.what());
  }
}

inline void AsyncWorker::OnWorkComplete(
    napi_env env, napi_status status, void* this_pointer) {
  AsyncWorker* self = static_cast<AsyncWorker*>(this_pointer);
  if (status != napi_cancelled) {
    HandleScope scope(self->_env);
    try {
      if (self->_error.size() == 0) {
        self->OnOK();
      }
      else {
        self->OnError(Error::New(self->_env, self->_error));
      }
    } catch (const Error& e) {
      e.ThrowAsJavaScriptException();
    }
  }
  delete self;
}

// This macro shouldn't be useful in user code, because all
// callbacks from JavaScript to C++ are wrapped here.
#undef NAPI_RETHROW_JS_ERROR

} // namespace Napi

#endif // SRC_NAPI_INL_H_
