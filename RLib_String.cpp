/*
 *
 *  @author : rrrfff@foxmail.com
 *  https://github.com/rrrfff/ndk_string
 *
 */
#ifdef __GNUC__
# ifdef __clang__
#  pragma clang diagnostic ignored "-Wimplicit-exception-spec-mismatch"
# elif defined(__INTEL_COMPILER)
 // ignored
# else
#  pragma gcc diagnostic ignored "-Wimplicit-exception-spec-mismatch"
# endif
#else
 // ignored
#endif
#include <new>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include "RLib_String.hpp"
using namespace System;

//-------------------------------------------------------------------------

#define RLIB_HAS_FLOAT_SUPPORT     1
#define RLIB_STRING_FORMAT_LENGTH  256
#define RLIB_GlobalAlloc(n)        malloc(n)
#define RLIB_GlobalCollect(p)      free(p)
#define RLIB_InitClass(p, c)       (::new (p) c)
#define RLIB_DEFER(t, a)           RLIB_DEFER_DATA(t,__delay_##a);t &a = *reinterpret_cast<t *>(__delay_##a)
#define RLIB_DEFER_INIT(p, c)      RLIB_InitClass(p, c)
#define RLIB_DEFER_DESTROY(t, d)   reinterpret_cast<t *>(d)->~t()
#define RLIB_DEFER_ALIAS(t, d, a)  auto a = reinterpret_cast<t *>(d)
#define RLIB_DEFER_DATA(t, d)	   char d[RLIB_ALIGN_UP(sizeof(t), RLIB_ALIGNMENT)]
#define RLIB_MAX(a, b)             (((a) > (b)) ? (a) : (b))
#define RLIB_MIN(a, b)             (((a) < (b)) ? (a) : (b))
#define RLIB_ALIGNMENT             static_cast<intptr_t>(sizeof(intptr_t))
#define RLIB_ALIGN_UP(x, n)        (((x) + ((n) - 1)) & ~((n) - 1))
#define RLIB_ALIGN_DOWN(x, n)      ((x) & -(n))
#define RLIB_INTERNAL_CALC_ON_NEED (-1)
#define TSIZE(len)			       static_cast<intptr_t>((len + 1) * sizeof(TCHAR))
#define TSIZE_ALIGNED(len)	       RLIB_ALIGN_UP(TSIZE(len), RLIB_ALIGNMENT)
#define StringTermAtBytes(s,n)     (*reinterpret_cast<TCHAR *>(reinterpret_cast<char *>(s) + n) = _C('\0'))

//-------------------------------------------------------------------------

struct System::InternalString
{
	static constexpr size_t kSize = sizeof(intptr_t);
	intptr_t refCount;
	TCHAR    instanceString[0];

public:
	__forceinline InternalString *tryInit() {
		static_assert(sizeof(*this) <= kSize, "BOOM");
		volatile auto p = this;
		if (p != nullptr) {
			this->refCount = 0;
			this->instanceString[0] = _C('\0');
		} //if
		return this;
	}
	__forceinline TCHAR *tryToStringPointer() {
		volatile auto p = this;
		if (p != nullptr) {
			assert(this->refCount >= 0);
			return this->instanceString;
		} //if
		return nullptr;
	}
	__forceinline intptr_t increaseReference() {
		assert(this->refCount >= 0);
		return ++this->refCount;
	}
	__forceinline intptr_t decreaseReference() {
		assert(this->refCount >= 0);
		if (this->refCount == 0) {
			RLIB_GlobalCollect(this);
			return -1;
		} //if
		return --this->refCount;
	}
	template<typename char_t> static InternalString *fromStringPtr(const char_t *pstr) {
		return reinterpret_cast<InternalString *>(const_cast<char *>(
			reinterpret_cast<const char *>(pstr) - sizeof(intptr_t)));
	}
};

//-------------------------------------------------------------------------

namespace Utility
{
	template <typename T = intptr_t> constexpr T select_max(const T &a, const T &b) {
		return RLIB_MAX(a, b);
	}

	template <typename T = intptr_t, typename ... Args> constexpr T select_max(const T &a, const T &b, const Args& ... args) {
		return select_max(RLIB_MAX(a, b), args...);
	}

	template <typename T = intptr_t> constexpr T select_min(const T &a, const T &b) {
		return RLIB_MIN(a, b);
	}

	template <typename T = intptr_t, typename ... Args> constexpr T select_min(const T &a, const T &b, const Args& ... args) {
		return select_min(RLIB_MIN(a, b), args...);
	}
}

//-------------------------------------------------------------------------

TCHAR String::nullpstr[1] = { _C('\0') };
const TCHAR *String::nullpcstr = String::nullpstr;

//-------------------------------------------------------------------------

InternalString *String::allocate_internal_string(intptr_t size)
{
	return static_cast<InternalString *>(RLIB_GlobalAlloc(InternalString::kSize + static_cast<size_t>(size)));
}

//-------------------------------------------------------------------------

bool String::is_releasable() const
{
	assert(this->m_size <= 0 || (this->m_size > 0 && this->m_pstr != nullptr));
	return this->m_size > 0;
}

//-------------------------------------------------------------------------

bool String::is_writable() const
{
	return this->is_releasable() && this->get_internal_string()->refCount == 0;
}

//-------------------------------------------------------------------------

InternalString *String::get_internal_string() const
{
#ifndef NDEBUG
	assert(this->m_pstr != nullptr);
	auto lpstr = InternalString::fromStringPtr(this->m_pstr);
	assert(lpstr->refCount >= 0);
	return lpstr;
#else
	return InternalString::fromStringPtr(this->m_pstr);
#endif // NDEBUG
}

//-------------------------------------------------------------------------

void String::release_local_data()
{
	assert(this->is_releasable());
	this->Collect(this->m_pstr);
}

//-------------------------------------------------------------------------

String::String()
{
	this->m_pstr = nullptr;
	this->m_len  = 0;
	this->m_size = 0;
}

//-------------------------------------------------------------------------

String::String(intptr_t length)
{
    this->m_size = TSIZE_ALIGNED(length);
	this->m_len  = 0;
	this->m_pstr = allocate_internal_string(this->m_size)
				   ->tryInit()
				   ->tryToStringPointer();
}

//-------------------------------------------------------------------------

String::String(TCHAR *lptstr, intptr_t length /* = -1 */)
{
    this->m_len  = length < 0 ? _tcslen(lptstr) : length;
    this->m_size = TSIZE_ALIGNED(this->m_len);
	this->m_pstr = allocate_internal_string(this->m_size)
				   ->tryInit()
				   ->tryToStringPointer();
	
	memcpy(this->m_pstr, lptstr, this->m_len * sizeof(TCHAR));
	this->m_pstr[this->m_len] = _C('\0');
}

//-------------------------------------------------------------------------

String::String(const STRInfoA &si)
{
	this->m_pstr = const_cast<char *>(si.lpstr);
	this->m_len  = si.length;
	this->m_size = 0;
}

//-------------------------------------------------------------------------

String::String(const TCHAR *lptstr, intptr_t length /* = -1 */)
{
	if (length < 0) {
		this->m_pstr = const_cast<TCHAR *>(lptstr);
		this->m_len  = RLIB_INTERNAL_CALC_ON_NEED;
		this->m_size = 0;
	} else {
		RLIB_InitClass(this, String(const_cast<TCHAR *>(lptstr), length));
	} //if
}

//-------------------------------------------------------------------------

String::String(const String &str)
{
	assert(this != &str);

	// copy directly
	memcpy(this, &str, sizeof(*this));

	if (this->is_releasable()) {
		this->get_internal_string()->increaseReference();
	} //if
}

//-------------------------------------------------------------------------

