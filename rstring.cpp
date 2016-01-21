// rstring
// A fast, reference counted, copy-on-write string class (c) Espen Harlinn

#include "rstring.h"
#include "routine.h"

LPWSTR rstring::empty = L"";

rstring::rstring () : data_ (nullptr)
{}

rstring::rstring (const rstring& other) : data_ (nullptr)
{
	if (other.data_)
	{
		data_ = other.data_;
		Buffer* buffer = toBuffer ();
		InterlockedIncrement64 (&buffer->referenceCount);
	}
}

rstring::rstring (rstring&& other) : data_ (nullptr)
{
	data_ = other.data_;
	other.data_ = nullptr;
}

rstring::rstring (LPCWSTR str) : data_ (nullptr)
{
	if (str)
	{
		size_t length = wcslen (str);
		if (length)
		{
			ReallocateUnique (length);
			wmemcpy_s (data_, length, str, length);
		}
	}
}

rstring::rstring (LPCSTR str) : data_ (nullptr)
{
	size_t length = strlen (str);
	if (length)
	{
		ReallocateUnique (length);
		MultiByteToWideChar (CP_ACP, 0, str, length, data_, length);
	}
}

rstring::rstring (LPCWSTR str1, size_t length1, LPCWSTR str2, size_t length2) : data_ (nullptr)
{
	size_t totalLength = length1 + length2;
	if (totalLength)
	{
		ReallocateUnique (totalLength);
		if (str1)
		{
			wmemcpy (data_, str1, length1);
		}
		else
		{
			wmemset (data_, 0, length1);
		}

		if (str2)
		{
			wmemcpy (&data_[length1], str2, length2);
		}
		else
		{
			wmemset (data_ + length1, 0, length2);
		}
	}
}

rstring::rstring (LPCWSTR str, size_t length) : data_ (nullptr)
{
	if (length)
	{
		ReallocateUnique (length);
		if (str)
		{
			wmemcpy (data_, str, length);
		}
		else
		{
			wmemset (data_, 0, length);
		}
	}
}

rstring::~rstring ()
{
	if (data_)
	{
		Release ();
	}
}

rstring::operator LPCWSTR () const
{
	return this->GetString ();
}

rstring::operator bool () const
{
	return GetLength () != 0;
}

bool rstring::operator== (const rstring& str) const
{
	return this->Compare (str) == 0;
}

bool rstring::operator!= (const rstring& str) const
{
	return this->Compare (str) != 0;
}

bool rstring::operator== (LPCWSTR str) const
{
	return this->Compare (str) == 0;
}

bool rstring::operator!= (LPCWSTR str) const
{
	return this->Compare (str) != 0;
}

WCHAR rstring::operator[] (const size_t index) const
{
	return At (index);
}

WCHAR& rstring::operator[] (const size_t index)
{
	return At (index);
}

rstring& rstring::operator= (const rstring& other)
{
	AddRef (other);
	return *this;
}

rstring& rstring::operator= (LPCWSTR str)
{
	Release ();
	if (str != data_)
	{
		Buffer* thisBuffer = toBuffer ();
		if (thisBuffer && str >= data_ && str < data_ + thisBuffer->length)
		{
			size_t offset = str - data_;
			size_t length = thisBuffer->length - offset;

			EnsureUnique ();
			str = data_ + offset;
			memmove (data_, str, length*sizeof (wchar_t));
			ReallocateUnique (length);
		}
		else
		{
			size_t length = wcslen (str);
			thisBuffer = ReallocateUnique (length);
			wmemcpy (data_, str, length);
		}
	}
	return *this;
}

rstring& rstring::operator= (rstring&& other)
{
	if (this != &other)
	{
		data_ = other.data_;
		other.data_ = nullptr;
	}
	return *this;
}

rstring& rstring::operator+= (const rstring& other)
{
	return this->Append (other);
}

rstring& rstring::operator+= (LPCWSTR str)
{
	return this->Append (str);
}

rstring operator+ (const rstring& str1, const rstring& str2)
{
	return str1.Appended (str2);
}

rstring operator+ (const rstring& str1, LPCWSTR str2)
{
	return str1.Appended (str2);
}

WCHAR& rstring::At (size_t index) const
{
	Buffer* theBuffer = toBuffer ();

	return theBuffer->data[min (index, theBuffer->length)];
}

