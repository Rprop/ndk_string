/*
 *
 *  @author : rrrfff@foxmail.com
 *  https://github.com/rrrfff/ndk_string
 *
 */
#pragma once
#include <string.h>
#include <ctype.h>
#include <inttypes.h>

#ifndef TCHAR
typedef char TCHAR;
/* Generic text macros to be used with string literals and character constants.
Will also allow symbolic constants that resolve to same. */
# define __T(x)       u8 ## x
# define _T(x)        __T(x)
# define _C(x)        x
# define _R(x)        (System::STRInfo { _T(x), static_cast<intptr_t>(sizeof(_T(x)) - sizeof(TCHAR)) })
# define _tcslen      strlen
# define _tcschr      strchr
# define _tcsrchr     strrchr
# define _tcsstr      strstr
# define _tcsistr     strcasestr
# define _tcscmp      strcmp
# define _tcsncmp     strncmp
# define _tcsicmp     strcasecmp
# define _tcsnicmp    strncasecmp
# define _istspace    isspace
# define _sntprintf   snprintf
# define _vsntprintf  vsnprintf
#endif // !TCHAR

#ifndef _HAS_JNI_EXTENSIONS
# define _HAS_JNI_EXTENSIONS __has_include(<jni.h>)
#endif // !_HAS_JNI_EXTENSIONS

#if _HAS_JNI_EXTENSIONS
# include <jni.h>
#endif // _HAS_JNI_EXTENSIONS

#ifndef _HAS_MS_EXTENSIONS
# define _HAS_MS_EXTENSIONS 1 // -fms-extensions
#endif // !_HAS_MS_EXTENSIONS

#if _HAS_MS_EXTENSIONS
# define RLIB_PROPERTY_GET(a, g)        __declspec(property(get = g)) a
# define RLIB_PROPERTY_SET(a, p)        __declspec(property(put = p)) a
# define RLIB_PROPERTY_GET_SET(a, g, p) __declspec(property(get = g, put = p)) a
#else
# define RLIB_PROPERTY_GET(a, g)
# define RLIB_PROPERTY_SET(a, p)
# define RLIB_PROPERTY_GET_SET(a, g, p)
#endif // _HAS_MS_EXTENSIONS

//-------------------------------------------------------------------------

#if	_HAS_JNI_EXTENSIONS
template<typename T> class ScopedLocalRef
{
public:
	ScopedLocalRef(JNIEnv *env, T localRef) : mEnv(env), mLocalRef(localRef) {
		this->ensureNoThrow();
	}
	ScopedLocalRef(JNIEnv *env, const void *localRef) : ScopedLocalRef(env, static_cast<T>(const_cast<void *>(localRef))) {}

	~ScopedLocalRef() {
		this->reset();
	}

	void ensureNoThrow() {
		if (this->mLocalRef == NULL)this->mEnv->ExceptionDescribe(), this->mEnv->ExceptionClear();
	}

	void reset(T ptr = nullptr) {
		if (ptr != this->mLocalRef) {
			if (this->mLocalRef != nullptr) {
				this->mEnv->DeleteLocalRef(this->mLocalRef);
			} //if
			this->mLocalRef = ptr;
			this->ensureNoThrow();
		} //if
	}

	T release() __attribute__((warn_unused_result)) {
		T localRef = mLocalRef;
		mLocalRef  = nullptr;
		return localRef;
	}

	T get() const {
		return mLocalRef;
	}

	operator T () const {
		return mLocalRef;
	}

private:
	JNIEnv *const mEnv;
	T mLocalRef;
};
#endif // _HAS_JNI_EXTENSIONS

//-------------------------------------------------------------------------

namespace System
{
	enum STRNull {
		Nothing
	};

	struct STRInfoA {
		const char *lpstr;
		intptr_t    length;
	};
	typedef STRInfoA STRInfo;

	class String
	{
	protected:
		TCHAR           *m_pstr;
		mutable intptr_t m_len;
		intptr_t         m_size;

	protected:
		bool is_releasable() const;
		bool is_writable() const;
		void release_local_data();	
		void pre_allocate(intptr_t length, bool copyval = true);
		void update_reference(TCHAR *pstr/* must be releaseable */, intptr_t size,
							  intptr_t length);
		TCHAR &get_char(intptr_t index);   
		const TCHAR &get_const_char(intptr_t index) const;
		struct InternalString *get_internal_string() const;
		static struct InternalString *allocate_internal_string(intptr_t size);

	public:
		static TCHAR nullpstr[1];
		static const TCHAR *nullpcstr;

	public:
		String();
		String(STRNull) : String() {}
		explicit String(intptr_t length);
		String(TCHAR *, intptr_t length = -1);
		String(const STRInfoA &);
		String(const TCHAR *, intptr_t length = -1);
		String(const String &); 
		String(String &&tmpstr);
		~String();
//		RLIB_DECLARE_DYNCREATE;

#if _HAS_JNI_EXTENSIONS
	public:
		String(JNIEnv *const env, const jstring jstr);
		String(JNIEnv *const env, const jobject jobj);
		jstring NewJString(JNIEnv *const env) const;
#endif // _HAS_JNI_EXTENSIONS