String::String(String &&tmpstr)
{
	assert(this != &tmpstr);

	// copy directly
	memcpy(this, &tmpstr, sizeof(*this));

	// release it
	tmpstr.m_size = 0;
}

//-------------------------------------------------------------------------

String::~String()
{
	if (this->is_releasable()) {
		this->release_local_data();
	} //if

#ifndef NDEBUG
	this->m_pstr = nullptr;
	this->m_len  = -1;
	this->m_size = -1;
#endif // NDEBUG
}

//-------------------------------------------------------------------------

void String::Collect(const void *cp)
{
	InternalString::fromStringPtr(cp)->decreaseReference();
}

//-------------------------------------------------------------------------

String String::Reserve(intptr_t length)
{
	assert(length >= this->GetLength());
	return String(length).copy(*this);
}

//-------------------------------------------------------------------------

String &String::reserve(intptr_t length, bool keep /* = true */)
{
	this->pre_allocate(length, keep);
	return *this;
}

//-------------------------------------------------------------------------

void String::pre_allocate(intptr_t length, bool copyval /* = true */)
{
	intptr_t old_length = copyval ? this->GetLength() : 0;

    if (this->is_releasable())
	{
		intptr_t size_unaligned = TSIZE(length); // !length, in TCHARs

		if (this->m_size < size_unaligned || this->get_internal_string()->refCount > 0) {
			// aligned size
			this->m_size = RLIB_ALIGN_UP(size_unaligned, RLIB_ALIGNMENT);
			auto t_pstr  = allocate_internal_string(this->m_size)->tryInit();
			if ( t_pstr != nullptr ) {
				if (copyval) {
					size_unaligned = Utility::select_min(this->m_size - sizeof(TCHAR), // leave room for null terminator
														 old_length * sizeof(TCHAR));
					memcpy(t_pstr->instanceString, this->m_pstr, static_cast<size_t>(size_unaligned));
					StringTermAtBytes(t_pstr->instanceString, size_unaligned);
				} else {
					this->m_len = 0;
				} //if
			} else {
				this->m_size = 0;
				this->m_len  = 0;
			} //if
			this->release_local_data();
			this->m_pstr = t_pstr->tryToStringPointer();
		} //if
    } else {
		// forces to allocate new string
		this->m_size = TSIZE_ALIGNED(length);
		auto t_pstr  = allocate_internal_string(this->m_size)->tryInit();
		if ( t_pstr != nullptr ) {
			if (copyval) {
				auto capacity = Utility::select_min(this->m_size - sizeof(TCHAR), // leave room for null terminator
													old_length * sizeof(TCHAR));
				memcpy(t_pstr->instanceString, this->m_pstr, capacity);
				StringTermAtBytes(t_pstr->instanceString, capacity);
			} else {
				this->m_len = 0;
			} //if
			this->m_pstr = t_pstr->instanceString;
		} else {
			this->m_size = 0;
			this->m_len  = 0;
			this->m_pstr = nullptr;
		} //if
    } //if
}

//-------------------------------------------------------------------------

void String::update_reference(TCHAR *pstr, intptr_t size, intptr_t length)
{
	this->m_pstr = pstr;
	this->m_len = length;
	this->m_size = size;
	assert(TSIZE(this->m_len) <= this->m_size);
}

//-------------------------------------------------------------------------

String::operator TCHAR *()
{
	return this->GetData();
}

//-------------------------------------------------------------------------

TCHAR *String::GetData()
{
	this->pre_allocate(this->GetLength());

	this->m_len = RLIB_INTERNAL_CALC_ON_NEED;
	return this->m_pstr;
}

//-------------------------------------------------------------------------

const TCHAR *String::GetConstData() const
{
	if (this->IsNull()) {
		return String::nullpcstr;
	} //if

	return this->m_pstr;
}

//-------------------------------------------------------------------------

String &String::operator = (STRNull null_t)
{
	this->~String();
	RLIB_InitClass(this, String(null_t));
	return *this;
}

//-------------------------------------------------------------------------

String &String::operator = (TCHAR *lptstr)
{
	intptr_t length = _tcslen(lptstr);
	if (length == 0) {
		this->empty();
	} else {
		this->copy(lptstr, length);
	} //if

	return *this;
}

//-------------------------------------------------------------------------

String &String::operator = (const STRInfoA &si)
{
	if (si.length != 0) {
		if (this->is_releasable()) {
			// first, try copy
			if (this->tryCopy(si.lpstr, si.length)) {
				return *this;
			} //if
			// otherwise, release and prepare making reference
			this->release_local_data();
		}
		// just make reference
		this->m_size = 0;
		this->m_len  = si.length;
		this->m_pstr = const_cast<TCHAR *>(si.lpstr);
	} else {
		this->empty();
	} //if

	return *this;
}

//-------------------------------------------------------------------------

String &String::operator = (const TCHAR *lptstr)
{
	intptr_t length = RLIB_INTERNAL_CALC_ON_NEED;
	if (this->is_releasable()) {
		// first, try copy
		length = static_cast<intptr_t>(_tcslen(lptstr));
		if (this->tryCopy(lptstr, length)) {
			return *this;
		} //if
		// otherwise, release and prepare making reference
		this->release_local_data();
	}
	// just make reference
	this->m_size = 0;
	this->m_len  = length;
	this->m_pstr = const_cast<TCHAR *>(lptstr);

	return *this;
}

//-------------------------------------------------------------------------

String &String::operator = (const String &str)
{
	if (this == &str) return *this;

	if (this->is_releasable()) {
		// try copy
		if (this->tryCopy(str, str.GetLength())) {
			return *this;
		} //if

		// discard current instance
		this->release_local_data();		
	} //if

	RLIB_InitClass(this, String(str));

	return *this;
}

//-------------------------------------------------------------------------

String &String::operator = (String &&tmpstr)
{
	assert(this != &tmpstr);

	if (tmpstr.m_size > this->m_size) {
		// force discard current instance
		this->~String();
	} else if (this->is_releasable()) {
		// try copy
		if (this->tryCopy(tmpstr, tmpstr.GetLength())) {
			return *this;
		} //if

		// discard current instance in this case
		this->~String();
	} //if

	// copy directly
	memcpy(this, &tmpstr, sizeof(*this));

	// release it
	tmpstr.m_size = 0;

	return *this;
}

//-------------------------------------------------------------------------

intptr_t String::GetSize() const
{
	return this->m_size;
}

//-------------------------------------------------------------------------

intptr_t String::GetLength() const
{
	if (this->m_len < 0 ||
		(this->is_releasable() && this->get_internal_string()->refCount > 0)) {
		this->m_len = _tcslen(this->m_pstr);
	} //if
	return this->m_len;
}

//-------------------------------------------------------------------------

intptr_t String::GetCanReadSize() const
{
	return TSIZE(this->GetLength() - 1);
}

//-------------------------------------------------------------------------

TCHAR *String::c_str() const
{
	if (this->is_releasable()) {
		this->get_internal_string()->increaseReference();
		return this->m_pstr;
	} //if

	if (this->IsNull()) {
		// empty string
		return allocate_internal_string(sizeof(TCHAR))->tryInit()->tryToStringPointer();
	} //if

	auto tstr = allocate_internal_string(TSIZE(this->GetLength()))->tryInit();
	if (tstr != nullptr) {
		memcpy(tstr->instanceString, this->m_pstr, TSIZE(this->m_len - 1));
		tstr->instanceString[this->m_len] = _C('\0');
		return tstr->instanceString;
	} //if

	return nullptr;
}

//-------------------------------------------------------------------------

bool String::Contains(const TCHAR *value) const
{
	if (this->IsNull()) return false;

	return !value[0] || _tcsstr(this->m_pstr, value) != nullptr;
}

//-------------------------------------------------------------------------

