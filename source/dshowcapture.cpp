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

#include "../dshowcapture.hpp"
#include "dshow-base.hpp"
#include "dshow-enum.hpp"
#include "device.hpp"
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

static void OpenPropertyPages(HWND hwnd, IUnknown *propertyObject)
{
	if (!propertyObject)
		return;

	CComQIPtr<ISpecifyPropertyPages> pages(propertyObject);
	CAUUID cauuid;

	if (pages != NULL) {
		if (SUCCEEDED(pages->GetPages(&cauuid)) && cauuid.cElems) {
			OleCreatePropertyFrame(hwnd, 0, 0, NULL, 1,
					(LPUNKNOWN*)&propertyObject,
					cauuid.cElems, cauuid.pElems,
					0, 0, NULL);
			CoTaskMemFree(cauuid.pElems);
		}
	}
}

void Device::OpenDialog(void *hwnd, DialogType type) const
{
	CComPtr<IUnknown> ptr;
	HRESULT           hr;

	if (type == DialogType::ConfigVideo) {
		ptr = context->videoFilter;
	} else if (type == DialogType::ConfigCrossbar ||
	           type == DialogType::ConfigCrossbar2) {
		hr = context->builder->FindInterface(NULL, NULL,
				context->videoFilter, IID_IAMCrossbar,
				(void**)&ptr);
		if (FAILED(hr)) {
			WarningHR(L"Failed to find crossbar", hr);
			return;
		}

		if (ptr != NULL && type == DialogType::ConfigCrossbar2) {
			CComQIPtr<IAMCrossbar> xbar(ptr);
			CComQIPtr<IBaseFilter> filter(xbar);

			hr = context->builder->FindInterface(
					&LOOK_UPSTREAM_ONLY,
					NULL, filter, IID_IAMCrossbar,
					(void**)&ptr);
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

static bool EnumVideoDevice(std::vector<VideoDevice> &devices,
		IBaseFilter *filter,
		const wchar_t *deviceName,
		const wchar_t *devicePath)
{
	CComPtr<IPin> pin;
	VideoDevice   info;

	bool success = GetFilterPin(filter, MEDIATYPE_Video,
			PIN_CATEGORY_CAPTURE, PINDIR_OUTPUT, &pin);
	if (!success)
		return true;

	if (!EnumVideoCaps(pin, info.caps))
		return true;

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

static bool EnumAudioDevice(vector<AudioDevice> &devices,
		IBaseFilter *filter,
		const wchar_t *deviceName,
		const wchar_t *devicePath)
{
	CComPtr<IPin> pin;
	AudioDevice   info;

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