	public:
		String Reserve(intptr_t length);
		String &reserve(intptr_t length, bool keep = true);

	public:
		operator TCHAR *();    
		__inline operator const TCHAR *() {
			return this->GetConstData();
		}
		__inline operator const TCHAR *() const {
			return this->GetConstData();
		}
		String &operator = (STRNull);  
		String &operator = (TCHAR *);
		String &operator = (const STRInfoA &);
		String &operator = (const TCHAR *);
		String &operator = (const String &); 
		String &operator = (String &&tmpstr);

	public:
		RLIB_PROPERTY_GET(const intptr_t Size, GetSize);
		RLIB_PROPERTY_GET(const intptr_t Length, GetLength);
		RLIB_PROPERTY_GET(const intptr_t CanReadSize, GetCanReadSize);
		intptr_t GetSize() const;  
		intptr_t GetLength() const;   
		intptr_t GetCanReadSize() const; 
		TCHAR *GetData();
		const TCHAR *GetConstData() const;
		TCHAR *GetType() const { return nullptr; }
		TCHAR *c_str() const;              
		TCHAR GetAt(intptr_t) const; 
		void SetAt(intptr_t, TCHAR);

	public:
		bool tryCopy(const TCHAR *pstr, intptr_t len);
		String &copy(const TCHAR *pstr, intptr_t len = -1);
		String &copy(const String &str, intptr_t len = -1);
		String &__cdecl copyf(const TCHAR *pstrFormat, ...) __printflike(2, 3);
		String &append(const TCHAR c);
		String &append(const TCHAR *pstr, intptr_t len = -1);
		String &append(const String &str, intptr_t len = -1);
		String &__cdecl appendf(const TCHAR *pstrFormat, ...) __printflike(2, 3);
		void CopyTo(TCHAR *pstr, intptr_t max_length_with_null);

	public:
		TCHAR front() const;
		TCHAR back() const;
		bool StartsWith(TCHAR) const;
		bool EndsWith(TCHAR) const;
		bool StartsWith(const TCHAR *pstr, intptr_t length = -1) const;
		bool EndsWith(const TCHAR *pstr, intptr_t length = -1) const;
		bool Contains(const TCHAR *value) const;
		bool Contains(TCHAR c) const;
		bool ContainsNoCase(const TCHAR *value) const;
		intptr_t Compare(const TCHAR *) const;
		intptr_t Compare(const TCHAR *, intptr_t n) const;
		intptr_t CompareNoCase(const TCHAR *) const;
		intptr_t CompareNoCase(const TCHAR *, intptr_t n) const;
		String &empty();
		bool IsConst() const; 
		bool IsEmpty() const;
		bool IsNull() const;
		bool IsNullOrEmpty() const;
		intptr_t IndexOf(TCHAR, intptr_t begin = 0) const;     
		intptr_t LastIndexOf(TCHAR) const; 
		intptr_t IndexOf(const TCHAR *, intptr_t begin = 0) const;   
		intptr_t LastIndexOf(const TCHAR *) const;
		intptr_t LastIndexOfL(const TCHAR *, intptr_t len) const;
		intptr_t IndexOfNoCase(const TCHAR *, intptr_t begin = 0) const;
		intptr_t LastIndexOfNoCase(const TCHAR *) const;
		intptr_t LastIndexOfLNoCase(const TCHAR *, intptr_t len) const;
		intptr_t IndexOfR(const TCHAR *, intptr_t begin = 0) const;   
		intptr_t IndexOfRL(const TCHAR *, intptr_t len, intptr_t begin = 0) const;
		intptr_t LastIndexOfR(const TCHAR *) const;
		intptr_t LastIndexOfRL(const TCHAR *, intptr_t len) const;
		intptr_t IndexOfRNoCase(const TCHAR *, intptr_t begin = 0) const;
		intptr_t IndexOfRLNoCase(const TCHAR *, intptr_t len, intptr_t begin = 0) const;
		intptr_t LastIndexOfRNoCase(const TCHAR *) const;
		intptr_t LastIndexOfRLNoCase(const TCHAR *, intptr_t len) const;
		intptr_t CountOf(const TCHAR *, intptr_t begin = 0) const;
		intptr_t CountOfNoCase(const TCHAR *, intptr_t begin = 0) const;
		String Concat(const TCHAR *, intptr_t len = -1) const;  
		String Concat(const String &, intptr_t len = -1) const; 
		String Reverse() const;
		String &reverse();
		String ToLower() const;
		String &toLower();
		String ToUpper() const;
		String &toUpper();
		String Trim(TCHAR c = 0) const;
		String &trim(TCHAR c = 0);
		String TrimStart(TCHAR c = 0) const;  
		String &trimStart(TCHAR c = 0);
		String TrimEnd(TCHAR c = 0) const;      
		String &trimEnd(TCHAR c = 0);
		String PadRight(intptr_t totalWidth, TCHAR paddingChar = _C(' ')) const;
		String &padRight(intptr_t totalWidth, TCHAR paddingChar = _C(' '));
		String PadLeft(intptr_t totalWidth, TCHAR paddingChar = _C(' ')) const;
		String &padLeft(intptr_t totalWidth, TCHAR paddingChar = _C(' '));
		String Replace(const TCHAR *pstrFrom, const TCHAR *pstrTo, intptr_t begin = 0, intptr_t n = 0,
					   intptr_t *replace_count = nullptr) const; 
		String ReplaceNoCase(const TCHAR *pstrFrom, const TCHAR *pstrTo, intptr_t begin = 0, intptr_t n = 0,
							 intptr_t *replace_count = nullptr) const;
		String &replace(const TCHAR *pstrFrom, const TCHAR *pstrTo, intptr_t begin = 0, intptr_t n = 0,
						intptr_t *replace_count = nullptr);
		String &replaceNoCase(const TCHAR *pstrFrom, const TCHAR *pstrTo, intptr_t begin = 0, intptr_t n = 0,
							  intptr_t *replace_count = nullptr);
		String Substring(intptr_t index, intptr_t length = -1) const;
		String &substring(intptr_t index, intptr_t length = -1);
//		class StringArray *Split(const String &strSeparator, bool removeEmptyEntries = false) const;
//		class StringArray *Split(const TCHAR *strSeparator, intptr_t lengthOfSeparator,
//								 intptr_t averageItemLength, bool removeEmptyEntries = false) const;
//		class StringSplitArray *FastSplit(const TCHAR *strSeparator, intptr_t lengthOfSeparator,
//										  intptr_t averageItemLength);

