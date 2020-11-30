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
#include "log.hpp"

#include <bdaiface.h>

#include <vector>
#include <string>

#include <mmddk.h>    // for DRV_QUERYDEVICEINTERFACE
#include <SetupAPI.h> // for SetupDixxx
#include <cfgmgr32.h> // for CM_xxx
#include <algorithm>  // for std::transform

#pragma comment(lib, "winmm.lib")    // for waveInMessage
#pragma comment(lib, "setupapi.lib") // for SetupDixxx

using namespace std;

namespace DShow {

bool CreateFilterGraph(IGraphBuilder **pgraph, ICaptureGraphBuilder2 **pbuilder,
		       IMediaControl **pcontrol)
{
	ComPtr<IGraphBuilder> graph;
	ComPtr<ICaptureGraphBuilder2> builder;
	ComPtr<IMediaControl> control;
	HRESULT hr;

	hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,
			      IID_IFilterGraph, (void **)&graph);
	if (FAILED(hr)) {
		ErrorHR(L"Failed to create IGraphBuilder", hr);
		return false;
	}

	hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL,
			      CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2,
			      (void **)&builder);
	if (FAILED(hr)) {
		ErrorHR(L"Failed to create ICaptureGraphBuilder2", hr);
		return false;
	}

	hr = builder->SetFiltergraph(graph);
	if (FAILED(hr)) {
		ErrorHR(L"Failed to set filter graph", hr);
		return false;
	}

	hr = graph->QueryInterface(IID_IMediaControl, (void **)&control);
	if (FAILED(hr)) {
		ErrorHR(L"Failed to create IMediaControl", hr);
		return false;
	}

	*pgraph = graph.Detach();
	*pbuilder = builder.Detach();
	*pcontrol = control.Detach();
	return true;
}

void LogFilters(IGraphBuilder *graph)
{
	ComPtr<IEnumFilters> filterEnum;
	ComPtr<IBaseFilter> filter;
	HRESULT hr;

	hr = graph->EnumFilters(&filterEnum);
	if (FAILED(hr))
		return;

	Debug(L"Loaded filters:");

	while (filterEnum->Next(1, &filter, NULL) == S_OK) {
		FILTER_INFO filterInfo;

		hr = filter->QueryFilterInfo(&filterInfo);
		if (SUCCEEDED(hr)) {
			if (filterInfo.pGraph)
				filterInfo.pGraph->Release();

			Debug(L"\t%s", filterInfo.achName);
		}
	}
}

struct DeviceFilterCallbackInfo {
	ComPtr<IBaseFilter> filter;
	const wchar_t *name;
	const wchar_t *path;
};

static bool GetDeviceCallback(DeviceFilterCallbackInfo &info,
			      IBaseFilter *filter, const wchar_t *name,
			      const wchar_t *path)
{
	if (info.name && *info.name && wcscmp(name, info.name) != 0)
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
	ComPtr<IAMStreamConfig> config;
	int count, size;

	hr = pin->QueryInterface(IID_IAMStreamConfig, (void **)&config);
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
	ComPtr<IEnumMediaTypes> mediaEnum;

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

static HRESULT GetPinCategory(IPin *pin, GUID &category)
{
	if (!pin)
		return E_POINTER;

	ComQIPtr<IKsPropertySet> propertySet(pin);
	DWORD size;

	if (propertySet == NULL)
		return E_NOINTERFACE;

	return propertySet->Get(AMPROPSETID_Pin, AMPROPERTY_PIN_CATEGORY, NULL,
				0, &category, sizeof(GUID), &size);
}

static inline bool PinIsCategory(IPin *pin, const GUID &category)
{
	if (!pin)
		return false;

	GUID pinCategory;
	HRESULT hr = GetPinCategory(pin, pinCategory);

	/* if the pin has no category interface, chances are we created it */
	if (FAILED(hr))
		return (hr == E_NOINTERFACE);

	return category == pinCategory;
}

static inline bool PinNameIs(IPin *pin, const wchar_t *name)
{
	if (!pin)
		return false;
	if (!name)
		return true;

	PIN_INFO pinInfo;

	if (FAILED(pin->QueryPinInfo(&pinInfo)))
		return false;

	if (pinInfo.pFilter)
		pinInfo.pFilter->Release();

	return wcscmp(name, pinInfo.achName) == 0;
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
	ComPtr<IPin> curPin;
	ComPtr<IEnumPins> pinsEnum;
	ULONG num;

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
	}

	return false;
}

