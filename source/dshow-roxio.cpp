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
#include "dshow-media-type.hpp"
#include "dshow-formats.hpp"
#include "dshow-demux.hpp"
#include "capture-filter.hpp"
#include "device.hpp"
#include "log.hpp"

#define VIDEO_PIN_PACKET_ID 0x1011UL
#define AUDIO_PIN_PACKET_ID 0x010FUL

namespace DShow {

static inline bool CreateRoxioFilters(IBaseFilter *filter,
		IBaseFilter **crossbar, IBaseFilter **demuxer)
{
	CComPtr<IPin> inputPin;
	REGPINMEDIUM  inMedium;
	HRESULT       hr;

	if (!GetPinByName(filter, PINDIR_INPUT, nullptr, &inputPin)) {
		Warning(L"Roxio: Failed to get input pin");
		return false;
	}

	if (!GetPinMedium(inputPin, inMedium)) {
		Warning(L"Roxio: Failed to get input pin medium");
		return false;
	}

	if (!GetFilterByMedium(AM_KSCATEGORY_CROSSBAR, inMedium, crossbar)) {
		Warning(L"Roxio: Failed to get crossbar filter");
		return false;
	}

	hr = CoCreateInstance(CLSID_MPEG2Demultiplexer, nullptr,
			CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)demuxer);
	if (FAILED(hr)) {
		WarningHR(L"Roxio: Failed to create demuxer", hr);
		return false;
	}

	return true;
}

static inline bool ConnectRoxioFilters(IGraphBuilder *graph,
		IBaseFilter *filter, IBaseFilter *crossbar,
		IBaseFilter *demuxer)
{
	if (!DirectConnectFilters(graph, crossbar, filter)) {
		Warning(L"Roxio: Failed to connect crossbar to device");
		return false;
	}

	if (!DirectConnectFilters(graph, filter, demuxer)) {
		Warning(L"Roxio: Failed to connect device to demuxer");
		return false;
	}

	return true;
}

static inline bool MapRoxioPacketIDs(IBaseFilter *demuxer)
{
	CComPtr<IPin> videoPin, audioPin;
	HRESULT       hr;

	if (!GetPinByName(demuxer, PINDIR_OUTPUT, DEMUX_VIDEO_PIN, &videoPin)) {
		Warning(L"Roxio: Could not get video pin from demuxer");
		return false;
	}

	if (!GetPinByName(demuxer, PINDIR_OUTPUT, DEMUX_AUDIO_PIN, &audioPin)) {
		Warning(L"Roxio: Could not get audio pin from demuxer");
		return false;
	}

	hr = MapPinToPacketID(videoPin, VIDEO_PIN_PACKET_ID);
	if (FAILED(hr)) {
		WarningHR(L"Roxio: Failed to map demuxer video pin packet ID",
				hr);
		return false;
	}

	hr = MapPinToPacketID(audioPin, AUDIO_PIN_PACKET_ID);
	if (FAILED(hr)) {
		WarningHR(L"Roxio: Failed to map demuxer audio pin packet ID",
				hr);
		return false;
	}

	return true;
}

bool HDevice::SetupRoxioVideoCapture(IBaseFilter *filter, VideoConfig &config)
{
	CComPtr<IBaseFilter> crossbar;
	CComPtr<IBaseFilter> demuxer;
	MediaType            mtVideo;
	MediaType            mtAudio;

	if (!CreateRoxioFilters(filter, &crossbar, &demuxer))
		return false;

	if (!CreateDemuxVideoPin(demuxer, mtVideo, ROXIO_CX, ROXIO_CY,
				ROXIO_INTERVAL, ROXIO_VFORMAT))
		return false;

	if (!CreateDemuxAudioPin(demuxer, mtAudio, ROXIO_SAMPLERATE, 16, 2,
				ROXIO_AFORMAT))
		return false;

	config.cx             = ROXIO_CX;
	config.cy             = ROXIO_CY;
	config.frameInterval  = ROXIO_INTERVAL;
	config.format         = ROXIO_VFORMAT;
	config.internalFormat = ROXIO_VFORMAT;

	PinCaptureInfo info;
	info.callback          = [this] (IMediaSample *s) {VideoCallback(s);};
	info.expectedMajorType = mtVideo->majortype;
	info.expectedSubType   = mtVideo->subtype;

	videoCapture = new CaptureFilter(info);
	videoFilter  = demuxer;

	graph->AddFilter(crossbar,     L"Roxio Crossbar");
	graph->AddFilter(filter,       L"Roxio");
	graph->AddFilter(demuxer,      L"Roxio Demuxer");
	graph->AddFilter(videoCapture, L"Capture Filter");

	if (!ConnectRoxioFilters(graph, filter, crossbar, demuxer))
		return false;

	return MapRoxioPacketIDs(demuxer);
}

}; /* namespace DShow */
