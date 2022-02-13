/*
 *  Copyright (C) 2022 Hugh Bailey <obs.jim@gmail.com>
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

#include "device.hpp"
#include "log.hpp"

#include <cinttypes>

namespace DShow {

#define HDMI_INFOFRAME_TYPE_RESERVED 0x00
#define HDMI_INFOFRAME_TYPE_VS 0x01
#define HDMI_INFOFRAME_TYPE_AVI 0x02
#define HDMI_INFOFRAME_TYPE_SPD 0x03
#define HDMI_INFOFRAME_TYPE_A 0x04
#define HDMI_INFOFRAME_TYPE_MS 0x05
#define HDMI_INFOFRAME_TYPE_VBI 0x06
#define HDMI_INFOFRAME_TYPE_DR 0x07

#define HDMI_DR_EOTF_SDRGAMMA 0x00
#define HDMI_DR_EOTF_HDRGAMMA 0x01
#define HDMI_DR_EOTF_ST2084 0x02
#define HDMI_DR_EOTF_HLG 0x03

typedef struct _HDMI_XY {
	uint8_t xyz[3];
} HDMI_XY;

typedef struct _HDMI_DR1_PAYLOAD {
	uint8_t bfEOTF : 3;
	uint8_t bfReserved1 : 5;

	uint8_t bfMetadataID : 3;
	uint8_t bfReserved2 : 5;

	HDMI_XY xyDisplayPrimaries[3];
	HDMI_XY xyWhitePoint;
	uint16_t wMaxDisplayLuminance;
	uint16_t wMinDisplayLuminance;
	uint16_t wMaxCLL;
	uint16_t wMaxFALL;
} HDMI_DR1_PAYLOAD;

typedef struct _HDMI_INFOFRAMEHEADER {
	uint8_t bfType : 7;
	uint8_t bfPacketType : 1;
	uint8_t bfVersion : 7;
	uint8_t bfChangeBit : 1;
	uint8_t bPayloadLength;
} HDMI_INFOFRAMEHEADER;

typedef struct _HDMI_GENERIC_INFOFRAME {
	HDMI_INFOFRAMEHEADER header;
	uint8_t bChecksum;

	HDMI_DR1_PAYLOAD plDR1;
} HDMI_GENERIC_INFOFRAME;

static bool HDMI_IsInfoFrameValid(const _HDMI_GENERIC_INFOFRAME *pInfoFrame)
{
	if (pInfoFrame == NULL)
		return false;

	unsigned char *data = (unsigned char *)pInfoFrame;
	int size = sizeof(HDMI_INFOFRAMEHEADER) + 1 +
		   pInfoFrame->header.bPayloadLength;

	unsigned char checksum = 0;
	while (size-- != 0)
		checksum += *data++;

	return (checksum == 0);
}

#define VENDOR_HDMI_PACKET_SIZE 32

enum Properties {
	GET_HDMI_HDR_PACKET_00_15 = 720,
	GET_HDMI_HDR_PACKET_16_31 = 721,
};

static constexpr GUID PROPSETID_4K60S_PLUS = {
	0xD1E5209F,
	0x68FD,
	0x4529,
	{
		0xBE,
		0xE0,
		0x5E,
		0x7A,
		0x1F,
		0x47,
		0x92,
		0x24,
	},
};

static HRESULT GetHDMIHDRStatusPacket(IKsPropertySet *propertySet,
				      uint8_t *outBuffer)
{
	DWORD returned;
	HRESULT res = propertySet->Get(PROPSETID_4K60S_PLUS,
				       GET_HDMI_HDR_PACKET_00_15, nullptr, 0,
				       &outBuffer[0], 16, &returned);
	if (SUCCEEDED(res)) {
		res = propertySet->Get(PROPSETID_4K60S_PLUS,
				       GET_HDMI_HDR_PACKET_16_31, nullptr, 0,
				       &outBuffer[16], 16, &returned);
	}

	return res;
}

static bool IsVideoHDRElgato4k60sPlus(IKsPropertySet *propertySet)
{
	bool isHDR = false;

	// Try to read HDR meta data
	static const uint8_t emptyBuffer[VENDOR_HDMI_PACKET_SIZE] = {0};
	uint8_t buffer[VENDOR_HDMI_PACKET_SIZE] = {0};
	HRESULT res = GetHDMIHDRStatusPacket(propertySet, buffer);
	if (SUCCEEDED(res)) {
		HDMI_GENERIC_INFOFRAME *frame =
			(_HDMI_GENERIC_INFOFRAME *)(&buffer[0]);
		bool isInfoFrameValid = HDMI_IsInfoFrameValid(frame);
		if (isInfoFrameValid) {
			// Check type in header and EOTF flag in payload
			if ((HDMI_INFOFRAME_TYPE_DR == frame->header.bfType) &&
			    HDMI_DR_EOTF_SDRGAMMA != frame->plDR1.bfEOTF) {
				isHDR = true;
			} else if (HDMI_INFOFRAME_TYPE_DR ==
					   frame->header.bfType &&
				   HDMI_DR_EOTF_SDRGAMMA ==
					   frame->plDR1.bfEOTF) {
			} else if (HDMI_INFOFRAME_TYPE_RESERVED ==
					   frame->header.bfType &&
				   (0 == memcmp(buffer, emptyBuffer,
						sizeof(buffer)))) {
			} else if (HDMI_INFOFRAME_TYPE_DR !=
				   frame->header.bfType) {
				Warning(L"HDMI Metadata:  Wrong header type: %d",
					(int)frame->header.bfType);
			}
		} else {
			Warning(L"HDMI Metadata: HDMI_IsInfoFrameValid() returned error (checksum)!");
		}
	}

	return isHDR;
}

bool IsVendorVideoHDR(IKsPropertySet *propertySet)
{
	return IsVideoHDRElgato4k60sPlus(propertySet);
}

static void SetVideoFormatElgato4k60sPlus(IKsPropertySet *propertySet,
					  bool hevcTrueAvcFalse)
{
	uint32_t propHevcTrueAvcFalse = hevcTrueAvcFalse;
	const HRESULT hr = propertySet->Set(PROPSETID_4K60S_PLUS, 400, nullptr,
					    0, &propHevcTrueAvcFalse,
					    sizeof(propHevcTrueAvcFalse));
	if (SUCCEEDED(hr))
		Info(L"Elgato tonemapper Enable=%" PRIu64,
		     propHevcTrueAvcFalse);
}

void SetVendorVideoFormat(IKsPropertySet *propertySet, bool hevcTrueAvcFalse)
{
	return SetVideoFormatElgato4k60sPlus(propertySet, hevcTrueAvcFalse);
}

static void SetTonemapperAvermedia(IKsPropertySet *propertySet, bool enable)
{
	typedef struct _KSPROPERTY_AVER_HW_HDR2SDR {
		KSPROPERTY Property;
		DWORD Enable;
	} KSPROPERTY_AVER_HW_HDR2SDR, *PKSPROPERTY_AVER_HW_HDR2SDR;

	static constexpr GUID KSPROPSETID_AVER_HDR_PROPERTY = {
		0X8A80D56F,
		0XFAC5,
		0X4692,
		{
			0XA4,
			0X16,
			0XCF,
			0X20,
			0XD4,
			0XA1,
			0X8F,
			0X47,
		},
	};
	KSPROPERTY_AVER_HW_HDR2SDR data{};
	data.Enable = enable;
	const HRESULT hr = propertySet->Set(
		KSPROPSETID_AVER_HDR_PROPERTY, 2, &data.Enable,
		sizeof(data) - sizeof(data.Property), &data, sizeof(data));
	if (SUCCEEDED(hr))
		Info(L"AVerMedia tonemapper Enable=%lu", data.Enable);
}

static void SetTonemapperElgato(IKsPropertySet *propertySet, bool enable)
{
	static constexpr GUID PROPSETID_4K60PROMK2 = {
		0xD1E5209F,
		0x68FD,
		0x4529,
		{
			0xBE,
			0xE0,
			0x5E,
			0x7A,
			0x1F,
			0x47,
			0x92,
			0x26,
		},
	};
	uint32_t propEnable = enable;
	const HRESULT hr = propertySet->Set(PROPSETID_4K60PROMK2, 722, nullptr,
					    0, &propEnable, sizeof(propEnable));
	if (SUCCEEDED(hr))
		Info(L"Elgato tonemapper Enable=%" PRIu32, propEnable);
}

void SetVendorTonemapperUsage(IBaseFilter *filter, bool enable)
{
	if (filter) {
		ComPtr<IKsPropertySet> propertySet =
			ComQIPtr<IKsPropertySet>(filter);
		if (propertySet) {
			SetTonemapperAvermedia(propertySet, enable);
			SetTonemapperElgato(propertySet, enable);
		}
	}
}

} /* namespace DShow */
