/**
 * @file
 *
 * Non-STL version of string.
 */

/******************************************************************************
 * Copyright 2010-2011, Qualcomm Innovation Center, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 ******************************************************************************/
#ifndef _QCC_STRING_H
#define _QCC_STRING_H

#include <qcc/platform.h>
#include <stdlib.h>
#include <string.h>
#include <qcc/atomic.h>

namespace qcc {

/**
 * String is a heap-allocated array of bytes whose life-cycle is managed
 * through reference counting. When all references to a qcc::String instance
 * go out of scope or are deleted, then the underlying heap-allocated storage
 * is freed.
 */
class String {
  public:

    /** String index constant indicating "past the end" */
    static const size_t npos = static_cast<size_t>(-1);

    /** String iterator type */
    typedef char* iterator;

    /** const String iterator type */
    typedef const char* const_iterator;

    /**
     * Construct an empty string.
     */
    String();

    /**
     * Construct a single character string.
     *
     * @param c         Initial value for string
     * @param sizeHint  Optional size hint for initial allocation.
     */
    String(char c, size_t sizeHint = MinCapacity);

    /**
     * Construct a String with n copies of char.
     *
     * @param n         Number of chars in string.
     * @param c         Character used to fill string.
     * @param sizeHint  Optional size hint for initial allocation.
     */
    String(size_t n, char c, size_t sizeHint = MinCapacity);

    /**
     * Construct a string from a const char*
     *
     * @param str        char* array to use as initial value for String.
     * @param strLen     Length of string or 0 if str is null terminated
     * @param sizeHint   Optional size hint used for initial malloc if larger than str length.
     */
    String(const char* str, size_t strLen = 0, size_t sizeHint = MinCapacity);

    /**
     * Copy Constructor
     *
     * @param str        String to copy
     */
    String(const String& str);

    /** Destructor */
    virtual ~String();

    /** Assignment operator */
    String& operator=(const String& assignFromMe);

    /**
     * Get the current storage capacity for this string.
     *
     * @return  Amount of storage allocated to this string.
     */
    size_t capacity() const { return context ? context->capacity : 0; }

    /**
     * Get an iterator to the beginning of the string.
     *
     * @return iterator to start of string
     */
    iterator begin() { return context ? context->c_str : emptyString; }

    /**
     * Get an iterator to the end of the string.
     *
     * @return iterator to end of string.
     */
    iterator end() { return context ? context->c_str + context->offset : emptyString; }

    /**
     * Get a const_iterator to the beginning of the string.
     *
     * @return iterator to start of string
     */
    const_iterator begin() const { return context ? context->c_str : emptyString; }

    /**
     * Get an iterator to the end of the string.
     *
     * @return iterator to end of string.
     */
    const_iterator end() const { return context ? context->c_str + context->offset : emptyString; }

    /**
     * Clear contents of string.
     *
     * @param sizeHint   Allocation size hint used if string must be realloced.
     */
    void clear(size_t sizeHint = MinCapacity);

    /**
     * Append to string.
     *
     * @param str  Value to append to string.
     * @param len  Number of characters to append or 0 to insert up to first nul byte in str.
     * @return  Reference to this string.
     */
    String& append(const char* str, size_t len = 0);

    /**
     * Append to string.
     *
     * @param str  Value to append to string.
     * @return  Reference to this string.
     */
    String& append(const String& str) { return append(str.c_str(), str.size()); }

    /**
     * Erase a range of chars from string.
     *
     * @param pos    Offset first char to erase.
     * @param n      Number of chars to erase.
     * @return  Reference to this string.
     */
    String& erase(size_t pos = 0, size_t n = npos);

    /**
     * Resize string by appending chars or removing them to make string a specified size.
     *
     * @param n     New size for string.
     * @param c     Charater to append to string if string size increases.
     */
    void resize(size_t n, char c = ' ');

    /**
     * Set storage space for this string.
     * The new capacity will be the larger of the current size and the specified new capacity.
     *
     * @param newCapacity   The desired capacity for this string. May be greater or less than current capacity.
     */
    void reserve(size_t newCapacity);

    /**
     * Push a single character to the end of the string.
     *
     * @param c  Char to push
     */
    void push_back(char c) { char _c = c; append(&_c, 1); }

    /**
     * Append to string.
     *
     * @param str  Value to append to string.
     * @return  Reference to this string.
     */
    String& operator+=(const char* str) { return append(str); }

    /**
     * Append to string.
     *
     * @param str  Value to append to string.
     * @return  Reference to this string.
     */
    String& operator+=(const String& str) { return append(str.c_str(), str.size()); }

    /**
     * Insert characters into string at position.
     *
     * @param pos   Insert position.
     * @param str   Character string to insert.
     * @param len   Optional number of chars to insert.
     * @return  Reference to the string.
     */
    String& insert(size_t pos, const char* str, size_t len = 0);

    /**
     * Return true if string is equal to this string.
     *
     * @param str  String to compare against this string.
     * @return true iff other is equal to this string.
     */
    bool operator==(const String& str) const;

    /**
     * Return true if string is not equal to this string.
     *
     * @param str  String to compare against this string.
     * @return true iff other is not equal to this string.
     */
    bool operator!=(const String& str) const { return !operator==(str); }

    /**
     * Return true if this string is less than other string.
     *
     * @param str  String to compare against this string.
     * @return true iff this string is less than other string
     */
    bool operator<(const String& str) const;

    /**
     * Get a reference to the character at a given position.
     * If pos >= size, then 0 will be returned.
     *
     * @param pos    Position offset into string.
     * @return  Reference to character at pos.
     */
    char& operator[](size_t pos);

