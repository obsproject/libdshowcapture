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

#include "dshow-base.hpp"
#include "dshow-enum.hpp"

#include <vector>
#include <string>

using namespace std;

namespace DShow {

struct DeviceFilterCallbackInfo {
	CComPtr<IBaseFilter> filter;
	const wchar_t        *name;
	const wchar_t        *path;
};

static bool GetDeviceCallback(DeviceFilterCallbackInfo &info,
		IBaseFilter *filter, const wchar_t *name, const wchar_t *path)
{
	if (info.name && wcscmp(name, info.name) != 0)
		return true;

	info.filter = filter;

	/* continue if path does not match */
	if (!path || !info.path || wcscmp(path, info.path) != 0)
		return true;

	return false;
}

bool GetDeviceFilter(const IID &type, const wchar_t *name, const wchar_t *path,
		IBaseFilter **out)
{
	DeviceFilterCallbackInfo info;
	info.name = name;
	info.path = path;

	if (!EnumDevices(type, EnumDeviceCallback(GetDeviceCallback), &info))
		return false;

	if (info.filter != NULL) {
		*out = info.filter.Detach();
		return true;
	}

	return false;
}

/* checks to see if a pin's config caps have a specific media type */
static bool PinConfigHasMajorType(IPin *pin, const GUID &type)
{
	HRESULT hr;
	CComPtr<IAMStreamConfig> config;
	int count, size;

	hr = pin->QueryInterface(IID_IAMStreamConfig, (void**)&config);
	if (FAILED(hr))
		return false;

	hr = config->GetNumberOfCapabilities(&count, &size);
	if (FAILED(hr))
		return false;

	vector<BYTE> caps;
	caps.resize(size);

	for (int i = 0; i < count; i++) {
		MediaTypePtr mt;
		if (SUCCEEDED(config->GetStreamCaps(i, &mt, caps.data())))
			if (mt->majortype == type)
				return true;
	}

	return false;
}

/* checks to see if a pin has a certain major media type */
static bool PinHasMajorType(IPin *pin, const GUID &type)
{
	HRESULT hr;
	MediaTypePtr mt;
	CComPtr<IEnumMediaTypes> mediaEnum;

	/* first, check the config caps. */
	if (PinConfigHasMajorType(pin, type))
		return true;

	/* then let's check the media type for the pin */
	if (FAILED(pin->EnumMediaTypes(&mediaEnum)))
		return false;

	ULONG curVal;
	hr = mediaEnum->Next(1, &mt, &curVal);
	if (hr != S_OK)
		return false;

	return mt->majortype == type;
}

static inline bool PinIsDirection(IPin *pin, PIN_DIRECTION dir)
{
	if (!pin)
		return false;

	PIN_DIRECTION pinDir;
	return SUCCEEDED(pin->QueryDirection(&pinDir)) && pinDir == dir;
}

static bool GetPinCategory(IPin *pin, GUID &category)
{
	if (!pin)
		return false;

	CComQIPtr<IKsPropertySet> propertySet(pin);
	PIN_INFO                  pinInfo;
	DWORD                     size;
	HRESULT                   hr;

	if (propertySet == NULL)
		return false;

	if (FAILED(pin->QueryPinInfo(&pinInfo)))
		return false;

	if (pinInfo.pFilter)
		pinInfo.pFilter->Release();

	hr = propertySet->Get(AMPROPSETID_Pin, AMPROPERTY_PIN_CATEGORY,
			NULL, 0, &category, sizeof(GUID), &size);
	return SUCCEEDED(hr);
}

static inline bool PinIsCategory(IPin *pin, const GUID &category)
{
	if (!pin) return false;

	GUID pinCategory;
	if (!GetPinCategory(pin, pinCategory))
		return false;

	return category == pinCategory;
}

static inline bool PinMatches(IPin *pin, const GUID &type, const GUID &category,
		PIN_DIRECTION &dir)
{
	if (!PinHasMajorType(pin, type))
		return false;
	if (!PinIsDirection(pin, dir))
		return false;
	if (!PinIsCategory(pin, category))
		return false;

	return true;
}

bool GetFilterPin(IBaseFilter *filter, const GUID &type, const GUID &category,
		PIN_DIRECTION dir, IPin **pin)
{
	CComPtr<IPin>      curPin;
	CComPtr<IEnumPins> pinsEnum;
	ULONG              num;

	if (!filter)
		return false;
	if (FAILED(filter->EnumPins(&pinsEnum)))
		return false;

	while (pinsEnum->Next(1, &curPin, &num) == S_OK) {

		if (PinMatches(curPin, type, category, dir)) {
			*pin = curPin;
			(*pin)->AddRef();
			return true;
		}

		curPin.Release();
	}

	return false;
}

wstring ConvertHRToEnglish(HRESULT hr)
{
	LPWSTR buffer = NULL;
	wstring str;

	FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, hr, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
			(LPTSTR)&buffer, 0, NULL);

	if (buffer) {
		str = buffer;
		LocalFree(buffer);
	}

	return str.c_str();
}

}; /* namespace DShow */
