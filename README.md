# ndk_string
An intuitive and easy to use C++ UTF-8 string class for ndk

# Features
- Copy-on-write
- Reference counting
- JNI `jstring` compatibility
- UTF-8 format

# Windows version
This implementation is from [RLib](https://github.com/rrrfff/RLib)

# Example
```C++
void string_test(JNIEnv *env)
{
	String a = _T("Hello, 中国!");
	DEBUG_DUMPSTR(a);
	DEBUG_DUMPVAL(a.Length);

	a.replace(_T("中国"), _T("中华人民共和国"));
	DEBUG_DUMPSTR(a);
	DEBUG_DUMPVAL(a.Length);

	// String to jstring
//	jstring js = a.NewJString(env);
	ScopedLocalRef<jstring> js(env, a.NewJString(env));
	// UTF-16
	DEBUG_DUMPVAL(env->GetStringLength(js));
	DEBUG_DUMPBYTES(env->GetStringChars(js, NULL), env->GetStringLength(js) * sizeof(jchar));
	// UTF-8
	DEBUG_DUMPVAL(env->GetStringUTFLength(js));
	DEBUG_DUMPSTR(env->GetStringUTFChars(js, NULL));

	// jstring to String
	String b(env, js);
	a += b;
	DEBUG_DUMPSTR(a);
	DEBUG_DUMPVAL(a.Length);
}
```
string_test: a = Hello, 中国! (at line 13)   
string_test: a.Length = 0xe (at line 14)   
string_test: a = Hello, 中华人民共和国! (at line 17)   
string_test: a.Length = 0x1d (at line 18)   
string_test: env->GetStringLength(js) = 0xf (at line 23)   
string_test: dumping 0xe888a1a0 (env->GetStringChars(js, NULL)) with 30 bytes...   
string_test: 48 00 65 00 6C 00 6C 00 6F 00 2C 00   
string_test: 20 00 2D 4E 4E 53 BA 4E 11 6C 71 51   
string_test: 8C 54 FD 56 21 00   
string_test: env->GetStringUTFLength(js) = 0x1d (at line 27)   
string_test: env->GetStringUTFChars(js, NULL) = Hello, 中华人民共和国! (at line 28)   
string_test: a = Hello, 中华人民共和国!Hello, 中华人民共和国! (at line 33)   
string_test: a.Length = 0x3a (at line 34)   
