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

#define COMMON_ENCODED_CX         720
#define COMMON_ENCODED_CY         480
#define COMMON_ENCODED_INTERVAL   (10010000000LL/60000LL)
#define COMMON_ENCODED_VFORMAT    VideoFormat::H264
#define COMMON_ENCODED_SAMPLERATE 48000

#define HD_PVR2_CX                COMMON_ENCODED_CX
#define HD_PVR2_CY                COMMON_ENCODED_CY
#define HD_PVR2_INTERVAL          COMMON_ENCODED_INTERVAL
#define HD_PVR2_VFORMAT           COMMON_ENCODED_VFORMAT
#define HD_PVR2_AFORMAT           AudioFormat::AAC
#define HD_PVR2_SAMPLERATE        COMMON_ENCODED_SAMPLERATE

#define ROXIO_CX                  COMMON_ENCODED_CX
#define ROXIO_CY                  COMMON_ENCODED_CY
#define ROXIO_INTERVAL            COMMON_ENCODED_INTERVAL
#define ROXIO_VFORMAT             COMMON_ENCODED_VFORMAT
#define ROXIO_AFORMAT             AudioFormat::AAC
#define ROXIO_SAMPLERATE          COMMON_ENCODED_SAMPLERATE
