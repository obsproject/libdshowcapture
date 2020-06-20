/*
 *  Copyright (C) 2014 Hugh Bailey <obs.jim@gmail.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include "dshow-formats.hpp"
#include "dshow-media-type.hpp"

#ifndef __MINGW32__

const GUID MEDIASUBTYPE_RAW_AAC1 = {0x000000FF,
				    0x0000,
				    0x0010,
				    {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b,
				     0x71}};

const GUID MEDIASUBTYPE_I420 = {0x30323449,
				0x0000,
				0x0010,
				{0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b,
				 0x71}};

const GUID MEDIASUBTYPE_DVM = {0x00002000,
			       0x0000,
			       0x0010,
			       {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b,
				0x71}};

#endif

const GUID MEDIASUBTYPE_Y800 = {0x30303859,
				0x0000,
				0x0010,
				{0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b,
				 0x71}};

namespace DShow {

DWORD VFormatToFourCC(VideoFormat format)
{
	switch (format) {
	/* raw formats */
	case VideoFormat::ARGB:
		return MAKEFOURCC('A', 'R', 'G', 'B');
	case VideoFormat::XRGB:
		return MAKEFOURCC('R', 'G', 'B', '4');

	/* planar YUV formats */
	case VideoFormat::I420:
		return MAKEFOURCC('I', '4', '2', '0');
	case VideoFormat::NV12:
		return MAKEFOURCC('N', 'V', '1', '2');
	case VideoFormat::YV12:
		return MAKEFOURCC('Y', 'V', '1', '2');
	case VideoFormat::Y800:
		return MAKEFOURCC('Y', '8', '0', '0');

	/* packed YUV formats */
	case VideoFormat::YVYU:
		return MAKEFOURCC('Y', 'V', 'Y', 'U');
	case VideoFormat::YUY2:
		return MAKEFOURCC('Y', 'U', 'Y', '2');
	case VideoFormat::UYVY:
		return MAKEFOURCC('U', 'Y', 'V', 'Y');
	case VideoFormat::HDYC:
		return MAKEFOURCC('H', 'D', 'Y', 'C');

	/* encoded formats */
	case VideoFormat::MJPEG:
		return MAKEFOURCC('M', 'J', 'P', 'G');
	case VideoFormat::H264:
		return MAKEFOURCC('H', '2', '6', '4');

	default:
		return 0;
	}
}

GUID VFormatToSubType(VideoFormat format)
{
	switch (format) {
	/* raw formats */
	case VideoFormat::ARGB:
		return MEDIASUBTYPE_ARGB32;
	case VideoFormat::XRGB:
		return MEDIASUBTYPE_RGB32;

	/* planar YUV formats */
	case VideoFormat::I420:
		return MEDIASUBTYPE_I420;
	case VideoFormat::NV12:
		return MEDIASUBTYPE_NV12;
	case VideoFormat::YV12:
		return MEDIASUBTYPE_YV12;
	case VideoFormat::Y800:
		return MEDIASUBTYPE_Y800;

	/* packed YUV formats */
	case VideoFormat::YVYU:
		return MEDIASUBTYPE_YVYU;
	case VideoFormat::YUY2:
		return MEDIASUBTYPE_YUY2;
	case VideoFormat::UYVY:
		return MEDIASUBTYPE_UYVY;

	/* encoded formats */
	case VideoFormat::MJPEG:
		return MEDIASUBTYPE_MJPG;
	case VideoFormat::H264:
		return MEDIASUBTYPE_H264;

	default:
		return GUID();
	}
}

WORD VFormatBits(VideoFormat format)
{
	switch (format) {
	/* raw formats */
	case VideoFormat::ARGB:
	case VideoFormat::XRGB:
		return 32;

	/* planar YUV formats */
	case VideoFormat::I420:
	case VideoFormat::NV12:
	case VideoFormat::YV12:
		return 12;
	case VideoFormat::Y800:
		return 8;

	/* packed YUV formats */
	case VideoFormat::YVYU:
	case VideoFormat::YUY2:
	case VideoFormat::UYVY:
		return 16;

	default:
		return 0;
	}
}