bool GetPinByName(IBaseFilter *filter, PIN_DIRECTION dir, const wchar_t *name,
		  IPin **pin)
{
	ComPtr<IPin> curPin;
	ComPtr<IEnumPins> pinsEnum;
	ULONG num;

	if (!filter)
		return false;
	if (FAILED(filter->EnumPins(&pinsEnum)))
		return false;

	while (pinsEnum->Next(1, &curPin, &num) == S_OK) {
		if (PinIsDirection(curPin, dir) && PinNameIs(curPin, name)) {
			*pin = curPin.Detach();
			return true;
		}
	}

	return false;
}

bool GetPinByMedium(IBaseFilter *filter, REGPINMEDIUM &medium, IPin **pin)
{
	ComPtr<IPin> curPin;
	ComPtr<IEnumPins> pinsEnum;
	ULONG num;

	if (!filter)
		return false;
	if (FAILED(filter->EnumPins(&pinsEnum)))
		return false;

	while (pinsEnum->Next(1, &curPin, &num) == S_OK) {
		REGPINMEDIUM curMedium;

		if (GetPinMedium(curPin, curMedium) &&
		    memcmp(&medium, &curMedium, sizeof(medium)) == 0) {
			*pin = curPin.Detach();
			return true;
		}
	}

	return false;
}

static bool GetFilterByMediumFromMoniker(IMoniker *moniker,
					 REGPINMEDIUM &medium,
					 IBaseFilter **filter)
{
	ComPtr<IBaseFilter> curFilter;
	HRESULT hr;

	hr = moniker->BindToObject(nullptr, nullptr, IID_IBaseFilter,
				   (void **)&curFilter);
	if (SUCCEEDED(hr)) {
		ComPtr<IPin> pin;
		if (GetPinByMedium(curFilter, medium, &pin)) {
			*filter = curFilter.Detach();
			return true;
		}
	} else {
		WarningHR(L"GetFilterByMediumFromMoniker: BindToObject failed",
			  hr);
	}

	return false;
}

bool GetFilterByMedium(const CLSID &id, REGPINMEDIUM &medium,
		       IBaseFilter **filter)
{
	ComPtr<ICreateDevEnum> deviceEnum;
	ComPtr<IEnumMoniker> enumMoniker;
	ComPtr<IMoniker> moniker;
	DWORD count = 0;
	HRESULT hr;

	hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr,
			      CLSCTX_INPROC_SERVER, IID_ICreateDevEnum,
			      (void **)&deviceEnum);
	if (FAILED(hr)) {
		WarningHR(L"GetFilterByMedium: Failed to create device enum",
			  hr);
		return false;
	}

	hr = deviceEnum->CreateClassEnumerator(id, &enumMoniker, 0);
	if (hr != S_OK) {
		WarningHR(L"GetFilterByMedium: Failed to create enum moniker",
			  hr);
		return false;
	}

	enumMoniker->Reset();

	while (enumMoniker->Next(1, &moniker, &count) == S_OK) {
		if (GetFilterByMediumFromMoniker(moniker, medium, filter))
			return true;
	}

	return false;
}

bool GetPinMedium(IPin *pin, REGPINMEDIUM &medium)
{
	ComQIPtr<IKsPin> ksPin(pin);
	CoTaskMemPtr<KSMULTIPLE_ITEM> items;

	if (!ksPin)
		return false;

	if (FAILED(ksPin->KsQueryMediums(&items)))
		return false;

	REGPINMEDIUM *curMed = reinterpret_cast<REGPINMEDIUM *>(items + 1);
	for (ULONG i = 0; i < items->Count; i++, curMed++) {
		if (!IsEqualGUID(curMed->clsMedium, GUID_NULL) &&
		    !IsEqualGUID(curMed->clsMedium, KSMEDIUMSETID_Standard)) {
			medium = *curMed;
			return true;
		}
	}

	memset(&medium, 0, sizeof(medium));
	return false;
}

static inline bool PinIsConnected(IPin *pin)
{
	ComPtr<IPin> connectedPin;
	return SUCCEEDED(pin->ConnectedTo(&connectedPin));
}

