/*
 * current.c
 *
 *  Created on: Dec 21, 2016
 *      Author: Zulolo
 */
#include "esp_common.h"
#include "driver/gpio.h"
#include "driver/spi_register.h"
#include "driver/spi_interface.h"
#include "smart_socket_global.h"
#include "cs5463.h"
#include "user_data.h"

#define CS5463_RST_IO_PIN		GPIO_Pin_5
#define CS5463_RST_PIN_NUM		5
#define CS5463_MISO_IO_PIN		GPIO_Pin_12
#define CS5463_MISO_PIN_NUM		12
#define CS5463_MOSI_IO_PIN		GPIO_Pin_13
#define CS5463_MOSI_PIN_NUM		13
#define CS5463_CLK_IO_PIN		GPIO_Pin_14
#define CS5463_CLK_PIN_NUM		14
// GPIO16 is the CS, but the GPIO 16 related  API is seperated
#define CS5463_SELECT()			gpio16_output_set(0)
#define CS5463_DESELECT()		gpio16_output_set(1)

#define CS5463_RW_REG_CMD_BITS	0xC0
#define CS5463_RD_REG_CMD		0x00
#define CS5463_WR_REG_CMD		0x40

#define CS5463_STATUS_BIT_DATA_READY	23
#define CS5463_CALIB_WAIT_PRESS_KEY		120000
#define CS5463_DELAY_AFTER_CALIB_CMD	5000

typedef enum{
	CS5463_CALI_STATE_STOP = 0,	// Stop
	CS5463_CALI_STATE_WAITING_START,
	CS5463_CALI_STATE_DC_V_OFFSET,
	CS5463_CALI_STATE_AC_V_OFFSET,
	CS5463_CALI_STATE_AC_V_GAIN,
	CS5463_CALI_STATE_DC_I_OFFSET,
	CS5463_CALI_STATE_AC_I_OFFSET,
	CS5463_CALI_STATE_AC_I_GAIN,
	CS5463_CALI_STATE_RESET
}CS5463CaliState_t;

static uint32_t unCS5463_Status;
//static uint32_t unCS5463_IRMS;
static int8_t fCS5463_T;
static uint8_t unCS5463_I_RMS[3];
static uint8_t unCS5463_V_RMS[3];
static uint8_t unCS5463_P_Active[3];

LOCAL os_timer_t tCheckCurrentTimer;
LOCAL os_timer_t tTrendRecord;
extern SmartSocketParameter_t tSmartSocketParameter;
extern xSemaphoreHandle xSmartSocketCaliSemaphore;
extern xSemaphoreHandle xSmartSocketParameterSemaphore;

void check_current_timer_cb(void)
{
    if(CS5463_fGetI_RMS() > DAT_fGetCurrentThrhld()){
    	user_plug_set_status(PLUG_STATUS_OPEN);
    }
}

/******************************************************************************
 * FunctionName : fConvert24BitToFloat
 * Description  : convert 24bits data (3 bytes) to unsigned 0~1 floating number
 * Parameters   : pointer of 3 bytes data
 * Returns      : float result
*******************************************************************************/
float fConvert24BitToFloat(uint8_t* pData)
{
	float fResult = 0;
	uint32_t unTemp = 1;
	uint8_t i = 0, j = 0;

	for(i = 0; i < 24; i++){
		if((pData[(uint8_t)(i/8)] << (i%8)) & 0x80){
			unTemp = 1;
			for(j = 0; j < i+1; j++){
				unTemp *= 2;
			}
			fResult +=  (((float)1)/((float)unTemp));
		}
	}
	return fResult;
}

/******************************************************************************
 * FunctionName : fConvert24BitToCmpltFloat
 * Description  : convert 24bits data (3 bytes) to signed -1~1 complement floating number,
 * 					High bit is the sign bit
 * Parameters   : pointer of 3 bytes data
 * Returns      : float result
*******************************************************************************/
float fConvert24BitToCmpltFloat(uint8_t* pData)
{
	float fResult = 0;
	uint32_t unTemp = 1;
	uint8_t i = 0, j = 0;

	for(i = 1; i < 24; i++){
		if((pData[(uint8_t)(i/8)] << (i%8)) & 0x80){
			unTemp = 1;
			for(j = 0;j<i;j++){
				unTemp *= 2;
			}
			fResult +=  (((float)1)/(float)unTemp);
		}
	}

	if(pData[0] & 0x80){
		fResult *= -1;
	}
	return fResult ;
}

void writeCS5463SPI(uint8_t unCmd)
{
	int8_t nBitIndex;

	for (nBitIndex = 7; nBitIndex >= 0; nBitIndex--){
		GPIO_OUTPUT_SET(CS5463_CLK_PIN_NUM, 0);
		GPIO_OUTPUT_SET(CS5463_MOSI_PIN_NUM, ((unCmd >> nBitIndex) & 0x01));
		GPIO_OUTPUT_SET(CS5463_CLK_PIN_NUM, 1);
	}
}