size_t rstring::Hash () const
{
	size_t result = 0;
	if (data_)
	{
		auto theBuffer = toBuffer ();
		switch (theBuffer->length)
		{
			case 0:
				break;
			case 1:
				result = *(unsigned char*)data_;
				break;
			case 2:
			case 3:
				result = *(unsigned short*)data_;
				break;
			case 4:
			case 5:
			case 6:
			case 7:
				result = *(unsigned int*)data_;
				break;
			default:
				result = *(size_t*)data_;
				break;
		}

	}
	return result;
}

rstring rstring::Appended (const rstring& other) const
{
	Buffer* thisBuffer = toBuffer ();
	Buffer* otherBuffer = other.toBuffer ();
	if (thisBuffer && otherBuffer)
	{
		rstring result (thisBuffer->data, thisBuffer->length, otherBuffer->data, otherBuffer->length);
		return result;
	}
	else if (thisBuffer)
	{
		rstring result (*this);
		return result;
	}
	return other;
}

rstring rstring::Appended (LPCWSTR str) const
{
	Buffer* thisBuffer = toBuffer ();
	if (thisBuffer && str && str[0])
	{
		rstring result (thisBuffer->data, thisBuffer->length, str, wcslen (str));
		return result;
	}
	else if (thisBuffer)
	{
		rstring result (*this);
		return result;
	}
	else if (str && str[0])
	{
		return rstring (str);
	}
	return rstring ();
}

rstring& rstring::Append (const rstring& other)
{
	Buffer* otherBuffer = other.toBuffer ();
	if (otherBuffer && otherBuffer->length)
	{
		Buffer* thisBuffer = toBuffer ();
		if (!thisBuffer)
		{
			data_ = otherBuffer->data;
			InterlockedIncrement64 (&otherBuffer->referenceCount);
		}
		else
		{
			size_t oldLength = thisBuffer->length;
			if (thisBuffer == otherBuffer)
			{
				thisBuffer = ReallocateUnique (thisBuffer->length + thisBuffer->length);
				wmemcpy (&thisBuffer->data[oldLength], thisBuffer->data, oldLength);
			}
			else
			{
				thisBuffer = ReallocateUnique (thisBuffer->length + otherBuffer->length);
				wmemcpy (&thisBuffer->data[oldLength], otherBuffer->data, otherBuffer->length);
			}
		}
	}
	return *this;
}

rstring&rstring::Append (LPCWSTR str)
{
	if (str && str[0])
	{
		Buffer* thisBuffer = toBuffer ();
		if (!thisBuffer)
		{
			size_t length = wcslen (str);
			if (length)
			{
				ReallocateUnique (length);
				wmemmove_s (data_, length, str, length);
			}
		}
		else if (str == data_)
		{
			size_t oldLength = thisBuffer->length;

			thisBuffer = ReallocateUnique (thisBuffer->length + thisBuffer->length);
			wmemmove_s (&thisBuffer->data[oldLength], oldLength, thisBuffer->data, oldLength);
		}
		else if (str > data_ && str < (data_ + thisBuffer->length))
		{
			size_t offset = str - data_;
			size_t length = thisBuffer->length - offset;

			size_t oldLength = thisBuffer->length;


			thisBuffer = ReallocateUnique (thisBuffer->length + length);
			str = data_ + offset;
			wmemmove_s (&thisBuffer->data[oldLength], length, str, length);
		}
		else
		{
			size_t length = wcslen (str);
			Buffer* thisBuffer = toBuffer ();
			size_t oldLength = thisBuffer->length;

			thisBuffer = ReallocateUnique (thisBuffer->length + length);
			wmemmove_s (&thisBuffer->data[oldLength], length, str, length);
		}
	}
	return *this;
}

INT rstring::Compare (const rstring& str) const
{
	return _Compare (toBuffer (), str.toBuffer ());
}

INT rstring::Compare (LPCWSTR str) const
{
	return _Compare (toBuffer (), str);
}

INT rstring::CompareNoCase (const rstring& str) const
{
	return _CompareI (toBuffer (), str.toBuffer ());
}

INT rstring::CompareNoCase (LPCWSTR str) const
{
	return _CompareI (toBuffer (), str);
}

rstring rstring::Mid (size_t start, size_t length) const
{
	rstring result;

	size_t clength = GetLength ();

	if (clength)
	{
		if ((start == 0) && (length >= clength))
		{
			return GetString ();
		}
		if (start >= clength)
		{
			return empty;
		}
		if (length > (clength - start))
		{
			length = clength - start;
		}
		memmove (result.GetBuffer (length), &data_[start], (length * sizeof (WCHAR)));
	}
	return result;
}

DWORD rstring::AsInt (INT radix) const
{
	if (this->IsEmpty ())
		return 0;

	return wcstoul (GetString (), nullptr, radix);
}