static bool DirectConnectOutputPin(IFilterGraph *graph, IPin *pin,
				   IBaseFilter *filterIn)
{
	ComPtr<IPin> curPin;
	ComPtr<IEnumPins> pinsEnum;
	ULONG num;

	if (!graph || !filterIn || !pin)
		return false;
	if (FAILED(filterIn->EnumPins(&pinsEnum)))
		return false;

	while (pinsEnum->Next(1, &curPin, &num) == S_OK) {

		if (PinIsDirection(curPin, PINDIR_INPUT) &&
		    !PinIsConnected(curPin)) {
			if (graph->ConnectDirect(pin, curPin, nullptr) == S_OK)
				return true;
		}
	}

	return false;
}

bool DirectConnectFilters(IFilterGraph *graph, IBaseFilter *filterOut,
			  IBaseFilter *filterIn)
{
	ComPtr<IPin> curPin;
	ComPtr<IEnumPins> pinsEnum;
	ULONG num;
	bool connected = false;

	if (!graph || !filterOut || !filterIn)
		return false;
	if (FAILED(filterOut->EnumPins(&pinsEnum)))
		return false;

	while (pinsEnum->Next(1, &curPin, &num) == S_OK) {

		if (PinIsDirection(curPin, PINDIR_OUTPUT) &&
		    !PinIsConnected(curPin)) {
			if (DirectConnectOutputPin(graph, curPin, filterIn))
				connected = true;
		}
	}

	return connected;
}

HRESULT MapPinToPacketID(IPin *pin, ULONG packetID)
{
	ComQIPtr<IMPEG2PIDMap> pidMap(pin);
	if (!pidMap)
		return E_NOINTERFACE;

	return pidMap->MapPID(1, &packetID, MEDIA_ELEMENTARY_STREAM);
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

	return str;
}

static HRESULT DevicePathToDeviceInstancePath(const wchar_t *devicePath,
					      wchar_t *devInstPath, int size)
{
	/* Sanity checks */
	if (!devicePath)
		return E_POINTER;
	if (!devInstPath)
		return E_POINTER;

	/* Convert to uppercase STL string */
	wstring parseDevicePath = devicePath;
	for (wchar_t &c : parseDevicePath)
		c = (wchar_t)toupper(c);

	/* Find start position ('\\?\' or '\?\') */
	wstring startToken = L"\\\\?\\";
	size_t start = parseDevicePath.find(startToken, 0);
	if (start == string::npos) {
		startToken = L"\\??\\";
		start = parseDevicePath.find(startToken, 0);
		if (start == string::npos)
			return E_FAIL;
	}
	parseDevicePath = parseDevicePath.substr(
		startToken.size(), parseDevicePath.size() - startToken.size());

	/* Find end position (last occurrence of '#') */
	wchar_t endToken = '#';
	size_t end =
		parseDevicePath.find_last_of(endToken, parseDevicePath.size());
	if (end == string::npos)
		return E_FAIL;
	parseDevicePath = parseDevicePath.substr(0, end);

	/* Replace '#' by '\' */
	std::replace(parseDevicePath.begin(), parseDevicePath.end(), L'#',
		     L'\\');

	/* Set output parameter */
	StringCchCopyW(devInstPath, size, parseDevicePath.c_str());

	return S_OK;
}

static HRESULT GetParentDeviceInstancePath(const wchar_t *devInstPath,
					   wchar_t *parentDevInstPath, int size)
{
	/* Init return value */
	HRESULT hr = E_FAIL;

	/* Get device info */
	HDEVINFO hDevInfo = SetupDiCreateDeviceInfoList(nullptr, NULL);
	if (NULL != hDevInfo) {
		SP_DEVINFO_DATA did;
		did.cbSize = sizeof(SP_DEVINFO_DATA);
		BOOL success = SetupDiOpenDeviceInfo(hDevInfo, devInstPath,
						     NULL, 0, &did);
		if (success) {

			/* Get parent device */
			DEVINST devParent;
			CONFIGRET ret =
				CM_Get_Parent(&devParent, did.DevInst, 0);
			if (CR_SUCCESS == ret) {

				/* Get parent device instance path */
				ret = CM_Get_Device_ID(
					devParent, parentDevInstPath, size, 0);
				if (CR_SUCCESS == ret)
					hr = S_OK;
			}

			/* Cleanup */
			SetupDiDeleteDeviceInfo(hDevInfo, &did);
		}

		/* Cleanup */
		SetupDiDestroyDeviceInfoList(hDevInfo);
	}

	return hr;
}

