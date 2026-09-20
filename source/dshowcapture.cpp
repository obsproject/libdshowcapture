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

#include "../dshowcapture.hpp"
#include "dshow-base.hpp"
#include "dshow-enum.hpp"
#include "device.hpp"
#include "dshow-device-defs.hpp"
#include "log.hpp"

#include <vector>

namespace DShow {

Device::Device(InitGraph initialize) : context(new HDevice)
{
	if (initialize == InitGraph::True)
		context->CreateGraph();
}

Device::~Device()
{
	delete context;
}

bool Device::Valid() const
{
	return context->initialized;
}

bool Device::ResetGraph()
{
	/* cheap and easy way to clear all the filters */
	delete context;
	context = new HDevice;

	return context->CreateGraph();
}

void Device::ShutdownGraph()
{
	delete context;
	context = new HDevice;
}

bool Device::SetVideoConfig(VideoConfig *config)
{
	return context->SetVideoConfig(config);
}

bool Device::SetAudioConfig(AudioConfig *config)
{
	return context->SetAudioConfig(config);
}

bool Device::ConnectFilters()
{
	return context->ConnectFilters();
}

Result Device::Start()
{
	return context->Start();
}

void Device::Stop()
{
	context->Stop();
}

bool Device::GetVideoConfig(VideoConfig &config) const
{
	if (context->videoCapture == NULL)
		return false;

	config = context->videoConfig;
	return true;
}

bool Device::GetAudioConfig(AudioConfig &config) const
{
	if (context->audioCapture == NULL)
		return false;

	config = context->audioConfig;
	return true;
}

bool Device::GetVideoDeviceId(DeviceId &id) const
{
	if (context->videoCapture == NULL)
		return false;

	id = context->videoConfig;
	return true;
}

bool Device::GetAudioDeviceId(DeviceId &id) const
{
	if (context->audioCapture == NULL)
		return false;

	id = context->audioConfig;
	return true;
}

struct VideoPropertyRange {
	long min = 0;
	long max = 0;
	long step = 0;
	long def = 0;
	long caps = 0;

	bool Contains(long value) const
	{
		return step > 0 && value >= min && value <= max &&
		       (static_cast<long long>(value) - min) % step == 0;
	}