rstring::vector rstring::AsVector (LPCWSTR delimiters) const
{
	rstring::vector result;

	size_t theSize = GetLength ();

	if (theSize)
	{
		LPCWSTR start = data_;
		LPCWSTR end = start;
		LPCWSTR thisEnd = start + theSize;

		while (end < thisEnd)
		{
			LPCWSTR ptr = delimiters;
			while (*ptr)
			{
				if (*end == *ptr)
				{
					if (start < end)
					{
						result.emplace_back (start, size_t (end - start));
					}
					start = end + 1;
					break;
				}
				ptr++;
			}
			end++;
		}
		if (start < end)
		{
			result.emplace_back (start, size_t (end - start));
		}
	}
	return std::move (result);
}

rstring& rstring::Clear ()
{
	Release ();
	return *this;
}

bool rstring::IsEmpty () const
{
	return GetLength () == 0;
}

LPWSTR rstring::GetBuffer (size_t newLength)
{
	EnsureUnique ();
	if (newLength)
	{
		Release ();
		SetLength (newLength);
	}
	else if (!data_)
	{
		return empty;
	}
	return data_;
}

void rstring::ReleaseBuffer ()
{
	size_t length = wcslen (data_);
	if (!length)
	{
		Release ();
	}
	else
	{
		SetLength (length);
	}
}

LPCWSTR rstring::GetString () const
{
	if (!data_)
	{
		return empty;
	}
	return data_;
}

size_t rstring::GetLength () const
{
	Buffer* buffer = toBuffer ();
	if (buffer)
	{
		return buffer->length;
	}
	return 0;
}

rstring& rstring::SetLength (size_t newLength)
{
	ReallocateUnique (newLength);

	return *this;
}

rstring& rstring::Format (LPCWSTR str, ...)
{
	va_list args = nullptr;
	va_start (args, str);

	FormatV (str, args);

	va_end (args);

	return *this;
}

rstring& rstring::FormatV (LPCWSTR fmt, va_list args)
{
	size_t length = _vscwprintf (fmt, args);

	StringCchVPrintf (this->GetBuffer (length), length + 1, fmt, args);

	return *this;
}

rstring& rstring::Replace (LPCWSTR from, LPCWSTR to)
{
	LPWSTR tok = nullptr;
	LPWSTR newstr = nullptr;
	LPWSTR oldstr = nullptr;
	LPWSTR head = nullptr;

	size_t from_len = wcslen (from);
	size_t to_len = wcslen (to);

	/* if either substr or replacement is NULL, duplicate string a let caller handle it */
	if (from == nullptr || to == nullptr) return *this;
	newstr = _wcsdup (GetString ());
	head = newstr;
	while ((tok = wcsstr (head, from)))
	{
		size_t orig_l = wcslen (newstr);
		oldstr = newstr;
		newstr = (LPWSTR)malloc ((orig_l - from_len + to_len + 1) * sizeof (WCHAR));
		/*failed to alloc mem, free old string and return NULL */
		if (newstr == NULL)
		{
			free (oldstr);
			return *this;
		}
		memcpy (newstr, oldstr, (tok - oldstr) * sizeof (WCHAR));
		memcpy (newstr + (tok - oldstr), to, to_len * sizeof (WCHAR));
		memcpy (newstr + (tok - oldstr) + to_len, tok + from_len, (orig_l - from_len - (tok - oldstr)) * sizeof (WCHAR));
		memset (newstr + orig_l - from_len + to_len, 0, 1 * sizeof (WCHAR));
		/* move back head right after the last replacement */
		head = newstr + (tok - oldstr) + to_len;
		free (oldstr);
	}

	//str.GetLength () + 1

	*this = newstr;

	return *this;
}

rstring& rstring::Trim (LPCWSTR chars)
{
	size_t start_pos = 0;
	size_t end_pos = 0;
	size_t chars_len = wcslen (chars);
	Buffer* thisBuffer = toBuffer ();
	if (thisBuffer && chars && chars_len)
	{
		// calculate start position
		start_pos = 0;
		do
		{
			LPCWSTR p = wmemchr (chars, thisBuffer->data[start_pos], chars_len);
			if (!p || thisBuffer->length == start_pos)
			{
				if (start_pos)
					start_pos--;

				break;
			}
		}
		while (start_pos++);

		// calculate end position
		end_pos = thisBuffer->length - 1;
		do
		{
			LPCWSTR p = wmemchr (chars, thisBuffer->data[end_pos], chars_len);
			if (!p || end_pos == thisBuffer->length)
			{
				end_pos++;
				break;
			}
		}
		while (end_pos--);

		*this = this->Mid (start_pos, end_pos - start_pos);
	}
	return *this;
}

