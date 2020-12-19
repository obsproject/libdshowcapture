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

#include "output-filter.hpp"
#include "dshow-formats.hpp"
#include "log.hpp"

#include <strsafe.h>

namespace DShow {

#if 0
#define PrintFunc(x) Debug(x)
#else
#define PrintFunc(x)
#endif

#define FILTER_NAME L"Output Filter"
#define VIDEO_PIN_NAME L"Video Output"
#define AUDIO_PIN_NAME L"Audio Output"

OutputPin::OutputPin(OutputFilter *filter_, VideoFormat format, int cx, int cy,
		     long long interval)
	: refCount(0), filter(filter_)
{
	curCX = cx;
	curCY = cy;
	curInterval = interval;

	AddVideoFormat(format, cx, cy, interval);
	SetVideoFormat(format, cx, cy, interval);
}

OutputPin::~OutputPin() {}

STDMETHODIMP OutputPin::QueryInterface(REFIID riid, void **ppv)
{
	if (riid == IID_IUnknown) {
		AddRef();
		*ppv = this;
	} else if (riid == IID_IPin) {
		AddRef();
		*ppv = (IPin *)this;
	} else if (riid == IID_IMemInputPin) {
		AddRef();
		*ppv = (IMemInputPin *)this;
	} else if (riid == IID_IAMStreamConfig) {
		AddRef();
		*ppv = (IAMStreamConfig *)this;
		return S_OK;
	} else if (riid == IID_IKsPropertySet) {
		AddRef();
		*ppv = (IKsPropertySet *)this;
		return S_OK;
	} else {
		*ppv = nullptr;
		return E_NOINTERFACE;
	}

	return NOERROR;
}

STDMETHODIMP_(ULONG) OutputPin::AddRef()
{
	return (ULONG)InterlockedIncrement(&refCount);
}

STDMETHODIMP_(ULONG) OutputPin::Release()
{
	long newRefs = InterlockedDecrement(&refCount);
	if (!newRefs) {
		delete this;
		return 0;
	}

	return (ULONG)newRefs;
}

// IPin methods
STDMETHODIMP OutputPin::Connect(IPin *pReceivePin, const AM_MEDIA_TYPE *pmt)
{
	HRESULT hr;

	PrintFunc(L"OutputPin::Connect");

	if (filter->state == State_Running)
		return VFW_E_NOT_STOPPED;

	if (connectedPin)
		return VFW_E_ALREADY_CONNECTED;

	hr = pReceivePin->ReceiveConnection(this, mt);
	if (FAILED(hr)) {
#if 0 /* debug code to test caps on fail */
		ComPtr<IEnumMediaTypes> enumMT;
		pReceivePin->EnumMediaTypes(&enumMT);

		if (enumMT) {
			MediaTypePtr mt;
			ULONG count = 0;

			while (enumMT->Next(1, &mt, &count) == S_OK) {
				int test = 0;
				test = 1;
			}
		}
#endif
		return E_FAIL;
	}

	if (!AllocateBuffers(pReceivePin, true)) {
		return E_FAIL;
	}

	connectedPin = pReceivePin;
	DSHOW_UNUSED(pmt);
	return S_OK;
}

STDMETHODIMP OutputPin::ReceiveConnection(IPin *pConnector,
					  const AM_MEDIA_TYPE *pmt)
{
	PrintFunc(L"OutputPin::ReceiveConnection");

	DSHOW_UNUSED(pConnector);
	DSHOW_UNUSED(pmt);
	return S_OK;
}

STDMETHODIMP OutputPin::Disconnect()
{
	PrintFunc(L"OutputPin::Disconnect");

	if (!connectedPin)
		return S_FALSE;

	if (!!allocator) {
		allocator->Decommit();
		allocator.Clear();
	}

	connectedPin = nullptr;
	return S_OK;
}

STDMETHODIMP OutputPin::ConnectedTo(IPin **pPin)
{
	PrintFunc(L"OutputPin::ConnectedTo");

	if (!connectedPin) {
		*pPin = nullptr;
		return VFW_E_NOT_CONNECTED;
	}

	IPin *pin = connectedPin;
	pin->AddRef();
	*pPin = pin;
	return S_OK;
}

STDMETHODIMP OutputPin::ConnectionMediaType(AM_MEDIA_TYPE *pmt)
{
	PrintFunc(L"OutputPin::ConnectionMediaType");

	if (!connectedPin)
		return VFW_E_NOT_CONNECTED;

	return CopyMediaType(pmt, mt);
}

STDMETHODIMP OutputPin::QueryPinInfo(PIN_INFO *pInfo)
{
	PrintFunc(L"OutputPin::QueryPinInfo");

	pInfo->pFilter = filter;
	if (filter) {
		IBaseFilter *ptr = filter;
		ptr->AddRef();
	}

	if (mt->majortype == MEDIATYPE_Video)
		memcpy(pInfo->achName, VIDEO_PIN_NAME, sizeof(VIDEO_PIN_NAME));
	else
		memcpy(pInfo->achName, AUDIO_PIN_NAME, sizeof(AUDIO_PIN_NAME));

	pInfo->dir = PINDIR_OUTPUT;

	return NOERROR;
}

STDMETHODIMP OutputPin::QueryDirection(PIN_DIRECTION *pPinDir)
{
	*pPinDir = PINDIR_OUTPUT;
	return NOERROR;
}

#define OUTPUT_PIN_NAME L"Output Pin"

STDMETHODIMP OutputPin::QueryId(LPWSTR *lpId)
{
	wchar_t *str = (wchar_t *)CoTaskMemAlloc(sizeof(OUTPUT_PIN_NAME));
	memcpy(str, OUTPUT_PIN_NAME, sizeof(OUTPUT_PIN_NAME));
	*lpId = str;
	return S_OK;
}

STDMETHODIMP OutputPin::QueryAccept(const AM_MEDIA_TYPE *)
{
	PrintFunc(L"OutputPin::QueryAccept");

	return S_OK;
}

STDMETHODIMP OutputPin::EnumMediaTypes(IEnumMediaTypes **ppEnum)
{
	PrintFunc(L"OutputPin::EnumMediaTypes");

	*ppEnum = new OutputEnumMediaTypes(this);
	if (!*ppEnum)
		return E_OUTOFMEMORY;

	return NOERROR;
}

STDMETHODIMP OutputPin::QueryInternalConnections(IPin **apPin, ULONG *nPin)
{
	PrintFunc(L"OutputPin::QueryInternalConnections");

	DSHOW_UNUSED(apPin);
	DSHOW_UNUSED(nPin);
	return E_NOTIMPL;
}

STDMETHODIMP OutputPin::EndOfStream()
{
	PrintFunc(L"OutputPin::EndOfStream");

	return S_OK;
}

STDMETHODIMP OutputPin::BeginFlush()
{
	PrintFunc(L"OutputPin::BeginFlush");

	flushing = true;
	return S_OK;
}

STDMETHODIMP OutputPin::EndFlush()
{
	PrintFunc(L"OutputPin::EndFlush");

	flushing = false;
	return S_OK;
}

STDMETHODIMP OutputPin::NewSegment(REFERENCE_TIME, REFERENCE_TIME, double)
{
	PrintFunc(L"OutputPin::NewSegment");

	return S_OK;
}

STDMETHODIMP OutputPin::GetFormat(AM_MEDIA_TYPE **ppmt)
{
	PrintFunc(L"OutputPin::GetFormat");

	if (!ppmt) {
		return E_POINTER;
	}

	*ppmt = mt.Duplicate();
	return S_OK;
}

STDMETHODIMP OutputPin::GetNumberOfCapabilities(int *piCount, int *piSize)
{
	PrintFunc(L"OutputPin::GetNumberOfCapabilities");

	if (!piCount || !piSize) {
		return E_POINTER;
	}

	*piCount = (int)mtList.size();
	*piSize = sizeof(VIDEO_STREAM_CONFIG_CAPS);
	return S_OK;
}

STDMETHODIMP OutputPin::GetStreamCaps(int iIndex, AM_MEDIA_TYPE **ppmt,
				      BYTE *pSCC)
{
	PrintFunc(L"OutputPin::GetStreamCaps");

	int count = (int)mtList.size();

	if (!ppmt || !pSCC) {
		return E_POINTER;
	}
	if (iIndex > (count - 1)) {
		return S_FALSE;
	}
	if (iIndex < 0) {
		return E_INVALIDARG;
	}

	AM_MEDIA_TYPE *pmt = mtList[iIndex].Duplicate();
	VIDEOINFOHEADER *vih = reinterpret_cast<decltype(vih)>(pmt->pbFormat);

	VIDEO_STREAM_CONFIG_CAPS caps = {};
	caps.guid = FORMAT_VideoInfo;
	caps.MinFrameInterval = vih->AvgTimePerFrame;
	caps.MaxFrameInterval = vih->AvgTimePerFrame;
	caps.MinOutputSize.cx = vih->bmiHeader.biWidth;
	caps.MinOutputSize.cy = vih->bmiHeader.biHeight;
	caps.MaxOutputSize = caps.MinOutputSize;
	caps.InputSize = caps.MinOutputSize;
	caps.MinCroppingSize = caps.MinOutputSize;
	caps.MaxCroppingSize = caps.MinOutputSize;
	caps.CropGranularityX = vih->bmiHeader.biWidth;
	caps.CropGranularityY = vih->bmiHeader.biHeight;
	caps.MinBitsPerSecond = vih->dwBitRate;
	caps.MaxBitsPerSecond = caps.MinBitsPerSecond;

	*ppmt = pmt;

	memcpy(pSCC, &caps, sizeof(caps));
	return S_OK;
}

STDMETHODIMP OutputPin::SetFormat(AM_MEDIA_TYPE *pmt)
{
	PrintFunc(L"OutputPin::SetFormat");

	if (pmt == nullptr)
		return VFW_E_INVALIDMEDIATYPE;

	mt = pmt;

	GetMediaTypeVFormat(mt, curVFormat);

	VIDEOINFOHEADER *vih = reinterpret_cast<decltype(vih)>(mt->pbFormat);
	curCX = vih->bmiHeader.biWidth;
	curCY = vih->bmiHeader.biHeight;
	curInterval = vih->AvgTimePerFrame;

	return S_OK;
}

STDMETHODIMP OutputPin::Set(REFGUID, DWORD, void *, DWORD, void *, DWORD)
{
	PrintFunc(L"OutputPin::Set");
	return E_NOTIMPL;
}

STDMETHODIMP OutputPin::Get(REFGUID guidPropSet, DWORD dwPropID, void *, DWORD,
			    void *pPropData, DWORD cbPropData,
			    DWORD *pcbReturned)
{
	PrintFunc(L"OutputPin::Get");

	if (guidPropSet != AMPROPSETID_Pin)
		return E_PROP_SET_UNSUPPORTED;
	if (dwPropID != AMPROPERTY_PIN_CATEGORY)
		return E_PROP_ID_UNSUPPORTED;
	if (pPropData == NULL && pcbReturned == NULL)
		return E_POINTER;

	if (pcbReturned)
		*pcbReturned = sizeof(GUID);
	if (pPropData == NULL)
		return S_OK;
	if (cbPropData < sizeof(GUID))
		return E_UNEXPECTED;

	*(GUID *)pPropData = PIN_CATEGORY_CAPTURE;
	return S_OK;
}

STDMETHODIMP OutputPin::QuerySupported(REFGUID guidPropSet, DWORD dwPropID,
				       DWORD *pTypeSupport)
{
	PrintFunc(L"OutputPin::QuerySupported");

	if (guidPropSet != AMPROPSETID_Pin)
		return E_PROP_SET_UNSUPPORTED;
	if (dwPropID != AMPROPERTY_PIN_CATEGORY)
		return E_PROP_ID_UNSUPPORTED;
	if (pTypeSupport)
		*pTypeSupport = KSPROPERTY_SUPPORT_GET;
	return S_OK;
}

bool OutputPin::AllocateBuffers(IPin *target, bool connecting)
{
	HRESULT hr;

	ComQIPtr<IMemInputPin> memInput(target);
	if (!memInput)
		return false;

	if (!!allocator) {
		allocator->Decommit();
	}

	hr = memInput->GetAllocator(&allocator);
	if (hr == VFW_E_NO_ALLOCATOR)
		hr = CoCreateInstance(CLSID_MemoryAllocator, NULL,
				      CLSCTX_INPROC_SERVER,
				      __uuidof(IMemAllocator),
				      (void **)&allocator);

	if (FAILED(hr))
		return false;

	VIDEOINFOHEADER *vih = reinterpret_cast<decltype(vih)>(mt->pbFormat);

	int cx = vih->bmiHeader.biWidth;
	int cy = vih->bmiHeader.biHeight;

	WORD bits = VFormatBits(curVFormat);
	bufSize = cx * cy * bits / 8;

	ALLOCATOR_PROPERTIES props;

	hr = memInput->GetAllocatorRequirements(&props);
	if (hr == E_NOTIMPL) {
		props.cBuffers = 4;
		props.cbAlign = 32;
		props.cbPrefix = 0;

	} else if (FAILED(hr)) {
		return false;
	}

	props.cbBuffer = (long)bufSize;

	ALLOCATOR_PROPERTIES actual;
	hr = allocator->SetProperties(&props, &actual);
	if (FAILED(hr))
		return false;

	if (!connecting && FAILED(allocator->Commit())) {
		return false;
	}

	memInput->NotifyAllocator(allocator, false);
	return true;
}

static MediaType CreateMediaType(VideoFormat format, int cx, int cy,
				 long long interval)
{
	MediaType mt;

	WORD bits = VFormatBits(format);
	DWORD size = cx * cy * bits / 8;
	uint64_t rate =
		(uint64_t)size * 10000000ULL / (uint64_t)interval * 8ULL;

	VIDEOINFOHEADER *vih = mt.AllocFormat<VIDEOINFOHEADER>();
	vih->bmiHeader.biSize = sizeof(vih->bmiHeader);
	vih->bmiHeader.biWidth = cx;
	vih->bmiHeader.biHeight = cy;
	vih->bmiHeader.biPlanes = VFormatPlanes(format);
	vih->bmiHeader.biBitCount = bits;
	vih->bmiHeader.biSizeImage = size;
	vih->bmiHeader.biCompression = VFormatToFourCC(format);
	vih->rcSource.right = cx;
	vih->rcSource.bottom = cy;
	vih->rcTarget = vih->rcSource;
	vih->dwBitRate = (DWORD)rate;
	vih->AvgTimePerFrame = interval;

	mt->majortype = MEDIATYPE_Video;
	mt->subtype = VFormatToSubType(format);
	mt->formattype = FORMAT_VideoInfo;
	mt->bFixedSizeSamples = true;
	mt->lSampleSize = size;

	return mt;
}

void OutputPin::AddVideoFormat(VideoFormat format, int cx, int cy,
			       long long interval)
{
	MediaType newMT = CreateMediaType(format, cx, cy, interval);
	mtList.push_back(newMT);
}

bool OutputPin::SetVideoFormat(VideoFormat format, int cx, int cy,
			       long long interval)
{
	mt = CreateMediaType(format, cx, cy, interval);

	if (curCX != cx || curCY != cy || curInterval != interval ||
	    curVFormat != format) {
		curVFormat = format;
		curCX = cx;
		curCY = cy;
		curInterval = interval;

		if (!!connectedPin) {
			setSampleMediaType = true;
			return ReallocateBuffers();
		}
	}

	return true;
}

bool OutputPin::LockSampleData(unsigned char **ptr)
{
	if (!connectedPin)
		return false;

	ComQIPtr<IMemInputPin> memInput(connectedPin);
	HRESULT hr;

	if (!memInput || !allocator)
		return false;

	hr = allocator->GetBuffer(&sample, nullptr, nullptr, 0);
	if (FAILED(hr))
		return false;

	if (FAILED(sample->SetActualDataLength((long)bufSize)))
		return false;
	if (FAILED(sample->SetDiscontinuity(false)))
		return false;
	if (FAILED(sample->SetPreroll(false)))
		return false;
	if (FAILED(sample->GetPointer(ptr)))
		return false;

	if (setSampleMediaType) {
		sample->SetMediaType(mt);
		setSampleMediaType = false;
	}

	return true;
}

void OutputPin::Send(unsigned char *data[DSHOW_MAX_PLANES],
		     size_t linesize[DSHOW_MAX_PLANES],
		     long long timestampStart, long long timestampEnd)
{
	BYTE *ptr;
	if (!LockSampleData(&ptr))
		return;

	size_t total = 0;
	for (size_t i = 0; i < DSHOW_MAX_PLANES; i++) {
		if (!linesize[i])
			break;

		memcpy(ptr + total, data[i], linesize[i]);
		total += linesize[i];
	}

	UnlockSampleData(timestampStart, timestampEnd);
}

void OutputPin::UnlockSampleData(long long timestampStart,
				 long long timestampEnd)
{
	if (!connectedPin)
		return;

	ComQIPtr<IMemInputPin> memInput(connectedPin);
	REFERENCE_TIME startTime = timestampStart;
	REFERENCE_TIME endTime = timestampEnd;

	sample->SetMediaTime(&startTime, &endTime);
	sample->SetTime(&startTime, &endTime);

	memInput->Receive(sample);

	sample.Clear();
}

void OutputPin::Stop()
{
	if (!!connectedPin) {
		connectedPin->BeginFlush();
		connectedPin->EndFlush();
	}
}

// ============================================================================

class SourceMiscFlags : public IAMFilterMiscFlags {
	volatile long refCount = 0;

public:
	inline SourceMiscFlags() {}
	virtual ~SourceMiscFlags() {}

	STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
	{
		if (riid == IID_IUnknown) {
			AddRef();
			*ppv = this;
		} else {
			*ppv = nullptr;
			return E_NOINTERFACE;
		}

		return NOERROR;
	}

	STDMETHODIMP_(ULONG) AddRef()
	{
		return InterlockedIncrement(&refCount);
	}

	STDMETHODIMP_(ULONG) Release()
	{
		if (!InterlockedDecrement(&refCount)) {
			delete this;
			return 0;
		}

		return refCount;
	}

	STDMETHODIMP_(ULONG) GetMiscFlags()
	{
		return AM_FILTER_MISC_FLAGS_IS_SOURCE;
	}
};

OutputFilter::OutputFilter(VideoFormat format, int cx, int cy,
			   long long interval)
	: refCount(0),
	  state(State_Stopped),
	  graph(nullptr),
	  pin(new OutputPin(this, format, cx, cy, interval)),
	  misc(new SourceMiscFlags)
{
}

OutputFilter::~OutputFilter() {}

// IUnknown methods
STDMETHODIMP OutputFilter::QueryInterface(REFIID riid, void **ppv)
{
	if (riid == IID_IUnknown) {
		AddRef();
		*ppv = this;
	} else if (riid == IID_IPersist) {
		AddRef();
		*ppv = (IPersist *)this;
	} else if (riid == IID_IMediaFilter) {
		AddRef();
		*ppv = (IMediaFilter *)this;
	} else if (riid == IID_IBaseFilter) {
		AddRef();
		*ppv = (IBaseFilter *)this;
	} else if (riid == IID_IAMFilterMiscFlags) {
		misc.CopyTo((IAMFilterMiscFlags **)ppv);
	} else {
		*ppv = nullptr;
		return E_NOINTERFACE;
	}

	return NOERROR;
}

STDMETHODIMP_(ULONG) OutputFilter::AddRef()
{
	return InterlockedIncrement(&refCount);
}

STDMETHODIMP_(ULONG) OutputFilter::Release()
{
	if (!InterlockedDecrement(&refCount)) {
		delete this;
		return 0;
	}

	return refCount;
}

// IPersist method
STDMETHODIMP OutputFilter::GetClassID(CLSID *pClsID)
{
	DSHOW_UNUSED(pClsID);
	return E_NOTIMPL;
}

// IMediaFilter methods
STDMETHODIMP OutputFilter::GetState(DWORD dwMSecs, FILTER_STATE *State)
{
	PrintFunc(L"OutputFilter::GetState");

	*State = state;

	DSHOW_UNUSED(dwMSecs);
	return S_OK;
}

STDMETHODIMP OutputFilter::SetSyncSource(IReferenceClock *pClock)
{
	clock = pClock;
	return S_OK;
}

STDMETHODIMP OutputFilter::GetSyncSource(IReferenceClock **pClock)
{
	*pClock = clock.Get();
	if (*pClock) {
		(*pClock)->AddRef();
	}
	return NOERROR;
}

STDMETHODIMP OutputFilter::Stop()
{
	PrintFunc(L"OutputFilter::Stop");

	if (state != State_Stopped) {
		pin->Stop();
	}

	state = State_Stopped;
	return S_OK;
}

STDMETHODIMP OutputFilter::Pause()
{
	PrintFunc(L"OutputFilter::Pause");

	OutputPin *pin = GetPin();
	if (!!pin->allocator && state == State_Stopped) {
		pin->allocator->Commit();
	}

	state = State_Paused;
	return S_OK;
}

STDMETHODIMP OutputFilter::Run(REFERENCE_TIME tStart)
{
	PrintFunc(L"OutputFilter::Run");

	state = State_Running;

	DSHOW_UNUSED(tStart);
	return S_OK;
}

// IBaseFilter methods
STDMETHODIMP OutputFilter::EnumPins(IEnumPins **ppEnum)
{
	PrintFunc(L"OutputFilter::EnumPins");

	*ppEnum = new OutputEnumPins(this, nullptr);
	return (*ppEnum == nullptr) ? E_OUTOFMEMORY : NOERROR;
}

STDMETHODIMP OutputFilter::FindPin(LPCWSTR Id, IPin **ppPin)
{
	PrintFunc(L"OutputFilter::FindPin");

	DSHOW_UNUSED(Id);
	DSHOW_UNUSED(ppPin);
	return E_NOTIMPL;
}

STDMETHODIMP OutputFilter::QueryFilterInfo(FILTER_INFO *pInfo)
{
	PrintFunc(L"OutputFilter::QueryFilterInfo");

	StringCbCopyW(pInfo->achName, sizeof(pInfo->achName), FilterName());

	pInfo->pGraph = graph;
	if (graph) {
		IFilterGraph *graph_ptr = graph;
		graph_ptr->AddRef();
	}
	return NOERROR;
}

STDMETHODIMP OutputFilter::JoinFilterGraph(IFilterGraph *pGraph, LPCWSTR pName)
{
	DSHOW_UNUSED(pName);

	graph = pGraph;
	return NOERROR;
}

STDMETHODIMP OutputFilter::QueryVendorInfo(LPWSTR *pVendorInfo)
{
	DSHOW_UNUSED(pVendorInfo);
	return E_NOTIMPL;
}

const wchar_t *OutputFilter::FilterName() const
{
	return FILTER_NAME;
}

// ============================================================================

OutputEnumPins::OutputEnumPins(OutputFilter *filter_, OutputEnumPins *pEnum)
	: filter(filter_)
{
	curPin = (pEnum != nullptr) ? pEnum->curPin : 0;
}

OutputEnumPins::~OutputEnumPins() {}

// IUnknown
STDMETHODIMP OutputEnumPins::QueryInterface(REFIID riid, void **ppv)
{
	if (riid == IID_IUnknown || riid == IID_IEnumPins) {
		AddRef();
		*ppv = (IEnumPins *)this;
		return NOERROR;
	} else {
		*ppv = nullptr;
		return E_NOINTERFACE;
	}
}

STDMETHODIMP_(ULONG) OutputEnumPins::AddRef()
{
	return (ULONG)InterlockedIncrement(&refCount);
}

STDMETHODIMP_(ULONG) OutputEnumPins::Release()
{
	if (!InterlockedDecrement(&refCount)) {
		delete this;
		return 0;
	}

	return (ULONG)refCount;
}

// IEnumPins
STDMETHODIMP OutputEnumPins::Next(ULONG cPins, IPin **ppPins, ULONG *pcFetched)
{
	UINT nFetched = 0;

	if (curPin == 0 && cPins > 0) {
		IPin *pPin = filter->GetPin();

		*ppPins = pPin;
		pPin->AddRef();

		nFetched = 1;
		curPin++;
	}

	if (pcFetched)
		*pcFetched = nFetched;

	return (nFetched == cPins) ? S_OK : S_FALSE;
}

STDMETHODIMP OutputEnumPins::Skip(ULONG cPins)
{
	return ((curPin += cPins) > 1) ? S_FALSE : S_OK;
}

STDMETHODIMP OutputEnumPins::Reset()
{
	curPin = 0;
	return S_OK;
}

STDMETHODIMP OutputEnumPins::Clone(IEnumPins **ppEnum)
{
	*ppEnum = new OutputEnumPins(filter, this);
	return (*ppEnum == nullptr) ? E_OUTOFMEMORY : NOERROR;
}

// ============================================================================

OutputEnumMediaTypes::OutputEnumMediaTypes(OutputPin *pin_) : pin(pin_) {}

OutputEnumMediaTypes::~OutputEnumMediaTypes() {}

STDMETHODIMP OutputEnumMediaTypes::QueryInterface(REFIID riid, void **ppv)
{
	if (riid == IID_IUnknown || riid == IID_IEnumMediaTypes) {
		AddRef();
		*ppv = this;
		return NOERROR;
	} else {
		*ppv = nullptr;
		return E_NOINTERFACE;
	}
}

STDMETHODIMP_(ULONG) OutputEnumMediaTypes::AddRef()
{
	return (ULONG)InterlockedIncrement(&refCount);
}

STDMETHODIMP_(ULONG) OutputEnumMediaTypes::Release()
{
	if (!InterlockedDecrement(&refCount)) {
		delete this;
		return 0;
	}

	return (ULONG)refCount;
}

// IEnumMediaTypes
STDMETHODIMP OutputEnumMediaTypes::Next(ULONG cMediaTypes,
					AM_MEDIA_TYPE **ppMediaTypes,
					ULONG *pcFetched)
{
	PrintFunc(L"OutputEnumMediaTypes::Next");

	UINT total = (UINT)pin->mtList.size();
	UINT nFetched = 0;

	for (ULONG i = 0; i < cMediaTypes && curMT < total; i++) {
		*(ppMediaTypes++) = pin->mtList[curMT++].Duplicate();
		nFetched++;
	}

	if (pcFetched)
		*pcFetched = nFetched;

	return (nFetched == cMediaTypes) ? S_OK : S_FALSE;
}

STDMETHODIMP OutputEnumMediaTypes::Skip(ULONG cMediaTypes)
{
	PrintFunc(L"OutputEnumMediaTypes::Skip");

	UINT total = (UINT)pin->mtList.size();
	return ((curMT += cMediaTypes) > total) ? S_FALSE : S_OK;
}

STDMETHODIMP OutputEnumMediaTypes::Reset()
{
	PrintFunc(L"OutputEnumMediaTypes::Reset");

	curMT = 0;
	return S_OK;
}

STDMETHODIMP OutputEnumMediaTypes::Clone(IEnumMediaTypes **ppEnum)
{
	*ppEnum = new OutputEnumMediaTypes(pin);
	return (*ppEnum == nullptr) ? E_OUTOFMEMORY : NOERROR;
}

}; /* namespace DShow */