	bool Supports(long flags) const
	{
		return (flags == CameraControl_Flags_Auto ||
			flags == CameraControl_Flags_Manual) &&
		       (caps & flags) != 0;
	}
};

template<typename Control>
bool GetVideoProperty(Control *control, long id, VideoDeviceProperty &property,
		      VideoPropertyRange &range)
{
	if (control->GetRange(id, &range.min, &range.max, &range.step,
			      &range.def, &range.caps) != S_OK ||
	    range.min > range.max || range.step <= 0) {
		return false;
	}

	property.property = id;
	if (control->Get(id, &property.val, &property.flags) != S_OK ||
	    !range.Supports(property.flags)) {
		return false;
	}

	range.def = range.Contains(range.def) ? range.def : range.min;
	if (property.flags == CameraControl_Flags_Auto)
		property.val = range.def;

	return range.Contains(property.val);
}

template<typename Control>
bool GetVideoProperties(Control *control, long lastProperty,
			std::vector<VideoDeviceProperty> &properties)
{
	if (!control)
		return false;
	for (long id = 0; id <= lastProperty; ++id) {
		VideoDeviceProperty property;
		VideoPropertyRange range;
		if (GetVideoProperty(control, id, property, range)) {
			properties.push_back(property);
		}
	}
	return true;
}

template<typename Control>
bool SetVideoProperties(Control *control, long lastProperty,
			const std::vector<VideoDeviceProperty> &properties)
{
	if (!control)
		return false;
	bool success = true;
	for (const auto &property : properties) {
		VideoDeviceProperty current;
		VideoPropertyRange range;
		if (property.property < 0 || property.property > lastProperty ||
		    !GetVideoProperty(control, property.property, current,
				      range) ||
		    !range.Supports(property.flags)) {
			success = false;
			continue;
		}

		const bool automatic = property.flags ==
				       CameraControl_Flags_Auto;
		const long value = automatic ? range.def : property.val;
		if (current.flags == property.flags &&
		    (automatic || current.val == value)) {
			continue;
		}

		if (!range.Contains(value) ||
		    control->Set(property.property, value, property.flags) !=
			    S_OK) {
			success = false;
		}
	}
	return success;
}

bool Device::GetVideoProperties(
	VideoPropertyType type,
	std::vector<VideoDeviceProperty> &properties) const
{
	properties.clear();
	if (!context->active || !context->videoFilter)
		return false;
	switch (type) {
	case VideoPropertyType::CameraControl:
		return DShow::GetVideoProperties(
			ComQIPtr<IAMCameraControl>(context->videoFilter).Get(),
			CameraControl_Focus, properties);
	case VideoPropertyType::VideoProcAmp:
		return DShow::GetVideoProperties(
			ComQIPtr<IAMVideoProcAmp>(context->videoFilter).Get(),
			VideoProcAmp_Gain, properties);
	}
	return false;
}

bool Device::SetVideoProperties(
	VideoPropertyType type,
	const std::vector<VideoDeviceProperty> &properties)
{
	if (!context->active || !context->videoFilter)
		return false;
	switch (type) {
	case VideoPropertyType::CameraControl:
		return DShow::SetVideoProperties(
			ComQIPtr<IAMCameraControl>(context->videoFilter).Get(),
			CameraControl_Focus, properties);
	case VideoPropertyType::VideoProcAmp:
		return DShow::SetVideoProperties(
			ComQIPtr<IAMVideoProcAmp>(context->videoFilter).Get(),
			VideoProcAmp_Gain, properties);
	}
	return false;
}

static void OpenPropertyPages(HWND hwnd, IUnknown *propertyObject)
{
	if (!propertyObject)
		return;

	ComQIPtr<ISpecifyPropertyPages> pages(propertyObject);
	CAUUID cauuid;

	if (pages != NULL) {
		if (SUCCEEDED(pages->GetPages(&cauuid)) && cauuid.cElems) {
			OleCreatePropertyFrame(hwnd, 0, 0, NULL, 1,
					       (LPUNKNOWN *)&propertyObject,
					       cauuid.cElems, cauuid.pElems, 0,
					       0, NULL);
			CoTaskMemFree(cauuid.pElems);
		}
	}
}

void Device::OpenDialog(void *hwnd, DialogType type) const
{
	ComPtr<IUnknown> ptr;
	HRESULT hr;

	if (type == DialogType::ConfigVideo) {
		ptr = context->videoFilter;
	} else if (type == DialogType::ConfigCrossbar ||
		   type == DialogType::ConfigCrossbar2) {
		hr = context->builder->FindInterface(NULL, NULL,
						     context->videoFilter,
						     IID_IAMCrossbar,
						     (void **)&ptr);
		if (FAILED(hr)) {
			WarningHR(L"Failed to find crossbar", hr);
			return;
		}

		if (ptr != NULL && type == DialogType::ConfigCrossbar2) {
			ComQIPtr<IAMCrossbar> xbar(ptr);
			ComQIPtr<IBaseFilter> filter(xbar);

			hr = context->builder->FindInterface(
				&LOOK_UPSTREAM_ONLY, NULL, filter,
				IID_IAMCrossbar, (void **)&ptr);
			if (FAILED(hr)) {
				WarningHR(L"Failed to find crossbar2", hr);
				return;
			}
		}
	} else if (type == DialogType::ConfigAudio) {
		ptr = context->audioFilter;
	}

	if (!ptr) {
		Warning(L"Could not find filter to open dialog type: %d with",
			(int)type);
		return;
	}

	OpenPropertyPages((HWND)hwnd, ptr);
}

static void EnumEncodedVideo(std::vector<VideoDevice> &devices,
			     const wchar_t *deviceName,
			     const wchar_t *devicePath,
			     const EncodedDevice &info)
{
	VideoDevice device;
	VideoInfo caps;

	device.name = deviceName;
	device.path = devicePath;
	device.audioAttached = true;
	device.separateAudioFilter = false;

	caps.minCX = caps.maxCX = info.width;
	caps.minCY = caps.maxCY = info.height;
	caps.granularityCX = caps.granularityCY = 1;
	caps.minInterval = caps.maxInterval = info.frameInterval;
	caps.format = info.videoFormat;

	device.caps.push_back(caps);
	devices.push_back(device);
}

static void EnumExceptionVideoDevice(std::vector<VideoDevice> &devices,
				     IBaseFilter *filter,
				     const wchar_t *deviceName,
				     const wchar_t *devicePath)
{
	ComPtr<IPin> pin;

	if (GetPinByName(filter, PINDIR_OUTPUT, L"656", &pin))
		EnumEncodedVideo(devices, deviceName, devicePath, HD_PVR2);

	else if (GetPinByName(filter, PINDIR_OUTPUT, L"TS Out", &pin))
		EnumEncodedVideo(devices, deviceName, devicePath, Roxio);
}

static bool EnumVideoDevice(std::vector<VideoDevice> &devices,
			    IBaseFilter *filter, const wchar_t *deviceName,
			    const wchar_t *devicePath)
{
	ComPtr<IPin> pin;
	ComPtr<IPin> audioPin;
	ComPtr<IBaseFilter> audioFilter;
	VideoDevice info;

	if (wcsstr(deviceName, L"C875") != nullptr ||
	    wcsstr(deviceName, L"Prif Streambox") != nullptr ||
	    wcsstr(deviceName, L"C835") != nullptr) {
		EnumEncodedVideo(devices, deviceName, devicePath, AV_LGP);
		return true;

	} else if (wcsstr(deviceName, L"Hauppauge HD PVR Capture") != nullptr) {
		EnumEncodedVideo(devices, deviceName, devicePath, HD_PVR1);
		return true;
	}

	bool success = GetFilterPin(filter, MEDIATYPE_Video,
				    PIN_CATEGORY_CAPTURE, PINDIR_OUTPUT, &pin);

	/* if this device has no standard capture pin, see if it's an
	 * encoded device, and get its information if so (all encoded devices
	 * are exception devices pretty much) */
	if (!success) {
		EnumExceptionVideoDevice(devices, filter, deviceName,
					 devicePath);
		return true;
	}

	if (!EnumVideoCaps(pin, info.caps))
		return true;

	info.audioAttached = GetFilterPin(filter, MEDIATYPE_Audio,
					  PIN_CATEGORY_CAPTURE, PINDIR_OUTPUT,
					  &audioPin);

	// Fallback: Find a corresponding audio filter for the same device
	if (!info.audioAttached) {
		info.separateAudioFilter =
			GetDeviceAudioFilter(devicePath, &audioFilter);
		info.audioAttached = info.separateAudioFilter;
	}

	info.name = deviceName;
	if (devicePath)
		info.path = devicePath;

	devices.push_back(info);
	return true;
}

bool Device::EnumVideoDevices(std::vector<VideoDevice> &devices)
{
	devices.clear();
	return EnumDevices(CLSID_VideoInputDeviceCategory,
			   EnumDeviceCallback(EnumVideoDevice), &devices);
}

static bool EnumAudioDevice(vector<AudioDevice> &devices, IBaseFilter *filter,
			    const wchar_t *deviceName,
			    const wchar_t *devicePath)
{
	ComPtr<IPin> pin;
	AudioDevice info;

	bool success = GetFilterPin(filter, MEDIATYPE_Audio,
				    PIN_CATEGORY_CAPTURE, PINDIR_OUTPUT, &pin);
	if (!success)
		return true;

	if (!EnumAudioCaps(pin, info.caps))
		return true;

	info.name = deviceName;
	if (devicePath)
		info.path = devicePath;

	devices.push_back(info);
	return true;
}

bool Device::EnumAudioDevices(vector<AudioDevice> &devices)
{
	devices.clear();
	return EnumDevices(CLSID_AudioInputDeviceCategory,
			   EnumDeviceCallback(EnumAudioDevice), &devices);
}

}; /* namespace DShow */
