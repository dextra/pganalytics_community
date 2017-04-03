#include <algorithm>
#include <cstring>
#include <sstream>
#include <exception>
#include <stdexcept>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <cstring>
#include <zlib.h>

#include "util/streams.h"
#include "debug.h"

// Based on examples at http://www.mr-edd.co.uk/blog/beginners_guide_streambuf

BEGIN_APP_NAMESPACE

namespace Util
{
namespace io
{

using std::size_t;

FILE_buffer::FILE_buffer(FILE *fptr, size_t buff_sz, size_t put_back) :
	fptr_(fptr),
	put_back_(std::max(put_back, size_t(1))),
	buffer_(std::max(buff_sz, put_back_) + put_back_)
{
	char *end = &buffer_.front() + buffer_.size();
	char *base = &buffer_.front();
	setg(end, end, end);
	setp(base, base + buffer_.size() - 1); // -1 to make overflow() easier
}

std::streambuf::int_type FILE_buffer::underflow()
{
	if (gptr() < egptr()) // buffer not exhausted
		return traits_type::to_int_type(*gptr());

	char *base = &buffer_.front();
	char *start = base;

	if (eback() == base) // true when this isn't the first fill
	{
		// Make arrangements for putback characters
		std::memmove(base, egptr() - put_back_, put_back_);
		start += put_back_;
	}

	// start is now the start of the buffer, proper.
	// Read from fptr_ in to the provided buffer
	if (fptr_ == NULL)
	{
		throw std::runtime_error("stream is closed, read not possible");
	}
	size_t n = std::fread(start, 1, buffer_.size() - (start - base), fptr_);
	if (n == 0)
		return traits_type::eof();

	// Set buffer pointers
	setg(base, start, start + n);

	return traits_type::to_int_type(*gptr());
}

std::streambuf::int_type FILE_buffer::overflow(std::streambuf::int_type ch)
{
	if (ch != traits_type::eof())
	{
		*pptr() = ch;
		pbump(1); // increment pptr
	}
	return ch;
}

int FILE_buffer::sync()
{
	char *start = pbase();
	char *end = pptr();
	int n = (end - start);
	if (n)
	{
		pbump(-n);
		if (fptr_ == NULL)
		{
			throw std::runtime_error("stream is closed, write not possible");
		}
		std::fwrite(start, 1, n, fptr_);
	}
	return n;
}

/********************************************************************
 * Implment oprocstream and iprocstream                             *
 ********************************************************************/

oprocstream::oprocstream(const std::string &process)
	: std::ostream(&buf), f(::popen(process.c_str(), "w")), buf(f)
{}
bool oprocstream::is_close() const
{
	return (f == NULL);
}
int oprocstream::close()
{
	int ret = ::pclose(f);
	if (ret == -1)
	{
		throw std::runtime_error(std::string("pclose failed: ") + strerror(errno));
	}
	f = NULL;
	buf.fptr_ = NULL;
#ifndef __WIN32__
	return WEXITSTATUS(ret);
#else
	return ret;
#endif
}
oprocstream::~oprocstream()
{
	if (!is_close())
		::pclose(f);
}

iprocstream::iprocstream(const std::string &process)
	: std::istream(&buf), f(::popen(process.c_str(), "r")), buf(f)
{}
bool iprocstream::is_close() const
{
	return (f == NULL);
}
int iprocstream::close()
{
	int ret = ::pclose(f);
	if (ret == -1)
	{
		throw std::runtime_error(std::string("pclose failed: ") + strerror(errno));
	}
	f = NULL;
	buf.fptr_ = NULL;
#ifndef __WIN32__
	return WEXITSTATUS(ret);
#else
	return ret;
#endif
}
iprocstream::~iprocstream()
{
	if (!is_close())
		::pclose(f);
}

std::string quoteProcArgument(const std::string &argument)
{
	std::ostringstream ret;
	ret << "\"";
	for (size_t i = 0; i < argument.size(); i++)
	{
		switch(argument[i])
		{
		case '\a':
			ret << "\\a";
			break;
		case '\r':
			ret << "\\r";
			break;
		case '\n':
			ret << "\\n";
			break;
		case '\t':
			ret << "\\t";
			break;
		case '"':
		case '\\':
			ret << '\\';
			/* no break, we need to add the following char */
		default:
			ret << argument[i];
		}
	}
	ret << "\"";
	return ret.str();
}

/*** zlib streams ***/

namespace gzipstreams_private
{

void zerr(int ret)
{
	if (ret < 0)
	{
		switch (ret)
		{
		case Z_ERRNO:
			throw std::runtime_error("error while reading/writing");
			break;
		case Z_STREAM_ERROR:
			throw std::runtime_error("invalid compression level");
			break;
		case Z_DATA_ERROR:
			throw std::runtime_error("invalid or incomplete deflate data");
			break;
		case Z_MEM_ERROR:
			throw std::runtime_error("out of memory");
			break;
		case Z_VERSION_ERROR:
			throw std::runtime_error("zlib version mismatch!");
			break;
		default:
			throw std::runtime_error("unknown exception on zlib");
		}
	}
}

} // namespace gzipstreams_private


gzipbuf::gzipbuf(std::ostream &ostr, std::size_t buffer_size)
	: ostr(ostr), buffer(new unsigned char[buffer_size + 1]), temp_out_buffer(new unsigned char[buffer_size + 1]), buffer_size(buffer_size)
{
	int windowsBits = 15;
	int GZIP_ENCODING = 16;
	/* allocate deflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	gzipstreams_private::zerr(deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, windowsBits | GZIP_ENCODING, 8, Z_DEFAULT_STRATEGY));
	/* set pointers */
	setp((char *)buffer, (char *)(buffer + buffer_size));
}

gzipbuf::~gzipbuf()
{
	(void)this->zdeflate(true);
	(void)deflateEnd(&strm);
	delete[] buffer;
	delete[] temp_out_buffer;
}

std::streambuf::int_type gzipbuf::overflow(std::streambuf::int_type ch)
{
	if (ch != traits_type::eof())
	{
		this->sync();
		*pptr() = ch;
		pbump(1);
	}
	return ch;
}

int gzipbuf::sync()
{
	int n = this->zdeflate(false);
	this->pbump(-n);
	return n;
}

int gzipbuf::zdeflate(bool finish)
{
	char *start = this->pbase();
	char *end = this->pptr();
	int n = (end - start);
	int n_out;
	if (finish || n)
	{
		strm.next_in = (unsigned char*)start;
		strm.avail_in = n;
		do
		{
			strm.next_out = this->temp_out_buffer;
			strm.avail_out = this->buffer_size;
			gzipstreams_private::zerr(deflate(&strm, (finish ? Z_FINISH : Z_NO_FLUSH)));
			n_out = this->buffer_size - strm.avail_out;
			this->ostr.write((char *)this->temp_out_buffer, n_out);
		}
		while (strm.avail_out == 0);
	}
	return n;
}

gzipstream::gzipstream(std::ostream &ostr, size_t buffer_size)
	: std::ostream(NULL), buf(NULL)
{
	this->open(ostr, buffer_size);
}

gzipstream::gzipstream()
	: std::ostream(NULL), buf(NULL)
{}

gzipstream::~gzipstream()
{
	if (this->is_open())
	{
		this->close();
	}
}

void gzipstream::open(std::ostream &ostr, size_t buffer_size)
{
	if (this->is_open())
	{
		throw std::runtime_error("stream already opened");
	}
	buf = new gzipbuf(ostr, buffer_size);
	this->rdbuf(buf);
}

void gzipstream::close()
{
	if (this->is_open())
	{
		delete buf;
		buf = NULL;
		this->rdbuf(buf);
	}
}

gunzipbuf::gunzipbuf(std::istream &istr, std::size_t buffer_size)
	: istr(istr), buffer(new unsigned char[buffer_size + 1]), temp_in_buffer(new unsigned char[buffer_size + 1]), buffer_size(buffer_size), zlib_eof(false)
{
	int windowBits = 15;
	int GZIP_ENCODING = 16;
	char *end = (char *)(buffer + buffer_size);
	setg(end, end, end);
	/* allocate inflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;
	gzipstreams_private::zerr(inflateInit2(&strm, windowBits | GZIP_ENCODING));
}

gunzipbuf::~gunzipbuf()
{
	(void)inflateEnd(&strm);
	delete[] buffer;
	delete[] temp_in_buffer;
}

std::streambuf::int_type gunzipbuf::underflow()
{
	size_t n;
	int ret;
	if (gptr() < egptr())
	{
		/* Buffer not exhausted */
		return traits_type::to_int_type(*gptr());
	}
	if (zlib_eof)
	{
		/* Both underline buffer (istr) and zlib finished, so real EOF */
		return traits_type::eof();
	}
	if (strm.avail_in == 0)
	{
		istr.read((char*)temp_in_buffer, buffer_size);
		strm.avail_in = istr.gcount();
		strm.next_in = temp_in_buffer;
		if (strm.avail_in == 0)
		{
			return traits_type::eof();
		}
	}
	strm.avail_out = buffer_size;
	strm.next_out = buffer;
	ret = inflate(&strm, Z_NO_FLUSH);
	n = buffer_size - strm.avail_out;
	setg((char *)buffer, (char *)buffer, (char *)(buffer + n));
	if (ret == Z_STREAM_END && istr.eof())
	{
		zlib_eof = true;
	}
	else
	{
		gzipstreams_private::zerr(ret);
	}
	if (zlib_eof && !n)
	{
		/**
		 * zlib found EOF and read nothing. It is necessary to check this because
		 * in some occasions it will find EOF in the middle of a buffer,  so  the
		 * caller still have something to read, while in other occasions it  will
		 * find EOF and nothing more to read
		 */
		return traits_type::eof();
	}
	return traits_type::to_int_type(*gptr());
}

gunzipstream::gunzipstream(std::istream &istr, size_t buffer_size)
	: std::istream(NULL), buf(NULL)
{
	this->open(istr, buffer_size);
}

gunzipstream::gunzipstream()
	: std::istream(NULL), buf(NULL)
{}

gunzipstream::~gunzipstream()
{
	if (this->is_open())
	{
		this->close();
	}
}

void gunzipstream::open(std::istream &istr, size_t buffer_size)
{
	if (this->is_open())
	{
		throw std::runtime_error("stream already opened");
	}
	buf = new gunzipbuf(istr, buffer_size);
	this->rdbuf(buf);
}

void gunzipstream::close()
{
	if (this->is_open())
	{
		delete buf;
		buf = NULL;
		this->rdbuf(buf);
	}
}

} // namespace io
} // namespace Util

END_APP_NAMESPACE

