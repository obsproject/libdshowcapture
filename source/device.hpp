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

#include "../dshowcapture.hpp"
#include "capture-filter.hpp"

#include <string>
#include <vector>
using namespace std;

namespace DShow {

struct EncodedData {
	long long                      lastStartTime = 0;
	long long                      lastStopTime  = 0;
	vector<unsigned char>          bytes;
};

struct EncodedDevice {
	VideoFormat videoFormat;
	ULONG       videoPacketID;
	long        width;
	long        height;
	long long   frameInterval;

	AudioFormat audioFormat;
	ULONG       audioPacketID;
	DWORD       samplesPerSec;
};

struct HDevice {
	CComPtr<IGraphBuilder>         graph;
	CComPtr<ICaptureGraphBuilder2> builder;
	CComPtr<IMediaControl>         control;

	CComPtr<IBaseFilter>           videoFilter;
	CComPtr<IBaseFilter>           audioFilter;
	CComPtr<CaptureFilter>         videoCapture;
	CComPtr<CaptureFilter>         audioCapture;
	MediaType                      videoMediaType;
	MediaType                      audioMediaType;
	VideoConfig                    videoConfig;
	AudioConfig                    audioConfig;

	bool                           initialized;
	bool                           active;

	EncodedData                    encodedVideo;
	EncodedData                    encodedAudio;

	HDevice();
	~HDevice();

	void LogFilters();

	void ConvertVideoSettings();
	void ConvertAudioSettings();

	bool EnsureInitialized(const wchar_t *func);
	bool EnsureActive(const wchar_t *func);
	bool EnsureInactive(const wchar_t *func);

	inline void SendToCallback(bool video,
			unsigned char *data, size_t size,
			long long startTime, long long stopTime);

	void Receive(bool video, IMediaSample *sample);

	bool SetupEncodedVideoCapture(IBaseFilter *filter,
				VideoConfig &config,
				const EncodedDevice &info);

	bool SetupExceptionVideoCapture(IBaseFilter *filter,
			VideoConfig &config);

	bool SetupExceptionAudioCapture(IPin *pin);

	bool SetupVideoCapture(IBaseFilter *filter, VideoConfig &config);
	bool SetupAudioCapture(IBaseFilter *filter, AudioConfig &config);

	bool SetVideoConfig(VideoConfig *config);
	bool SetAudioConfig(AudioConfig *config);

	bool CreateGraph();
	bool ConnectPins(const GUID &category, const GUID &type,
			IBaseFilter *filter, CaptureFilter *capture);
	bool RenderFilters(const GUID &category, const GUID &type,
			IBaseFilter *filter, CaptureFilter *capture);
	bool ConnectFilters();
	Result Start();
	void Stop();
};

}; /* namespace DShow */
