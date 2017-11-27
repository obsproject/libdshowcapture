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

#pragma once

#define WIN32_MEAN_AND_LEAN
#define __STREAMS__
#include <windows.h>
#include <dshow.h>
#include <ks.h>
#include <ksmedia.h>
#include <ksproxy.h>
#include <Amaudio.h>
#include <Dvdmedia.h>

#include "ComPtr.hpp"
#include "CoTaskMemPtr.hpp"

#include <string>
using namespace std;

#define DSHOW_UNUSED(param) (void)param;

namespace DShow {

bool CreateFilterGraph(IGraphBuilder **graph, ICaptureGraphBuilder2 **builder,
		IMediaControl **control);

void LogFilters(IGraphBuilder *graph);

bool GetDeviceFilter(const IID &type, const wchar_t *name, const wchar_t *path,
		IBaseFilter **filter);

bool GetFilterPin(IBaseFilter *filter, const GUID &type, const GUID &category,
		PIN_DIRECTION dir, IPin **pin);

bool GetPinByName(IBaseFilter *filter, PIN_DIRECTION dir, const wchar_t *name,
		IPin **pin);

bool GetPinByMedium(IBaseFilter *filter, REGPINMEDIUM &medium, IPin **pin);
bool GetFilterByMedium(const CLSID &id, REGPINMEDIUM &medium,
		IBaseFilter **filter);

bool GetPinMedium(IPin *pin, REGPINMEDIUM &medium);

bool DirectConnectFilters(IFilterGraph *graph, IBaseFilter *filterOut,
		IBaseFilter *filterIn);

/**
 * This maps a created demuxer pin to a packet ID for the mux stream.  Note
 * that this needs to be called after the device filters are connected to the
 * demux filter.
 */
HRESULT MapPinToPacketID(IPin *pin, ULONG packetID);

wstring ConvertHRToEnglish(HRESULT hr);

/**
 * Get audio filter for the same device as the given video device path
 */
bool GetDeviceAudioFilter(const wchar_t *videoDevicePath,
		IBaseFilter **audioCaptureFilter);
bool GetDeviceAudioFilter(REFCLSID deviceClass,
		const wchar_t *videoDevicePath, IBaseFilter **audioCaptureFilter);

/** Returns true if device instance path is an Elgato USB or PCIe device */
bool IsElgatoDevice(const wchar_t *videoDeviceInstancePath);

/** Read property from moniker */
HRESULT ReadProperty(IMoniker *moniker, const wchar_t *property,
		wchar_t *value, int size);

/** Parse device instance path from device path */
HRESULT DevicePathToDeviceInstancePath(const wchar_t *devicePath,
		wchar_t *deviceInstancePath, int size);

/** Get device instance path of the parent of the given device. */
HRESULT GetParentDeviceInstancePath(const wchar_t *deviceInstancePath,
		wchar_t *parentDeviceInstancePath, int size);

/** Get device instance path of the parent of the given audio capture device. */
HRESULT GetAudioCaptureParentDeviceInstancePath(IMoniker *audioCapture,
		wchar_t *parentDeviceInstancePath, int size);

}; /* namespace DShow */
