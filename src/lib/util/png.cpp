// license:BSD-3-Clause
// copyright-holders:Aaron Giles, Vas Crabb
/*********************************************************************

    png.c

    PNG reading functions.

***************************************************************************/

#include "png.h"

#include <zlib.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cmath>
#include <list>
#include <memory>
#include <new>
#include <utility>

#include <math.h>
#include <stdlib.h>


/***************************************************************************
    TYPE DEFINITIONS
***************************************************************************/

struct image_data_chunk
{
	image_data_chunk(std::uint32_t l, std::unique_ptr<std::uint8_t []> &&d) : length(l), data(std::move(d)) { }

	std::uint32_t                       length = 0;
	std::unique_ptr<std::uint8_t []>    data;
};



/***************************************************************************
    GLOBAL VARIABLES
***************************************************************************/

static const int samples[] = { 1, 0, 3, 1, 2, 0, 4 };



/***************************************************************************
    INLINE FUNCTIONS
***************************************************************************/

static inline uint8_t fetch_8bit(uint8_t *v)
{
	return *v;
}


#ifdef UNUSED_FUNCTION
static inline uint16_t fetch_16bit(uint8_t *v)
{
	return big_endianize_int16(*(uint16_t *)v);
}
#endif

static inline uint32_t fetch_32bit(uint8_t *v)
{
	return big_endianize_int32(*(uint32_t *)v);
}


static inline void put_8bit(uint8_t *v, uint8_t data)
{
	*v = data;
}


#ifdef UNUSED_FUNCTION
static inline void put_16bit(uint8_t *v, uint16_t data)
{
	*(uint16_t *)v = big_endianize_int16(data);
}
#endif

static inline void put_32bit(uint8_t *v, uint32_t data)
{
	*(uint32_t *)v = big_endianize_int32(data);
}


static inline int compute_rowbytes(const png_info &pnginfo)
{
	return (pnginfo.width * samples[pnginfo.color_type] * pnginfo.bit_depth + 7) / 8;
}



/***************************************************************************
    GENERAL FUNCTIONS
***************************************************************************/

/*-------------------------------------------------
    png_free - free all memory allocated in a
    pnginfo structure
-------------------------------------------------*/

static void png_free(png_info &pnginfo)
{
	while (pnginfo.textlist)
	{
		png_text *const temp = pnginfo.textlist;
		pnginfo.textlist = temp->next;
		if (temp->keyword)
			delete[] (std::uint8_t *)temp->keyword;
		free(temp);
	}

	if (pnginfo.palette)
		delete[] pnginfo.palette;
	pnginfo.palette = nullptr;

	if (pnginfo.trans)
		delete[] pnginfo.trans;
	pnginfo.trans = nullptr;

	if (pnginfo.image)
		delete[] pnginfo.image;
	pnginfo.image = nullptr;
}

void png_free(png_info *pnginfo) { png_free(*pnginfo); } // TODO: make external interface use reference



namespace {

class png_private
{
public:
	png_private(png_info &info) : pnginfo(info)
	{
	}