bool String::Contains(TCHAR c) const
{
	if (this->IsNull()) return false;

	return !c || _tcschr(this->m_pstr, c) != nullptr;
}

//-------------------------------------------------------------------------

bool String::ContainsNoCase(const TCHAR *value) const
{
	if (this->IsNull()) return false;

	if (!value[0]) return true;

	return _tcsistr(this->m_pstr, value) != nullptr;
}

//-------------------------------------------------------------------------

String &String::copy(const String &str, intptr_t len /* = -1 */)
{
	assert(str.m_pstr != nullptr || len == 0 || str.GetLength() == 0);
	return this->copy(str.m_pstr, len < 0 ? str.GetLength() : len);
}

//-------------------------------------------------------------------------

bool String::tryCopy(const TCHAR *pstr, intptr_t len)
{
	if (len == 0) {
		this->empty();
		return true;
	} //if

	if (this->is_writable()) {
		if (TSIZE(len) <= this->m_size) {
			this->m_len = len;
			memcpy(this->m_pstr, pstr, TSIZE(len - 1));
			this->m_pstr[this->m_len] = _C('\0');
			return true;
		} //if
	} //if

	return false;
}

//-------------------------------------------------------------------------

String &String::append(const TCHAR c)
{
	return this->append(&c, 1);
}

//-------------------------------------------------------------------------

String &String::append(const TCHAR *pstr, intptr_t len /* = -1 */)
{
	if (len < 0)  len = _tcslen(pstr);
	if (len == 0) goto __return;

	if (this->IsNullOrEmpty()) {
		this->copy(pstr, len);
		goto __return;
	} //if

	this->pre_allocate(len + this->GetLength()); 

	assert(this->m_len >= 0);
    memcpy(this->m_pstr + this->m_len, pstr, len * sizeof(TCHAR));

    this->m_len += len;
    this->m_pstr[this->m_len] = _C('\0');

__return:
	return *this;
}

//-------------------------------------------------------------------------

String &String::append(const String &str, intptr_t len /* = -1 */)
{
	return this->append(str.GetConstData(), len < 0 ? str.GetLength() : len);
}

//-------------------------------------------------------------------------

String &__cdecl String::appendf(const TCHAR *pstrFormat, ...)
{
#if RLIB_HAS_FLOAT_SUPPORT
	float enable_float;
	enable_float = 0.0f;
#endif // RLIB_HAS_FLOAT_SUPPORT

	if (!this->is_writable()) {
		this->pre_allocate(this->GetLength() + RLIB_STRING_FORMAT_LENGTH, true);
	} //if

	auto lpstr = this->m_pstr + this->GetLength();
	auto nMax  = this->m_size / sizeof(TCHAR) - this->m_len; // with null terminator

	va_list argList;
	va_start(argList, pstrFormat);
	int rLen;
	while ((rLen = _vsntprintf(lpstr, nMax, pstrFormat, argList)) < 0) {
		assert(rLen == -1); // -1 indicating that truncation occurred
#ifdef _UNICODE
		if (errno == EOVERFLOW) {
			rLen  = RLIB_STRING_FORMAT_LENGTH; 
			this->pre_allocate(this->m_len + rLen, true);
			lpstr = this->m_pstr + this->m_len;
			nMax  = this->m_size / sizeof(TCHAR) - this->m_len;
			continue;
		} //if
#else
		rLen = vsnprintf(NULL, 0, pstrFormat, argList);
		if (rLen > 0) {
			this->pre_allocate(this->m_len + rLen, true);
			lpstr = this->m_pstr + this->m_len;
			nMax  = this->m_size / sizeof(TCHAR) - this->m_len;
			rLen  = vsnprintf(lpstr, nMax, pstrFormat, argList);
			if (rLen > 0) break;
		} //if
#endif // _UNICODE

		// failed to write formatted output
		rLen = 0;
		break;
	}

	va_end(argList);
	this->m_len += rLen;
	this->m_pstr[this->m_len] = 0;

	return *this;
}

//-------------------------------------------------------------------------

String &__cdecl String::copyf(const TCHAR *pstrFormat, ...)
{
#if RLIB_HAS_FLOAT_SUPPORT
	float enable_float;
	enable_float = 0.0f;
#endif // RLIB_HAS_FLOAT_SUPPORT

	if (!this->is_writable()) {
		this->pre_allocate(RLIB_STRING_FORMAT_LENGTH, false);
	} //if

	auto lpstr = this->m_pstr;
	auto nMax  = this->m_size / sizeof(TCHAR); // with null terminator

	va_list argList;
	va_start(argList, pstrFormat);
	int rLen;
	while ((rLen = _vsntprintf(lpstr, nMax, pstrFormat, argList)) < 0) {
		assert(rLen == -1); // -1 indicating that truncation occurred
#ifdef _UNICODE
		if (errno == EOVERFLOW) {
			rLen  = RLIB_STRING_FORMAT_LENGTH; 
			this->pre_allocate(this->m_len + rLen, true);
			lpstr = this->m_pstr;
			nMax  = this->m_size / sizeof(TCHAR);
			continue;
		} //if
#else
		rLen = vsnprintf(NULL, 0, pstrFormat, argList);
		if (rLen > 0) {
			this->pre_allocate(this->m_len + rLen, true);
			lpstr = this->m_pstr;
			nMax  = this->m_size / sizeof(TCHAR);
			rLen  = vsnprintf(lpstr, nMax, pstrFormat, argList);
			if (rLen > 0) break;
		} //if
#endif // _UNICODE

		// failed to write formatted output
		rLen = 0;
		break;
	}

	va_end(argList);
	this->m_len = rLen;
	this->m_pstr[this->m_len] = 0;

	return *this;
}

//-------------------------------------------------------------------------

String &String::copy(const TCHAR *lptstr, intptr_t len /* = -1 */)
{
	if (len < 0) {
		len = static_cast<intptr_t>(_tcslen(lptstr));
	} //if

	if (this->tryCopy(lptstr, len)) {
		return *this;
	} //if

	this->pre_allocate(len, false);
	this->m_len = len;
	memcpy(this->m_pstr, lptstr, this->m_len * sizeof(TCHAR));
	this->m_pstr[this->m_len] = _C('\0');

	return *this;
}

//-------------------------------------------------------------------------

String operator + (const String &src, const TCHAR *pstr)
{
    intptr_t length = _tcslen(pstr);
    if (length == 0) return src;

    return String(src.GetLength() + length).copy(src).append(pstr, length);
}

//-------------------------------------------------------------------------

String operator + (const String &src, const STRInfo &si)
{
	return String(src.GetLength() + si.length).copy(src).append(si.lpstr, si.length);
}

//-------------------------------------------------------------------------

String operator + (const String &sl, const String &sr)
{
	return String(sl.GetLength() + sr.GetLength()).copy(sl).append(sr);
}

//-------------------------------------------------------------------------

String &String::operator += (const TCHAR c)
{
	return this->append(&c, 1);
}

//-------------------------------------------------------------------------

String &String::operator += (const TCHAR *pstr)
{
    return this->append(pstr);
}

//-------------------------------------------------------------------------

String &String::operator += (const String &src)
{
    return this->append(src);
}

//-------------------------------------------------------------------------

intptr_t String::Compare(const TCHAR *lpsz) const
{
    return _tcscmp(this->GetConstData(), lpsz);
}

//-------------------------------------------------------------------------

intptr_t String::Compare(const TCHAR *lpsz, intptr_t n) const
{
	return _tcsncmp(this->GetConstData(), lpsz, n);
}

//-------------------------------------------------------------------------

intptr_t String::CompareNoCase(const TCHAR *lpsz) const
{
	return _tcsicmp(this->GetConstData(), lpsz);
}

