/*
 * ScreamClien.h
 *
 *  Created on: 20.10.2019
 *      Author: BrainHunter
 */

#ifndef SCREAMCLIEN_H_
#define SCREAMCLIEN_H_

#include "stdint.h"
// #include "lwip/api.h" // maybe?
#include "lwip/netbuf.h"

#define SCREAM_DEFAULT_MULTICAST_GROUP "239.255.77.77"
#define SCREAM_DEFAULT_PORT 4010

typedef enum{
	ScreamOK = 0,

// general Error
	ScreamErr = -1,

// Format Error
	ScreamFmt = -2,

// Unsupported
	ScreamUnsup = -3,

// Buffer full
	ScreamBFull = -4

} Scream_ret_enum;


typedef enum{
	Scream_Play,
	Scream_Stop
}Scream_state_enum;

#define HEADER_SIZE 5
#define MAX_PAYLOAD 1152
#define MAX_SO_PACKETSIZE MAX_PAYLOAD+HEADER_SIZE

typedef struct {
	unsigned int 	sampleRate;
	unsigned char 	bytesPerSample;
	unsigned char 	channels;
	uint16_t		channelMap;
} screamHeader;

typedef struct {
	unsigned int 	size;
	//unsigned char 	data[MAX_PAYLOAD];
	int16_t 	data[MAX_PAYLOAD/2];
} screamPacket;


Scream_ret_enum Scream_ParsePacket(unsigned char* pbuf, int size);
Scream_ret_enum Scream_Init(void);
Scream_ret_enum Scream_SinkBuffer (struct netbuf *buf);

#endif /* SCREAMCLIEN_H_ */