WORD VFormatPlanes(VideoFormat format)
{
	switch (format) {
	/* raw formats */
	case VideoFormat::ARGB:
	case VideoFormat::XRGB:
		return 1;

	/* planar YUV formats */
	case VideoFormat::I420:
		return 3;
	case VideoFormat::NV12:
	case VideoFormat::YV12:
		return 2;
	case VideoFormat::Y800:
		return 1;

	/* packed YUV formats */
	case VideoFormat::YVYU:
	case VideoFormat::YUY2:
	case VideoFormat::UYVY:
		return 1;

	default:
		return 0;
	}
}

static bool GetFourCCVFormat(DWORD fourCC, VideoFormat &format)
{
	switch (fourCC) {
	/* raw formats */
	case MAKEFOURCC('R', 'G', 'B', '2'):
		format = VideoFormat::XRGB;
		break;
	case MAKEFOURCC('R', 'G', 'B', '4'):
		format = VideoFormat::XRGB;
		break;
	case MAKEFOURCC('A', 'R', 'G', 'B'):
		format = VideoFormat::ARGB;
		break;

	/* planar YUV formats */
	case MAKEFOURCC('I', '4', '2', '0'):
	case MAKEFOURCC('I', 'Y', 'U', 'V'):
		format = VideoFormat::I420;
		break;
	case MAKEFOURCC('Y', 'V', '1', '2'):
		format = VideoFormat::YV12;
		break;
	case MAKEFOURCC('N', 'V', '1', '2'):
		format = VideoFormat::NV12;
		break;
	case MAKEFOURCC('Y', '8', '0', '0'):
		format = VideoFormat::Y800;
		break;

	/* packed YUV formats */
	case MAKEFOURCC('Y', 'V', 'Y', 'U'):
		format = VideoFormat::YVYU;
		break;
	case MAKEFOURCC('Y', 'U', 'Y', '2'):
		format = VideoFormat::YUY2;
		break;
	case MAKEFOURCC('U', 'Y', 'V', 'Y'):
		format = VideoFormat::UYVY;
		break;
	case MAKEFOURCC('H', 'D', 'Y', 'C'):
		format = VideoFormat::HDYC;
		break;

	/* compressed formats */
	case MAKEFOURCC('H', '2', '6', '4'):
		format = VideoFormat::H264;
		break;

	/* compressed formats that can automatically create intermediary
	 * filters for decompression */
	case MAKEFOURCC('M', 'J', 'P', 'G'):
		format = VideoFormat::MJPEG;
		break;

	default:
		return false;
	}

	return true;
}

bool GetMediaTypeVFormat(const AM_MEDIA_TYPE &mt, VideoFormat &format)
{
	if (mt.majortype != MEDIATYPE_Video)
		return false;

	const BITMAPINFOHEADER *bmih = GetBitmapInfoHeader(mt);

	format = VideoFormat::Unknown;

	/* raw formats */
	if (mt.subtype == MEDIASUBTYPE_RGB24)
		format = VideoFormat::XRGB;
	else if (mt.subtype == MEDIASUBTYPE_RGB32)
		format = VideoFormat::XRGB;
	else if (mt.subtype == MEDIASUBTYPE_ARGB32)
		format = VideoFormat::ARGB;

	/* planar YUV formats */
	else if (mt.subtype == MEDIASUBTYPE_I420)
		format = VideoFormat::I420;
	else if (mt.subtype == MEDIASUBTYPE_IYUV)
		format = VideoFormat::I420;
	else if (mt.subtype == MEDIASUBTYPE_YV12)
		format = VideoFormat::YV12;
	else if (mt.subtype == MEDIASUBTYPE_NV12)
		format = VideoFormat::NV12;
	else if (mt.subtype == MEDIASUBTYPE_Y800)
		format = VideoFormat::Y800;

	/* packed YUV formats */
	else if (mt.subtype == MEDIASUBTYPE_YVYU)
		format = VideoFormat::YVYU;
	else if (mt.subtype == MEDIASUBTYPE_YUY2)
		format = VideoFormat::YUY2;
	else if (mt.subtype == MEDIASUBTYPE_UYVY)
		format = VideoFormat::UYVY;

	/* compressed formats */
	else if (mt.subtype == MEDIASUBTYPE_H264)
		format = VideoFormat::H264;

	/* compressed formats that can automatically create intermediary
	 * filters for decompression */
	else if (mt.subtype == MEDIASUBTYPE_MJPG)
		format = VideoFormat::MJPEG;

	/* no valid types, check fourcc value instead */
	else
		return bmih ? GetFourCCVFormat(bmih->biCompression, format)
			    : false;

	return true;
}

}; /* namespace DShow */
