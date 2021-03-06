/**************************************************************************************************
  Filename:       npi_lnx_serial_configuration.c
  Revised:        $Date: 2011-11-23 12:02:49 -0800 (Wed, 23 Nov 2011) $
  Revision:       $Revision: 108 $

  Description:    This file defines linux specific interface to Network Processor Interface
                  module.


  Copyright (C) {2015} Texas Instruments Incorporated - http://www.ti.com/


   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

     Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

     Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the
     distribution.

     Neither the name of Texas Instruments Incorporated nor the names of
     its contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**************************************************************************************************/
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include <unistd.h>

#include "npi_lnx.h"
#include "npi_lnx_serial_configuration.h"
#include "npi_lnx_error.h"
#include "tiLogging.h"

static char* pStrBufRoot;


#define IDX_GPIO          0
#define IDX_LEVEL_SHIFTER 1

const char* sectionNamesArray[5][2] =
{
		{
				"GPIO_SRDY.GPIO",
				"GPIO_SRDY.LEVEL_SHIFTER"
		},
		{
				"GPIO_MRDY.GPIO",
				"GPIO_MRDY.LEVEL_SHIFTER"
		},
		{
				"GPIO_RESET.GPIO",
				"GPIO_RESET.LEVEL_SHIFTER"
		},
		{
				"GPIO_DD.GPIO",
				"GPIO_DD.LEVEL_SHIFTER"
		},
		{
				"GPIO_DC.GPIO",
				"GPIO_DC.LEVEL_SHIFTER"
		},
};

