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

#include <ElgatoUVCDevice.h>
#include "../external/capture-device-support/SampleCode/DriverInterface.h"
#include "device.hpp"
#include "log.hpp"

#include <cinttypes>

namespace DShow {

bool IsVendorVideoHDR(IKsPropertySet *propertySet)
{
	EGAVDeviceProperties properties(
		propertySet, EGAVDeviceProperties::DeviceType::GC4K60SPlus);
	bool isHDR = false;
	return SUCCEEDED(properties.IsVideoHDR(isHDR)) ? isHDR : false;
}

void SetVendorVideoFormat(IKsPropertySet *propertySet, bool hevcTrueAvcFalse)
{
	EGAVDeviceProperties properties(
		propertySet, EGAVDeviceProperties::DeviceType::GC4K60SPlus);
	const HRESULT hr = properties.SetEncoderType(hevcTrueAvcFalse);
	if (SUCCEEDED(hr)) {
		Info(L"Elgato GC4K60SPlus encoder type=%ls",
		     hevcTrueAvcFalse ? L"HEVC" : L"AVC");
	}
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
		Info(L"AVerMedia tonemapper enable=%lu", data.Enable);
}

static void SetTonemapperElgato(IKsPropertySet *propertySet, bool enable)
{
	EGAVDeviceProperties properties(
		propertySet, EGAVDeviceProperties::DeviceType::GC4K60ProMK2);
	const HRESULT hr = properties.SetHDRTonemapping(enable);
	if (SUCCEEDED(hr)) {
		Info(L"Elgato GC4K60ProMK2 tonemapper enable=%d", (int)enable);
	} else {
		std::shared_ptr<EGAVHIDInterface> hid =
			CreateEGAVHIDInterface();
		if (hid->InitHIDInterface(deviceIDHD60SPlus).Succeeded()) {
			ElgatoUVCDevice device(hid, false);
			device.SetHDRTonemappingEnabled(enable);
			Info(L"Elgato HD60SPlus tonemapper enable=%d",
			     (int)enable);
		}
	}
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