//-------------------------------------------------------------------------

intptr_t String::CompareNoCase(const TCHAR *lpsz, intptr_t n) const
{
    return _tcsnicmp(this->GetConstData(), lpsz, n);
}

//-------------------------------------------------------------------------

bool String::operator == (const TCHAR *str) const
{
    return this->Compare(str) == 0;
}

//-------------------------------------------------------------------------

bool String::operator != (const TCHAR *str) const
{
    return this->Compare(str) != 0;
}

//-------------------------------------------------------------------------

bool String::operator <= (const TCHAR *str) const
{
    return this->Compare(str) <= 0;
}

//-------------------------------------------------------------------------

bool String::operator < (const TCHAR *str) const
{
    return this->Compare(str) < 0;
}

//-------------------------------------------------------------------------

bool String::operator >= (const TCHAR *str) const
{
    return this->Compare(str) >= 0;
}

//-------------------------------------------------------------------------

bool String::operator > (const TCHAR *str) const
{
    return this->Compare(str) > 0;
}

//-------------------------------------------------------------------------

bool String::operator == (const String &str)const
{
	if (this->IsNull()) return str.IsNull();
	if (str.IsNull()) return false;
    return this->operator == (str.GetConstData());
}

//-------------------------------------------------------------------------

bool String::operator != (const String &str) const
{
	if (this->IsNull()) return !str.IsNull();
	if (str.IsNull()) return true;
    return this->operator != (str.GetConstData());
}

//-------------------------------------------------------------------------

bool String::operator <= (const String &str) const
{
    return this->operator <= (str.GetConstData());
}

//-------------------------------------------------------------------------

bool String::operator < (const String &str) const
{
    return this->operator < (str.GetConstData());
}

//-------------------------------------------------------------------------

bool String::operator >= (const String &str) const
{
    return this->operator >= (str.GetConstData());
}

//-------------------------------------------------------------------------

bool String::operator > (const String &str) const
{
    return this->operator > (str.GetConstData());
}

//-------------------------------------------------------------------------

const TCHAR &String::operator [] (intptr_t nIndex) const
{
    return this->get_const_char(nIndex);
}

//-------------------------------------------------------------------------

TCHAR &String::operator [] (intptr_t nIndex)
{
	return this->get_char(nIndex);
}

//-------------------------------------------------------------------------

bool String::IsEmpty() const
{
    return this->GetLength() == 0;
}

//-------------------------------------------------------------------------

bool String::IsNull() const
{
    return this->m_pstr == nullptr;
}

//-------------------------------------------------------------------------

bool String::IsNullOrEmpty() const
{
	return this->IsNull() || this->IsEmpty();
}

//-------------------------------------------------------------------------

TCHAR String::front() const
{
	return this->m_pstr ? this->m_pstr[0] : _C('\0');
}

//-------------------------------------------------------------------------

TCHAR String::back() const
{
	return this->m_pstr && this->GetLength() ? this->m_pstr[this->m_len - 1] : _C('\0');
}

//-------------------------------------------------------------------------

bool String::StartsWith(TCHAR c) const
{
    return this->GetLength() > 0 && this->GetAt(0) == c;
}

//-------------------------------------------------------------------------

bool String::EndsWith(TCHAR c) const
{
    return this->GetLength() > 0 && this->GetAt(this->GetLength() - 1) == c;
}

//-------------------------------------------------------------------------

bool String::StartsWith(const TCHAR *pstr, intptr_t length /* = -1 */) const
{
	if (length < 0) {
		length = static_cast<intptr_t>(_tcslen(pstr));
	} //if
	if (this->GetLength() < length) return false;
	
	while (--length >= 0) {
		if (this->m_pstr[length] != pstr[length]) return false;
	}
	return true;
}

//-------------------------------------------------------------------------

bool String::EndsWith(const TCHAR *pstr, intptr_t length /* = -1 */) const
{
	if (length < 0) {
		length = static_cast<intptr_t>(_tcslen(pstr));
	} //if
	if (this->GetLength() < length) return false;

	auto suffixstr = this->m_pstr + this->m_len - length;
	while (--length >= 0) {
		if (suffixstr[length] != pstr[length]) return false;
	}
	return true;
}

//-------------------------------------------------------------------------

String &String::empty()
{
	if (this->is_writable()) {
		this->m_pstr[0] = _C('\0');
		this->m_len     = 0;
	} else {
		if (this->is_releasable()) {
			this->release_local_data();
		} //if

		this->m_pstr = String::nullpstr;
		this->m_size = 0;
		this->m_len  = 0;
	} //if
    
	return *this;
}

//-------------------------------------------------------------------------

bool String::IsConst() const
{
	return !this->is_writable();
}

//-------------------------------------------------------------------------

const TCHAR &String::get_const_char(intptr_t index) const
{
	assert(index >= 0);
	if (this->IsNull()) {
		return String::nullpstr[0];
	} //if

	assert(index < this->GetLength());
    return this->m_pstr[index];
}

//-------------------------------------------------------------------------

TCHAR &String::get_char(intptr_t index)
{
	assert(index >= 0);
	if (this->IsNull()) {
		return String::nullpstr[0];
	} //if
	assert(index < this->GetLength());

	this->pre_allocate(this->GetLength());
	this->m_len = RLIB_INTERNAL_CALC_ON_NEED;

	return this->m_pstr[index];
}

//-------------------------------------------------------------------------

TCHAR String::GetAt(intptr_t nIndex) const
{
	if (this->IsNull()) {
		return String::nullpstr[0];
	} //if
    return this->m_pstr[nIndex];
}

//-------------------------------------------------------------------------

void String::SetAt(intptr_t nIndex, TCHAR ch)
{
    this->pre_allocate(nIndex + 1);
    
    this->m_pstr[nIndex] = ch;
	this->m_len          = RLIB_INTERNAL_CALC_ON_NEED;
}

//-------------------------------------------------------------------------

void String::CopyTo(TCHAR *pstr, intptr_t max_length_with_null)
{
	intptr_t max_length = max_length_with_null - 1;

	if (this->GetLength() < max_length) {
		max_length = this->m_len;
	} //if

	memcpy(pstr, this->m_pstr, max_length * sizeof(TCHAR));
	pstr[max_length] = _C('\0');
}

//-------------------------------------------------------------------------

intptr_t String::IndexOf(TCHAR c, intptr_t begin /* = 0 */) const
{
	if (!this->IsNullOrEmpty() && begin < this->GetLength()) {
		TCHAR *pstr = _tcschr(this->m_pstr + begin, static_cast<unsigned int>(c));
		if (pstr != nullptr) {
			return pstr - this->m_pstr;
		} //if
	} //if
    
	return -1;
}

//-------------------------------------------------------------------------

intptr_t String::LastIndexOf(TCHAR c) const
{
    if (!this->IsNullOrEmpty()) {
		TCHAR *pstr = _tcsrchr(this->m_pstr, static_cast<unsigned int>(c));
		if (pstr != nullptr) {
			return pstr - this->m_pstr;
		} //if
    } //if
    
	return -1;
}

//-------------------------------------------------------------------------

template<bool kCase>
inline intptr_t __IndexOf_impl(const TCHAR *lpstr, intptr_t len, intptr_t begin, const TCHAR *needle)
{
	assert(begin >= 0);
	if (begin < len) {
		needle = kCase ? _tcsstr(lpstr + begin, needle) : _tcsistr(lpstr + begin, needle);

		if (needle != nullptr) return needle - lpstr;
	} //if

	return -1;
}

//-------------------------------------------------------------------------

intptr_t String::IndexOf(const TCHAR *p, intptr_t begin /* = 0 */) const
{
	return __IndexOf_impl<true>(this->m_pstr, this->m_len, begin, p);
}