size_t rstring::Find (WCHAR chars, size_t start_pos) const
{
	Buffer* thisBuffer = toBuffer ();
	if (thisBuffer)
	{
		for (size_t i = start_pos; i <= GetLength (); i++)
		{
			if (towupper (thisBuffer->data[i]) == towupper (chars))
			{
				return i;
			}
		}
	}

	return npos;
}

size_t rstring::Find (LPCWSTR chars, size_t start_pos) const
{
	Buffer* thisBuffer = toBuffer ();
	size_t len = wcslen (chars);
	if (thisBuffer && chars && len)
	{
		while (start_pos < thisBuffer->length)
		{
			WCHAR c = towupper (thisBuffer->data[start_pos]);
			size_t i = 0;
			for (; i < len; i++)
			{
				WCHAR sc = towupper (chars[i]);
				if (c == sc)
				{
					break;
				}
			}
			if (i == len)
			{
				return start_pos;
			}
			start_pos++;
		}
	}
	return npos;
}

size_t rstring::ReverseFind (WCHAR chars, size_t start_pos) const
{
	Buffer* thisBuffer = toBuffer ();
	if (thisBuffer)
	{
		if (start_pos >= thisBuffer->length)
		{
			start_pos = thisBuffer->length - 1;
		}
		for (size_t i = start_pos; i != npos; i--)
		{
			if (towupper (thisBuffer->data[i]) == towupper (chars))
			{
				return i;
			}
		}
	}

	return npos;
}

size_t rstring::ReverseFind (LPCWSTR chars, size_t start_pos) const
{
	Buffer* thisBuffer = toBuffer ();
	size_t len = wcslen (chars);
	if (thisBuffer && len)
	{
		if (start_pos >= thisBuffer->length)
		{
			start_pos = thisBuffer->length - 1;
		}
		do
		{
			WCHAR c = towupper (thisBuffer->data[start_pos]);
			for (size_t i = 0; i < len; i++)
			{
				WCHAR sc = towupper (chars[i]);
				if (c == sc)
				{
					return start_pos;
				}
			}
		}
		while (start_pos--);
	}
	return npos;
}

bool rstring::Match (LPCWSTR pattern)
{
	return Match (data_, pattern);
}

bool rstring::Match (LPCWSTR str, LPCWSTR pattern)
{
	// If we reach at the end of both strings, we are done
	if (*pattern == 0 && *str == 0) { return TRUE; }

	// Make sure that the characters after '*' are present
	// in second string. This function assumes that the first
	// string will not contain two consecutive '*'
	if (*pattern == L'*' && *(pattern + 1) != 0 && *str == 0) { return FALSE; }

	// If the first string contains '?', or current characters
	// of both strings match
	if (*pattern == L'?' || towlower (*str) == towlower (*pattern)) { return Match (str + 1, pattern + 1); }

	// If there is *, then there are two possibilities
	// a) We consider current character of second string
	// b) We ignore current character of second string.
	if (*pattern == L'*') { return Match (str + 1, pattern) || Match (str, pattern + 1); }

	return FALSE;
}

rstring::Buffer* rstring::toBuffer () const
{
	if (data_)
	{
		return (Buffer*)(((char*)data_) - offsetof (Buffer, data));
	}
	return nullptr;
}

void rstring::AddRef (const rstring& other)
{
	if (other.data_ != data_)
	{
		Release ();
		if (other.data_)
		{
			data_ = other.data_;
			Buffer* buffer = toBuffer ();
			InterlockedIncrement64 (&buffer->referenceCount);
		}
	}
}

void rstring::Release ()
{
	Buffer* buffer = toBuffer ();
	if (buffer)
	{
		LONG64 res = InterlockedDecrement64 (&buffer->referenceCount);
		if (res == 0)
		{
			//char* bytes = (char*)buffer;
			delete[] buffer;
		}
		data_ = nullptr;
	}
}

size_t rstring::allocationByteCount (size_t length)
{
	if (length)
	{
		if (length < minReservedCharacters)
		{
			return minBufferByteCount;
		}

		size_t bytesRequired = (nonTextBufferByteCount+length)*sizeof (wchar_t);

		size_t remainder = bytesRequired % minBufferByteCount;
		if (remainder != 0)
		{
			bytesRequired += minBufferByteCount - remainder;
		}
		return bytesRequired;
	}
	return 0;
}

