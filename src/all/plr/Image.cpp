////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 John Chapman -- http://john-chapman.net
// This software is distributed freely under the terms of the MIT License.
// See http://opensource.org/licenses/MIT
////////////////////////////////////////////////////////////////////////////////
#include <plr/Image.h>

#include <plr/def.h>
#include <plr/log.h>

#include <cstring>

using namespace plr;

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <plr/extern/stb_image.h>
#include <plr/extern/stb_image_write.h>

template <typename tSrc, typename tDst>
static tDst Convert(tSrc _src) { return tDst(_src); }
	// from uint8
	template <> static uint8  Convert(uint8 _src) { return _src; }
	template <> static uint16 Convert(uint8 _src) { return (uint16)_src * (UINT16_MAX / UINT8_MAX); }
	template <> static uint32 Convert(uint8 _src) { return (uint32)_src * (UINT32_MAX / UINT8_MAX); }
	template <> static float  Convert(uint8 _src) { return (float )_src / (float)UINT8_MAX; }
	// from uint16
	template <> static uint8  Convert(uint16 _src) { return (uint8 )(_src / UINT8_MAX); }
	template <> static uint16 Convert(uint16 _src) { return _src; }
	template <> static uint32 Convert(uint16 _src) { return (uint32)_src * (UINT32_MAX / UINT16_MAX); }
	template <> static float  Convert(uint16 _src) { return (float )_src / (float)UINT16_MAX; }
	// from uint32
	template <> static uint8  Convert(uint32 _src) { return (uint8 )(_src / UINT8_MAX); }
	template <> static uint16 Convert(uint32 _src) { return (uint16)(_src / UINT16_MAX); }
	template <> static uint32 Convert(uint32 _src) { return _src; }
	template <> static float  Convert(uint32 _src) { return (float )_src / (float)UINT32_MAX; }
	// from float
	template <> static uint8  Convert(float _src) { return (uint8 )(_src * (float)UINT8_MAX); }
	template <> static uint16 Convert(float _src) { return (uint16)(_src * (float)UINT16_MAX); }
	template <> static uint32 Convert(float _src) { return (uint32)(_src * (float)UINT32_MAX); }
	template <> static float  Convert(float _src) { return _src; }

/// Copy at most _srcCount objects from _src to _dst, performing conversion from 
/// tSrc to tDst. If _srcCount < _dstCount, the remaining elements of _dst are
/// initialized as tDst(0).
template <typename tSrc, typename tDst>
static void ConvertCopy(const tSrc* _src, tDst* _dst, uint _srcCount, uint _dstCount)
{
	do {
		*_dst = Convert<tSrc, tDst>(*_src);
		++_src;
		++_dst;
		--_dstCount;
		--_srcCount;
	} while (_dstCount != 0 && _srcCount != 0);
	while (_dstCount != 0) {
		*_dst = tDst(0);
		++_dst;
		--_dstCount;
	}
}

/// Copy image data (_size texels) from _src to _dst. _srcCount/_dstCount are the
/// number of components per texel in _src/_dst respectively.
template <typename tSrcT, typename tDstT>
static void ConvertCopyImage(const void* _src, void* _dst, uint _srcCount, uint _dstCount, uint _size)
{
	const tSrcT::Type* src = (const tSrcT::Type*)_src;
	tDstT::Type* dst = (tDstT::Type*)_dst;
	for (uint i = 0, n = _size / _srcCount; i < n; ++i) {
		ConvertCopy(src, dst, _srcCount, _dstCount);
		src += _srcCount;
		dst += _dstCount;
	}
}

/// Data type tags (convert DataType to an actual type).
template <Image::DataType T> struct DataTypeT {};
	template<> struct DataTypeT<Image::DataType::kUint8>    { typedef uint8  Type; };
	template<> struct DataTypeT<Image::DataType::kUint16>   { typedef uint16 Type; };
	template<> struct DataTypeT<Image::DataType::kUint32>   { typedef uint32 Type; };	
	template<> struct DataTypeT<Image::DataType::kSint8>    { typedef sint8  Type; };
	template<> struct DataTypeT<Image::DataType::kSint16>   { typedef sint16 Type; };
	template<> struct DataTypeT<Image::DataType::kSint32>   { typedef sint32 Type; };
	template<> struct DataTypeT<Image::DataType::kFloat32>  { typedef float  Type; };

/*******************************************************************************

                                   Image

*******************************************************************************/

// PUBLIC