static bool IsSameInstPath(const wchar_t *audDevPath,
			   const wchar_t *vidDevInstPath)
{
	/* Get audio device instance path */
	wchar_t audDevInstPath[512];
	HRESULT hr = DevicePathToDeviceInstancePath(audDevPath, audDevInstPath,
						    _ARRAYSIZE(audDevInstPath));

	/* Compare audio and video device instance path */
	if (FAILED(hr))
		return false;

	return wcscmp(audDevInstPath, vidDevInstPath) == 0;
}

static HRESULT
GetAudioCaptureParentDeviceInstancePath(IMoniker *audioCapture,
					wchar_t *parentDevInstPath, int size)
{
	/* Sanity checks */
	if (!audioCapture)
		return E_POINTER;

	/* Bind to property bag */
	ComPtr<IPropertyBag> propertyBag;
	HRESULT hr = audioCapture->BindToStorage(0, 0, IID_IPropertyBag,
						 (void **)&propertyBag);
	if (SUCCEEDED(hr)) {

		/* Init variant */
		VARIANT var;
		VariantInit(&var);

		/* Get "WaveInId" */
		hr = propertyBag->Read(L"WaveInId", &var, nullptr);
		if (SUCCEEDED(hr) && var.vt == VT_I4) {

			/* Get device path */
			wchar_t devicePath[512];
			MMRESULT res = waveInMessage((HWAVEIN)var.iVal,
						     DRV_QUERYDEVICEINTERFACE,
						     (DWORD_PTR)devicePath,
						     sizeof(devicePath));
			if (res == MMSYSERR_NOERROR) {
				/* Get device instance path */
				wchar_t devInstPath[512];
				hr = DevicePathToDeviceInstancePath(
					devicePath, devInstPath,
					_ARRAYSIZE(devInstPath));

				/* Get parent */
				if (SUCCEEDED(hr))
					hr = GetParentDeviceInstancePath(
						devInstPath, parentDevInstPath,
						size);
			}
		}

		/* Cleanup */
		VariantClear(&var);
	}

	return hr;
}

static bool IsMonikerSameParentInstPath(IMoniker *moniker,
					const wchar_t *vidDevInstPath)
{
	/* Get video parent device instance path */
	wchar_t vidParentDevInstPath[512];
	HRESULT hr = GetParentDeviceInstancePath(
		vidDevInstPath, vidParentDevInstPath,
		_ARRAYSIZE(vidParentDevInstPath));

	/* Get audio parent device instance path */
	wchar_t audParentDevInstPath[512];
	if (SUCCEEDED(hr))
		hr = GetAudioCaptureParentDeviceInstancePath(
			moniker, audParentDevInstPath,
			_ARRAYSIZE(audParentDevInstPath));

	/* Compare audio and video parent device instance path */
	if (FAILED(hr))
		return false;

	return wcscmp(audParentDevInstPath, vidParentDevInstPath) == 0;
}

#define VEN_ID_SIZE 4

static inline bool MatchingStartToken(const wstring &path,
				      const wstring &start_token)
{
	return path.find(start_token) == 0 &&
	       path.size() >= start_token.size() + VEN_ID_SIZE;
}

static bool IsUncoupledDevice(const wchar_t *vidDevInstPath)
{
	/* Sanity checks */
	if (!vidDevInstPath)
		return false;

	wstring path = vidDevInstPath;

	/* USB */
	wstring usbToken = L"USB\\VID_";
	wstring usbVidIdWhitelist[] = {
		L"0FD9", /* elgato */
		L"3842", /* evga */
	};

	if (MatchingStartToken(path, usbToken)) {
		/* Get USB vendor ID */
		wstring vid = path.substr(usbToken.size(), VEN_ID_SIZE);
		for (wstring &whitelistId : usbVidIdWhitelist) {
			if (vid == whitelistId) {
				return true;
			}
		}
	}

	/* PCI */
	wstring pciVenToken = L"PCI\\VEN_";
	wstring pciSubsysToken = L"SUBSYS_";
	wstring pciVenIdWhitelist[] = {
		L"1CD7", /* magewell */
	};
	wstring pciSubsysIdWhitelist[] = {
		L"1CFA", /* elgato */
	};

	if (MatchingStartToken(path, pciVenToken)) {
		wstring vid = path.substr(usbToken.size(), VEN_ID_SIZE);
		for (wstring &whitelistId : pciVenIdWhitelist) {
			if (vid == whitelistId) {
				return true;
			}
		}

		size_t subsysPos = path.find(pciSubsysToken);
		size_t subsysIdPos =
			subsysPos + pciSubsysToken.size() + VEN_ID_SIZE;
		size_t expectedSize = subsysIdPos + VEN_ID_SIZE;

		if (subsysPos != string::npos && path.size() >= expectedSize) {
			/* Get PCI subsystem vendor ID */
			wstring ssid = path.substr(subsysIdPos, VEN_ID_SIZE);
			for (wstring &whitelistId : pciSubsysIdWhitelist) {
				if (ssid == whitelistId) {
					return true;
				}
			}
		}
	}

	return false;
}