void readCS5463SPI(uint8_t* pDataBuff, uint8_t unDataLen)
{
	int8_t nBitIndex;
	int8_t nByteIndex;
	for (nByteIndex = 0; nByteIndex < unDataLen; nByteIndex++){
		*(pDataBuff + nByteIndex) = 0;
		for (nBitIndex = 7; nBitIndex >= 0; nBitIndex--){
			GPIO_OUTPUT_SET(CS5463_CLK_PIN_NUM, 0);
			GPIO_OUTPUT_SET(CS5463_CLK_PIN_NUM, 1);
			*(pDataBuff + nByteIndex) |= (GPIO_INPUT_GET(CS5463_MISO_PIN_NUM) << nBitIndex);
		}
	}
}

int32_t CS5463IF_WriteCmd(uint8_t unWriteCmd)
{
	CS5463_SELECT();
	writeCS5463SPI(unWriteCmd);
//	writeCS5463SPI(CS5463_CMD_SYNC_1);
//	writeCS5463SPI(CS5463_CMD_SYNC_1);
//	writeCS5463SPI(CS5463_CMD_SYNC_1);
	CS5463_DESELECT();
	return 0;
}

int32_t CS5463IF_WriteReg(uint8_t unWriteCmd, uint8_t* pWriteDataBuff, uint8_t unWriteDataLen)
{
	int8_t nRegIndex;
	if ((unWriteDataLen != 3) && (unWriteDataLen != 4)){
		printf("CS5463 read data length error.\n");
		return (-1);
	}

	if ((unWriteCmd & CS5463_RW_REG_CMD_BITS) != CS5463_WR_REG_CMD){
		printf("It is not write register type command.\n");
		return (-1);
	}

	CS5463_SELECT();
	writeCS5463SPI(unWriteCmd);
	for (nRegIndex = 0; nRegIndex < unWriteDataLen; nRegIndex++){
		writeCS5463SPI(pWriteDataBuff[nRegIndex]);
	}
	CS5463_DESELECT();

	return 0;
}

int32_t CS5463IF_Read(uint8_t unReadCmd, uint8_t* pReadDataBuff, uint8_t unReadDataLen)
{
	int8_t nBitIndex;
	int8_t nByteIndex;

	if ((unReadDataLen != 3) && (unReadDataLen != 4)){
		printf("CS5463 read data length error.\n");
		return (-1);
	}

	if ((unReadCmd & CS5463_RW_REG_CMD_BITS) != CS5463_RD_REG_CMD){
		printf("It is not read register type command.\n");
		return (-1);
	}

	CS5463_SELECT();
	writeCS5463SPI(unReadCmd);
	GPIO_OUTPUT_SET(CS5463_MOSI_PIN_NUM, 1);
	readCS5463SPI(pReadDataBuff, unReadDataLen);
	CS5463_DESELECT();

	return 0;
}

void CS5463IF_Init()
{
	GPIO_ConfigTypeDef tGPIO_Conf;

	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14);

	tGPIO_Conf.GPIO_IntrType = GPIO_PIN_INTR_DISABLE;
	tGPIO_Conf.GPIO_Mode = GPIO_Mode_Output;
	tGPIO_Conf.GPIO_Pin = CS5463_RST_IO_PIN ;
	tGPIO_Conf.GPIO_Pullup = GPIO_PullUp_DIS;
	gpio_config(&tGPIO_Conf);

	tGPIO_Conf.GPIO_IntrType = GPIO_PIN_INTR_DISABLE;
	tGPIO_Conf.GPIO_Mode = GPIO_Mode_Output;
	tGPIO_Conf.GPIO_Pin = CS5463_MOSI_IO_PIN ;
	tGPIO_Conf.GPIO_Pullup = GPIO_PullUp_DIS;
	gpio_config(&tGPIO_Conf);

	tGPIO_Conf.GPIO_IntrType = GPIO_PIN_INTR_DISABLE;
	tGPIO_Conf.GPIO_Mode = GPIO_Mode_Output;
	tGPIO_Conf.GPIO_Pin = CS5463_CLK_IO_PIN ;
	tGPIO_Conf.GPIO_Pullup = GPIO_PullUp_DIS;
	gpio_config(&tGPIO_Conf);

	tGPIO_Conf.GPIO_IntrType = GPIO_PIN_INTR_DISABLE;
	tGPIO_Conf.GPIO_Mode = GPIO_Mode_Input;
	tGPIO_Conf.GPIO_Pin = CS5463_MISO_IO_PIN ;
	tGPIO_Conf.GPIO_Pullup = GPIO_PullUp_DIS;
	gpio_config(&tGPIO_Conf);

	gpio16_output_conf();
	CS5463_DESELECT();
}