//-------------------------------------------------------------------------

intptr_t String::IndexOfNoCase(const TCHAR *p, intptr_t begin /* = 0 */) const
{
	return __IndexOf_impl<false>(this->m_pstr, this->m_len, begin, p);
}

//-------------------------------------------------------------------------

template<bool kCase, bool kRight>
inline intptr_t __LastIndexOfL_impl(const TCHAR *lpstr, intptr_t len, const TCHAR *needle, intptr_t needle_len)
{
	if (len >= needle_len) {
		len -= needle_len;
		const TCHAR *p0 = lpstr + len;
		while ((kCase ? _tcsncmp : _tcsnicmp)(p0, needle, needle_len) != 0) {
			if (--len < 0) return -1;
			--p0;
		}
		return (p0 - lpstr) + (kRight ? needle_len : 0);
	} //if

	return -1;
}

//-------------------------------------------------------------------------

intptr_t String::LastIndexOf(const TCHAR *p) const
{
	return this->LastIndexOfL(p, _tcslen(p));
}

//-------------------------------------------------------------------------

intptr_t String::LastIndexOfL(const TCHAR *p, intptr_t len) const
{
	return __LastIndexOfL_impl<true, false>(this->m_pstr, this->m_len, p, len);
}

//-------------------------------------------------------------------------

intptr_t String::LastIndexOfNoCase(const TCHAR *p) const
{
	return this->LastIndexOfLNoCase(p, _tcslen(p));
}

//-------------------------------------------------------------------------

intptr_t String::LastIndexOfLNoCase(const TCHAR *p, intptr_t len) const
{
	return __LastIndexOfL_impl<false, false>(this->m_pstr, this->m_len, p, len);
}

//-------------------------------------------------------------------------

intptr_t String::IndexOfR(const TCHAR *p, intptr_t begin /* = 0 */) const
{
	return this->IndexOfRL(p, _tcslen(p), begin);
}

//-------------------------------------------------------------------------

intptr_t String::IndexOfRL(const TCHAR *p, intptr_t len, intptr_t begin) const
{
    intptr_t result = this->IndexOf(p, begin);
	if (result != -1) {
		return result + len;
	} //if
	return -1;
}

//-------------------------------------------------------------------------

intptr_t String::IndexOfRNoCase(const TCHAR *p, intptr_t begin /* = 0 */) const
{
	return this->IndexOfRLNoCase(p, _tcslen(p), begin);
}

//-------------------------------------------------------------------------

intptr_t String::IndexOfRLNoCase(const TCHAR *p, intptr_t len, intptr_t begin) const
{
	intptr_t result = this->IndexOfNoCase(p, begin);
	if (result != -1) {
		return result + len;
	} //if
	return -1;
}

//-------------------------------------------------------------------------

intptr_t String::LastIndexOfR(const TCHAR *p) const
{
	return this->LastIndexOfRL(p, _tcslen(p));
}

//-------------------------------------------------------------------------

intptr_t String::LastIndexOfRL(const TCHAR *p, intptr_t len) const
{
	return __LastIndexOfL_impl<true, true>(this->m_pstr, this->m_len, p, len);
}

//-------------------------------------------------------------------------

intptr_t String::LastIndexOfRNoCase(const TCHAR *p) const
{
	return this->LastIndexOfRLNoCase(p, _tcslen(p));
}

//-------------------------------------------------------------------------

intptr_t String::LastIndexOfRLNoCase(const TCHAR *p, intptr_t len) const
{
	return __LastIndexOfL_impl<false, true>(this->m_pstr, this->m_len, p, len);
}

//-------------------------------------------------------------------------

#ifndef _UNICODE
static inline void string_reverse(char *buffer, intptr_t length)
{
	for (intptr_t i = 0, j = length - 1 ; i < length; ++i, --j) {
		// we can only process ASCII characters as buffer is in UTF-8 format
		buffer[j] = buffer[i];
	}
	assert(buffer[length] == '\0');
}
#endif // _UNICODE

//-------------------------------------------------------------------------

String String::Concat(const TCHAR *pstr, intptr_t len /* = -1 */) const
{
	if (len < 0) len = _tcslen(pstr);
	if (len == 0) return *this;

	return String(this->GetLength() + len).copy(this->GetConstData(), this->m_len).append(pstr, len);
}

//-------------------------------------------------------------------------

String String::Concat(const String &src, intptr_t len /* = -1 */) const
{
	return this->Concat(src.GetConstData(), len >= 0 ? len : src.GetLength());
}

//-------------------------------------------------------------------------

String String::Reverse() const
{
	return String(*this).reverse();
}

//-------------------------------------------------------------------------

String &String::reverse()
{
	if (!this->IsNull()) {
		this->pre_allocate(this->GetLength());

#ifdef _UNICODE
		intptr_t n = this->GetLength();
		intptr_t i = (n / 2) - 1; 
		for (--n; i >= 0; --i) {
			this->m_pstr[i]     = static_cast<TCHAR>(this->m_pstr[i] ^ this->m_pstr[n - i]);
			this->m_pstr[n - i] = static_cast<TCHAR>(this->m_pstr[i] ^ this->m_pstr[n - i]);
			this->m_pstr[i]     = static_cast<TCHAR>(this->m_pstr[i] ^ this->m_pstr[n - i]);
			//Utility::swap(this->m_pstr[i], this->m_pstr[n - i]);
		}
#else
		string_reverse(this->m_pstr, this->GetLength());
#endif // _UNICODE
	}

	return *this;
}

//-------------------------------------------------------------------------

String String::ToLower() const
{
	return String(*this).toLower();
}

//-------------------------------------------------------------------------

String &String::toLower()
{
	this->pre_allocate(this->GetLength());

	TCHAR *__restrict p = this->m_pstr;
	while (*p != _C('\0')) {
		if (*p >= _C('A') && *p <= _C('Z')) {
			*p = _C('a') + *p - _C('A');
		} //if
	}

	return *this;
}

//-------------------------------------------------------------------------

String String::ToUpper() const
{
	return String(*this).toUpper();
}

//-------------------------------------------------------------------------

String &String::toUpper()
{
	this->pre_allocate(this->GetLength());

	TCHAR *__restrict p = this->m_pstr;
	while (*p != _C('\0')) {
		if (*p >= _C('a') && *p <= _C('z')) {
			*p = _C('A') + *p - _C('a');
		} //if
	}

	return *this;
}

//-------------------------------------------------------------------------

String String::Trim(TCHAR c /* = 0 */) const
{
	return String(*this).trim(c);
}

//-------------------------------------------------------------------------

String &String::trim(TCHAR c /* = 0 */)
{
	intptr_t firstIndex = 0;
	for (intptr_t i = 0; i < this->GetLength(); ++i) {
		if ((c == 0 && !_istspace(static_cast<unsigned int>(this->m_pstr[i]))) ||
			(c != 0 && this->m_pstr[i] != c)) {
			break;
		} //if
		++firstIndex;
	}
	if (firstIndex >= this->GetLength()) {
		this->empty();
		return *this;
	} //if

	intptr_t lastIndex = this->GetLength();
	for (intptr_t k = this->GetLength() - 1; k >= 0; --k) {
		if ((c == 0 && !_istspace(static_cast<unsigned int>(this->m_pstr[k]))) ||
			(c != 0 && this->m_pstr[k] != c)) {
			break;
		} //if
		--lastIndex;
	}
	assert(lastIndex > 0);
	
	// not found any
	if (firstIndex == 0 && lastIndex == this->GetLength()) {
		return *this;
	} //if

	if (this->is_writable()) {
		this->m_len     = lastIndex - firstIndex;
		if (firstIndex != 0) {
			memmove(this->m_pstr, this->m_pstr + firstIndex, static_cast<size_t>(TSIZE(this->m_len - 1)));
		} //if	
		this->m_pstr[this->m_len] = _C('\0');
	} else {
		this->copy(this->m_pstr + firstIndex, lastIndex - firstIndex);
	} // if
	
	return *this;
}