/******************************************************************************
* @fn        getSerialConfiguration
*
* @brief     This function populates the serial configuration parameters as
* 			read from the configuration file.
*
* input parameters
*
* @param		configFilePath	-- configuration file path
* @param     serialCfg 		-- pointer to structure holding all parameters
*
* output parameters
*
* @param     serialCfg 		-- pointer to structure holding all parameters
*
* @return     TRUE if the configuration parameters were read successfully.
*             FALSE, otherwise.
******************************************************************************
*/
int getSerialConfiguration(const char *configFilePath, npiSerialCfg_t *serialCfg)
{
	int retVal = NPI_LNX_SUCCESS;

	char* strBuf;
	int gpioIdx = 0;

	// Allocate memory for string buffer and configuration buffer
	strBuf = (char*) malloc(128);
	memset(strBuf, 0, 128);
	pStrBufRoot = strBuf;
	memset(serialCfg->devPath, 0, sizeof(serialCfg->devPath));
	memset(serialCfg->logPath, 0, sizeof(serialCfg->logPath));
	for (gpioIdx = 0; gpioIdx < SERIAL_CFG_MAX_NUM_OF_GPIOS; gpioIdx++)
	{
		memset(&serialCfg->gpioCfg[gpioIdx], 0, sizeof(halGpioCfg_t));
	}

	// Open file for parsing
	FILE *serialCfgFd = fopen(configFilePath, "r");
	if (serialCfgFd == NULL)
	{
		//                            debug_
		LOG_ERROR("Could not open file '%s'\n", configFilePath);
		npi_ipc_errno = NPI_LNX_ERROR_IPC_OPEN_REMOTI_RNP_CFG;
	}

   // Check start-up option(s)

	if (NPI_LNX_FAILURE != (SerialConfigParser(serialCfgFd, "STARTUP", "delaySeconds", strBuf)))
	{
		int delaySeconds = atoi(strBuf);
		LOG_INFO("NOTICE: Found optional STARTUP delaySeconds = %d\n", delaySeconds);
		if (delaySeconds > 0)
		{
			LOG_INFO("Sleeping %d seconds before continuing.\n", delaySeconds);
			sleep(delaySeconds);
			LOG_INFO("Resuming.\n");
		}
	}

	// Get device type
	if (NPI_LNX_FAILURE == (SerialConfigParser(serialCfgFd, "DEVICE", "deviceKey", strBuf)))
	{
		LOG_FATAL("Could not find 'deviceKey' inside config file '%s'\n", configFilePath);
		npi_ipc_errno = NPI_LNX_ERROR_IPC_REMOTI_RNP_CFG_PARSER_DEVICE_KEY;
		retVal = NPI_LNX_FAILURE;
	}

	// Copy from buffer to variable
	serialCfg->devIdx = strBuf[0] - '0';
	//            debug_
	LOG_DEBUG("deviceKey = %i  (%s - %s)\n", serialCfg->devIdx, strBuf,
	      (serialCfg->devIdx == NPI_SERVER_DEVICE_INDEX_UART) ? "UART" :
			(serialCfg->devIdx == NPI_SERVER_DEVICE_INDEX_SPI)  ? "SPI" :
			(serialCfg->devIdx == NPI_SERVER_DEVICE_INDEX_I2C)  ? "I2C" : "?");

	// Get path to the device
	strBuf = pStrBufRoot;
	if (NPI_LNX_FAILURE == (SerialConfigParser(serialCfgFd, "DEVICE", "devPath", strBuf)))
	{
		LOG_FATAL("Could not find 'devPath' inside config file '%s'\n", configFilePath);
		npi_ipc_errno = NPI_LNX_ERROR_IPC_REMOTI_RNP_CFG_PARSER_DEVICE_PATH;
		retVal = NPI_LNX_FAILURE;
	}
	// Copy from buffer to variable
	memcpy(serialCfg->devPath, strBuf, strlen(strBuf));
	//            debug_
	LOG_DEBUG("serialCfg->devPath = '%s'\n", serialCfg->devPath);

	// Get path to the log file
	strBuf = pStrBufRoot;
	if (NPI_LNX_FAILURE == (SerialConfigParser(serialCfgFd, "LOG", "log", strBuf)))
	{
		LOG_FATAL("Could not find 'log' inside config file '%s'\n", configFilePath);
		npi_ipc_errno = NPI_LNX_ERROR_IPC_REMOTI_RNP_CFG_PARSER_LOG_PATH;
		retVal = NPI_LNX_FAILURE;
	}
	// Copy from buffer to variable
	strcpy(serialCfg->logPath, strBuf);
	//            debug_
	LOG_DEBUG("serialCfg->logPath = '%s'\n", serialCfg->logPath);
	if (!*serialCfg->logPath)
	{
		LOG_ALWAYS("No log file path configured. Logs will go to stderr.\n");
	}

	// If Debug Interface is supported, configure it.
	if (NPI_LNX_FAILURE == (SerialConfigParser(serialCfgFd, "DEBUG", "supported", strBuf)))
	{
		LOG_DEBUG("Could not find [DEBUG]'supported' inside config file '%s'\n", configFilePath);
		serialCfg->debugSupported = 0;
	}
	else
	{
		// Copy from buffer to variable
		serialCfg->debugSupported = strBuf[0] - '0';
	}

	uint8 gpioStart = 0, gpioEnd = 0;
	if (serialCfg->debugSupported)
	{
		LOG_DEBUG("Debug Interface is supported\n");
		gpioEnd = 5;
		// If UART then skip MRDY, SRDY
		if (serialCfg->devIdx == NPI_SERVER_DEVICE_INDEX_UART)
		{
			gpioStart = 2;
		}
	}
	else if ((serialCfg->devIdx == NPI_SERVER_DEVICE_INDEX_SPI) ||
	         (serialCfg->devIdx == NPI_SERVER_DEVICE_INDEX_I2C))
	{
		gpioEnd = 3;
	}
	else if (serialCfg->devIdx == NPI_SERVER_DEVICE_INDEX_UART)
	{
		gpioStart = 2;
		gpioEnd = 3;
	}

	// GPIO configuration
	if ((serialCfg->devIdx == NPI_SERVER_DEVICE_INDEX_UART) ||
	    (serialCfg->devIdx == NPI_SERVER_DEVICE_INDEX_SPI) ||
	    (serialCfg->devIdx == NPI_SERVER_DEVICE_INDEX_I2C))
	{
		for (gpioIdx = gpioStart; gpioIdx < gpioEnd; gpioIdx++)
		{
			// Get SRDY, MRDY or RESET GPIO
			LOG_DEBUG("serialCfg->gpioCfg[gpioIdx].gpio \t\t\t%p\n",
					(void *)&(serialCfg->gpioCfg[gpioIdx].gpio));

			// Get SRDY, MRDY or RESET GPIO value
			strBuf = pStrBufRoot;
			if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, sectionNamesArray[gpioIdx][IDX_GPIO],
					"value", strBuf)))
			{
			// Copy from buffer to variable
				LOG_DEBUG("strBuf \t\t\t\t\t%p\n",
						(void *)&strBuf);
				LOG_DEBUG("serialCfg->gpioCfg[gpioIdx].gpio.value \t\t%p\n",
						(void *)&(serialCfg->gpioCfg[gpioIdx].gpio.value));
				memcpy(serialCfg->gpioCfg[gpioIdx].gpio.value, strBuf, strlen(strBuf));
				LOG_DEBUG("serialCfg->gpioCfg[%i]->gpio.value = '%s'\n",
						gpioIdx, serialCfg->gpioCfg[gpioIdx].gpio.value);
			}
			else
			{
				LOG_FATAL("[CONFIG] Key 'value' is missing for mandatory GPIO %s\n", sectionNamesArray[gpioIdx][IDX_GPIO]);
				npi_ipc_errno = NPI_LNX_ERROR_IPC_REMOTI_RNP_CFG_PARSER_DEVICE_GPIO(gpioIdx, 0, serialCfg->devIdx);
				retVal = NPI_LNX_FAILURE;
			}

			// Get SRDY, MRDY or RESET GPIO direction
			strBuf = pStrBufRoot;
			if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, sectionNamesArray[gpioIdx][IDX_GPIO],
					"direction", strBuf)))
			{
			// Copy from buffer to variable
				LOG_DEBUG("strBuf \t\t\t\t\t%p\n",
						(void *)&strBuf);
				LOG_DEBUG("serialCfg->gpioCfg[gpioIdx].gpio.direction \t%p\n",
						(void *)&(serialCfg->gpioCfg[gpioIdx].gpio.direction));
				memcpy(serialCfg->gpioCfg[gpioIdx].gpio.direction, strBuf,
						strlen(strBuf));
				LOG_DEBUG("serialCfg->gpioCfg[%i]->gpio.direction = '%s'\n",
						gpioIdx, serialCfg->gpioCfg[gpioIdx].gpio.direction);
			}
		#ifndef USE_BCM_NEXUS_GPIO // direction is unused for Nexus GPIO config, so it's not an error if missing
			else
			{
				LOG_FATAL("[CONFIG] Key 'direction' is missing for mandatory GPIO %s\n", sectionNamesArray[gpioIdx][IDX_GPIO]);
				npi_ipc_errno = NPI_LNX_ERROR_IPC_REMOTI_RNP_CFG_PARSER_DEVICE_GPIO(gpioIdx, 0, serialCfg->devIdx);
				retVal = NPI_LNX_FAILURE;
			}
		#endif

		#ifdef SRDY_INTERRUPT
			// Get SRDY, MRDY or RESET GPIO edge
			if (gpioIdx == 0)
			{
				strBuf = pStrBufRoot;
				if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, sectionNamesArray[gpioIdx][IDX_GPIO],
						"edge", strBuf)))
				{
					// Copy from buffer to variable
					LOG_DEBUG("strBuf \t\t\t\t\t%p\n",
							(void *)&strBuf);
					LOG_DEBUG("serialCfg->gpioCfg[gpioIdx].gpio.edge \t%p\n",
							(void *)&(serialCfg->gpioCfg[gpioIdx].gpio.edge));
					memcpy(serialCfg->gpioCfg[gpioIdx].gpio.edge, strBuf, strlen(strBuf));
					LOG_DEBUG("serialCfg->gpioCfg[%i]->gpio.edge = '%s'\n",
							gpioIdx, serialCfg->gpioCfg[gpioIdx].gpio.edge);
				}
			#ifndef USE_BCM_NEXUS_GPIO // Edge is unused for Nexus GPIO (hard-coded to both edges) so it's not an error if missing
				else
				{
					LOG_FATAL("[CONFIG] Key 'edge' is missing for mandatory GPIO %s\n", sectionNamesArray[gpioIdx][IDX_GPIO]);
					npi_ipc_errno = NPI_LNX_ERROR_IPC_REMOTI_RNP_CFG_PARSER_DEVICE_GPIO(gpioIdx, 0, serialCfg->devIdx);
					retVal = NPI_LNX_FAILURE;
				}
			#endif
			}
		#endif
			// Get SRDY, MRDY or RESET GPIO Active High/Low
			strBuf = pStrBufRoot;
			if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd,
					sectionNamesArray[gpioIdx][IDX_GPIO], "active_high_low",
					strBuf)))
			{
			// Copy from buffer to variable
				serialCfg->gpioCfg[gpioIdx].gpio.active_high_low = strBuf[0] - '0';
				LOG_DEBUG("serialCfg->gpioCfg[%i]->gpio.active_high_low = %d\n",
								gpioIdx, serialCfg->gpioCfg[gpioIdx].gpio.active_high_low);
			}
			else
			{
				LOG_WARN("[CONFIG] Key 'active_high_low' is missing for optional GPIO %s\n", sectionNamesArray[gpioIdx][IDX_GPIO]);
			}

		#ifndef USE_BCM_NEXUS_GPIO // Level Shifter is unused for Nexus GPIO, so don't look for it.
			// Get SRDY, MRDY or RESET Level Shifter
			LOG_DEBUG("serialCfg->gpioCfg[gpioIdx].levelshifter \t\t\t%p\n",
						(void *)&(serialCfg->gpioCfg[gpioIdx].levelshifter));

			// Get SRDY, MRDY or RESET Level Shifter value
			strBuf = pStrBufRoot;
			if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd,
					sectionNamesArray[gpioIdx][IDX_LEVEL_SHIFTER], "value", strBuf)))
			{
			// Copy from buffer to variable
				memcpy(serialCfg->gpioCfg[gpioIdx].levelshifter.value, strBuf,
						strlen(strBuf));
				LOG_DEBUG("serialCfg->gpioCfg[%i]->levelshifter.value = '%s'\n",
						gpioIdx, serialCfg->gpioCfg[gpioIdx].levelshifter.value);
			}
			else
			{
				LOG_WARN("[CONFIG] Key 'value' is missing for optional GPIO %s\n", sectionNamesArray[gpioIdx][IDX_LEVEL_SHIFTER]);
			}

			// Get SRDY, MRDY or RESET Level Shifter direction
			strBuf = pStrBufRoot;
			if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd,
					sectionNamesArray[gpioIdx][IDX_LEVEL_SHIFTER], "direction", strBuf)))
			{
				// Copy from buffer to variable
				memcpy(serialCfg->gpioCfg[gpioIdx].levelshifter.direction, strBuf,
						strlen(strBuf));
				LOG_DEBUG("serialCfg->gpioCfg[%i]->levelshifter.direction = '%s'\n",
							gpioIdx, serialCfg->gpioCfg[gpioIdx].levelshifter.direction);
			}
			else
			{
				LOG_WARN("[CONFIG] Key 'direction' is missing for optional GPIO %s\n", sectionNamesArray[gpioIdx][IDX_LEVEL_SHIFTER]);
			}


			// Get SRDY, MRDY or RESET Level Shifter Active High/Low
			strBuf = pStrBufRoot;
			if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd,
					sectionNamesArray[gpioIdx][IDX_LEVEL_SHIFTER], "active_high_low", strBuf)))
			{
			// Copy from buffer to variable
				serialCfg->gpioCfg[gpioIdx].levelshifter.active_high_low = atoi(strBuf);
				LOG_DEBUG("serialCfg->gpioCfg[%i]->levelshifter.active_high_low = %d\n",
							gpioIdx, serialCfg->gpioCfg[gpioIdx].levelshifter.active_high_low);
			}
			else
			{
				LOG_WARN("[CONFIG] Key 'active_high_low' is missing for optional GPIO %s\n", sectionNamesArray[gpioIdx][IDX_LEVEL_SHIFTER]);
			}
		#endif  // USE_BCM_NEXUS_GPIO
		}
	}

	switch(serialCfg->devIdx)
	{
		case NPI_SERVER_DEVICE_INDEX_UART_USB:
			// Initialization of UART for USB is the same as for physical UART.
			// Except for Reset GPIO
		case NPI_SERVER_DEVICE_INDEX_UART:
		#if (defined NPI_UART) && (NPI_UART == TRUE)
			strBuf = pStrBufRoot;
			if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "UART", "speed", strBuf)))
			{
				serialCfg->serial.npiUartCfg.speed = atoi(strBuf);
			}
			else
			{
				serialCfg->serial.npiUartCfg.speed=115200;
			}
			if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "UART", "flowcontrol", strBuf)))
			{
				serialCfg->serial.npiUartCfg.flowcontrol = atoi(strBuf);
			}
			else
			{
				serialCfg->serial.npiUartCfg.flowcontrol=0;
			}
		#endif
			break;

		case NPI_SERVER_DEVICE_INDEX_SPI:
		#if (defined NPI_SPI) && (NPI_SPI == TRUE)
			// SPI Specific configuration
			if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "SPI", "speed", strBuf)))
			{
				serialCfg->serial.npiSpiCfg.spiCfg.speed = strtol(strBuf, NULL, 10);
			}
			else
			{
				serialCfg->serial.npiSpiCfg.spiCfg.speed = 500000;
			}
			if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "SPI", "mode", strBuf)))
			{
				serialCfg->serial.npiSpiCfg.spiCfg.mode = strtol(strBuf, NULL, 16);
			}
			else
			{
				serialCfg->serial.npiSpiCfg.spiCfg.mode = 0;
			}
			if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "SPI", "bitsPerWord", strBuf)))
			{
				serialCfg->serial.npiSpiCfg.spiCfg.bitsPerWord = strtol(strBuf, NULL, 10);
			}
			else
			{
				serialCfg->serial.npiSpiCfg.spiCfg.bitsPerWord = 8;
			}
			if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "SPI", "forceRunOnReset", strBuf)))
			{
				serialCfg->serial.npiSpiCfg.forceRunOnReset = strtol(strBuf, NULL, 16);
			}
			else
			{
				// If it is not defined then set value for RNP
				serialCfg->serial.npiSpiCfg.forceRunOnReset = NPI_LNX_UINT8_ERROR;
			}

			// Configuration that is common between all devices that employ MRDY SRDY signaling
			if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "MRDY_SRDY", "useFullDuplexAPI", strBuf)))
			{
				serialCfg->serial.npiSpiCfg.spiCfg.useFullDuplexAPI = strtol(strBuf, NULL, 10);
			}
			else
			{
				serialCfg->serial.npiSpiCfg.spiCfg.useFullDuplexAPI = TRUE;
			}
			if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "MRDY_SRDY", "earlyMrdyDeAssert", strBuf)))
			{
				serialCfg->serial.npiSpiCfg.earlyMrdyDeAssert = strtol(strBuf, NULL, 10);
			}
			else
			{
				// If it is not defined then set value for RNP
				serialCfg->serial.npiSpiCfg.earlyMrdyDeAssert = TRUE;
			}
			if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "MRDY_SRDY", "detectResetFromSlowSrdyAssert", strBuf)))
			{
				serialCfg->serial.npiSpiCfg.detectResetFromSlowSrdyAssert = strtol(strBuf, NULL, 10);
			}
			else
			{
				// If it is not defined then set value for RNP
				serialCfg->serial.npiSpiCfg.detectResetFromSlowSrdyAssert = TRUE;
			}
			if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "MRDY_SRDY", "srdyMrdyHandshakeSupport", strBuf)))
			{
				serialCfg->serial.npiSpiCfg.srdyMrdyHandshakeSupport = strtol(strBuf, NULL, 10);
			}
			else
			{
				// If it is not defined then set value for RNP
				serialCfg->serial.npiSpiCfg.srdyMrdyHandshakeSupport = TRUE;
			}

			serialCfg->serial.npiSpiCfg.gpioCfg = (halGpioCfg_t *)serialCfg->gpioCfg;
		#endif
			break;

		case NPI_SERVER_DEVICE_INDEX_I2C:
		#if (defined NPI_I2C) && (NPI_I2C == TRUE)
			serialCfg->serial.npiI2cCfg.gpioCfg = (halGpioCfg_t *)serialCfg->gpioCfg;
		#endif
			break;

		default:
			retVal = NPI_LNX_FAILURE;
			break;
	}

	// Get port from configuration file
	if (NPI_LNX_FAILURE == (SerialConfigParser(serialCfgFd, "PORT", "port", strBuf)))
	{
		// Fall back to default if port was not found in the configuration file
		strncpy(serialCfg->port, NPI_PORT, sizeof(serialCfg->port)-1);
		LOG_WARN("Port not found in configuration file! Will use default port: %s\n", serialCfg->port);
	}
	else
	{
		strncpy(serialCfg->port, strBuf, sizeof(serialCfg->port)-1);
	}

	return retVal;
}