void trend_record_callback(void *arg)
{
	TrendContent_t tValue;
	tValue.fTemperature = fCS5463_T;
	tValue.unI_RMS = (unCS5463_I_RMS[0] << 16) | (unCS5463_I_RMS[1] << 8) | unCS5463_I_RMS[2];
	tValue.unV_RMS = (unCS5463_V_RMS[0] << 16) | (unCS5463_V_RMS[1] << 8) | unCS5463_V_RMS[2];
//	tValue.unActivePower = unCS5463_P;
	tValue.unTime = sntp_get_current_timestamp();
    DAT_bTrendRecordAdd(tValue);
}

void initCS5463(void)
{
	uint8_t unPara[3];

	unPara[0] = 0x80;
	unPara[1] = 0;
	unPara[2] = 0;
	CS5463IF_WriteReg(CS5463_CMD_WR_STATUS, unPara, sizeof(unPara));

	unPara[0] = 0;
	unPara[1] = 0;
	unPara[2] = 0x01;
	CS5463IF_WriteReg(CS5463_CMD_WR_CONFIG, unPara, sizeof(unPara));

	unPara[0] = 0;
	unPara[1] = 0x03;
	unPara[2] = 0x20;
	CS5463IF_WriteReg(CS5463_CMD_WR_CYCLE_COUNT, unPara, sizeof(unPara));

	unPara[0] = 0;
	unPara[1] = 0;
	unPara[2] = 0;
	CS5463IF_WriteReg(CS5463_CMD_WR_MASK, unPara, sizeof(unPara));

	unPara[0] = 0;
	unPara[1] = 0;
	unPara[2] = 0x01;
	CS5463IF_WriteReg(CS5463_CMD_WR_MODE, unPara, sizeof(unPara));
	CS5463IF_Read(CS5463_CMD_RD_MODE, unPara, sizeof(unPara));

	unPara[0] = 0;
	unPara[1] = 0;
	unPara[2] = 0;
	CS5463IF_WriteReg(CS5463_CMD_WR_CTRL, unPara, sizeof(unPara));

	unPara[0] = 0;
	unPara[1] = 0x34;
	unPara[2] = 0x9C;
	CS5463IF_WriteReg(CS5463_CMD_WR_PULSE_RATE, unPara, sizeof(unPara));

	CS5463IF_Read(CS5463_CMD_RD_STATUS, unPara, sizeof(unPara));
	CS5463IF_WriteReg(CS5463_CMD_WR_STATUS, unPara, sizeof(unPara));
}

void writeCS5463Calib(void)
{
	uint8_t unPara[3];
	uint32_t unTemp;

	unTemp = DAT_unGetDC_V_Offset();
	unPara[0] = (unTemp >> 16) & 0xFF;
	unPara[1] = (unTemp >> 8) & 0xFF;
	unPara[2] = unTemp & 0xFF;
	CS5463IF_WriteReg(CS5463_CMD_WR_DC_V_OFFSET, unPara, sizeof(unPara));
//	CS5463IF_Read(CS5463_CMD_RD_DC_V_OFFSET, unPara, sizeof(unPara));
//	printf("Calib data write is: 0x%08X.\n", unTemp);
//	printf("Calib data check is: 0x%02X%02X%02X.\n", unPara[0], unPara[1], unPara[2]);


	unTemp = DAT_unGetV_Gain();
	unPara[0] = (unTemp >> 16) & 0xFF;
	unPara[1] = (unTemp >> 8) & 0xFF;
	unPara[2] = unTemp & 0xFF;
	CS5463IF_WriteReg(CS5463_CMD_WR_V_GAIN, unPara, sizeof(unPara));
//	CS5463IF_Read(CS5463_CMD_RD_V_GAIN, unPara, sizeof(unPara));
//	printf("Calib data write is: 0x%08X.\n", unTemp);
//	printf("Calib data check is: 0x%02X%02X%02X.\n", unPara[0], unPara[1], unPara[2]);

	unTemp = DAT_unGetAC_V_Offset();
	unPara[0] = (unTemp >> 16) & 0xFF;
	unPara[1] = (unTemp >> 8) & 0xFF;
	unPara[2] = unTemp & 0xFF;
	CS5463IF_WriteReg(CS5463_CMD_WR_AC_V_OFFSET, unPara, sizeof(unPara));
//	CS5463IF_Read(CS5463_CMD_RD_AC_V_OFFSET, unPara, sizeof(unPara));
//	printf("Calib data write is: 0x%08X.\n", unTemp);
//	printf("Calib data check is: 0x%02X%02X%02X.\n", unPara[0], unPara[1], unPara[2]);

	unTemp = DAT_unGetDC_I_Offset();
	unPara[0] = (unTemp >> 16) & 0xFF;
	unPara[1] = (unTemp >> 8) & 0xFF;
	unPara[2] = unTemp & 0xFF;
	CS5463IF_WriteReg(CS5463_CMD_WR_DC_I_OFFSET, unPara, sizeof(unPara));
//	CS5463IF_Read(CS5463_CMD_RD_DC_I_OFFSET, unPara, sizeof(unPara));
//	printf("Calib data write is: 0x%08X.\n", unTemp);
//	printf("Calib data check is: 0x%02X%02X%02X.\n", unPara[0], unPara[1], unPara[2]);

	unTemp = DAT_unGetI_Gain();
	unPara[0] = (unTemp >> 16) & 0xFF;
	unPara[1] = (unTemp >> 8) & 0xFF;
	unPara[2] = unTemp & 0xFF;
	CS5463IF_WriteReg(CS5463_CMD_WR_I_GAIN, unPara, sizeof(unPara));
//	CS5463IF_Read(CS5463_CMD_RD_I_GAIN, unPara, sizeof(unPara));
//	printf("Calib data write is: 0x%08X.\n", unTemp);
//	printf("Calib data check is: 0x%02X%02X%02X.\n", unPara[0], unPara[1], unPara[2]);

	unTemp = DAT_unGetAC_I_Offset();
	unPara[0] = (unTemp >> 16) & 0xFF;
	unPara[1] = (unTemp >> 8) & 0xFF;
	unPara[2] = unTemp & 0xFF;
	CS5463IF_WriteReg(CS5463_CMD_WR_AC_I_OFFSET, unPara, sizeof(unPara));
//	CS5463IF_Read(CS5463_CMD_RD_AC_I_OFFSET, unPara, sizeof(unPara));
//	printf("Calib data write is: 0x%08X.\n", unTemp);
//	printf("Calib data check is: 0x%02X%02X%02X.\n", unPara[0], unPara[1], unPara[2]);
}

