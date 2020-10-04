/*
 * ScreamClient.c
 *
 *  Created on: 20.10.2019
 *      Author: BrainHunter
 */


#include "ScreamClient.h"
#include "FreeRTOS.h"
#include "task.h"
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
 * Scream Data buffers:
 *
 * */
#define NUM_BUFFERS 32

screamPacket ScreamBuffer[NUM_BUFFERS];
int buffer_read = 0;
int buffer_write = 0;

#define ZERO_BUFFER_SIZE 100
uint16_t zeroBuffer[ZERO_BUFFER_SIZE];

int buffer_isFull()
{
	if( (buffer_write + 1 == buffer_read) ||
		(buffer_read == 0 && buffer_write +1 == NUM_BUFFERS))
	{
		return 1;
	}
	return 0;
}

int buffer_numItems()
{
	int w = buffer_write;
	int r = buffer_read;

	int num = (w + NUM_BUFFERS - r) % NUM_BUFFERS;
	return num;

}


/*
 *
 *
 *
 *
 */


unsigned long timediff(unsigned long t1, unsigned long t2)
{
    signed long d = (signed long)t1 - (signed long)t2;
    if(d < 0) d = -d;
    return (unsigned long) d;
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

	if(BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_HEADPHONE,80, 44100)!= AUDIO_OK)
	//if(BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_HEADPHONE,80, 48000)!= AUDIO_OK)
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
	//static int pllnOffset = 0;
	void* databuf;
	uint16_t datasize;

	// copy data from netbuf to databuf
	netbuf_data(buf, &databuf, &datasize);
	uint16_t paketsize = netbuf_len(buf);

	Scream_ret_enum ret= Scream_ParsePacket(databuf, paketsize);
	if(ScreamOK != ret)
	{
		// Packet header is wrong -> ignore packet
		netbuf_delete(buf);
		return ret;
	}

	// measure bandwidth:
	static TickType_t startTime = 0;
	static uint64_t transferedSamples = 0;
	static uint64_t SampleRateMeasured = 0;

	TickType_t now = xTaskGetTickCount();
	uint32_t td = timediff(now, startTime);
	//sum all samples:
	transferedSamples += (paketsize - HEADER_SIZE)/4;	//4 byte = 32bit= 16 r + 16 l

	if(td >= 1000 * portTICK_RATE_MS *10)
	{
		// calculate SampleRate:
		SampleRateMeasured = transferedSamples *100 * portTICK_RATE_MS * 1000 / td;

		// reset all
		transferedSamples = 0;
		startTime = now;
	}
	// END measure bandwidth





	// copy buffer:
	int buffer_tmp;
	if(!buffer_isFull())
	{
		// copy over the netbuf
		netbuf_copy_partial (buf, ScreamBuffer[buffer_write].data, MAX_PAYLOAD, HEADER_SIZE);
		ScreamBuffer[buffer_write].size = paketsize-HEADER_SIZE;
		netbuf_delete(buf);

		buffer_tmp = buffer_write;
		buffer_write++;
		if(buffer_write >= NUM_BUFFERS)	// check for write counter overflow.
		{
			buffer_write=0;
		}
	}
	else
	{	// Buffer is full: blink LED3
		HAL_GPIO_WritePin(GPIOD, LD3_Pin, GPIO_PIN_SET);
		netbuf_delete(buf);
		HAL_GPIO_WritePin(GPIOD, LD3_Pin, GPIO_PIN_RESET);
		return ScreamBFull;

	}



	// P & I control loop for PLL:
//	#define kp 	1000	//100
//	#define ki 	1		//3

	int buffs = buffer_numItems();
//	static int I=0;
//   int P = 0;
//	#define BUFFER_TARGET ((NUM_BUFFERS/2))

