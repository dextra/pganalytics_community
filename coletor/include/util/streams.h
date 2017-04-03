#ifndef __UTIL_PSTREAM_H__
#define __UTIL_PSTREAM_H__

// Based on examples at http://www.mr-edd.co.uk/blog/beginners_guide_streambuf

#include "config.h"

#include <iostream>
#include <cstdio>
#include <streambuf>
#include <vector>
#include <cstdlib>
#include <cstdio>

#include <zlib.h>

BEGIN_APP_NAMESPACE

namespace Util
{
namespace io
{

class iprocstream;
class oprocstream;

/*** FILE streams ***/
class FILE_buffer : public std::streambuf
{
	friend class iprocstream;
	friend class oprocstream;
public:
	explicit FILE_buffer(FILE *fptr, std::size_t buff_sz = 256, std::size_t put_back = 8);
private:
	// overrides base class underflow()
	int_type underflow();
	int_type overflow(int_type ch);
	int sync();
	// copy ctor and assignment not implemented;
	// copying not allowed
	FILE_buffer(const FILE_buffer &);
	FILE_buffer &operator= (const FILE_buffer &);
private:
	FILE *fptr_;
	const std::size_t put_back_;
	std::vector<char> buffer_;
};

class FILE_istream : public std::istream
{
protected:
	FILE_buffer buf;
public:
	FILE_istream(FILE *f)
		: std::istream(&buf), buf(f)
	{}
};

class FILE_ostream : public std::ostream
{
protected:
	FILE_buffer buf;
public:
	FILE_ostream(FILE *f)
		: std::ostream(&buf), buf(f)
	{}
};

/*** procstreams (wrap over popen/pclose) ***/
class oprocstream : public std::ostream
{
protected:
	FILE *f;
	FILE_buffer buf;
public:
	oprocstream(const std::string &process);
	bool is_close() const;
	int close();
	virtual ~oprocstream();
};

class iprocstream : public std::istream
{
protected:
	FILE *f;
	FILE_buffer buf;
public:
	iprocstream(const std::string &process);
	bool is_close() const;
	int close();
	virtual ~iprocstream();
};

/*** Utility functions to use with procstreams ***/
std::string quoteProcArgument(const std::string &argument);

/*** zlib streams ***/
class gzipbuf : public std::streambuf
{
public:
	explicit gzipbuf(std::ostream &ostr, size_t buffer_size = 16384);
	virtual ~gzipbuf();
private:
	int_type overflow(int_type ch);
	int sync();
	// copy ctor and assignment not implemented;
	// copying not allowed
	gzipbuf(const gzipbuf &);
	gzipbuf &operator= (const gzipbuf &);
private:
	std::ostream &ostr;
	unsigned char *buffer;
	unsigned char *temp_out_buffer;
	size_t buffer_size;
	z_stream strm;
	int zdeflate(bool finish);
};

class gzipstream : public std::ostream
{
protected:
	gzipbuf *buf;
public:
	explicit gzipstream(std::ostream &ostr, size_t buffer_size = 16384);
	gzipstream();
	virtual ~gzipstream();
	void open(std::ostream &ostr, size_t buffer_size = 16384);
	void close();
	inline bool is_open()
	{
		return (buf != NULL);
	}
};

class gunzipbuf : public std::streambuf
{
public:
	explicit gunzipbuf(std::istream &istr, size_t buffer_size = 16384);
	virtual ~gunzipbuf();

private:
	std::streambuf::int_type underflow();

	// copy ctor and assignment not implemented;
	// copying not allowed
	gunzipbuf(const gunzipbuf &);
	gunzipbuf &operator= (const gunzipbuf &);

private:
	std::istream &istr;
	unsigned char *buffer;
	unsigned char *temp_in_buffer;
	size_t buffer_size;
	bool zlib_eof;
	z_stream strm;
};

class gunzipstream : public std::istream
{
protected:
	gunzipbuf *buf;
public:
	explicit gunzipstream(std::istream &istr, size_t buffer_size = 16384);
	gunzipstream();
	virtual ~gunzipstream();
	void open(std::istream &istr, size_t buffer_size = 16384);
	void close();
	inline bool is_open()
	{
		return (buf != NULL);
	}
};

} // namespace io
} // namespace Util

END_APP_NAMESPACE

#endif // __UTIL_PSTREAM_H__