Image* Image::Create1d(uint _width, Layout _layout, DataType _dataType, uint _mipmapCount, CompressionType _compressionType)
{
	Image* ret = new Image;
	PLR_ASSERT(ret);
	ret->init();
	ret->m_type        = Type::k1d;
	ret->m_width       = _width;
	ret->m_layout      = _layout;
	ret->m_dataType    = _dataType;
	ret->m_mipmapCount = _mipmapCount;
	ret->m_compression = _compressionType;
	ret->alloc();

	return ret;
}

Image* Image::Create2d(uint _width, uint _height, Layout _layout, DataType _dataType, uint _mipmapCount, CompressionType _compressionType)
{
	Image* ret = new Image;
	PLR_ASSERT(ret);
	ret->init();
	ret->m_type        = Type::k2d;
	ret->m_width       = _width;
	ret->m_height      = _height;
	ret->m_layout      = _layout;
	ret->m_dataType    = _dataType;
	ret->m_mipmapCount = _mipmapCount;
	ret->m_compression = _compressionType;
	ret->alloc();

	return ret;
}


Image* Image::Create3d(uint _width, uint _height, uint _depth, Layout _layout, DataType _dataType, uint _mipmapCount, CompressionType _compressionType)
{
	Image* ret = new Image;
	PLR_ASSERT(ret);
	ret->init();
	ret->m_type        = Type::k3d;
	ret->m_width       = _width;
	ret->m_height      = _height;
	ret->m_depth       = _depth;
	ret->m_layout      = _layout;
	ret->m_dataType    = _dataType;
	ret->m_mipmapCount = _mipmapCount;
	ret->m_compression = _compressionType;
	ret->alloc();

	return ret;
}

void Image::Destroy(Image*& _img_)
{
	PLR_ASSERT(_img_);
	if (_img_) {
		delete _img_;
		_img_ = 0;
	}
}

Image* Image::Load(const char* _path, FileFormat _format)
{
	Image* ret = new Image;
	PLR_ASSERT(ret);
	ret->init();

	if (_format == FileFormat::kInvalid) {
		_format = GuessFormat(_path);
		if (_format == FileFormat::kInvalid) {
			ret->m_errorState = ErrorState::kFileFormatUnsupported;
			goto Image_Load_end;
		}
	}

	switch (_format) {
	case FileFormat::kDds:
		ret->m_errorState = ReadDds(_path, ret);
		break;
	case FileFormat::kPng:
		break;
	case FileFormat::kTga:
		break;
	};

Image_Load_end:
	if (ret->getErrorState() != ErrorState::kOk) {
		PLR_LOG_ERR("Error loading '%s':\n\t%s", _path, GetErrorString(ret->getErrorState()));
	}
	return ret;
}

Image::ErrorState Image::Save(Image* _img, const char* _path, FileFormat _format)
{
	PLR_ASSERT(_img);
	PLR_ASSERT(_path);

	ErrorState ret = ErrorState::kOk;

	if (_format == FileFormat::kInvalid) {
		_format = GuessFormat(_path);
		if (_format == FileFormat::kInvalid) {
			ret = ErrorState::kFileFormatUnsupported;
			goto Image_Save_end;
		}
	}

	if (!_img->validateFileFormat(_format)) {
		ret = ErrorState::kFileFormatUnsupported;
		goto Image_Save_end;
	}

	switch (_format) {
	case FileFormat::kDds:
		ret = WriteDds(_path, _img);
		break;
	case FileFormat::kPng:
		if (!stbi_write_png(_path, (int)_img->m_width, (int)_img->m_height, (int)GetComponentCount(_img->m_layout), _img->m_data, 0)) {
			ret = ErrorState::kFileIoError;
		}
		break;
	case FileFormat::kTga:
		if (!stbi_write_tga(_path, (int)_img->m_width, (int)_img->m_height, (int)GetComponentCount(_img->m_layout), _img->m_data)) {
			ret = ErrorState::kFileIoError;
		}
		break;
	default:
		ret = ErrorState::kFileFormatUnsupported;
		goto Image_Save_end;
	};

Image_Save_end:
	if (ret != ErrorState::kOk) {
		PLR_LOG_ERR("Error saving to '%s':\n\t%s", _path, GetErrorString(ret));
	}
	return ret;
}

