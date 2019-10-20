/*
 * ScreamClien.h
 *
 *  Created on: 20.10.2019
 *      Author: BrainHunter
 */

#ifndef SCREAMCLIEN_H_
#define SCREAMCLIEN_H_

#include "stdint.h"

#define SCREAM_DEFAULT_MULTICAST_GROUP "239.255.77.77"
#define SCREAM_DEFAULT_PORT 4010

typedef enum{
	ScreamOK = 0,

// general Error
	ScreamErr = -1,

// Format Error
	ScreamFmt = -2,

// Unsupported
	ScreamUnsup = -3

} Scream_ret_enum;


#define HEADER_SIZE 5
#define MAX_SO_PACKETSIZE 1152+HEADER_SIZE

typedef struct {
	unsigned int 	sampleRate;
	unsigned char 	bytesPerSample;
	unsigned char 	channels;
	uint16_t		channelMap;
} screamHeader;


Scream_ret_enum Scream_ParsePacket(unsigned char* pbuf, int size);


#endif /* SCREAMCLIEN_H_ */