bool getCS5463RegBit(uint8_t* pPara, uint8_t unBitIndex)
{
	if (((pPara[2 - unBitIndex/8] >> (unBitIndex%8)) & 0x01) == 0x01){
		return true;
	}else {
		return false;
	}
}

bool waitDataReady(uint32_t unWaitTime)
{
	uint8_t unPara[3];
	uint32_t unWaitCNT = unWaitTime/100;

	while(unWaitCNT--){
		CS5463IF_Read(CS5463_CMD_RD_STATUS, unPara, sizeof(unPara));
		if (getCS5463RegBit(unPara, CS5463_STATUS_BIT_DATA_READY) == true){
			printf("%u ms was spent to get data ready.\n", unWaitTime - unWaitCNT * 100);
			return true;
		}
		vTaskDelay(100/portTICK_RATE_MS);
	}
	return false;
}

void clearDRDY(void)
{
	uint8_t unPara[3];
	unPara[0] = 0xFF;
	unPara[1] = 0xFF;
	unPara[2] = 0xFF;
	CS5463IF_WriteReg(CS5463_CMD_WR_STATUS, unPara, sizeof(unPara));
}

int32_t CS5463IF_Calib(void)
{
	CS5463CaliState_t tCaliState;
	uint8_t unPara[3];
	tCaliState = CS5463_CALI_STATE_WAITING_START;
	DAT_unSetCalib(true);
	CS5463IF_TrendEnable(false);
	CS5463IF_ProtectEnable(false);
//	memset(&(tSmartSocketParameter.tCS5463Valid), 0, sizeof(tSmartSocketParameter.tCS5463Valid));
	user_plug_set_status(PLUG_STATUS_OPEN);
	user_relay_led_output(LED_1HZ);

	GPIO_OUTPUT_SET(CS5463_RST_PIN_NUM, 0);
	vTaskDelay(200/portTICK_RATE_MS);
	GPIO_OUTPUT_SET(CS5463_RST_PIN_NUM, 1);
	vTaskDelay(200/portTICK_RATE_MS);

	writeCS5463SPI(CS5463_CMD_SYNC_1);
	writeCS5463SPI(CS5463_CMD_SYNC_1);
	writeCS5463SPI(CS5463_CMD_SYNC_1);
	writeCS5463SPI(CS5463_CMD_SYNC_0);
	initCS5463();
	CS5463IF_WriteCmd(CS5463_CMD_START_CNTN_CNVS);

	xSemaphoreTake(xSmartSocketCaliSemaphore, (portTickType)(1000/portTICK_RATE_MS));
	while ((true == DAT_unGetCalib()) &&
			(tCaliState != CS5463_CALI_STATE_STOP)){
		switch (tCaliState){
		case CS5463_CALI_STATE_WAITING_START:
			printf("Ready to calibrate DC current offset, press button when ready.\n");
			if(xSemaphoreTake(xSmartSocketCaliSemaphore, (portTickType)(CS5463_CALIB_WAIT_PRESS_KEY/portTICK_RATE_MS)) == pdTRUE ){
				tCaliState = CS5463_CALI_STATE_DC_I_OFFSET;
			}else{
				printf("CS5463 DC current offset calibration timeout, exit...\n");
				tCaliState = CS5463_CALI_STATE_STOP;
		    }
			break;

		case CS5463_CALI_STATE_DC_I_OFFSET:
			user_relay_led_output(LED_5HZ);

			clearDRDY();
			vTaskDelay(10/portTICK_RATE_MS);
			CS5463IF_WriteCmd(CS5463_CMD_POWER_UP_HALT);
			vTaskDelay(20/portTICK_RATE_MS);

			unPara[0] = 0x40;
			unPara[1] = 0;
			unPara[2] = 0;
			CS5463IF_WriteReg(CS5463_CMD_WR_DC_I_OFFSET, unPara, sizeof(unPara));
			CS5463IF_WriteCmd(CS5463_CMD_START_DC_I_OFFSET_CALIB);
			vTaskDelay(20/portTICK_RATE_MS);
			if (waitDataReady(CS5463_DELAY_AFTER_CALIB_CMD) == true){
				CS5463IF_Read(CS5463_CMD_RD_DC_I_OFFSET, unPara, sizeof(unPara));
				DAT_unSetDC_I_Offset((unPara[0] << 16) | (unPara[1] << 8) | unPara[2]);
				user_relay_led_output(LED_1HZ);

				printf("DC current offset is 0x%06X.\nReady to calibrate AC current gain, press button when ready.\n",
						DAT_unGetDC_I_Offset());
				CS5463IF_WriteCmd(CS5463_CMD_START_CNTN_CNVS);
				if(xSemaphoreTake(xSmartSocketCaliSemaphore, (portTickType)(CS5463_CALIB_WAIT_PRESS_KEY/portTICK_RATE_MS)) == pdTRUE ){
					tCaliState = CS5463_CALI_STATE_AC_I_GAIN;
				}else{
					printf("CS5463 calibration AC current gain timeout, exit...\n");
					tCaliState = CS5463_CALI_STATE_STOP;
			    }
			}else{
				printf("CS5463 status register read: 0x%02X, 0x%02X, 0x%02X\n", unPara[0], unPara[1], unPara[2]);
				printf("Calibration failed, data ready timeout.\n");
				tCaliState = CS5463_CALI_STATE_STOP;
			}
			break;

		case CS5463_CALI_STATE_AC_I_GAIN:
			user_relay_led_output(LED_5HZ);

			clearDRDY();
			vTaskDelay(10/portTICK_RATE_MS);
			CS5463IF_WriteCmd(CS5463_CMD_POWER_UP_HALT);
			vTaskDelay(20/portTICK_RATE_MS);

			user_plug_set_status(PLUG_STATUS_CLOSE);
			vTaskDelay(2000/portTICK_RATE_MS);
			unPara[0] = 0x40;
			unPara[1] = 0;
			unPara[2] = 0;
			CS5463IF_WriteReg(CS5463_CMD_WR_I_GAIN, unPara, sizeof(unPara));
			CS5463IF_WriteCmd(CS5463_CMD_START_AC_I_GAIN_CALIB);
			vTaskDelay(20/portTICK_RATE_MS);
			if (waitDataReady(CS5463_DELAY_AFTER_CALIB_CMD) == true){
				user_plug_set_status(PLUG_STATUS_OPEN);
				CS5463IF_Read(CS5463_CMD_RD_I_GAIN, unPara, sizeof(unPara));
				DAT_unSetI_Gain((unPara[0] << 16) | (unPara[1] << 8) | unPara[2]);
				user_relay_led_output(LED_1HZ);

				printf("AC current gain is 0x%06X.\nReady to calibrate AC current offset, press button when ready.\n", DAT_unGetI_Gain());
				CS5463IF_WriteCmd(CS5463_CMD_START_CNTN_CNVS);
				if(xSemaphoreTake(xSmartSocketCaliSemaphore, (portTickType)(CS5463_CALIB_WAIT_PRESS_KEY/portTICK_RATE_MS)) == pdTRUE ){
					tCaliState = CS5463_CALI_STATE_AC_I_OFFSET;
				}else{
					printf("CS5463 calibration AC current offset timeout, exit...\n");
					tCaliState = CS5463_CALI_STATE_STOP;
			    }
			}else{
				user_plug_set_status(PLUG_STATUS_OPEN);
				printf("CS5463 status register read: 0x%02X, 0x%02X, 0x%02X\n", unPara[0], unPara[1], unPara[2]);
				printf("Calibration failed, data ready timeout.\n");
				tCaliState = CS5463_CALI_STATE_STOP;
			}
			break;

		case CS5463_CALI_STATE_AC_I_OFFSET:
			user_relay_led_output(LED_5HZ);

			clearDRDY();
			vTaskDelay(10/portTICK_RATE_MS);
			CS5463IF_WriteCmd(CS5463_CMD_POWER_UP_HALT);
			vTaskDelay(20/portTICK_RATE_MS);

			unPara[0] = 0;
			unPara[1] = 0;
			unPara[2] = 0;
			CS5463IF_WriteReg(CS5463_CMD_WR_AC_I_OFFSET, unPara, sizeof(unPara));
			CS5463IF_WriteCmd(CS5463_CMD_START_AC_I_OFFSET_CALIB);
			vTaskDelay(20/portTICK_RATE_MS);
			if (waitDataReady(CS5463_DELAY_AFTER_CALIB_CMD) == true){
				CS5463IF_Read(CS5463_CMD_RD_AC_I_OFFSET, unPara, sizeof(unPara));
				DAT_unSetAC_I_Offset((unPara[0] << 16) | (unPara[1] << 8) | unPara[2]);
				user_relay_led_output(LED_1HZ);

				printf("AC current offset is 0x%06X.\nReady to calibrate DC voltage offset, press button when ready.\n",
						DAT_unGetAC_I_Offset());
				CS5463IF_WriteCmd(CS5463_CMD_START_CNTN_CNVS);
				if(xSemaphoreTake(xSmartSocketCaliSemaphore, (portTickType)(CS5463_CALIB_WAIT_PRESS_KEY/portTICK_RATE_MS)) == pdTRUE ){
					tCaliState = CS5463_CALI_STATE_DC_V_OFFSET;
				}else{
					printf("CS5463 calibration DC voltage offset timeout, exit...\n");
					tCaliState = CS5463_CALI_STATE_STOP;
			    }
			}else{
				printf("CS5463 status register read: 0x%02X, 0x%02X, 0x%02X\n", unPara[0], unPara[1], unPara[2]);
				printf("Calibration failed, data ready timeout.\n");
				tCaliState = CS5463_CALI_STATE_STOP;
			}
			break;

		case CS5463_CALI_STATE_DC_V_OFFSET:
			user_relay_led_output(LED_5HZ);

			clearDRDY();
			vTaskDelay(10/portTICK_RATE_MS);
			CS5463IF_WriteCmd(CS5463_CMD_POWER_UP_HALT);
			vTaskDelay(20/portTICK_RATE_MS);

			unPara[0] = 0;
			unPara[1] = 0;
			unPara[2] = 0;
			CS5463IF_WriteReg(CS5463_CMD_WR_DC_V_OFFSET, unPara, sizeof(unPara));
			CS5463IF_WriteCmd(CS5463_CMD_START_DC_V_OFFSET_CALIB);
			vTaskDelay(20/portTICK_RATE_MS);
			if (waitDataReady(CS5463_DELAY_AFTER_CALIB_CMD) == true){
				CS5463IF_Read(CS5463_CMD_RD_DC_V_OFFSET, unPara, sizeof(unPara));
				DAT_unSetDC_V_Offset((unPara[0] << 16) | (unPara[1] << 8) | unPara[2]);
				user_relay_led_output(LED_1HZ);

				printf("DC voltage offset is 0x%06X.\nReady to calibrate AC voltage gain, press button when ready.\n",
						DAT_unGetDC_V_Offset());
				CS5463IF_WriteCmd(CS5463_CMD_START_CNTN_CNVS);
				if(xSemaphoreTake(xSmartSocketCaliSemaphore, (portTickType)(CS5463_CALIB_WAIT_PRESS_KEY/portTICK_RATE_MS)) == pdTRUE ){
					tCaliState = CS5463_CALI_STATE_AC_V_GAIN;
				}else{
					printf("CS5463 calibration AC voltage gain timeout, exit...\n");
					tCaliState = CS5463_CALI_STATE_STOP;
			    }
			}else{
				printf("CS5463 status register read: 0x%02X, 0x%02X, 0x%02X\n", unPara[0], unPara[1], unPara[2]);
				printf("Calibration failed, data ready timeout.\n");
				tCaliState = CS5463_CALI_STATE_STOP;
			}

			break;

		case CS5463_CALI_STATE_AC_V_GAIN:
			user_relay_led_output(LED_5HZ);

			clearDRDY();
			vTaskDelay(10/portTICK_RATE_MS);
			CS5463IF_WriteCmd(CS5463_CMD_POWER_UP_HALT);
			vTaskDelay(20/portTICK_RATE_MS);

			user_plug_set_status(PLUG_STATUS_CLOSE);
			vTaskDelay(2000/portTICK_RATE_MS);
			unPara[0] = 0x40;
			unPara[1] = 0;
			unPara[2] = 0;
			CS5463IF_WriteReg(CS5463_CMD_WR_V_GAIN, unPara, sizeof(unPara));
			CS5463IF_WriteCmd(CS5463_CMD_START_AC_V_GAIN_CALIB);
			vTaskDelay(20/portTICK_RATE_MS);
			if (waitDataReady(CS5463_DELAY_AFTER_CALIB_CMD) == true){
				user_plug_set_status(PLUG_STATUS_OPEN);
				CS5463IF_Read(CS5463_CMD_RD_V_GAIN, unPara, sizeof(unPara));
				DAT_unSetV_Gain((unPara[0] << 16) | (unPara[1] << 8) | unPara[2]);
				user_relay_led_output(LED_1HZ);

				printf("AC voltage gain is 0x%06X.\nReady to calibrate AC voltage offset, press button when ready.\n",
						DAT_unGetV_Gain());
				CS5463IF_WriteCmd(CS5463_CMD_START_CNTN_CNVS);
				if(xSemaphoreTake(xSmartSocketCaliSemaphore, (portTickType)(CS5463_CALIB_WAIT_PRESS_KEY/portTICK_RATE_MS)) == pdTRUE ){
					tCaliState = CS5463_CALI_STATE_AC_V_OFFSET;
				}else{
					printf("CS5463 calibration AC voltage offset timeout, exit...\n");
					tCaliState = CS5463_CALI_STATE_STOP;
			    }
			}else{
				user_plug_set_status(PLUG_STATUS_OPEN);
				printf("CS5463 status register read: 0x%02X, 0x%02X, 0x%02X\n", unPara[0], unPara[1], unPara[2]);
				printf("Calibration failed, data ready timeout.\n");
				tCaliState = CS5463_CALI_STATE_STOP;
			}
			break;

		case CS5463_CALI_STATE_AC_V_OFFSET:
			user_relay_led_output(LED_5HZ);

			clearDRDY();
			vTaskDelay(10/portTICK_RATE_MS);
			CS5463IF_WriteCmd(CS5463_CMD_POWER_UP_HALT);
			vTaskDelay(20/portTICK_RATE_MS);

			unPara[0] = 0;
			unPara[1] = 0;
			unPara[2] = 0;
			CS5463IF_WriteReg(CS5463_CMD_WR_AC_V_OFFSET, unPara, sizeof(unPara));
			CS5463IF_WriteCmd(CS5463_CMD_START_AC_V_OFFSET_CALIB);
			vTaskDelay(20/portTICK_RATE_MS);
			if (waitDataReady(CS5463_DELAY_AFTER_CALIB_CMD) == true){
				CS5463IF_Read(CS5463_CMD_RD_AC_V_OFFSET, unPara, sizeof(unPara));
				DAT_unSetAC_V_Offset((unPara[0] << 16) | (unPara[1] << 8) | unPara[2]);
				user_relay_led_output(LED_1HZ);

				printf("AC voltage offset is 0x%06X.\n",
						DAT_unGetAC_V_Offset());
				CS5463IF_WriteCmd(CS5463_CMD_START_CNTN_CNVS);
				tCaliState = CS5463_CALI_STATE_RESET;
			}else{
				printf("CS5463 status register read: 0x%02X, 0x%02X, 0x%02X\n", unPara[0], unPara[1], unPara[2]);
				printf("Calibration failed, data ready timeout.\n");
				tCaliState = CS5463_CALI_STATE_STOP;
			}
			break;

		case CS5463_CALI_STATE_RESET:
			tSmartSocketParameter.tCS5463Calib.unValidation = 0xA5A5A5A5;
			if(xSemaphoreTake(xSmartSocketParameterSemaphore, (portTickType)(10000/portTICK_RATE_MS)) == pdTRUE){
				system_param_save_with_protect(GET_USER_DATA_SECTORE(USER_DATA_CONF_PARA),
									&tSmartSocketParameter, sizeof(tSmartSocketParameter));
				xSemaphoreGive(xSmartSocketParameterSemaphore);
				printf("Save calibration data finished.\n");
			}else{
				printf("Take parameter semaphore failed.\n");
			}
			tCaliState = CS5463_CALI_STATE_STOP;
			break;

		default:
			printf("CS5463 calibration state error.\n");
			tCaliState = CS5463_CALI_STATE_STOP;
			break;
		}
	}
	DAT_unSetCalib(false);
	user_plug_set_status(PLUG_STATUS_OPEN);
	user_relay_led_output(LED_OFF);
	return 0;
}

