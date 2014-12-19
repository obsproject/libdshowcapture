///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2014 AVerMedia Inc.
// All rights reserved.
//
// Description:
// This property is provide to 3rd party AP to use our HW encode function.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

static const GUID AVER_HW_ENCODE_PROPERTY =
{0x1bd55918, 0xbaf5, 0x4781, {0x8d, 0x76, 0xe0, 0xa0, 0xa5, 0xe1, 0xd2, 0xb8}};

enum {
	PROPERTY_HW_ENCODE_PARAMETER           = 0
};

enum {
	AVER_PARAMETER_ENCODE_FRAME_RATE       = 0,
	AVER_PARAMETER_ENCODE_BIT_RATE         = 1,
	AVER_PARAMETER_CURRENT_RESOLUTION      = 2,
	AVER_PARAMETER_ENCODE_RESOLUTION       = 3,
	AVER_PARAMETER_ENCODE_GOP              = 4,
	AVER_PARAMETER_INSERT_I_FRAME          = 6
};

struct AVER_PARAMETERS {
	ULONG ulIndex;
	ULONG ulParam1;
	ULONG ulParam2;
	ULONG ulParam3;
};
