/*
 * ScreamClient.c
 *
 *  Created on: 20.10.2019
 *      Author: BrainHunter
 */


#include "ScreamClient.h"

/*
 * Data is transferred in UDP frames with a payload size of max. 1157 bytes, consisting of 5 bytes header and 1152 bytes PCM data.
 * The latter number is divisible by 4, 6 and 8, so a full number of samples for all channels will always fit into a packet.
 * The first header byte denotes the sampling rate. Bit 7 specifies the base rate: 0 for 48kHz, 1 for 44,1kHz. Other bits specify the
 * multiplier for the base rate.
 * The second header byte denotes the sampling width, in bits.
 * The third header byte denotes the number of channels being transferred.
 * The fourth and fifth header bytes make up the DWORD dwChannelMask from Microsofts WAVEFORMATEXTENSIBLE structure,
 * describing the mapping of channels to speaker positions.
 *
 * */

screamHeader currentSetting;

Scream_ret_enum Scream_ParsePacket(unsigned char* pbuf, int size){
	// packet < Headder
	if (size < HEADER_SIZE) return ScreamErr;
	// packet larger than allowed?
	if (size > MAX_SO_PACKETSIZE) return ScreamErr;

	currentSetting.sampleRate = ((pbuf[0] >= 128) ? 44100 : 48000) * (pbuf[0] % 128);

	switch (pbuf[1])
	{
		case 16:  currentSetting.bytesPerSample = 2; break;
		case 24:  currentSetting.bytesPerSample = 3; break;
		case 32:  currentSetting.bytesPerSample = 4; break;
		default:
			//if (verbosity > 0) {
			// 	fprintf(stderr, "Unsupported sample size %hhu, not playing until next format switch.\n", cur_server_size);
			//}
			currentSetting.sampleRate = 0;
			return ScreamFmt;
	}

	currentSetting.channels = pbuf[2];
	if(currentSetting.channels != 2)
	{
		return ScreamUnsup;
	}

    currentSetting.channelMap = (pbuf[4] << 8) | pbuf[3];

}