//-------------------------------------------------------------------------

String String::TrimStart(TCHAR c /* = 0 */) const
{
	return String(*this).trimStart(c);
}

//-------------------------------------------------------------------------

String &String::trimStart(TCHAR c /* = 0 */)
{
	intptr_t firstIndex = 0;
	for (intptr_t i = 0; i < this->GetLength(); ++i)
	{
		if ((c == 0 && !_istspace(static_cast<unsigned int>(this->m_pstr[i]))) ||
			(c != 0 && this->m_pstr[i] != c)) {
			break;
		} //if
		++firstIndex;
	}
	if (firstIndex == 0) {
		return *this;
	} //if
	if (firstIndex >= this->GetLength()) {
		this->empty();
		return *this;
	} //if

	if (this->is_writable()) {
		this->m_len -= firstIndex;
		memmove(this->m_pstr,
				this->m_pstr + firstIndex,
				static_cast<size_t>(TSIZE(this->m_len - 1)));
		this->m_pstr[this->m_len] = _C('\0');
	} else {
		this->copy(this->m_pstr + firstIndex, this->m_len - firstIndex);
	} // if

	return *this;
}

//-------------------------------------------------------------------------

String String::TrimEnd(TCHAR c /* = 0 */) const
{
	return String(*this).trimEnd(c);
}

//-------------------------------------------------------------------------

String &String::trimEnd(TCHAR c /* = 0 */)
{
	intptr_t lastIndex = this->GetLength();
	for (intptr_t i = this->GetLength() - 1; i >= 0; --i)
	{
		if ((c == 0 && !_istspace(static_cast<unsigned int>(this->m_pstr[i]))) ||
			(c != 0 && this->m_pstr[i] != c)) {
			break;
		} //if
		--lastIndex;
	}
	if (lastIndex == this->GetLength()) {
		return *this;
	} //if
	if (lastIndex <= 0) {
		this->empty();
		return *this;
	} //if
	
	if (this->is_writable()) {
		this->m_len               = lastIndex - 0;
		this->m_pstr[this->m_len] = _C('\0');
	} else {
		this->copy(this->m_pstr, lastIndex - 0);
	} // if

	return *this;
}

//-------------------------------------------------------------------------

String String::PadRight(intptr_t totalWidth, TCHAR paddingChar /* = T(' ') */) const
{
	intptr_t totalSpace = totalWidth - this->GetLength();
	if (totalSpace <= 0) return *this;

	String pad_str(totalWidth);
	pad_str.append(*this, totalWidth - totalSpace);
	while (--totalSpace >= 0) {
		pad_str.append(&paddingChar, 1);
	}
	return pad_str;
}

//-------------------------------------------------------------------------

String &String::padRight(intptr_t totalWidth, TCHAR paddingChar /* = T(' ') */)
{
	intptr_t totalSpace = totalWidth - this->GetLength();
	while (--totalSpace >= 0) {
		this->append(&paddingChar, 1);
	}
	return *this;
}

//-------------------------------------------------------------------------

String String::PadLeft(intptr_t totalWidth, TCHAR paddingChar /* = T(' ') */) const
{
	intptr_t totalSpace = totalWidth - this->GetLength();
	if (totalSpace <= 0) return *this;

	String pad_str(totalWidth);
	while (--totalSpace >= 0) {
		pad_str.append(&paddingChar, 1);
	}
	pad_str.append(*this, totalWidth - totalSpace);
	return pad_str;
}

//-------------------------------------------------------------------------

String &String::padLeft(intptr_t totalWidth, TCHAR paddingChar /* = T(' ') */)
{
	intptr_t totalSpace = totalWidth - this->GetLength();
	if (totalSpace > 0) {
		if (this->is_writable() &&
			this->m_size >= TSIZE(totalWidth)) {
			// move blocks
			memmove(this->m_pstr + totalSpace, this->m_pstr,
					this->m_len * sizeof(TCHAR));
			// fill chars
			while (--totalSpace >= 0) {
				this->m_pstr[totalSpace] = paddingChar;
			}
			this->m_len               = totalWidth;
			this->m_pstr[this->m_len] = _C('\0');
		} else {
			String pad_str(totalWidth);
			while (--totalSpace >= 0) {
				pad_str.append(&paddingChar, 1);
			}
			*this = pad_str.append(*this);;
		} //if
	}
	return *this;
}

//-------------------------------------------------------------------------

String String::Replace(const TCHAR *pstrFrom,
					   const TCHAR *pstrTo,
					   intptr_t begin /* = 0 */,
					   intptr_t n /* = 0 */,
					   intptr_t *replace_count /* = nullptr */) const
{
	if (pstrFrom[0] == _C('\0') ||
		begin >= this->GetLength() ||
		(begin = this->IndexOf(pstrFrom, begin)) == -1) {
		return *this;
	} //if
	
	auto sF = _tcslen(pstrFrom);
	auto sT = _tcslen(pstrTo);
	bool diff_check = (n == 0) ? false : true;

	String tstr;
	if (sF == sT) {
		tstr.copy(*this);

		TCHAR *sL = tstr.m_pstr + begin;
		TCHAR *sP = sL - sF;
		while ((sP = _tcsstr(sP + sF, pstrFrom)) != nullptr) {
			memcpy(sP, pstrTo, sT * sizeof(TCHAR));

			if (replace_count != nullptr) {
				++(*replace_count);
			} //if

			if (diff_check) {
				if (n == 1) break;
				--n;
			} //if
		}

		return tstr;
	} //if

	if (sF < sT) {
		intptr_t affected_length   = this->GetLength() - begin;
		intptr_t max_replace_count = static_cast<intptr_t>(affected_length / sF) + 1;
		tstr.pre_allocate(affected_length + max_replace_count * (sT - sF) + begin, false);
	} else {
		tstr.pre_allocate(this->GetLength(), false);
	} //if

	if (begin != 0) {
		tstr.append(this->GetConstData(), begin - 0);
	} //if

	TCHAR *sL   = this->m_pstr + begin;
	TCHAR *sP   = sL - sF;
	intptr_t sO = begin;
	while ((sP  = _tcsstr(sP + sF, pstrFrom)) != nullptr) {
		if ((sP - sL) > 0) {
			tstr.append(&this->m_pstr[sO], sP - sL).append(pstrTo, sT);
		} else {
			tstr.append(pstrTo, sT);
		} //if
		sO = (sP - this->m_pstr) + sF;
		sL = sP + sF;

		if (replace_count != nullptr) {
			++(*replace_count);
		} //if

		if (diff_check) {
			if (n == 1) break;
			--n;
		} //if
	}

	if (sO < this->m_len) {
		tstr.append(this->GetConstData() + sO, this->GetLength() - sO);
	} //if

	return tstr;
}

//-------------------------------------------------------------------------