/**************************************************************************************************
 *
 * @fn          SerialConfigParser
 *
 * @brief       This function searches for a string a returns its value
 *
 * input parameters
 *
 * @param          configFilePath   - path to configuration file
 * @param          section          - section to search for
 * @param          key                                                         - key to return value of within section
 *
 * output parameters
 *
 * None.
 *
 * @return      None.
 *
 **************************************************************************************************/
int SerialConfigParser(FILE* serialCfgFd, const char* section, const char* key, char* resultString)
{
	uint8 sectionFound = FALSE, invalidLineLen = FALSE;
	char* resString = NULL;
	char* resStringToFree = NULL;
	char* psStr; // Processing string pointer
	int res = NPI_LNX_FAILURE;


	resString = (char *)malloc (128);
	if (resString == NULL)
	{
		npi_ipc_errno = NPI_LNX_ERROR_IPC_GENERIC;
		return NPI_LNX_FAILURE;
	}
	resStringToFree = resString;
	LOG_DEBUG("------------------------------------------------------\n");
	LOG_DEBUG("Serial Config Parsing:\n");
	LOG_DEBUG("- \tSection: \t%s\n", section);
	LOG_DEBUG("- \tKey: \t\t%s\n", key);

	// Do nothing if the file doesn't exist
	if (serialCfgFd != NULL)
	{
		// Make sure we start search from the beginning of the file
		fseek(serialCfgFd, 0, SEEK_SET);

		// Search through the configuration file for the wanted
		while ((resString = fgets(resString, 128, serialCfgFd)) != NULL)
		{
			// Check if we have a valid line, i.e. begins with [.
			// Note! No valid line can span more than 128 bytes. Hence we
			// must hold off parsing until we hit a newline.
			if (strlen(resString) == 128)
			{
				invalidLineLen = TRUE;
				LOG_DEBUG("Found line > 128 bytes! Too long!\n");
				fflush(LOG_DESTINATION_FP);
			}
			else
			{
				// First time we find a valid line length after having
				// found invalid line length may be the end of the
				// invalid line. Hence, do not process this string.
				// We set the invalidLineLen parameter to FALSE after
				// the processing logic.
				if (invalidLineLen == FALSE)
				{
					// Remove the newline character (ok even if line had length 128)
					resString[strlen(resString) - 1] = '\0';

					LOG_TRACE("Found line < 128 bytes...");
					fflush(LOG_DESTINATION_FP);
					if (resString[0] == '[')
					{
						LOG_DEBUG("Found section %s\n", resString);
						// Search for wanted section
						psStr = strstr(resString, section);
						if (psStr != NULL)
						{
							resString = psStr;
							// We found our wanted section. Now search for wanted key.
							sectionFound = TRUE;
							LOG_DEBUG("Found wanted section!\n");
						}
						else
						{
							// We found another section.
							sectionFound = FALSE;
						}
					}
					else if (sectionFound == TRUE)
					{
						LOG_DEBUG("Line to process %s (strlen=%zd)\n",
								resString,
								strlen(resString));
						// We have found our section, now we search for wanted key
						// Check for commented lines, tagged with '#', and
						// lines > 0 in length
						if ((resString[0] != '#') && (strlen(resString) > 0))
						{
							// Search for wanted section
							psStr = strstr(resString, key);
							if (psStr != NULL)
							{
								LOG_DEBUG("Found key \t'%s' in \t'%s'\n", key, resString);
								// We found our key. The value is located after the '='
								// after the key.
								// LOG_DEBUG("%s\n", psStr);
								psStr = strtok(psStr, "=");

								// strtok doesn't work if the value is specified as "" (empty string) because it will
								// skip both quotes.  If we're at a quote, check for "" and handle that without strtok.
								if (strncmp(psStr+strlen(psStr)+1, "\"\"", 2) == 0)
									psStr = psStr+strlen(psStr); // No need to parse, just point to null so string is empty.
								else
									psStr = strtok(NULL, "=;\"");
								LOG_DEBUG("%s\n", psStr);

								resString = psStr;
								res = NPI_LNX_SUCCESS;
								LOG_DEBUG("Found value '%s'\n", resString);
								strcpy(resultString, resString);
								LOG_DEBUG("Found value2 '%s'\n", resultString);
								// We can return this string to the calling function
								break;
							}
						}
					}
               else
               {
                  // LOG_DEBUG("Irrelevant line (%s)\n", resString);
               }
				}
				else
				{
					LOG_DEBUG("Found end of line > 128 bytes\n");
					invalidLineLen = FALSE;
				}
			}
		}
	}
	else
	{
		npi_ipc_errno = NPI_LNX_ERROR_IPC_SERIAL_CFG_FILE_DOES_NOT_EXIST;
		free(resStringToFree);
		return NPI_LNX_FAILURE;
	}

	free(resStringToFree);
	return res;
}