void CS5463IF_TrendEnable(bool bEnable)
{
	if (true == bEnable){
	    os_timer_disarm(&tTrendRecord);
	    os_timer_setfn(&tTrendRecord, trend_record_callback , NULL);
	    os_timer_arm(&tTrendRecord, TREND_RECORD_INTERVAL, 1);
	}else{
		os_timer_disarm(&tTrendRecord);
	}
}

void CS5463IF_ProtectEnable(bool bEnable)
{
	if (true == tSmartSocketParameter.tConfigure.bCurrentPrtctEnable){
		os_timer_disarm(&tCheckCurrentTimer);
		os_timer_setfn(&tCheckCurrentTimer, (os_timer_func_t *)check_current_timer_cb, NULL);
		os_timer_arm(&tCheckCurrentTimer, 1000, 1);
	}else{
		os_timer_disarm(&tCheckCurrentTimer);
	}
}

void CS5463IF_Routine(void)
{
	uint8_t unCS5463Data[3];	//[] = {CS5463_CMD_SYNC_1, CS5463_CMD_SYNC_1, CS5463_CMD_SYNC_1};

	GPIO_OUTPUT_SET(CS5463_RST_PIN_NUM, 0);
	vTaskDelay(200/portTICK_RATE_MS);
	GPIO_OUTPUT_SET(CS5463_RST_PIN_NUM, 1);
	vTaskDelay(200/portTICK_RATE_MS);

	writeCS5463SPI(CS5463_CMD_SYNC_1);
	writeCS5463SPI(CS5463_CMD_SYNC_1);
	writeCS5463SPI(CS5463_CMD_SYNC_1);
	writeCS5463SPI(CS5463_CMD_SYNC_0);
	initCS5463();
	writeCS5463Calib();
	CS5463IF_WriteCmd(CS5463_CMD_START_CNTN_CNVS);

	CS5463IF_TrendEnable(tSmartSocketParameter.tConfigure.bTrendEnable);
	CS5463IF_ProtectEnable(tSmartSocketParameter.tConfigure.bCurrentPrtctEnable);

	while(1){
		// Temperature
		CS5463IF_Read(CS5463_CMD_RD_T, unCS5463Data, sizeof(unCS5463Data));
		fCS5463_T = (int8_t)(unCS5463Data[0]);
		vTaskDelay(200/portTICK_RATE_MS);

		// Status
		CS5463IF_Read(CS5463_CMD_RD_STATUS, unCS5463Data, sizeof(unCS5463Data));
		unCS5463_Status = (unCS5463Data[0] << 16) | (unCS5463Data[1] << 8) | unCS5463Data[2];
		vTaskDelay(200/portTICK_RATE_MS);

		// Current
		CS5463IF_Read(CS5463_CMD_RD_I_RMS, unCS5463_I_RMS, sizeof(unCS5463_I_RMS));
		vTaskDelay(200/portTICK_RATE_MS);

		// Voltage
		CS5463IF_Read(CS5463_CMD_RD_V_RMS, unCS5463_V_RMS, sizeof(unCS5463_V_RMS));
		vTaskDelay(200/portTICK_RATE_MS);

		// Power
		CS5463IF_Read(CS5463_CMD_RD_P_ACTIVE, unCS5463_P_Active, sizeof(unCS5463_P_Active));
		vTaskDelay(200/portTICK_RATE_MS);
	}
	os_timer_disarm(&tTrendRecord);
	os_timer_disarm(&tCheckCurrentTimer);
}