uint Image::GetMaxMipmapSize(uint _width, uint _height, uint _depth)
{
	const double rlog2 = 1.0 / log(2.0);
	const uint log2Width  = (uint)(log((double)_width)  * rlog2);
	const uint log2Height = (uint)(log((double)_height) * rlog2);
	const uint log2Depth  = (uint)(log((double)_depth)  * rlog2);
	uint mipCount = PLR_MAX(log2Width, PLR_MAX(log2Height, log2Depth)) + 1u; // +1 for level 0
	mipCount = PLR_MIN(PLR_MIN(mipCount, (uint)1u), kMaxMipmapCount);
	return mipCount;
}

const char* Image::GetErrorString(ErrorState _err)
{
	switch (_err) {
		case ErrorState::kOk:                           return "Ok";
		case ErrorState::kFileNotFound:                 return "File not found";
		case ErrorState::kFileIoError:                  return "File IO error";
		case ErrorState::kFileFormatUnsupported:        return "File format unsupported";
		case ErrorState::kBadAlloc:                     return "Bad alloc";
		default: break;
	};
	return "Unknown error";
}

const char* Image::getRawImage(uint _array, uint _mip) const
{
	PLR_ASSERT(m_data);
	PLR_ASSERT(_array < m_arrayCount);
	PLR_ASSERT(_mip < m_mipmapCount);
	if (m_data == 0 || _array >= m_arrayCount || _mip >= m_mipmapCount) {
		return 0;
	}

	uint offset = _array * m_arrayLayerSize + m_mipOffsets[_mip];
	return m_data + offset;
}

uint Image::getRawImageSize(uint _mip) const
{
	PLR_ASSERT(m_data);
	PLR_ASSERT(_mip < m_mipmapCount);
	if (m_data == 0 || _mip >= m_mipmapCount) {
		return 0;
	}

	return m_mipSizes[_mip];
}


void Image::setRawImage(uint _array, uint _mip, const void* _src, Layout _layout, DataType _dataType, CompressionType _compressionType)
{
	PLR_ASSERT(_src);

	PLR_ASSERT_MSG(_compressionType == m_compression, "Compression types must match");

 // layout/type match, memcpy data
	if (_layout == m_layout && _dataType == m_dataType && _compressionType == m_compression) {
		memcpy(const_cast<char*>(getRawImage(_array, _mip)), _src, getRawImageSize(_mip));
		return;
	}

 // layout/type don't match, must convert
	uint srcCount = GetComponentCount(_layout);
	uint dstCount = GetComponentCount(m_layout);
	void* dst = const_cast<char*>(getRawImage(_array, _mip));

	#define CONVERT_FROM(type) \
		switch (m_dataType) { \
			case DataType::kUint8:   ConvertCopyImage<DataTypeT<DataType:: ## type>, DataTypeT<DataType::kUint8 > >(_src, dst, srcCount, dstCount, m_arrayLayerSize); break; \
			case DataType::kUint16:  ConvertCopyImage<DataTypeT<DataType:: ## type>, DataTypeT<DataType::kUint16> >(_src, dst, srcCount, dstCount, m_arrayLayerSize); break; \
			case DataType::kUint32:  ConvertCopyImage<DataTypeT<DataType:: ## type>, DataTypeT<DataType::kUint32> >(_src, dst, srcCount, dstCount, m_arrayLayerSize); break; \
			case DataType::kFloat32: ConvertCopyImage<DataTypeT<DataType:: ## type>, DataTypeT<DataType::kFloat32> >(_src, dst, srcCount, dstCount, m_arrayLayerSize); break; \
			default: PLR_ASSERT(false); \
		}
	switch (_dataType) {
		case DataType::kUint8:   CONVERT_FROM(kUint8); break;
		case DataType::kUint16:  CONVERT_FROM(kUint16); break;
		case DataType::kUint32:  CONVERT_FROM(kUint32); break;
		case DataType::kFloat32: CONVERT_FROM(kFloat32); break;
		default: PLR_ASSERT(false);
	};
	#undef CONVERT_FROM
}

// PRIVATE

Image::Image()
	: m_data(0)
{
	init();
}

Image::~Image()
{
	if (m_data) {
		delete[] m_data;
	}
}

void Image::init()
{
	m_width = m_height = m_depth = 1u;
	m_arrayCount = 1u;
	m_mipmapCount = 1u;
	m_type = Type::kInvalid;
	m_compression = CompressionType::kNone;
	m_layout = Layout::kInvalid;
	m_dataType = DataType::kInvalid;
	m_errorState = ErrorState::kOk;

	char* m_data = 0;
	memset(m_mipOffsets, 0u, sizeof(uint) * kMaxMipmapCount);
	memset(m_mipSizes, 0u, sizeof(uint) * kMaxMipmapCount);
	m_arrayLayerSize = 0u;
	m_texelSize = 0u;
}