String String::ReplaceNoCase(const TCHAR *pstrFrom, 
							 const TCHAR *pstrTo, 
							 intptr_t begin /* = 0 */, 
							 intptr_t n /* = 0 */, 
							 intptr_t *replace_count /* = nullptr */) const
{
	if (pstrFrom[0] == _C('\0') ||
		begin >= this->GetLength() ||
		(begin = this->IndexOfNoCase(pstrFrom, begin)) == -1) {
		return *this;
	} //if

	auto sF = _tcslen(pstrFrom);
	auto sT = _tcslen(pstrTo);
	bool diff_check = (n == 0) ? false : true;

	String tstr;
	if (sF == sT) {
		tstr.copy(*this);

		TCHAR *sL = tstr.m_pstr + begin;
		TCHAR *sP = sL - sF;
		while ((sP = _tcsistr(sP + sF, pstrFrom)) != nullptr) {
			memcpy(sP, pstrTo, sT * sizeof(TCHAR));

			if (replace_count != nullptr) {
				++(*replace_count);
			} //if

			if (diff_check) {
				if (n == 1) break;
				--n;
			} //if
		}

		return tstr;
	} //if

	if (sF < sT) {
		intptr_t affected_length   = this->GetLength() - begin;
		intptr_t max_replace_count = static_cast<intptr_t>(affected_length / sF) + 1;
		tstr.pre_allocate(affected_length + max_replace_count * (sT - sF) + begin, false);
	} else {
		tstr.pre_allocate(this->GetLength() + begin, false);
	} //if

	if (begin != 0) {
		tstr.append(this->GetConstData(), begin - 0);
	} //if

	TCHAR *sL = this->m_pstr + begin;
	TCHAR *sP = sL - sF;
	intptr_t sO = begin;
	while ((sP = _tcsistr(sP + sF, pstrFrom)) != nullptr) {
		if ((sP - sL) > 0) {
			tstr.append(&this->m_pstr[sO], sP - sL).append(pstrTo, sT);
		} else {
			tstr.append(pstrTo, sT);
		} //if
		sO = (sP - this->m_pstr) + sF;
		sL = sP + sF;

		if (replace_count != nullptr) {
			++(*replace_count);
		} //if

		if (diff_check) {
			if (n == 1) break;
			--n;
		} //if
	}

	if (sO < this->m_len) {
		tstr.append(this->GetConstData() + sO, this->GetLength() - sO);
	} //if

	return tstr;
}

//-------------------------------------------------------------------------

String &String::replace(const TCHAR *pstrFrom,
						const TCHAR *pstrTo,
						intptr_t begin /* = 0 */,
						intptr_t n /* = 0 */,
						intptr_t *replace_count /* = nullptr */)
{
	if (pstrFrom[0] == _C('\0') ||
		begin >= this->GetLength() ||
		(begin = this->IndexOf(pstrFrom, begin)) == -1) {
		return *this;
	} //if

	bool diff_check = (n == 0) ? false : true;
	auto sF = _tcslen(pstrFrom);
	auto sT = _tcslen(pstrTo);
	if (sF == sT) {
		this->pre_allocate(this->GetLength());

		TCHAR *sL  = this->m_pstr + begin;
		TCHAR *sP  = sL - sF;
		while ((sP = _tcsstr(sP + sF, pstrFrom)) != nullptr) {
			memcpy(sP, pstrTo, sT * sizeof(TCHAR));

			if (replace_count != nullptr) {
				++(*replace_count);
			} //if

			if (diff_check) {
				if (n == 1) break;
				--n;
			} //if
		}

		return *this;
	} //if

	RLIB_DEFER(String, tstr);
	if (sF < sT) {
		intptr_t affected_length   = this->GetLength() - begin;
		intptr_t max_replace_count = static_cast<intptr_t>(affected_length / sF) + 1;
		RLIB_DEFER_INIT(&tstr, String(affected_length + max_replace_count * (sT - sF) + begin));
	} else {
		RLIB_DEFER_INIT(&tstr, String(this->GetLength()));
	} //if

	if (begin != 0) {
		tstr.append(this->GetConstData(), begin - 0);
	} //if

	TCHAR *sL = this->m_pstr + begin;
	TCHAR *sP = sL - sF;
	intptr_t sO = begin;
	while ((sP = _tcsstr(sP + sF, pstrFrom)) != nullptr) {
		if ((sP - sL) > 0) {
			tstr.append(&this->m_pstr[sO], sP - sL).append(pstrTo, sT);
		} else {
			tstr.append(pstrTo, sT);
		}
		sO = (sP - this->m_pstr) + sF;
		sL = sP + sF;

		if (replace_count != nullptr) {
			++(*replace_count);
		} //if

		if (diff_check) {
			if (n == 1) break;
			--n;
		} //if
	}

	if (sO < this->m_len) {
		tstr.append(this->GetConstData() + sO, this->GetLength() - sO);
	} //if

	*this = tstr;
	RLIB_DEFER_DESTROY(String, &tstr);
	return *this;
}

//-------------------------------------------------------------------------

String &String::replaceNoCase(const TCHAR *pstrFrom,
							  const TCHAR *pstrTo,
							  intptr_t begin /* = 0 */,
							  intptr_t n /* = 0 */,
							  intptr_t *replace_count /* = nullptr */)
{
	if (pstrFrom[0] == _C('\0') ||
		begin >= this->GetLength() ||
		(begin = this->IndexOfNoCase(pstrFrom, begin)) == -1) {
		return *this;
	} //if

	bool diff_check = (n == 0) ? false : true;
	auto sF = _tcslen(pstrFrom);
	auto sT = _tcslen(pstrTo);
	if (sF == sT) {
		this->pre_allocate(this->GetLength());

		TCHAR *sL = this->m_pstr + begin;
		TCHAR *sP = sL - sF;
		while ((sP = _tcsistr(sP + sF, pstrFrom)) != nullptr) {
			memcpy(sP, pstrTo, sT * sizeof(TCHAR));

			if (replace_count != nullptr) {
				++(*replace_count);
			} //if

			if (diff_check) {
				if (n == 1) break;
				--n;
			} //if
		}

		return *this;
	} //if

	RLIB_DEFER(String, tstr);
	if (sF < sT) {
		intptr_t affected_length   = this->GetLength() - begin;
		intptr_t max_replace_count = static_cast<intptr_t>(affected_length / sF) + 1;
		RLIB_DEFER_INIT(&tstr, String(affected_length + max_replace_count * (sT - sF) + begin));
	} else {
		RLIB_DEFER_INIT(&tstr, String(this->GetLength()));
	} //if

	if (begin != 0) {
		tstr.append(this->GetConstData(), begin - 0);
	} //if

	TCHAR *sL = this->m_pstr + begin;
	TCHAR *sP = sL - sF;
	intptr_t sO = begin;
	while ((sP = _tcsistr(sP + sF, pstrFrom)) != nullptr) {
		if ((sP - sL) > 0) {
			tstr.append(&this->m_pstr[sO], sP - sL).append(pstrTo, sT);
		} else {
			tstr.append(pstrTo, sT);
		} //if
		sO = (sP - this->m_pstr) + sF;
		sL = sP + sF;

		if (replace_count != nullptr) {
			++(*replace_count);
		} //if

		if (diff_check) {
			if (n == 1) break;
			--n;
		} //if
	}

	if (sO < this->m_len) {
		tstr.append(this->GetConstData() + sO, this->GetLength() - sO);
	} //if

	*this = tstr;
	RLIB_DEFER_DESTROY(String, &tstr);

	return *this;
}

//-------------------------------------------------------------------------

String String::Substring(intptr_t nIndex, intptr_t nLen /* = -1*/) const
{
	if (nIndex >= this->GetLength() || nLen == 0) {
		return Nothing;
	} //if

	if (nLen > (this->m_len - nIndex) || nLen < 0) {
		nLen = this->m_len - nIndex;
	} //if

	// nothing happens
	if (nLen == this->m_len) return *this;

	return String(nLen).copy(&this->m_pstr[nIndex], nLen);
}

//-------------------------------------------------------------------------