	public: // simple string matching, not regular expression support
		typedef String(*__match_callback)(const TCHAR *lpbegin, const TCHAR *lpend);
		typedef __match_callback MatchCallback;
		String Match(const TCHAR *, const TCHAR *, intptr_t begin = 0) const;
		String MatchReplace(const TCHAR *, const TCHAR *, const TCHAR *replaceTo, intptr_t begin = 0) const;
		String MatchReplace(const TCHAR *, const TCHAR *, MatchCallback callback, intptr_t begin = 0) const;
//		class StringArray *MatchAll(const TCHAR *, const TCHAR *, intptr_t begin = 0) const;
		String MatchNoCase(const TCHAR *, const TCHAR *, intptr_t begin = 0) const;
		String MatchReplaceNoCase(const TCHAR *, const TCHAR *, const TCHAR *replaceTo, intptr_t begin = 0) const;
		String MatchReplaceNoCase(const TCHAR *, const TCHAR *, MatchCallback callback, intptr_t begin = 0) const;
//		StringArray *MatchAllNoCase(const TCHAR *, const TCHAR *, intptr_t begin = 0) const;

	public:
		TCHAR &operator [] (intptr_t); 
		const TCHAR &operator [] (intptr_t) const; 
		String &operator += (const TCHAR c);
		String &operator += (const TCHAR *);
		String &operator += (const String &);
		__inline String &operator += (const STRInfo &si) {
			return this->append(si.lpstr, si.length);
		}
		bool operator == (const TCHAR *) const;
		bool operator == (const String &) const;
		bool operator != (const TCHAR *) const;
		bool operator != (const String &) const;
		bool operator <= (const TCHAR *) const;
		bool operator <= (const String &) const;
		bool operator <  (const TCHAR *) const;
		bool operator <  (const String &) const;
		bool operator >= (const TCHAR *) const;
		bool operator >= (const String &) const;
		bool operator >  (const TCHAR *) const;
		bool operator >  (const String &) const;

	public:
		template <typename T> String &appendCompose(const T &t) {
			return this->append(t);
		}
		template <typename T, typename ... Args> String &appendCompose(const T &t, const Args& ... args) {
//			auto count = sizeof...(args) + 1;
			return this->append(t).appendCompose(args...);
		}

	public:
		static void Collect(const void *);

	public: // iteration
		TCHAR *begin() /* noexcept */ {
			return this->GetLength() > 0 ? this->m_pstr : nullptr;
		}
		const TCHAR *begin() const  /* noexcept */ {
			return this->GetLength() > 0 ? this->m_pstr : nullptr;
		}
		TCHAR *end() /* noexcept */ {
			return this->GetLength() > 0 ? this->m_pstr + this->GetLength() : nullptr;
		}
		const TCHAR *end() const /* noexcept */ {
			return this->GetLength() > 0 ? this->m_pstr + this->GetLength() : nullptr;
		}
	};
};

//-------------------------------------------------------------------------

template <typename T> static System::String StringCompose(const T &t) {
	return t;
}
template <typename T, typename ... Args> static System::String StringCompose(const T &t, const Args& ... args) {
	return t + StringCompose(args...);
}

//-------------------------------------------------------------------------

extern System::String operator + (const System::String &, const TCHAR *);
extern System::String operator + (const System::String &, const System::STRInfo &);
extern System::String operator + (const System::String &, const System::String &);