void Image::alloc()
{
	if (m_data) {
		delete[] m_data;
	}

	m_texelSize = GetDataTypeSize(m_dataType) * GetComponentCount(m_layout);
	uint w = m_width, h = m_height, d = m_depth;
	m_arrayLayerSize = 0;
	uint i = 0, lim = PLR_MIN(m_mipmapCount, GetMaxMipmapSize(w, h, d));
	do {
		m_mipOffsets[i] = m_arrayLayerSize;
		m_mipSizes[i] = m_texelSize * w * h * d;
		m_arrayLayerSize += m_mipSizes[i];
		w = PLR_MAX(w / 2u, (uint)1u);
		h = PLR_MAX(h / 2u, (uint)1u);
		d = PLR_MAX(d / 2u, (uint)1u);
		++i;
		PLR_ASSERT(i < kMaxMipmapCount);
	} while (i < lim);

	m_data = new char[m_arrayLayerSize * m_arrayCount];
	PLR_ASSERT(m_data);
	if (!m_data) {
		m_errorState = ErrorState::kBadAlloc;
	}
}

bool Image::validateFileFormat(FileFormat _format) const
{
	switch (_format) {
	case FileFormat::kDds:
		if (m_type == Type::k3dArray) return false;
		return true;
		break;
	case FileFormat::kPng:
		if (m_compression != CompressionType::kNone) return false;
		if (IsDataTypeFloat(m_dataType))  return false;
		if (IsDataTypeSigned(m_dataType)) return false;
		if (IsDataTypeBpc(m_dataType, 16) || IsDataTypeBpc(m_dataType, 32)) return false;
	case FileFormat::kTga:
		if (m_compression != CompressionType::kNone) return false;
		if (IsDataTypeFloat(m_dataType))  return false;
		if (IsDataTypeSigned(m_dataType)) return false;
		if (IsDataTypeBpc(m_dataType, 16) || IsDataTypeBpc(m_dataType, 32)) return false;
		break;
	default:
		break;
	};

	return true;
}

uint Image::GetDataTypeSize(DataType _type)
{
	switch (_type) {
	case DataType::kUint8:
	case DataType::kSint8:   return 1;
	
	case DataType::kUint16:
	case DataType::kSint16:  return 2;
	
	case DataType::kUint32:
	case DataType::kSint32:
	case DataType::kFloat32: return 4;
	
	case DataType::kInvalid:
	default:                return 0;
	};
}

uint Image::GetComponentCount(Layout _layout)
{
	switch (_layout) {
	case Layout::kR:         return 1;
	case Layout::kRG:        return 2;
	case Layout::kRGB:       return 3;
	case Layout::kRGBA:      return 4;
	case Layout::kInvalid:
	default:                return 0;
	};
}


static char lowercase(char c)
{
	if (c >= 'A' && c <= 'Z') {
		c += 'a' - 'A';
	}
	return c;
}
static int strcmp_ignore_case(const char* str1, const char* str2)
{
	PLR_ASSERT(str1 && str2);
	while (*str1 && *str2) {
		int dif = (int)lowercase(*str1) - (int)lowercase(*str2);
		if (dif != 0) {
			return dif;
		}
		++str1;
		++str2;
	}
	return 0;
}

Image::FileFormat Image::GuessFormat(const char* _path)
{
	const char* ext = strrchr(_path, (int)'.');
	if (ext) {
		if (strcmp_ignore_case(ext, ".dds") == 0) {
			return FileFormat::kDds;
		} else if (strcmp_ignore_case(ext, ".png") == 0) {
			return FileFormat::kPng;
		} else if (strcmp_ignore_case(ext, ".tga") == 0) {
			return FileFormat::kTga;
		}
	}

	return FileFormat::kInvalid;
}

bool Image::IsDataTypeFloat(DataType _type)
{
	return _type == DataType::kFloat32;
}
bool Image::IsDataTypeSigned(DataType _type)
{
	return _type >= DataType::kSint8 && _type <= DataType::kSint32;
}
bool Image::IsDataTypeBpc(DataType _type, int _bpc)
{
	switch (_type) {
	case DataType::kUint8:
	case DataType::kSint8:
		return _bpc == 8;
	case DataType::kUint16:
	case DataType::kSint16:
		return _bpc == 16;
	case DataType::kUint32:
	case DataType::kSint32:
	case DataType::kFloat32:
		return _bpc == 32;
	default:
		break;
	}
	return false;
}