static HRESULT ReadProperty(IMoniker *moniker, const wchar_t *property,
			    wchar_t *value, int size)
{
	/* Sanity checks */
	if (!moniker)
		return E_POINTER;
	if (!property)
		return E_POINTER;
	if (!value)
		return E_POINTER;

	/* Increment reference count */
	moniker->AddRef();

	/* Bind to property bag */
	ComPtr<IPropertyBag> propertyBag;
	HRESULT hr = moniker->BindToStorage(0, 0, IID_IPropertyBag,
					    (void **)&propertyBag);
	if (SUCCEEDED(hr)) {
		/* Initialize variant */
		VARIANT var;
		VariantInit(&var);

		/* Read property */
		hr = propertyBag->Read(property, &var, nullptr);
		if (SUCCEEDED(hr))
			StringCchCopyW(value, size, var.bstrVal);

		/* Cleanup */
		VariantClear(&var);
	}

	/* Decrement reference count */
	moniker->Release();

	return hr;
}

static HRESULT GetFriendlyName(REFCLSID deviceClass, const wchar_t *devPath,
			       wchar_t *name, int nameSize)
{
	/* Sanity checks */
	if (!devPath)
		return E_POINTER;
	if (!name)
		return E_POINTER;

	/* Create device enumerator */
	ComPtr<ICreateDevEnum> createDevEnum;
	HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL,
				      CLSCTX_INPROC_SERVER, IID_ICreateDevEnum,
				      (void **)&createDevEnum);

	/* Enumerate filters */
	ComPtr<IEnumMoniker> enumMoniker;
	if (SUCCEEDED(hr)) {
		/* returns S_FALSE if no devices are installed */
		hr = createDevEnum->CreateClassEnumerator(deviceClass,
							  &enumMoniker, 0);
		if (!enumMoniker)
			hr = E_FAIL;
	}

	/* Cycle through the enumeration */
	if (SUCCEEDED(hr)) {
		ULONG fetched = 0;
		ComPtr<IMoniker> moniker;

		enumMoniker->Reset();

		while (enumMoniker->Next(1, &moniker, &fetched) == S_OK) {

			/* Get device path from moniker */
			wchar_t monikerDevPath[512];
			hr = ReadProperty(moniker, L"DevicePath",
					  monikerDevPath,
					  _ARRAYSIZE(monikerDevPath));

			/* Find desired filter */
			if (wcscmp(devPath, monikerDevPath) == 0) {

				/* Get friendly name */
				hr = ReadProperty(moniker, L"FriendlyName",
						  name, nameSize);
				return hr;
			}
		}
	}

	return E_FAIL;
}

static bool MatchFriendlyNames(const wchar_t *vidName, const wchar_t *audName)
{
	/* Sanity checks */
	if (!vidName)
		return false;
	if (!audName)
		return false;

	/* Convert strings to lower case */
	wstring strVidName = vidName;
	for (wchar_t &c : strVidName)
		c = (wchar_t)tolower(c);
	wstring strAudName = audName;
	for (wchar_t &c : strAudName)
		c = (wchar_t)tolower(c);

	/* Remove 'video' from friendly name */
	size_t posVid;
	wstring searchVid[] = {L"(video) ", L"(video)", L"video ", L"video"};
	for (int i = 0; i < _ARRAYSIZE(searchVid); i++) {
		wstring &search = searchVid[i];
		while ((posVid = strVidName.find(search)) !=
		       std::string::npos) {
			strVidName.replace(posVid, search.length(), L"");
		}
	}

	/* Remove 'audio' from friendly name */
	size_t posAud;
	wstring searchAud[] = {L"(audio) ", L"(audio)", L"audio ", L"audio"};
	for (int i = 0; i < _ARRAYSIZE(searchAud); i++) {
		wstring &search = searchAud[i];
		while ((posAud = strAudName.find(search)) !=
		       std::string::npos) {
			strAudName.replace(posAud, search.length(), L"");
		}
	}

	return strVidName == strAudName;
}