String &String::substring(intptr_t nIndex, intptr_t nLen /* = -1*/)
{
	if (nIndex > this->GetLength() || nLen == 0) {
		return *this;
	} //if

	this->pre_allocate(this->GetLength(), true);
	
	if (nIndex != this->m_len) {
		if (nLen > (this->m_len - nIndex) || nLen < 0) {
			nLen = this->m_len - nIndex;
		} //if

		if (nIndex > 0) {
			memmove(this->m_pstr, &this->m_pstr[nIndex], nLen * sizeof(TCHAR));
		} //if
	} else {
		assert(nLen <= 0);
		nLen = 0;
	} //if

	this->m_pstr[nLen] = _C('\0');
	this->m_len        = nLen;
	return *this;
}

//-------------------------------------------------------------------------

intptr_t String::CountOf(const TCHAR *s, intptr_t begin /* = 0*/) const
{
	intptr_t count = 0, len = _tcslen(s);
	if (len > begin) {
		const TCHAR *p = this->GetConstData() + begin;
		while ((p = _tcsstr(p, s)) != NULL) {
			p += len;
			++count;
		}
	} //if
	return count;
}

//-------------------------------------------------------------------------

intptr_t String::CountOfNoCase(const TCHAR *s, intptr_t begin /* = 0*/) const
{
	intptr_t count = 0, len = _tcslen(s);
	if (len > begin) {
		const TCHAR *p = this->GetConstData() + begin;
		while ((p = _tcsistr(p, s)) != NULL) {
			p += len;
			++count;
		}
	} //if
	return count;
}

//-------------------------------------------------------------------------

template<bool kCase>
static String __Match_impl(const TCHAR *p1, const TCHAR *p2, intptr_t begin_offset,
						   const String *__this)
{
	intptr_t begin = kCase ? __this->IndexOfR(p1, begin_offset) : __this->IndexOfRNoCase(p1, begin_offset);
	if (begin != -1) {
		intptr_t end = kCase ? __this->IndexOf(p2, begin) : __this->IndexOfNoCase(p2, begin);
		if (end != -1) 
			return __this->Substring(begin, end - begin);
	} //if
	return Nothing;
}

//-------------------------------------------------------------------------

String String::Match(const TCHAR *p1, const TCHAR *p2, intptr_t begin_offset /* = 0 */) const
{
	return __Match_impl<true>(p1, p2, begin_offset, this);
}

//-------------------------------------------------------------------------

String String::MatchNoCase(const TCHAR *p1, const TCHAR *p2, intptr_t begin_offset /* = 0 */) const
{
	return __Match_impl<false>(p1, p2, begin_offset, this);
}

//-------------------------------------------------------------------------

template<bool kCase>
static String __MatchReplace_impl(const TCHAR *p1, const TCHAR *p2, const TCHAR *replaceTo, 
								  intptr_t begin, const String *__this)
{
	begin = kCase ? __this->IndexOf(p1, begin) : __this->IndexOfNoCase(p1, begin);
	if (begin == -1) return *__this;

	intptr_t end = begin + _tcslen(p1);
	end = kCase ? __this->IndexOfR(p2, end) : __this->IndexOfRNoCase(p2, end);
	if (end == -1) return *__this;

	if (replaceTo[0] != _C('\0')) {
		intptr_t len = _tcslen(replaceTo);
		return String(begin + len + (__this->GetLength() - end))
			.append(__this->GetConstData(), begin - 0)
			.append(replaceTo, len)
			.append(__this->GetConstData() + end, (__this->GetLength() - end));
	} else {
		return String(begin + (__this->GetLength() - end))
			.append(__this->GetConstData(), begin - 0)
			.append(__this->GetConstData() + end, (__this->GetLength() - end));
	} //if	
}

//-------------------------------------------------------------------------

String String::MatchReplace(const TCHAR *p1, const TCHAR *p2,
							const TCHAR *replaceTo, intptr_t begin /* = 0 */) const
{
	return __MatchReplace_impl<true>(p1, p2, replaceTo, begin, this);
}

//-------------------------------------------------------------------------

String String::MatchReplaceNoCase(const TCHAR *p1, const TCHAR *p2,
								  const TCHAR *replaceTo, intptr_t begin /* = 0 */) const
{
	return __MatchReplace_impl<false>(p1, p2, replaceTo, begin, this);
}

//-------------------------------------------------------------------------

template<bool kCase>
static String __MatchReplace_impl(const TCHAR *p1, const TCHAR *p2, String::MatchCallback callback,
								  intptr_t begin, const String *__this)
{
	if (begin > __this->GetLength() || begin < 0) return *__this;

	bool matchany = false;
	auto lpdata   = __this->GetConstData() + begin;
	intptr_t len1 = _tcslen(p1);
	intptr_t len2 = _tcslen(p2);
	RLIB_DEFER(String, szBuffer);
	String r;
	do 
	{
		auto lpbegin = kCase ? _tcsstr(lpdata, p1) : _tcsistr(lpdata, p1);
		if (lpbegin == nullptr) break;

		auto lpend = kCase ? _tcsstr(lpbegin + len1, p2) : _tcsistr(lpbegin + len1, p2);
		if (lpend == nullptr) break;

		r = callback(lpbegin + len1, lpend);

		if (!matchany) {
			matchany = true;
			RLIB_DEFER_INIT(&szBuffer, String(__this->GetLength()));
			szBuffer.copy(lpdata, lpbegin - lpdata);		
		} else {
			szBuffer.append(lpdata, lpbegin - lpdata);
		} //if	

		szBuffer.append(r);
		lpdata = lpend + len2;
	} while (*lpdata != _C('\0'));
	
	if (matchany) {
		szBuffer.append(lpdata, _tcslen(lpdata));
		r = szBuffer;
		RLIB_DEFER_DESTROY(String, &szBuffer);
		szBuffer.~String();
		return r;
	} //if

	return *__this;
}

//-------------------------------------------------------------------------

String String::MatchReplace(const TCHAR *p1, const TCHAR *p2, 
							MatchCallback callback, intptr_t begin /* = 0 */) const
{
	return __MatchReplace_impl<true>(p1, p2, callback, begin, this);
}

//-------------------------------------------------------------------------

String String::MatchReplaceNoCase(const TCHAR *p1, const TCHAR *p2, 
								  MatchCallback callback, intptr_t begin /* = 0 */) const
{
	return __MatchReplace_impl<false>(p1, p2, callback, begin, this);
}

//-------------------------------------------------------------------------

#if _HAS_JNI_EXTENSIONS

String::String(JNIEnv *const env, const jstring jstr)
{
	const char *utfchars = env->GetStringUTFChars(jstr, NULL);
	if (__predict_true(utfchars != NULL)) {
		RLIB_InitClass(this, String(const_cast<char *>(utfchars))); // disables reference
		env->ReleaseStringUTFChars(jstr, utfchars);
	} else {
		env->ExceptionDescribe();
		env->ExceptionClear();
	} //if	
}

//-------------------------------------------------------------------------

String::String(JNIEnv *const env, const jobject jobj)
{
	ScopedLocalRef<jclass> java_lang_String(env, 
											env->FindClass("java/lang/String"));
	if (__predict_false(env->IsInstanceOf(jobj, java_lang_String))) {
		RLIB_InitClass(this, String(env, static_cast<const jstring>(jobj)));
	} else {
		ScopedLocalRef<jclass> object_class(env,
											env->GetObjectClass(jobj));
		jmethodID toString = env->GetMethodID(object_class, "toString", "()Ljava/lang/String;");
		if (__predict_false(toString == NULL)) {
			env->ExceptionClear();
			RLIB_InitClass(this, String(Nothing));
		} else {
			ScopedLocalRef<jstring> object_to_string(env,
													 env->CallObjectMethod(jobj, toString));
			RLIB_InitClass(this, String(env, object_to_string));
		} //if
	} //if
}

//-------------------------------------------------------------------------

jstring String::NewJString(JNIEnv *const env) const
{
	return env->NewStringUTF(this->GetConstData());
}

#endif // _HAS_JNI_EXTENSIONS