void CS5463_Manager(void *pvParameters)
{
	CS5463IF_Init();

	while(1){
		if (tSmartSocketParameter.tCS5463Calib.unValidation != 0xA5A5A5A5){
			CS5463IF_Calib();
		}else{
			CS5463IF_Routine();
		}
	}
}

uint32_t CS5463_unGetStatus(void)
{
	return unCS5463_Status;
}

float CS5463_fGetI_RMS(void)
{
	float fResult;
//	printf("DAT_fGetIMax %d\n", (int32_t)(DAT_fGetIMax()));
	fResult = fConvert24BitToFloat(unCS5463_I_RMS);
	fResult = (fResult * DAT_fGetIMax()) / 0.6 ;
    return fResult;
}

float CS5463_fGetV_RMS(void)
{
	float fResult;
//	printf("unValidation 0x%08X\n", tSmartSocketParameter.unValidation);
//	printf("DAT_unGetDC_V_Offset %u\n", DAT_unGetDC_V_Offset());
//	printf("DAT_unGetDC_V_Offset %u\n", tSmartSocketParameter.tCS5463Calib.unDC_V_Offset);
	fResult = fConvert24BitToFloat(unCS5463_V_RMS);//(unCS5463_V_RMS);
	fResult = (fResult * DAT_fGetVMax()) / 0.6 ;

    return fResult;
}

float CS5463_fGetActivePower(void)
{
	float fResult = 0;
	fResult = fConvert24BitToCmpltFloat(unCS5463_P_Active);
    if(fResult <0)
    	fResult +=1;

    fResult *= DAT_fGetIMax();
    fResult /= 1000;
    fResult *= DAT_fGetVMax();
    fResult /= 0.6;
    fResult /= 0.6;

	return ((int32_t)fResult);
}

float CS5463_fGetTemperature(void)
{
	return fCS5463_T;
}