	png_error copy_to_bitmap(bitmap_argb32 &bitmap, bool &hasalpha) const
	{
		// do some basic checks for unsupported images
		if ((ARRAY_LENGTH(samples) <= pnginfo.color_type) || !samples[pnginfo.color_type])
			return PNGERR_UNSUPPORTED_FORMAT; // unknown colour sample format
		if ((0 != pnginfo.interlace_method) && (1 != pnginfo.interlace_method))
			return PNGERR_UNSUPPORTED_FORMAT; // unknown interlace method
		if (8 != pnginfo.bit_depth)
			return PNGERR_UNSUPPORTED_FORMAT; // only do 8bpp here - expand lower bit depth first

		// everything looks sane, allocate the bitmap and deinterlace into it
		bitmap.allocate(pnginfo.width, pnginfo.height);
		std::uint8_t accumalpha(0xff);
		unsigned const pass_count(get_pass_count());
		std::uint32_t pass_offset[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		for (unsigned pass = 0; pass_count > pass; ++pass)
		{
			// calculate parameters for interlace pass
			pass_offset[pass + 1] = pass_offset[pass] + get_pass_bytes(pass);
			unsigned const x_shift(pnginfo.interlace_method ? ADAM7_X_SHIFT[pass] : 0);
			unsigned const y_shift(pnginfo.interlace_method ? ADAM7_Y_SHIFT[pass] : 0);
			unsigned const x_offs(pnginfo.interlace_method ? ADAM7_X_OFFS[pass] : 0);
			unsigned const y_offs(pnginfo.interlace_method ? ADAM7_Y_OFFS[pass] : 0);
			std::pair<std::uint32_t, std::uint32_t> const dimensions(get_pass_dimensions(pass));
			std::uint8_t const *src(pnginfo.image + pass_offset[pass]);

			if (3 == pnginfo.color_type)
			{
				// handle 8bpp palettized case
				for (std::uint32_t y = 0; dimensions.second > y; ++y)
				{
					for (std::uint32_t x = 0; dimensions.first > x; ++x, ++src)
					{
						// determine alpha and expand to 32bpp
						std::uint8_t const alpha((*src < pnginfo.num_trans) ? pnginfo.trans[*src] : 0xff);
						accumalpha &= alpha;
						std::uint16_t const paloffs(std::uint16_t(*src) * 3);
						rgb_t const pix(alpha, pnginfo.palette[paloffs], pnginfo.palette[paloffs + 1], pnginfo.palette[paloffs + 2]);
						bitmap.pix32((y << y_shift) + y_offs, (x << x_shift) + x_offs) = pix;
					}
				}
			}
			else if (0 == pnginfo.color_type)
			{
				// handle 8bpp grayscale non-alpha case
				for (std::uint32_t y = 0; dimensions.second > y; ++y)
				{
					for (std::uint32_t x = 0; dimensions.first > x; ++x, ++src)
					{
						rgb_t const pix(0xff, src[0], src[0], src[0]);
						bitmap.pix32((y << y_shift) + y_offs, (x << x_shift) + x_offs) = pix;
					}
				}
			}
			else if (4 == pnginfo.color_type)
			{
				// handle 8bpp grayscale alpha case
				for (std::uint32_t y = 0; dimensions.second > y; ++y)
				{
					for (std::uint32_t x = 0; dimensions.first > x; ++x, src += 2)
					{
						accumalpha &= src[1];
						rgb_t const pix(src[1], src[0], src[0], src[0]);
						bitmap.pix32((y << y_shift) + y_offs, (x << x_shift) + x_offs) = pix;
					}
				}
			}
			else if (2 == pnginfo.color_type)
			{
				// handle 32bpp non-alpha case
				for (std::uint32_t y = 0; dimensions.second > y; ++y)
				{
					for (std::uint32_t x = 0; dimensions.first > x; ++x, src += 3)
					{
						rgb_t const pix(0xff, src[0], src[1], src[2]);
						bitmap.pix32((y << y_shift) + y_offs, (x << x_shift) + x_offs) = pix;
					}
				}
			}
			else
			{
				// handle 32bpp alpha case
				for (std::uint32_t y = 0; dimensions.second > y; ++y)
				{
					for (std::uint32_t x = 0; dimensions.first > x; ++x, src += 4)
					{
						accumalpha &= src[3];
						rgb_t const pix(src[3], src[0], src[1], src[2]);
						bitmap.pix32((y << y_shift) + y_offs, (x << x_shift) + x_offs) = pix;
					}
				}
			}
		}

		// set hasalpha flag and return
		hasalpha = 0xffU != accumalpha;
		return PNGERR_NONE;
	}

	png_error expand_buffer_8bit()
	{
		// nothing to do if we're at 8 or greater already
		if (pnginfo.bit_depth >= 8)
			return PNGERR_NONE;

		// do some basic checks for unsupported images
		if ((0 != pnginfo.color_type) && (3 != pnginfo.color_type))
			return PNGERR_UNSUPPORTED_FORMAT; // unknown colour sample format
		if ((0 != pnginfo.interlace_method) && (1 != pnginfo.interlace_method))
			return PNGERR_UNSUPPORTED_FORMAT; // unknown interlace method
		if (!pnginfo.bit_depth || (8 % pnginfo.bit_depth))
			return PNGERR_UNSUPPORTED_FORMAT; // bit depth must be a factor of eight

		// calculate the offset for each pass of the interlace on the input and output
		unsigned const pass_count(get_pass_count());
		std::uint32_t inp_offset[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		std::uint32_t outp_offset[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		for (unsigned pass = 0; pass_count > pass; ++pass)
		{
			inp_offset[pass + 1] = inp_offset[pass] + get_pass_bytes(pass);
			outp_offset[pass + 1] = outp_offset[pass] + get_pass_bytes(pass, 8);
		}

		// allocate a new buffer at 8-bit
		std::unique_ptr<std::uint8_t []> outbuf;
		try { outbuf.reset(new std::uint8_t [outp_offset[pass_count]]); }
		catch (std::bad_alloc const &) { return PNGERR_OUT_OF_MEMORY; }

		std::uint8_t const bytesamples(8 / pnginfo.bit_depth);
		for (unsigned pass = 0; pass_count > pass; ++pass)
		{
			std::pair<std::uint32_t, std::uint32_t> const dimensions(get_pass_dimensions(pass));
			std::uint32_t const rowsamples(samples[pnginfo.color_type] * dimensions.first);
			std::uint32_t const wholebytes(rowsamples / bytesamples);
			std::uint32_t const leftover(rowsamples % bytesamples);
			std::uint8_t const *inp(&pnginfo.image[inp_offset[pass]]);
			std::uint8_t *outp(&outbuf[outp_offset[pass]]);

			for (std::uint32_t y = 0; dimensions.second > y; ++y)
			{
				for (std::uint32_t i = 0; wholebytes > i; ++i, ++inp)
				{
					for (std::int8_t j = bytesamples - 1; 0 <= j; --j, ++outp)
					{
						*outp = (*inp >> (j * pnginfo.bit_depth)) & (0xffU >> (8 - pnginfo.bit_depth));
						if (!pnginfo.color_type)
						{
							for (unsigned k = 4; pnginfo.bit_depth <= k; k >>= 1)
								*outp |= *outp << k;
						}
					}
				}
				if (leftover)
				{
					for (std::int8_t j = leftover - 1; 0 <= j; --j,++outp)
					{
						*outp = (*inp >> (j * pnginfo.bit_depth)) & (0xffU >> (8 - pnginfo.bit_depth));
						if (!pnginfo.color_type)
						{
							for (unsigned k = 4; pnginfo.bit_depth <= k; k >>= 1)
								*outp |= *outp << k;
						}
					}
					inp++;
				}
			}
		}

		delete[] pnginfo.image;
		pnginfo.image = outbuf.release();
		pnginfo.bit_depth = 8;
		return PNGERR_NONE;
	}

	png_error read_file(util::core_file &fp)
	{
		// initialize the data structures
		png_error error = PNGERR_NONE;
		std::memset(&pnginfo, 0, sizeof(pnginfo));
		std::list<image_data_chunk> idata;

		// verify the signature at the start of the file
		error = verify_header(fp);

		// loop until we hit an IEND chunk
		while (PNGERR_NONE == error)
		{
			// read a chunk
			std::unique_ptr<std::uint8_t []> chunk_data;
			std::uint32_t chunk_type, chunk_length;
			error = read_chunk(fp, chunk_data, chunk_type, chunk_length);
			if (PNGERR_NONE == error)
			{
				if (chunk_type == PNG_CN_IEND)
					break; // stop when we hit an IEND chunk
				else
					error = process_chunk(idata, std::move(chunk_data), chunk_type, chunk_length);
			}
		}

		// finish processing the image
		if (PNGERR_NONE == error)
			error = process(idata);

		// if we have an error, free all the output data
		if (error != PNGERR_NONE)
		{
			png_free(pnginfo);
			memset(&pnginfo, 0, sizeof(pnginfo));
		}
		return error;
	}

	png_info &  pnginfo;

private:
	static constexpr unsigned ADAM7_X_BIAS[7]   = { 7, 3, 3, 1, 1, 0, 0 };
	static constexpr unsigned ADAM7_Y_BIAS[7]   = { 7, 7, 3, 3, 1, 1, 0 };
	static constexpr unsigned ADAM7_X_SHIFT[7]  = { 3, 3, 2, 2, 1, 1, 0 };
	static constexpr unsigned ADAM7_Y_SHIFT[7]  = { 3, 3, 3, 2, 2, 1, 1 };
	static constexpr unsigned ADAM7_X_OFFS[7]   = { 0, 4, 0, 2, 0, 1, 0 };
	static constexpr unsigned ADAM7_Y_OFFS[7]   = { 0, 0, 4, 0, 2, 0, 1 };

	png_error process(std::list<image_data_chunk> const &idata)
	{
		// do some basic checks for unsupported images
		if ((ARRAY_LENGTH(samples) <= pnginfo.color_type) || !samples[pnginfo.color_type])
			return PNGERR_UNSUPPORTED_FORMAT; // unknown colour sample format
		if ((0 != pnginfo.interlace_method) && (1 != pnginfo.interlace_method))
			return PNGERR_UNSUPPORTED_FORMAT; // unknown interlace method

		// calculate the offset for each pass of the interlace
		unsigned const pass_count(get_pass_count());
		std::uint32_t pass_offset[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		for (unsigned pass = 0; pass_count > pass; ++pass)
			pass_offset[pass + 1] = pass_offset[pass] + get_pass_bytes(pass);

		// allocate memory for the filtered image
		try { pnginfo.image = new std::uint8_t [pass_offset[pass_count]]; }
		catch (std::bad_alloc const &) { return PNGERR_OUT_OF_MEMORY; }

		// decompress image data
		png_error error = PNGERR_NONE;
		error = decompress(idata, pass_offset[pass_count]);
		std::uint32_t const bpp(get_bytes_per_pixel());
		for (unsigned pass = 0; (pass_count > pass) && (PNGERR_NONE == error); ++pass)
		{
			// compute some basic parameters
			std::pair<std::uint32_t, std::uint32_t> const dimensions(get_pass_dimensions(pass));
			std::uint32_t const rowbytes(get_row_bytes(dimensions.first));

			// we de-filter in place, stripping the filter bytes off the rows
			uint8_t *dst = pnginfo.image + pass_offset[pass];
			uint8_t const *src = dst;
			for (std::uint32_t y = 0; (dimensions.second > y) && (PNGERR_NONE == error); ++y)
			{
				// first byte of each row is the filter type
				uint8_t const filter(*src++);
				error = unfilter_row(filter, src, dst, y ? (dst - rowbytes) : nullptr, bpp, rowbytes);
				src += rowbytes;
				dst += rowbytes;
			}
		}

		// if we errored, free the image data
		if (error != PNGERR_NONE)
		{
			delete[] pnginfo.image;
			pnginfo.image = nullptr;
		}
		return error;
	}

	png_error decompress(std::list<image_data_chunk> const &idata, std::uint32_t expected)
	{
		// only deflate is permitted
		if (pnginfo.compression_method != 0)
			return PNGERR_DECOMPRESS_ERROR;

		// allocate zlib stream
		z_stream stream;
		int zerr;
		std::memset(&stream, 0, sizeof(stream));
		stream.zalloc = Z_NULL;
		stream.zfree = Z_NULL;
		stream.opaque = Z_NULL;
		stream.avail_in = 0;
		stream.next_in = Z_NULL;
		zerr = inflateInit(&stream);
		if (Z_OK != zerr)
			return PNGERR_DECOMPRESS_ERROR;

		// decompress IDAT blocks
		stream.next_out = pnginfo.image;
		stream.avail_out = expected;
		stream.avail_in = 0;
		std::list<image_data_chunk>::const_iterator it = idata.begin();
		while ((idata.end() != it) && ((Z_OK == zerr) || (Z_BUF_ERROR == zerr)) && !stream.avail_in)
		{
			stream.avail_in = it->length;
			stream.next_in = it->data.get();
			do
			{
				zerr = inflate(&stream, Z_NO_FLUSH);
			}
			while (stream.avail_in && (Z_OK == zerr));
			if (!stream.avail_in)
				++it;
		}

		// it's all good if we got end-of-stream or we have with no data remaining
		if ((Z_OK == inflateEnd(&stream)) && ((Z_STREAM_END == zerr) || ((Z_OK == zerr) && (idata.end() == it) && !stream.avail_in)))
			return PNGERR_NONE;
		else
			return PNGERR_DECOMPRESS_ERROR;
	}

	png_error unfilter_row(std::uint8_t type, uint8_t const *src, uint8_t *dst, uint8_t const *dstprev, int bpp, std::uint32_t rowbytes)
	{
		if (0 != pnginfo.filter_method)
			return PNGERR_UNKNOWN_FILTER;

		switch (type)
		{
		case PNG_PF_None: // no filter, just copy
			std::copy_n(src, rowbytes, dst);
			return PNGERR_NONE;

		case PNG_PF_Sub: // SUB = previous pixel
			dst = std::copy_n(src, bpp, dst);
			src += bpp;
			for (std::uint32_t x = bpp; rowbytes > x; ++x, ++src, ++dst)
				*dst = *src + dst[-bpp];
			return PNGERR_NONE;

		case PNG_PF_Up: // UP = pixel above
			if (dstprev)
			{
				for (std::uint32_t x = 0; rowbytes > x; ++x, ++src, ++dst, ++dstprev)
					*dst = *src + *dstprev;
			}
			else
			{
				std::copy_n(src, rowbytes, dst);
			}
			return PNGERR_NONE;

		case PNG_PF_Average: // AVERAGE = average of pixel above and previous pixel
			if (dstprev)
			{
				for (std::uint32_t x = 0; bpp > x; ++x, ++src, ++dst, ++dstprev)
					*dst = *src + (*dstprev >> 1);
				for (std::uint32_t x = bpp; rowbytes > x; ++x, ++src, ++dst, ++dstprev)
					*dst = *src + ((*dstprev + dst[-bpp]) >> 1);
			}
			else
			{
				dst = std::copy_n(src, bpp, dst);
				src += bpp;
				for (std::uint32_t x = bpp; rowbytes > x; ++x, ++src, ++dst)
					*dst = *src + (dst[-bpp] >> 1);
			}
			return PNGERR_NONE;

		case PNG_PF_Paeth: // PAETH = special filter
			for (std::uint32_t x = 0; rowbytes > x; ++x, ++src, ++dst)
			{
				int32_t const pa((x < bpp) ? 0 : dst[-bpp]);
				int32_t const pc(((x < bpp) || !dstprev) ? 0 : dstprev[-bpp]);
				int32_t const pb(!dstprev ? 0 : *dstprev++);
				int32_t const prediction(pa + pb - pc);
				int32_t const da(std::abs(prediction - pa));
				int32_t const db(std::abs(prediction - pb));
				int32_t const dc(std::abs(prediction - pc));
				*dst = ((da <= db) && (da <= dc)) ? (*src + pa) : (db <= dc) ? (*src + pb) : (*src + pc);
			}
			return PNGERR_NONE;

		default: // unknown filter type
			return PNGERR_UNKNOWN_FILTER;
		}
	}

	png_error process_chunk(std::list<image_data_chunk> &idata, std::unique_ptr<std::uint8_t []> &&data, uint32_t type, uint32_t length)
	{
		/* switch off of the type */
		switch (type)
		{
		/* image header */
		case PNG_CN_IHDR:
			pnginfo.width = fetch_32bit(&data[0]);
			pnginfo.height = fetch_32bit(&data[4]);
			pnginfo.bit_depth = fetch_8bit(&data[8]);
			pnginfo.color_type = fetch_8bit(&data[9]);
			pnginfo.compression_method = fetch_8bit(&data[10]);
			pnginfo.filter_method = fetch_8bit(&data[11]);
			pnginfo.interlace_method = fetch_8bit(&data[12]);
			break;

		/* palette */
		case PNG_CN_PLTE:
			pnginfo.num_palette = length / 3;
			pnginfo.palette = data.release();
			break;

		/* transparency information */
		case PNG_CN_tRNS:
			pnginfo.num_trans = length;
			pnginfo.trans = data.release();
			break;

		/* image data */
		case PNG_CN_IDAT:

			/* allocate a new image data descriptor and add it to the tail of the list */
			try { idata.emplace_back(length, std::move(data)); }
			catch (std::bad_alloc const &) { return PNGERR_OUT_OF_MEMORY; }

			break;

		/* gamma */
		case PNG_CN_gAMA:
			pnginfo.source_gamma = fetch_32bit(data.get()) / 100000.0;
			break;

		/* physical information */
		case PNG_CN_pHYs:
			pnginfo.xres = fetch_32bit(&data[0]);
			pnginfo.yres = fetch_32bit(&data[4]);
			pnginfo.resolution_unit = fetch_8bit(&data[8]);
			break;

		/* text */
		case PNG_CN_tEXt:
			{
				/* allocate a new text item */
				png_text *const text = (png_text *)malloc(sizeof(*text));
				if (!text)
					return PNGERR_OUT_OF_MEMORY;

				/* set the elements */
				text->keyword = (char *)data.release();
				text->text = text->keyword + strlen(text->keyword) + 1;
				text->next = nullptr;

				/* add to the end of the list */
				png_text *pt, *ct;
				for (pt = nullptr, ct = pnginfo.textlist; ct != nullptr; pt = ct, ct = ct->next) { }
				if (pt == nullptr)
					pnginfo.textlist = text;
				else
					pt->next = text;

				break;
			}

		/* anything else */
		default:
			if ((type & 0x20000000) == 0)
				return PNGERR_UNKNOWN_CHUNK;
			break;
		}
		return PNGERR_NONE;
	}

	unsigned get_pass_count() const
	{
		return (1 == pnginfo.interlace_method) ? 7 : 1;
	}

	std::pair<uint32_t, uint32_t> get_pass_dimensions(unsigned pass) const
	{
		if (0 == pnginfo.interlace_method)
			return std::make_pair(pnginfo.width, pnginfo.height);
		else
			return std::make_pair((pnginfo.width + ADAM7_X_BIAS[pass]) >> ADAM7_X_SHIFT[pass], (pnginfo.height + ADAM7_Y_BIAS[pass]) >> ADAM7_Y_SHIFT[pass]);
	}

	std::uint32_t get_pass_bytes(unsigned pass) const
	{
		return get_pass_bytes(pass, pnginfo.bit_depth);
	}

	std::uint32_t get_pass_bytes(unsigned pass, uint8_t bit_depth) const
	{
		std::pair<std::uint32_t, std::uint32_t> const dimensions(get_pass_dimensions(pass));
		return (get_row_bytes(dimensions.first, bit_depth) + 1) * dimensions.second;
	}

	std::uint32_t get_row_bytes(std::uint32_t width) const
	{
		return get_row_bytes(width, pnginfo.bit_depth);
	}

	std::uint32_t get_row_bytes(std::uint32_t width, uint8_t bit_depth) const
	{
		return ((width * samples[pnginfo.color_type] * bit_depth) + 7) >> 3;
	}

	std::uint32_t get_bytes_per_pixel() const
	{
		return ((samples[pnginfo.color_type] * pnginfo.bit_depth) + 7) >> 3;
	}

	static png_error verify_header(util::core_file &fp)
	{
		uint8_t signature[8];

		/* read 8 bytes */
		if (fp.read(signature, 8) != 8)
			return PNGERR_FILE_TRUNCATED;

		/* return an error if we don't match */
		if (memcmp(signature, PNG_Signature, 8) != 0)
			return PNGERR_BAD_SIGNATURE;

		return PNGERR_NONE;
	}

	static png_error read_chunk(util::core_file &fp, std::unique_ptr<std::uint8_t []> &data, std::uint32_t &type, std::uint32_t &length)
	{
		std::uint8_t tempbuff[4];

		/* fetch the length of this chunk */
		if (fp.read(tempbuff, 4) != 4)
			return PNGERR_FILE_TRUNCATED;
		length = fetch_32bit(tempbuff);

		/* fetch the type of this chunk */
		if (fp.read(tempbuff, 4) != 4)
			return PNGERR_FILE_TRUNCATED;
		type = fetch_32bit(tempbuff);

		/* stop when we hit an IEND chunk */
		if (type == PNG_CN_IEND)
			return PNGERR_NONE;

		/* start the CRC with the chunk type (but not the length) */
		std::uint32_t crc = crc32(0, tempbuff, 4);

		/* read the chunk itself into an allocated memory buffer */
		if (length)
		{
			/* allocate memory for this chunk */
			try { data.reset(new std::uint8_t [length]); }
			catch (std::bad_alloc const &) { return PNGERR_OUT_OF_MEMORY; }

			/* read the data from the file */
			if (fp.read(data.get(), length) != length)
			{
				data.reset();
				return PNGERR_FILE_TRUNCATED;
			}

			/* update the CRC */
			crc = crc32(crc, data.get(), length);
		}

		/* read the CRC */
		if (fp.read(tempbuff, 4) != 4)
		{
			data.reset();
			return PNGERR_FILE_TRUNCATED;
		}
		std::uint32_t const chunk_crc = fetch_32bit(tempbuff);

		/* validate the CRC */
		if (crc != chunk_crc)
		{
			data.reset();
			return PNGERR_FILE_CORRUPT;
		}

		return PNGERR_NONE;
	}
};

constexpr unsigned png_private::ADAM7_X_BIAS[7];
constexpr unsigned png_private::ADAM7_Y_BIAS[7];
constexpr unsigned png_private::ADAM7_X_SHIFT[7];
constexpr unsigned png_private::ADAM7_Y_SHIFT[7];
constexpr unsigned png_private::ADAM7_X_OFFS[7];
constexpr unsigned png_private::ADAM7_Y_OFFS[7];

} // anonymous namespace




/*-------------------------------------------------
    png_read_file - read a PNG from a core stream
-------------------------------------------------*/

png_error png_read_file(util::core_file &fp, png_info *pnginfo)
{
	png_private png(*pnginfo);
	return png.read_file(fp);
}


/*-------------------------------------------------
    png_read_bitmap - load a PNG file into a
    bitmap
-------------------------------------------------*/

png_error png_read_bitmap(util::core_file &fp, bitmap_argb32 &bitmap)
{
	png_error result;
	png_info pnginfo;
	png_private png(pnginfo);

	// read the PNG data
	result = png.read_file(fp);
	if (PNGERR_NONE != result)
		return result;

	// resample to 8bpp if necessary
	result = png.expand_buffer_8bit();
	if (PNGERR_NONE != result)
	{
		png_free(pnginfo);
		return result;
	}

	// allocate a bitmap of the appropriate size and copy it
	bool hasalpha;
	result = png.copy_to_bitmap(bitmap, hasalpha);

	// free our temporary data and return
	png_free(pnginfo);
	return result;
}


/*-------------------------------------------------
    png_expand_buffer_8bit - copy PNG data into a
    bitmap
-------------------------------------------------*/

png_error png_copy_to_bitmap(png_info *pnginfo, bitmap_argb32 &bitmap, bool &hasalpha)
{
	return png_private(*pnginfo).copy_to_bitmap(bitmap, hasalpha);
}


/*-------------------------------------------------
    png_expand_buffer_8bit - expand a buffer from
    sub 8-bit to 8-bit
-------------------------------------------------*/

png_error png_expand_buffer_8bit(png_info *pnginfo)
{
	return png_private(*pnginfo).expand_buffer_8bit();
}



/***************************************************************************
    PNG WRITING FUNCTIONS
***************************************************************************/

/*-------------------------------------------------
    png_add_text - add a text entry to the png_info
-------------------------------------------------*/

png_error png_add_text(png_info *pnginfo, const char *keyword, const char *text)
{
	png_text *newtext, *pt, *ct;
	char *textdata;
	int keylen;

	/* allocate a new text element */
	newtext = (png_text *)malloc(sizeof(*newtext));
	if (newtext == nullptr)
		return PNGERR_OUT_OF_MEMORY;

	/* allocate a string long enough to hold both */
	keylen = (int)strlen(keyword);
	textdata = (char *)malloc(keylen + 1 + strlen(text) + 1);
	if (textdata == nullptr)
	{
		free(newtext);
		return PNGERR_OUT_OF_MEMORY;
	}

	/* copy in the data */
	strcpy(textdata, keyword);
	strcpy(textdata + keylen + 1, text);

	/* text follows a trailing nullptr */
	newtext->keyword = textdata;
	newtext->text = textdata + keylen + 1;
	newtext->next = nullptr;

	/* add us to the end of the linked list */
	for (pt = nullptr, ct = pnginfo->textlist; ct != nullptr; pt = ct, ct = ct->next) { }
	if (pt == nullptr)
		pnginfo->textlist = newtext;
	else
		pt->next = newtext;

	return PNGERR_NONE;
}


/*-------------------------------------------------
    write_chunk - write an in-memory chunk to
    the given file
-------------------------------------------------*/

static png_error write_chunk(util::core_file &fp, const uint8_t *data, uint32_t type, uint32_t length)
{
	uint8_t tempbuff[8];
	uint32_t crc;

	/* stuff the length/type into the buffer */
	put_32bit(tempbuff + 0, length);
	put_32bit(tempbuff + 4, type);
	crc = crc32(0, tempbuff + 4, 4);

	/* write that data */
	if (fp.write(tempbuff, 8) != 8)
		return PNGERR_FILE_ERROR;

	/* append the actual data */
	if (length > 0)
	{
		if (fp.write(data, length) != length)
			return PNGERR_FILE_ERROR;
		crc = crc32(crc, data, length);
	}

	/* write the CRC */
	put_32bit(tempbuff, crc);
	if (fp.write(tempbuff, 4) != 4)
		return PNGERR_FILE_ERROR;

	return PNGERR_NONE;
}


/*-------------------------------------------------
    write_deflated_chunk - write an in-memory
    chunk to the given file by deflating it
-------------------------------------------------*/

static png_error write_deflated_chunk(util::core_file &fp, uint8_t *data, uint32_t type, uint32_t length)
{
	uint64_t lengthpos = fp.tell();
	uint8_t tempbuff[8192];
	uint32_t zlength = 0;
	z_stream stream;
	uint32_t crc;
	int zerr;

	/* stuff the length/type into the buffer */
	put_32bit(tempbuff + 0, length);
	put_32bit(tempbuff + 4, type);
	crc = crc32(0, tempbuff + 4, 4);

	/* write that data */
	if (fp.write(tempbuff, 8) != 8)
		return PNGERR_FILE_ERROR;

	/* initialize the stream */
	memset(&stream, 0, sizeof(stream));
	stream.next_in = data;
	stream.avail_in = length;
	zerr = deflateInit(&stream, Z_DEFAULT_COMPRESSION);
	if (zerr != Z_OK)
		return PNGERR_COMPRESS_ERROR;

	/* now loop until we run out of data */
	for ( ; ; )
	{
		/* compress this chunk */
		stream.next_out = tempbuff;
		stream.avail_out = sizeof(tempbuff);
		zerr = deflate(&stream, Z_FINISH);

		/* if there's data to write, do it */
		if (stream.avail_out < sizeof(tempbuff))
		{
			int bytes = sizeof(tempbuff) - stream.avail_out;
			if (fp.write(tempbuff, bytes) != bytes)
			{
				deflateEnd(&stream);
				return PNGERR_FILE_ERROR;
			}
			crc = crc32(crc, tempbuff, bytes);
			zlength += bytes;
		}

		/* stop at the end of the stream */
		if (zerr == Z_STREAM_END)
			break;

		/* other errors are fatal */
		if (zerr != Z_OK)
		{
			deflateEnd(&stream);
			return PNGERR_COMPRESS_ERROR;
		}
	}

	/* clean up deflater(maus) */
	zerr = deflateEnd(&stream);
	if (zerr != Z_OK)
		return PNGERR_COMPRESS_ERROR;

	/* write the CRC */
	put_32bit(tempbuff, crc);
	if (fp.write(tempbuff, 4) != 4)
		return PNGERR_FILE_ERROR;

	/* seek back and update the length */
	fp.seek(lengthpos, SEEK_SET);
	put_32bit(tempbuff + 0, zlength);
	if (fp.write(tempbuff, 4) != 4)
		return PNGERR_FILE_ERROR;

	/* return to the end */
	fp.seek(lengthpos + 8 + zlength + 4, SEEK_SET);
	return PNGERR_NONE;
}


/*-------------------------------------------------
    convert_bitmap_to_image_palette - convert a
    bitmap to a palettized image
-------------------------------------------------*/

static png_error convert_bitmap_to_image_palette(png_info *pnginfo, const bitmap_t &bitmap, int palette_length, const rgb_t *palette)
{
	int rowbytes;
	int x, y;

	/* set the common info */
	pnginfo->width = bitmap.width();
	pnginfo->height = bitmap.height();
	pnginfo->bit_depth = 8;
	pnginfo->color_type = 3;
	pnginfo->num_palette = 256;
	rowbytes = pnginfo->width;

	/* allocate memory for the palette */
	pnginfo->palette = (uint8_t *)malloc(3 * 256);
	if (pnginfo->palette == nullptr)
		return PNGERR_OUT_OF_MEMORY;

	/* build the palette */
	memset(pnginfo->palette, 0, 3 * 256);
	for (x = 0; x < palette_length; x++)
	{
		rgb_t color = palette[x];
		pnginfo->palette[3 * x + 0] = color.r();
		pnginfo->palette[3 * x + 1] = color.g();
		pnginfo->palette[3 * x + 2] = color.b();
	}

	/* allocate memory for the image */
	pnginfo->image = (uint8_t *)malloc(pnginfo->height * (rowbytes + 1));
	if (pnginfo->image == nullptr)
	{
		free(pnginfo->palette);
		return PNGERR_OUT_OF_MEMORY;
	}

	/* copy in the pixels, specifying a nullptr filter */
	for (y = 0; y < pnginfo->height; y++)
	{
		uint16_t *src = reinterpret_cast<uint16_t *>(bitmap.raw_pixptr(y));
		uint8_t *dst = pnginfo->image + y * (rowbytes + 1);

		/* store the filter byte, then copy the data */
		*dst++ = 0;
		for (x = 0; x < pnginfo->width; x++)
			*dst++ = *src++;
	}

	return PNGERR_NONE;
}


/*-------------------------------------------------
    convert_bitmap_to_image_rgb - convert a
    bitmap to an RGB image
-------------------------------------------------*/

static png_error convert_bitmap_to_image_rgb(png_info *pnginfo, const bitmap_t &bitmap, int palette_length, const rgb_t *palette)
{
	int alpha = (bitmap.format() == BITMAP_FORMAT_ARGB32);
	int rowbytes;
	int x, y;

	/* set the common info */
	pnginfo->width = bitmap.width();
	pnginfo->height = bitmap.height();
	pnginfo->bit_depth = 8;
	pnginfo->color_type = alpha ? 6 : 2;
	rowbytes = pnginfo->width * (alpha ? 4 : 3);

	/* allocate memory for the image */
	pnginfo->image = (uint8_t *)malloc(pnginfo->height * (rowbytes + 1));
	if (pnginfo->image == nullptr)
		return PNGERR_OUT_OF_MEMORY;

	/* copy in the pixels, specifying a nullptr filter */
	for (y = 0; y < pnginfo->height; y++)
	{
		uint8_t *dst = pnginfo->image + y * (rowbytes + 1);

		/* store the filter byte, then copy the data */
		*dst++ = 0;

		/* 16bpp palettized format */
		if (bitmap.format() == BITMAP_FORMAT_IND16)
		{
			uint16_t *src16 = reinterpret_cast<uint16_t *>(bitmap.raw_pixptr(y));
			for (x = 0; x < pnginfo->width; x++)
			{
				rgb_t color = palette[*src16++];
				*dst++ = color.r();
				*dst++ = color.g();
				*dst++ = color.b();
			}
		}

		/* 32-bit RGB direct */
		else if (bitmap.format() == BITMAP_FORMAT_RGB32)
		{
			uint32_t *src32 = reinterpret_cast<uint32_t *>(bitmap.raw_pixptr(y));
			for (x = 0; x < pnginfo->width; x++)
			{
				rgb_t raw = *src32++;
				*dst++ = raw.r();
				*dst++ = raw.g();
				*dst++ = raw.b();
			}
		}

		/* 32-bit ARGB direct */
		else if (bitmap.format() == BITMAP_FORMAT_ARGB32)
		{
			uint32_t *src32 = reinterpret_cast<uint32_t *>(bitmap.raw_pixptr(y));
			for (x = 0; x < pnginfo->width; x++)
			{
				rgb_t raw = *src32++;
				*dst++ = raw.r();
				*dst++ = raw.g();
				*dst++ = raw.b();
				*dst++ = raw.a();
			}
		}

		/* unsupported format */
		else
			return PNGERR_UNSUPPORTED_FORMAT;
	}

	return PNGERR_NONE;
}


/*-------------------------------------------------
    write_png_stream - stream a series of PNG
    chunks to the given file
-------------------------------------------------*/

static png_error write_png_stream(util::core_file &fp, png_info *pnginfo, const bitmap_t &bitmap, int palette_length, const rgb_t *palette)
{
	uint8_t tempbuff[16];
	png_text *text;
	png_error error;

	/* create an unfiltered image in either palette or RGB form */
	if (bitmap.format() == BITMAP_FORMAT_IND16 && palette_length <= 256)
		error = convert_bitmap_to_image_palette(pnginfo, bitmap, palette_length, palette);
	else
		error = convert_bitmap_to_image_rgb(pnginfo, bitmap, palette_length, palette);
	if (error != PNGERR_NONE)
		goto handle_error;

	/* if we wanted to get clever and do filtering, we would do it here */

	/* write the IHDR chunk */
	put_32bit(tempbuff + 0, pnginfo->width);
	put_32bit(tempbuff + 4, pnginfo->height);
	put_8bit(tempbuff + 8, pnginfo->bit_depth);
	put_8bit(tempbuff + 9, pnginfo->color_type);
	put_8bit(tempbuff + 10, pnginfo->compression_method);
	put_8bit(tempbuff + 11, pnginfo->filter_method);
	put_8bit(tempbuff + 12, pnginfo->interlace_method);
	error = write_chunk(fp, tempbuff, PNG_CN_IHDR, 13);
	if (error != PNGERR_NONE)
		goto handle_error;

	/* write the PLTE chunk */
	if (pnginfo->num_palette > 0)
		error = write_chunk(fp, pnginfo->palette, PNG_CN_PLTE, pnginfo->num_palette * 3);
	if (error != PNGERR_NONE)
		goto handle_error;

	/* write a single IDAT chunk */
	error = write_deflated_chunk(fp, pnginfo->image, PNG_CN_IDAT, pnginfo->height * (compute_rowbytes(*pnginfo) + 1));
	if (error != PNGERR_NONE)
		goto handle_error;

	/* write TEXT chunks */
	for (text = pnginfo->textlist; text != nullptr; text = text->next)
	{
		error = write_chunk(fp, (uint8_t *)text->keyword, PNG_CN_tEXt, (uint32_t)strlen(text->keyword) + 1 + (uint32_t)strlen(text->text));
		if (error != PNGERR_NONE)
			goto handle_error;
	}

	/* write an IEND chunk */
	error = write_chunk(fp, nullptr, PNG_CN_IEND, 0);

handle_error:
	return error;
}


png_error png_write_bitmap(util::core_file &fp, png_info *info, bitmap_t &bitmap, int palette_length, const rgb_t *palette)
{
	png_info pnginfo;
	png_error error;

	/* use a dummy pnginfo if none passed to us */
	if (info == nullptr)
	{
		info = &pnginfo;
		memset(&pnginfo, 0, sizeof(pnginfo));
	}

	/* write the PNG signature */
	if (fp.write(PNG_Signature, 8) != 8)
	{
		if (info == &pnginfo)
			png_free(&pnginfo);
		return PNGERR_FILE_ERROR;
	}

	/* write the rest of the PNG data */
	error = write_png_stream(fp, info, bitmap, palette_length, palette);
	if (info == &pnginfo)
		png_free(&pnginfo);
	return error;
}



/********************************************************************************

  MNG write functions

********************************************************************************/

/**
 * @fn  png_error mng_capture_start(util::core_file &fp, bitmap_t &bitmap, double rate)
 *
 * @brief   Mng capture start.
 *
 * @param [in,out]  fp      If non-null, the fp.
 * @param [in,out]  bitmap  The bitmap.
 * @param   rate            The rate.
 *
 * @return  A png_error.
 */

png_error mng_capture_start(util::core_file &fp, bitmap_t &bitmap, double rate)
{
	uint8_t mhdr[28];
	png_error error;

	if (fp.write(MNG_Signature, 8) != 8)
		return PNGERR_FILE_ERROR;

	memset(mhdr, 0, 28);
	put_32bit(mhdr + 0, bitmap.width());
	put_32bit(mhdr + 4, bitmap.height());
	put_32bit(mhdr + 8, rate);
	put_32bit(mhdr + 24, 0x0041); /* Simplicity profile */
	/* frame count and play time unspecified because
	   we don't know at this stage */
	error = write_chunk(fp, mhdr, MNG_CN_MHDR, 28);
	if (error != PNGERR_NONE)
		return error;

	return PNGERR_NONE;
}

/**
 * @fn  png_error mng_capture_frame(util::core_file &fp, png_info *info, bitmap_t &bitmap, int palette_length, const rgb_t *palette)
 *
 * @brief   Mng capture frame.
 *
 * @param [in,out]  fp      If non-null, the fp.
 * @param [in,out]  info    If non-null, the information.
 * @param [in,out]  bitmap  The bitmap.
 * @param   palette_length  Length of the palette.
 * @param   palette         The palette.
 *
 * @return  A png_error.
 */

png_error mng_capture_frame(util::core_file &fp, png_info *info, bitmap_t &bitmap, int palette_length, const rgb_t *palette)
{
	return write_png_stream(fp, info, bitmap, palette_length, palette);
}

/**
 * @fn  png_error mng_capture_stop(util::core_file &fp)
 *
 * @brief   Mng capture stop.
 *
 * @param [in,out]  fp  If non-null, the fp.
 *
 * @return  A png_error.
 */

png_error mng_capture_stop(util::core_file &fp)
{
	return write_chunk(fp, nullptr, MNG_CN_MEND, 0);
}