    /**
     * Get a const reference to the character at a given position.
     * If pos >= size, then 0 will be returned.
     *
     * @param pos    Position offset into string.
     * @return  Reference to character at pos.
     */
    char operator[](size_t pos) const
    {
        return context ? context->c_str[pos] : *emptyString;
    }

    /**
     * Get the size of the string.
     *
     * @return size of string.
     */
    size_t size() const { return context ? context->offset : 0; }

    /**
     * Get the length of the string.
     *
     * @return size of string.
     */
    size_t length() const { return size(); }

    /**
     * Get the null termination char* representation for this String.
     *
     * @return Null terminated string.
     */
    const char* c_str() const { return context ? context->c_str : emptyString; }

    /**
     * Get the not-necessarily null termination char* representation for this String.
     *
     * @return Null terminated string.
     */
    const char* data() const { return c_str(); }

    /**
     * Return true if string contains no chars.
     *
     * @return true iff string is empty
     */
    bool empty() const { return context ? (context->offset == 0) : true; }

    /**
     * Find first occurence of null terminated string within this string.
     *
     * @param str  String to find.
     * @param pos  Optional starting position (in this string) for search.
     * @return     Position of first occurrence of c within string or npos if not found.
     */
    size_t find(const char* str, size_t pos = 0) const;

    /**
     * Find first occurence of string within this string.
     *
     * @param str  String to find within this string instance.
     * @param pos  Optional starting position (in this string) for search.
     * @return     Position of first occurrence of c within string or npos if not found.
     */
    size_t find(const qcc::String& str, size_t pos = 0) const;

    /**
     * Find first occurence of character within string.
     *
     * @param c    Charater to find.
     * @param pos  Optional starting position for search.
     * @return     Position of first occurrence of c within string or npos if not found.
     */
    size_t find_first_of(const char c, size_t pos = 0) const;

    /**
     * Find last occurence of character within string in range [0, pos).
     *
     * @param c    Charater to find.
     * @param pos  Optional starting position for search (one past end of substring to search).
     * @return     Position of last occurrence of c within string or npos if not found.
     */
    size_t find_last_of(const char c, size_t pos = npos) const;

    /**
     * Find first occurence of any of a set of characters within string.
     *
     * @param inChars    Array of characters to look for in this string.
     * @param pos        Optional starting position within this string for search.
     * @return           Position of first occurrence of one of inChars within string or npos if not found.
     */
    size_t find_first_of(const char* inChars, size_t pos = 0) const;

    /**
     * Find first occurence of a character NOT in a set of characters within string.
     *
     * @param setChars   Array of characters to (NOT) look for in this string.
     * @param pos        Optional starting position within this string for search.
     * @return           Position of first occurrence a character not in setChars or npos if none exists.
     */
    size_t find_first_not_of(const char* setChars, size_t pos = 0) const;

    /**
     * Find last occurence of a character NOT in a set of characters within string range [0, pos).
     *
     * @param setChars   Array of characters to (NOT) look for in this string.
     * @param pos        Position one past the end of the charactaer of the substring that should be examined or npos for entire string.
     * @return           Position of first occurrence a character not in setChars or npos if none exists.
     */
    size_t find_last_not_of(const char* setChars, size_t pos = npos) const;

    /**
     * Return a substring of this string.
     *
     * @param  pos  Starting position of substring.
     * @param  n    Number of bytes in substring.
     * @return  Substring of this string.
     */
    String substr(size_t pos = 0, size_t n = npos) const;

    /**
     * Compare this string with other.
     *
     * @param other   String to compare with.
     * @return  &lt;0 if this string is less than other, &gt;0 if this string is greater than other, 0 if equal.
     */
    int compare(const String& other) const { return compare(0, npos, other); }

    /**
     * Compare a substring of this string with a substring of other.
     *
     * @param pos       Start position of this string.
     * @param n         Number of characters of this string to use for compare.
     * @param other     String to compare with.
     * @param otherPos  Start position of other string.
     * @param otherN    Number of characters of other string to use for compare.
     *
     * @return  &lt;0 if this string is less than other, &gt;0 if this string is greater than other, 0 if equal.
     */
    int compare(size_t pos, size_t n, const String& other, size_t otherPos, size_t otherN) const;

    /**
     * Compare a substring of this string with other.
     *
     * @param pos    Start position of this string.
     * @param n      Number of characters of this string to use for compare.
     * @param other  String to compare with.
     * @return  &lt;0 if this string is less than other, &gt;0 if this string is greater than other, 0 if equal.
     */
    int compare(size_t pos, size_t n, const String& other) const;

    /**
     * Compare this string with an array of chars.
     *
     * @param str   Null terminated array of chars to comapare against this string.
     * @return  &lt;0 if this string is less than str, &gt;0 if this string is greater than str, 0 if equal.
     */
    int compare(const char* str) const { return ::strcmp(context ? context->c_str : emptyString, str); }

  private:

    /** Empty string constant */
    static char emptyString[];

    static const size_t MinCapacity = 16;

    typedef struct {
        int32_t refCount;
        uint32_t offset;
        uint32_t capacity;
        char c_str[MinCapacity];
    } ManagedCtx;

    ManagedCtx* context;

    void IncRef();

    void DecRef(ManagedCtx* context);

    void NewContext(const char* str, size_t strLen, size_t sizeHint);
};

}

/**
 * Global "+" operator for qcc::String
 *
 * @param   s1  String to be concatenated.
 * @param   s2  String to be concatenated.
 * @return  Concatenated s1 + s2.
 */
qcc::String operator+(const qcc::String& s1, const qcc::String& s2);

#endif
