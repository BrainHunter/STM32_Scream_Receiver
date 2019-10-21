/*
 * ScreamClient.c
 *
 *  Created on: 20.10.2019
 *      Author: BrainHunter
 */


#include "ScreamClient.h"
#include "stm32f4_discovery_audio.h"

/*
 * Scream netbuf Fifo
 *
 */

#define FIFO_SIZE 16

volatile int readptr = 0;
int writeptr = 0;

struct netbuf* FifoMem[FIFO_SIZE];

int sFIFO_isFull()
{
	if( (writeptr + 1 == readptr) ||
		(readptr == 0 && writeptr +1 == FIFO_SIZE))
	{
		return 1;
	}
	return 0;
}

// add a buffer to the fifo:
int sFIFO_write(struct netbuf *buf)
{
	if(!sFIFO_isFull())
	{
		FifoMem[writeptr++] = buf;
	}
	else
	{
		return 0;
	}
	// check for writeptr overflow:
	if(writeptr >= FIFO_SIZE)
	{
		writeptr = 0;
	}
	return 1;
}

// add a buffer to the fifo:
int sFIFO_read(struct netbuf **buf)
{
	if(writeptr == readptr)
	{ // fifo empty
		return 0;
	}

	*buf = FifoMem[readptr];
	// check for readptr overflow:
	if(readptr >= FIFO_SIZE)
	{
		readptr = 0;
	}
	return 1;
}



/*
 *
 *
 *
 */
volatile Scream_state_enum ActualState;

Scream_ret_enum Scream_Init(void)
{
	ActualState = Scream_Stop;

	if(BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_HEADPHONE,50, 44100)!= AUDIO_OK)
	{
		while (1)
		{
			osDelay(1);
		}
	}

	return ScreamOK;
}

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

Scream_ret_enum Scream_ParsePacket(unsigned char* pbuf, int paketsize){
	// packet < Headder
	if (paketsize < HEADER_SIZE) return ScreamErr;
	// packet larger than allowed?
	if (paketsize > MAX_SO_PACKETSIZE) return ScreamErr;

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

    return ScreamOK;
}


volatile struct netbuf *playing_buffer;

Scream_ret_enum Scream_SinkBuffer (struct netbuf *buf)
{
	void* databuf;
	uint16_t datasize;
	netbuf_data(buf, &databuf, &datasize);

	uint16_t paketsize = netbuf_len(buf);

	Scream_ret_enum ret= Scream_ParsePacket(databuf, paketsize);
	if(ScreamOK != ret)
	{
		// Packet header is wrong -> ignore packet
		netbuf_delete(buf);
		return ret;
	}


	// add the netbuff to the fifo
	sFIFO_write(buf);

	if(ActualState == Scream_Stop)
	{
			playing_buffer = buf;
			// start playing of the buffer
			BSP_AUDIO_OUT_Play(databuf+5, datasize-5); // data - header
			ActualState = Scream_Play;
	}


//	switch(ActualState)
//	{
//		Scream_Play:
//
//				break;
//		Scream_Stop:
//
//			break;
//		default:
//			// something bad happened:
//			ActualState = Scream_Stop;
//			return ScreamErr;
//			break;
//	}

	//
	return ScreamOK;
}

/**
* @brief  Calculates the remaining file size and new position of the pointer.
* @param  None
* @retval None
*/
void BSP_AUDIO_OUT_TransferComplete_CallBack(void)
{
	void* databuf;
	uint16_t datasize;
	// get next pbuf from curently playing netbuf:
	if(netbuf_next(playing_buffer) >= 0 )
	{
		netbuf_data(playing_buffer, &databuf, &datasize);
		BSP_AUDIO_OUT_ChangeBuffer((uint16_t*)databuf, datasize);
		return;
	}
	//last_pbuf already used. try to get the next netbuf from the fifo:
	netbuf_delete(playing_buffer);
	sFIFO_read(&playing_buffer);

	netbuf_data(playing_buffer, &databuf, &datasize);
	BSP_AUDIO_OUT_ChangeBuffer((uint16_t*)(databuf+5), datasize-5);		// headersize!



	// = BUFFER_OFFSET_FULL;
  //BSP_AUDIO_OUT_ChangeBuffer((uint16_t*)&Audio_Buffer[0], AUDIO_BUFFER_SIZE / 2);
}







