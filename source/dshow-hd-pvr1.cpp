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
#define AUDIO_PIN_PACKET_ID 0x1100UL

namespace DShow {

static inline bool CreateHDPVR1Filters(IBaseFilter *filter,
		IBaseFilter **crossbar, IBaseFilter **encoder,
		IBaseFilter **demuxer)
{
	CComPtr<IPin> inputPin;
	CComPtr<IPin> outputPin;
	REGPINMEDIUM  inMedium;
	REGPINMEDIUM  outMedium;
	HRESULT       hr;

	if (!GetPinByName(filter, PINDIR_INPUT, nullptr, &inputPin)) {
		Warning(L"HD-PVR 1: Failed to get input pin");
		return false;
	}

	if (!GetPinByName(filter, PINDIR_OUTPUT, nullptr, &outputPin)) {
		Warning(L"HD-PVR 1: Failed to get output pin");
		return false;
	}

	if (!GetPinMedium(inputPin, inMedium)) {
		Warning(L"HD-PVR 1: Failed to get input pin medium");
		return false;
	}

	if (!GetPinMedium(outputPin, outMedium)) {
		Warning(L"HD-PVR 1: Failed to get output pin medium");
		return false;
	}

	if (!GetFilterByMedium(AM_KSCATEGORY_CROSSBAR, inMedium, crossbar)) {
		Warning(L"HD-PVR 1: Failed to get crossbar filter");
		return false;
	}

	if (!GetFilterByMedium(KSCATEGORY_ENCODER, outMedium, encoder)) {
		Warning(L"HD-PVR 1: Failed to get encoder filter");
		return false;
	}

	hr = CoCreateInstance(CLSID_MPEG2Demultiplexer, nullptr,
			CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)demuxer);
	if (FAILED(hr)) {
		WarningHR(L"HD-PVR 1: Failed to create demuxer", hr);
		return false;
	}

	return true;
}

static inline bool ConnectHDPVR1Filters(IGraphBuilder *graph,
		IBaseFilter *filter, IBaseFilter *crossbar,
		IBaseFilter *encoder, IBaseFilter *demuxer)
{
	if (!DirectConnectFilters(graph, crossbar, filter)) {
		Warning(L"HD-PVR 1: Failed to connect crossbar to device");
		return false;
	}

	if (!DirectConnectFilters(graph, filter, encoder)) {
		Warning(L"HD-PVR 1: Failed to connect device to encoder");
		return false;
	}

	if (!DirectConnectFilters(graph, encoder, demuxer)) {
		Warning(L"HD-PVR 1: Failed to connect encoder to demuxer");
		return false;
	}

	return true;
}

static inline bool MapHDPVR1PacketIDs(IBaseFilter *demuxer)
{
	CComPtr<IPin> videoPin, audioPin;
	HRESULT       hr;

	if (!GetPinByName(demuxer, PINDIR_OUTPUT, DEMUX_VIDEO_PIN, &videoPin)) {
		Warning(L"HD-PVR 1: Could not get video pin from demuxer");
		return false;
	}

	if (!GetPinByName(demuxer, PINDIR_OUTPUT, DEMUX_AUDIO_PIN, &audioPin)) {
		Warning(L"HD-PVR 1: Could not get audio pin from demuxer");
		return false;
	}

	hr = MapPinToPacketID(videoPin, VIDEO_PIN_PACKET_ID);
	if (FAILED(hr)) {
		WarningHR(L"HD-PVR 1: Failed to map demuxer video pin "
		          L"packet ID", hr);
		return false;
	}

	hr = MapPinToPacketID(audioPin, AUDIO_PIN_PACKET_ID);
	if (FAILED(hr)) {
		WarningHR(L"HD-PVR 1: Failed to map demuxer audio pin "
		          L"packet ID", hr);
		return false;
	}

	return true;
}

bool HDevice::SetupHDPVR1VideoCapture(IBaseFilter *filter,
			VideoConfig &config)
{
	CComPtr<IBaseFilter> crossbar;
	CComPtr<IBaseFilter> encoder;
	CComPtr<IBaseFilter> demuxer;
	MediaType            mtVideo;
	MediaType            mtAudio;

	if (!CreateHDPVR1Filters(filter, &crossbar, &encoder, &demuxer))
		return false;

	if (!CreateDemuxVideoPin(demuxer, mtVideo, HD_PVR1_CX,
				HD_PVR1_CY, HD_PVR1_INTERVAL,
				HD_PVR1_VFORMAT))
		return false;

	if (!CreateDemuxAudioPin(demuxer, mtAudio, HD_PVR1_SAMPLERATE,
				16, 2, HD_PVR1_AFORMAT))
		return false;

	config.cx             = HD_PVR1_CX;
	config.cy             = HD_PVR1_CY;
	config.frameInterval  = HD_PVR1_INTERVAL;
	config.format         = HD_PVR1_VFORMAT;
	config.internalFormat = HD_PVR1_VFORMAT;

	PinCaptureInfo info;
	info.callback          = [this] (IMediaSample *s) {Receive(true, s);};
	info.expectedMajorType = mtVideo->majortype;
	info.expectedSubType   = mtVideo->subtype;

	videoCapture = new CaptureFilter(info);
	videoFilter  = demuxer;

	graph->AddFilter(crossbar,     L"HD-PVR 1 Crossbar");
	graph->AddFilter(filter,       L"HD-PVR 1");
	graph->AddFilter(encoder,      L"HD-PVR 1 Encoder");
	graph->AddFilter(demuxer,      L"HD-PVR 1 Demuxer");
	graph->AddFilter(videoCapture, L"Capture Filter");

	bool success = ConnectHDPVR1Filters(graph, filter, crossbar,
			encoder, demuxer);
	if (success)
		success = MapHDPVR1PacketIDs(demuxer);

	return success;
}

}; /* namespace DShow */