static bool GetDeviceAudioFilterInternal(REFCLSID deviceClass,
					 const wchar_t *vidDevPath,
					 IBaseFilter **audioCaptureFilter,
					 bool matchFilterName = false)
{
	/* Get video device instance path */
	wchar_t vidDevInstPath[512];
	HRESULT hr = DevicePathToDeviceInstancePath(vidDevPath, vidDevInstPath,
						    _ARRAYSIZE(vidDevInstPath));
	if (FAILED(hr))
		return false;

#if 1
	/* Only enabled for certain whitelisted devices for now */
	if (!IsUncoupledDevice(vidDevInstPath))
		return false;
#endif

	/* Get friendly name */
	wchar_t vidName[512];
	if (matchFilterName) {
		hr = GetFriendlyName(CLSID_VideoInputDeviceCategory, vidDevPath,
				     vidName, _ARRAYSIZE(vidName));
		if (FAILED(hr))
			return false;
	}

	/* Create device enumerator */
	ComPtr<ICreateDevEnum> createDevEnum;
	if (SUCCEEDED(hr))
		hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL,
				      CLSCTX_INPROC_SERVER, IID_ICreateDevEnum,
				      (void **)&createDevEnum);

	/* Enumerate filters */
	ComPtr<IEnumMoniker> enumMoniker;
	if (SUCCEEDED(hr)) {
		/* returns S_FALSE if no devices are installed */
		hr = createDevEnum->CreateClassEnumerator(deviceClass,
							  &enumMoniker, 0);
		if (!enumMoniker)
			hr = E_FAIL;
	}

	/* Cycle through the enumeration */
	if (SUCCEEDED(hr)) {
		ULONG fetched = 0;
		ComPtr<IMoniker> moniker;

		enumMoniker->Reset();

		while (enumMoniker->Next(1, &moniker, &fetched) == S_OK) {
			bool samePath = false;

			/* Get device path */
			wchar_t audDevPath[512];
			hr = ReadProperty(moniker, L"DevicePath", audDevPath,
					  _ARRAYSIZE(audDevPath));
			if (SUCCEEDED(hr)) {
				/* Skip if it is the video device */
				if (wcscmp(audDevPath, vidDevPath) == 0)
					continue;

				samePath = IsSameInstPath(audDevPath,
							  vidDevInstPath);
			} else {
				samePath = IsMonikerSameParentInstPath(
					moniker, vidDevInstPath);
			}

			/* Get audio capture filter */
			if (samePath) {
				/* Match video and audio filter names */
				bool isSameFilterName = false;
				if (matchFilterName) {
					wchar_t audName[512];
					hr = ReadProperty(moniker,
							  L"FriendlyName",
							  audName,
							  _ARRAYSIZE(audName));
					if (SUCCEEDED(hr)) {
						isSameFilterName =
							MatchFriendlyNames(
								vidName,
								audName);
					}
				}

				if (!matchFilterName || isSameFilterName) {
					hr = moniker->BindToObject(
						0, 0, IID_IBaseFilter,
						(void **)audioCaptureFilter);
					if (SUCCEEDED(hr))
						return true;
				}
			}
		}
	}

	return false;
}

bool GetDeviceAudioFilter(const wchar_t *vidDevPath,
			  IBaseFilter **audioCaptureFilter)
{
	/* Search in "Audio capture sources" and match filter name */
	bool success = GetDeviceAudioFilterInternal(
		CLSID_AudioInputDeviceCategory, vidDevPath, audioCaptureFilter,
		true);

	/* Search in "WDM Streaming Capture Devices" and match filter name */
	if (!success)
		success = GetDeviceAudioFilterInternal(KSCATEGORY_CAPTURE,
						       vidDevPath,
						       audioCaptureFilter,
						       true);

	/* Search in "Audio capture sources" */
	if (!success)
		success = GetDeviceAudioFilterInternal(
			CLSID_AudioInputDeviceCategory, vidDevPath,
			audioCaptureFilter);

	/* Search in "WDM Streaming Capture Devices" */
	if (!success)
		success = GetDeviceAudioFilterInternal(
			KSCATEGORY_CAPTURE, vidDevPath, audioCaptureFilter);

	return success;
}

}; /* namespace DShow */
