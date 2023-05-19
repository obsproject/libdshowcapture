/*
 *  Copyright (C) 2023 Lain Bailey <lain@obsproject.com>
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

#include <vidcap.h>

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
		0x8A80D56F,
		0xFAC5,
		0x4692,
		{
			0xA4,
			0x16,
			0xCF,
			0x20,
			0xD4,
			0xA1,
			0x8F,
			0x47,
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

/* Special thanks to AVerMedia development team */
static bool FindExtensionUnitNodeID(DWORD *pnNode, IBaseFilter *spCapture)
{
	bool bFindNode = false;
	if (!spCapture)
		return bFindNode;

	ComPtr<IKsTopologyInfo> spKsTopologyInfo = NULL;
	HRESULT hr = spCapture->QueryInterface(IID_PPV_ARGS(&spKsTopologyInfo));
	if (spKsTopologyInfo == NULL || FAILED(hr))
		return bFindNode;

	DWORD nNodeNum = 0;
	hr = spKsTopologyInfo->get_NumNodes(&nNodeNum);
	if (FAILED(hr) || nNodeNum <= 0)
		return bFindNode;

	GUID guidNodeType;
	for (int i = 0; i < nNodeNum; i++) {
		spKsTopologyInfo->get_NodeType(i, &guidNodeType);
		if (IsEqualGUID(guidNodeType, KSNODETYPE_DEV_SPECIFIC)) {
			*pnNode = i;
			bFindNode = true;
		}
	}

	return bFindNode;
}

static bool SetTonemapperAvermedia2(IBaseFilter *filter, bool enable)
{
	static constexpr GUID GUID_GC553 = {
		0xC835261B,
		0xFF1C,
		0x4C9A,
		{
			0xB2,
			0xF7,
			0x93,
			0xC9,
			0x1F,
			0xCF,
			0xBE,
			0x77,
		},
	};

	static constexpr int nId = 11;

	ComPtr<IKsControl> spKsControl;
	HRESULT hr = filter->QueryInterface(IID_PPV_ARGS(&spKsControl));
	if (spKsControl == NULL || FAILED(hr))
		return false;

	KSP_NODE ExtensionProp{};
	if (!FindExtensionUnitNodeID(&ExtensionProp.NodeId, filter))
		return false;

	ExtensionProp.Property.Set = GUID_GC553;
	ExtensionProp.Property.Id = nId;
	ExtensionProp.Property.Flags = KSPROPERTY_TYPE_GET |
				       KSPROPERTY_TYPE_TOPOLOGY;

	char pData[20];

	ULONG ulBytesReturned;
	hr = spKsControl->KsProperty(&ExtensionProp.Property,
				     sizeof(ExtensionProp), pData,
				     sizeof(pData), &ulBytesReturned);
	if (FAILED(hr) || (ulBytesReturned < 18))
		return false;

	pData[15] = 0x02;
	pData[17] = enable;

	ExtensionProp.Property.Flags = KSPROPERTY_TYPE_SET |
				       KSPROPERTY_TYPE_TOPOLOGY;
	hr = spKsControl->KsProperty(&ExtensionProp.Property,
				     sizeof(ExtensionProp), pData,
				     sizeof(pData), &ulBytesReturned);
	const bool succeeded = SUCCEEDED(hr);
	if (succeeded)
		Info(L"AVerMedia GC553 tonemapper enable=%d", (int)enable);

	return succeeded;
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
			SetTonemapperAvermedia2(filter, enable);
			SetTonemapperElgato(propertySet, enable);
		}
	}
}

} /* namespace DShow */
