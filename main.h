/*
 * main.h
 *
 *  Created on: 10 Mar 2025
 *      Author: Sayed
 */

#ifndef MAIN_H_
#define MAIN_H_

/* Program Version*/
#define MAJOR_VERSION 1
#define MINOR_VERSION 0
#define BUILD_VERSION 0


/* Private typedef */
// Sensor address and poll count definitions

#define GAS_READING_MEASURAND   30057



/* Exported defines ----------------------------------------------------------*/

// Comms type
#define MODBUS_PROTOCOL     0
#define DYNAMENT_PROTOCOL   1
#define COMMS_PROTOCOL      DYNAMENT_PROTOCOL

// Checksum type
#define CSUM_STANDARD   0
#define CSUM_CRC        1
#define CSUM_TYPE       CSUM_STANDARD


#endif /* MAIN_H_ */