rstring::Buffer* rstring::allocate (size_t length)
{
	if (length)
	{
		size_t byteCount = allocationByteCount (length);
		Buffer* result = (Buffer*)new char[byteCount];
		result->referenceCount = 1;
		result->length = length;
		result->data[length] = 0;

		return result;
	}
	return nullptr;
}

rstring::Buffer* rstring::EnsureUnique ()
{
	Buffer* buffer = toBuffer ();
	if (buffer)
	{
		if (buffer->referenceCount > 1)
		{
			size_t byteCount = allocationByteCount (buffer->length);

			Buffer* newBuffer = (Buffer*)new char[byteCount];
			newBuffer->referenceCount = 1;
			newBuffer->length = buffer->length;
			memcpy (newBuffer->data, buffer->data, buffer->length*sizeof (wchar_t));
			newBuffer->data[buffer->length] = 0;
			Release ();
			data_ = newBuffer->data;
			return newBuffer;
		}
	}
	return buffer;
}

rstring::Buffer* rstring::ReallocateUnique (size_t length)
{
	Buffer* buffer = toBuffer ();
	if (buffer)
	{
		size_t currentByteCount = allocationByteCount (buffer->length);
		size_t newByteCount = allocationByteCount (length);
		if (!newByteCount)
		{
			Release ();
			return nullptr;
		}
		else
		{
			if ((buffer->referenceCount > 1) || (currentByteCount != newByteCount))
			{
				Buffer* newBuffer = (Buffer*)new char[newByteCount];
				newBuffer->referenceCount = 1;
				newBuffer->length = length;
				size_t copyCount = min (buffer->length, length) + 2;
				memcpy (newBuffer->data, buffer->data, copyCount*sizeof (WCHAR));
				newBuffer->data[length] = 0;
				Release ();
				data_ = newBuffer->data;
				return newBuffer;
			}
			buffer->length = length;
			buffer->data[length] = 0;
		}
	}
	else
	{
		buffer = allocate (length);
		data_ = buffer->data;
	}
	return buffer;
}

int rstring::_Compare (Buffer* buffer1, Buffer* buffer2)
{
	if (buffer1 == buffer2)
	{
		return 0;
	}
	if (buffer1 && buffer2)
	{
		size_t compareLength = min (buffer1->length, buffer2->length);
		for (size_t i = 0; i < compareLength; i++)
		{
			int res = int (buffer1->data[i]) - int (buffer2->data[i]);
			if (res != 0)
			{
				return res;
			}
		}
		if (buffer1->length > buffer2->length)
		{
			return 1;
		}
		if (buffer1->length < buffer2->length)
		{
			return -1;
		}
		return 0;
	}
	else if (buffer1)
	{
		return 1;
	}
	else if (buffer2)
	{
		return -1;
	}
	return 0;
}

int rstring::_Compare (Buffer* buffer1, LPCWSTR buffer2)
{
	if (buffer1 && buffer2 && buffer2[0] != 0)
	{
		size_t length = wcslen (buffer2);
		size_t compareLength = min (buffer1->length, length);
		for (size_t i = 0; i < compareLength; i++)
		{
			INT res = INT (buffer1->data[i]) - INT (buffer2[i]);

			if (res != 0)
			{
				return res;
			}
		}
		if (buffer1->length > length)
		{
			return 1;
		}
		if (buffer1->length < length)
		{
			return -1;
		}
		return 0;
	}
	else if (buffer1)
	{
		return 1;
	}
	else if (buffer2 && buffer2[0] != 0)
	{
		return -1;
	}
	return 0;
}

int rstring::_CompareI (Buffer* buffer1, Buffer* buffer2)
{
	if (buffer1 == buffer2)
	{
		return 0;
	}
	if (buffer1 && buffer2)
	{
		auto result = CompareString (LOCALE_USER_DEFAULT, NORM_IGNORECASE, buffer1->data, static_cast<int>(buffer1->length), buffer2->data, static_cast<int>(buffer2->length));

		return result - CSTR_EQUAL;
	}
	else if (buffer1)
	{
		return 1;
	}
	else if (buffer2)
	{
		return -1;
	}
	return 0;
}

int rstring::_CompareI (Buffer* buffer1, LPCWSTR buffer2)
{
	if (buffer1 && buffer2)
	{
		auto result = CompareString (LOCALE_USER_DEFAULT, NORM_IGNORECASE, buffer1->data, static_cast<int>(buffer1->length), buffer2, -1);

		return result - CSTR_EQUAL;
	}
	else if (buffer1)
	{
		return 1;
	}
	else if (buffer2)
	{
		return -1;
	}
	return 0;
}