//	int diff = ((buffs - BUFFER_TARGET) * 100) / BUFFER_TARGET;		// difference from target in percent
    //if(diff < 12 && diff > -12) diff = 0;
    //if(diff >= 12) diff -= 12;
    //if(diff <= -12) diff += 12;

	// diff lowpass filter:
//	static int fdiff  =0;
//	fdiff = ((fdiff * 9)+(diff*1))/10;




	// Proportional:
//	P = fdiff * kp;

	// Integral :
//	I = I + fdiff * ki;

//	if(I < -250000) I = -250000;
//	if(I > 250000)  I = 250000;



	//
//	int set = (P+I)/10000;
	//set_PLL(13);

	// debug output:
	char buff[100];
	//sprintf(buff, "P: %-5d I: %-6d set: %3d fill: %3d diff: %3d fdiff: %3d SRm: %8lu \r\n", P, I, set, buffs, diff, fdiff, SampleRateMeasured);
	sprintf(buff, "fill: %3d SRm: %8lu \r\n",  buffs, SampleRateMeasured);
	//CDC_Transmit_FS(buff,sizeof(buff));
	CDC_Transmit_FS(buff,strlen(buff));


	// Start the Playback if there are 10 buffers stored
	if(ActualState == Scream_Stop && buffs > 10)
	{
			// start playing of the buffer
			//BSP_AUDIO_OUT_Play(ScreamBuffer[buffer_tmp].data, ScreamBuffer[buffer_tmp].size/2); // data - header
			// buffer_read = buffer_write;		// set the fifo correctly
			BSP_AUDIO_OUT_Play(ScreamBuffer[buffer_read].data, ScreamBuffer[buffer_read].size/2); // data - header
			buffer_read++;
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

	//check if data available:
	if ( buffer_write != buffer_read)
	{
		BSP_AUDIO_OUT_ChangeBuffer((uint16_t*)ScreamBuffer[buffer_read].data, ScreamBuffer[buffer_read].size/2);

		buffer_read++;
		if(buffer_read >= NUM_BUFFERS)	// check for read counter overflow.
		{
			buffer_read=0;
		}

	}
	else
	{
		// output zero
		BSP_AUDIO_OUT_ChangeBuffer((uint16_t*)zeroBuffer, ZERO_BUFFER_SIZE);

		//ActualState = Scream_Stop;

	}


	// = BUFFER_OFFSET_FULL;
  //BSP_AUDIO_OUT_ChangeBuffer((uint16_t*)&Audio_Buffer[0], AUDIO_BUFFER_SIZE / 2);
}



//void set_PLL(int offset){
//	RCC_PeriphCLKInitTypeDef rccclkinit;
//	static int oldOffset = 0;
//	// do not let offset run out of limits
//	if(offset > 23)
//		offset = 23;
//
//	if(offset < -23)
//		offset = -23;
//
//	if(offset == oldOffset) return;
//	oldOffset = offset;
//
//	HAL_RCCEx_GetPeriphCLKConfig(&rccclkinit);
//
//    /* I2S clock config
//    PLLI2S_VCO = f(VCO clock) = f(PLLI2S clock input) � (PLLI2SN/PLLM)
//    I2SCLK = f(PLLI2S clock output) = f(VCO clock) / PLLI2SR */
//    rccclkinit.PeriphClockSelection = RCC_PERIPHCLK_I2S;
//    rccclkinit.PLLI2S.PLLI2SN = 258+offset;
//    rccclkinit.PLLI2S.PLLI2SR = 3;
//    HAL_RCCEx_PeriphCLKConfig(&rccclkinit);
//
//
//    //
//    //__HAL_RCC_PLLI2S_CONFIG(PeriphClkInit->PLLI2S.PLLI2SN , PeriphClkInit->PLLI2S.PLLI2SR);
//    //
//    //__HAL_RCC_PLLI2S_DISABLE();
//   // __HAL_RCC_PLLI2S_CONFIG(258+offset , 3);
//    //__HAL_RCC_PLLI2S_ENABLE();
//}




