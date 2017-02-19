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

static int8_t fCS5463_T;
static uint32_t unCS5463_IRMS;
static uint32_t unCS5463_Status;
static uint32_t unCS5463_VRMS;
static uint32_t unCS5463_P;

//FLASH_SECTOR_SIZE
extern SmartSocketParameter_t tSmartSocketParameter;
extern xSemaphoreHandle xSmartSocketCaliSemaphore;
extern xSemaphoreHandle xSmartSocketParameterSemaphore;

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

bool getCS5463RegBit(uint8_t* pPara, uint8_t unBitIndex)
{
	if (((pPara[unBitIndex/8] >> (unBitIndex%8)) & 0x01) == 0x01){
		return true;
	}else {
		return false;
	}
}

int32_t CS5463IF_Calib(void)
{
	CS5463CaliState_t tCaliState;
	uint8_t unPara[3];
	tCaliState = CS5463_CALI_STATE_WAITING_START;

//	memset(&(tSmartSocketParameter.tCS5463Valid), 0, sizeof(tSmartSocketParameter.tCS5463Valid));
	user_plug_set_status(PLUG_STATUS_CLOSE);
	user_relay_led_output(LED_1HZ);

	GPIO_OUTPUT_SET(CS5463_RST_PIN_NUM, 0);
	vTaskDelay(200/portTICK_RATE_MS);
	GPIO_OUTPUT_SET(CS5463_RST_PIN_NUM, 1);
	vTaskDelay(200/portTICK_RATE_MS);

	xSemaphoreTake(xSmartSocketCaliSemaphore, (portTickType)(1000/portTICK_RATE_MS));
	while ((true == tSmartSocketParameter.tConfigure.bCS5463Cali) &&
			(tCaliState != CS5463_CALI_STATE_STOP)){
		switch (tCaliState){
		case CS5463_CALI_STATE_WAITING_START:
			if(xSemaphoreTake(xSmartSocketCaliSemaphore, (portTickType)(60000/portTICK_RATE_MS)) == pdTRUE ){
				tCaliState = CS5463_CALI_STATE_DC_V_OFFSET;
			}else{
				printf("CS5463 DC voltage offset calibration timeout, exit...\n");
				tCaliState = CS5463_CALI_STATE_STOP;
		    }
			break;
		case CS5463_CALI_STATE_DC_V_OFFSET:
			user_relay_led_output(LED_5HZ);

			CS5463IF_WriteCmd(CS5463_CMD_SW_RESET);
			vTaskDelay(10/portTICK_RATE_MS);
			CS5463IF_WriteCmd(CS5463_CMD_POWER_UP_HALT);
			vTaskDelay(20/portTICK_RATE_MS);

			unPara[0] = 0;
			unPara[1] = 0;
			unPara[2] = 0;
			CS5463IF_WriteReg(CS5463_CMD_WR_DC_V_OFFSET, unPara, sizeof(unPara));
			CS5463IF_WriteCmd(CS5463_CMD_START_DC_V_OFFSET_CALIB);
			vTaskDelay(30/portTICK_RATE_MS);
			CS5463IF_Read(CS5463_CMD_RD_STATUS, unPara, sizeof(unPara));
			if (getCS5463RegBit(unPara, CS5463_STATUS_BIT_DATA_READY) == true){
				CS5463IF_Read(CS5463_CMD_RD_DC_V_OFFSET, unPara, sizeof(unPara));
				tSmartSocketParameter.tCS5463Calib.unDC_V_Offset = (unPara[0] << 16) | (unPara[1] << 8) | unPara[2];
				user_relay_led_output(LED_1HZ);
				if(xSemaphoreTake(xSmartSocketCaliSemaphore, (portTickType)(60000/portTICK_RATE_MS)) == pdTRUE ){
					tCaliState = CS5463_CALI_STATE_AC_V_OFFSET;
				}else{
					printf("CS5463 calibration AC voltage offset timeout, exit...\n");
					tCaliState = CS5463_CALI_STATE_STOP;
			    }
			}else{
				tCaliState = CS5463_CALI_STATE_STOP;
			}

			break;

		case CS5463_CALI_STATE_AC_V_OFFSET:
			user_relay_led_output(LED_5HZ);

			CS5463IF_WriteCmd(CS5463_CMD_SW_RESET);
			vTaskDelay(10/portTICK_RATE_MS);
			CS5463IF_WriteCmd(CS5463_CMD_POWER_UP_HALT);
			vTaskDelay(20/portTICK_RATE_MS);

			unPara[0] = 0;
			unPara[1] = 0;
			unPara[2] = 0;
			CS5463IF_WriteReg(CS5463_CMD_WR_AC_V_OFFSET, unPara, sizeof(unPara));
			CS5463IF_WriteCmd(CS5463_CMD_START_AC_V_OFFSET_CALIB);
			vTaskDelay(30/portTICK_RATE_MS);
			CS5463IF_Read(CS5463_CMD_RD_STATUS, unPara, sizeof(unPara));
			if (getCS5463RegBit(unPara, CS5463_STATUS_BIT_DATA_READY) == true){
				CS5463IF_Read(CS5463_CMD_RD_AC_V_OFFSET, unPara, sizeof(unPara));
				tSmartSocketParameter.tCS5463Calib.unAC_V_Offset = (unPara[0] << 16) | (unPara[1] << 8) | unPara[2];
				user_relay_led_output(LED_1HZ);
				if(xSemaphoreTake(xSmartSocketCaliSemaphore, (portTickType)(60000/portTICK_RATE_MS)) == pdTRUE ){
					tCaliState = CS5463_CALI_STATE_AC_V_GAIN;
				}else{
					printf("CS5463 calibration AC voltage gain timeout, exit...\n");
					tCaliState = CS5463_CALI_STATE_STOP;
			    }
			}else{
				tCaliState = CS5463_CALI_STATE_STOP;
			}

			break;

		case CS5463_CALI_STATE_AC_V_GAIN:
			user_relay_led_output(LED_5HZ);

			CS5463IF_WriteCmd(CS5463_CMD_SW_RESET);
			vTaskDelay(10/portTICK_RATE_MS);
			CS5463IF_WriteCmd(CS5463_CMD_POWER_UP_HALT);
			vTaskDelay(20/portTICK_RATE_MS);

			unPara[0] = 0;
			unPara[1] = 0;
			unPara[2] = 0;
			CS5463IF_WriteReg(CS5463_CMD_WR_V_GAIN, unPara, sizeof(unPara));
			CS5463IF_WriteCmd(CS5463_CMD_START_AC_V_GAIN_CALIB);
			vTaskDelay(30/portTICK_RATE_MS);
			CS5463IF_Read(CS5463_CMD_RD_STATUS, unPara, sizeof(unPara));
			if (getCS5463RegBit(unPara, CS5463_STATUS_BIT_DATA_READY) == true){
				CS5463IF_Read(CS5463_CMD_RD_V_GAIN, unPara, sizeof(unPara));
				tSmartSocketParameter.tCS5463Calib.unV_Gain = (unPara[0] << 16) | (unPara[1] << 8) | unPara[2];
				user_relay_led_output(LED_1HZ);
				if(xSemaphoreTake(xSmartSocketCaliSemaphore, (portTickType)(60000/portTICK_RATE_MS)) == pdTRUE ){
					tCaliState = CS5463_CALI_STATE_DC_I_OFFSET;
				}else{
					printf("CS5463 calibration DC current offset timeout, exit...\n");
					tCaliState = CS5463_CALI_STATE_STOP;
			    }
			}else{
				tCaliState = CS5463_CALI_STATE_STOP;
			}

			break;

		case CS5463_CALI_STATE_DC_I_OFFSET:
			user_relay_led_output(LED_5HZ);

			CS5463IF_WriteCmd(CS5463_CMD_SW_RESET);
			vTaskDelay(10/portTICK_RATE_MS);
			CS5463IF_WriteCmd(CS5463_CMD_POWER_UP_HALT);
			vTaskDelay(20/portTICK_RATE_MS);

			unPara[0] = 0;
			unPara[1] = 0;
			unPara[2] = 0;
			CS5463IF_WriteReg(CS5463_CMD_WR_DC_I_OFFSET, unPara, sizeof(unPara));
			CS5463IF_WriteCmd(CS5463_CMD_START_DC_I_OFFSET_CALIB);
			vTaskDelay(30/portTICK_RATE_MS);
			CS5463IF_Read(CS5463_CMD_RD_STATUS, unPara, sizeof(unPara));
			if (getCS5463RegBit(unPara, CS5463_STATUS_BIT_DATA_READY) == true){
				CS5463IF_Read(CS5463_CMD_RD_DC_I_OFFSET, unPara, sizeof(unPara));
				tSmartSocketParameter.tCS5463Calib.unDC_I_Offset = (unPara[0] << 16) | (unPara[1] << 8) | unPara[2];
				user_relay_led_output(LED_1HZ);
				if(xSemaphoreTake(xSmartSocketCaliSemaphore, (portTickType)(60000/portTICK_RATE_MS)) == pdTRUE ){
					tCaliState = CS5463_CALI_STATE_AC_I_OFFSET;
				}else{
					printf("CS5463 calibration AC current offset timeout, exit...\n");
					tCaliState = CS5463_CALI_STATE_STOP;
			    }
			}else{
				tCaliState = CS5463_CALI_STATE_STOP;
			}

			break;

		case CS5463_CALI_STATE_AC_I_OFFSET:
			user_relay_led_output(LED_5HZ);

			CS5463IF_WriteCmd(CS5463_CMD_SW_RESET);
			vTaskDelay(10/portTICK_RATE_MS);
			CS5463IF_WriteCmd(CS5463_CMD_POWER_UP_HALT);
			vTaskDelay(20/portTICK_RATE_MS);

			unPara[0] = 0;
			unPara[1] = 0;
			unPara[2] = 0;
			CS5463IF_WriteReg(CS5463_CMD_WR_AC_I_OFFSET, unPara, sizeof(unPara));
			CS5463IF_WriteCmd(CS5463_CMD_START_AC_I_OFFSET_CALIB);
			vTaskDelay(30/portTICK_RATE_MS);
			CS5463IF_Read(CS5463_CMD_RD_STATUS, unPara, sizeof(unPara));
			if (getCS5463RegBit(unPara, CS5463_STATUS_BIT_DATA_READY) == true){
				CS5463IF_Read(CS5463_CMD_RD_AC_I_OFFSET, unPara, sizeof(unPara));
				tSmartSocketParameter.tCS5463Calib.unAC_I_Offset = (unPara[0] << 16) | (unPara[1] << 8) | unPara[2];
				user_relay_led_output(LED_1HZ);
				if(xSemaphoreTake(xSmartSocketCaliSemaphore, (portTickType)(60000/portTICK_RATE_MS)) == pdTRUE ){
					tCaliState = CS5463_CALI_STATE_AC_I_GAIN;
				}else{
					printf("CS5463 calibration AC current gain timeout, exit...\n");
					tCaliState = CS5463_CALI_STATE_STOP;
			    }
			}else{
				tCaliState = CS5463_CALI_STATE_STOP;
			}

			break;

		case CS5463_CALI_STATE_AC_I_GAIN:
			user_relay_led_output(LED_5HZ);

			CS5463IF_WriteCmd(CS5463_CMD_SW_RESET);
			vTaskDelay(10/portTICK_RATE_MS);
			CS5463IF_WriteCmd(CS5463_CMD_POWER_UP_HALT);
			vTaskDelay(20/portTICK_RATE_MS);

			unPara[0] = 0;
			unPara[1] = 0;
			unPara[2] = 0;
			CS5463IF_WriteReg(CS5463_CMD_WR_I_GAIN, unPara, sizeof(unPara));
			CS5463IF_WriteCmd(CS5463_CMD_START_AC_I_GAIN_CALIB);
			vTaskDelay(30/portTICK_RATE_MS);
			CS5463IF_Read(CS5463_CMD_RD_STATUS, unPara, sizeof(unPara));
			if (getCS5463RegBit(unPara, CS5463_STATUS_BIT_DATA_READY) == true){
				CS5463IF_Read(CS5463_CMD_RD_I_GAIN, unPara, sizeof(unPara));
				tSmartSocketParameter.tCS5463Calib.unI_Gain = (unPara[0] << 16) | (unPara[1] << 8) | unPara[2];
				user_relay_led_output(LED_1HZ);
				tCaliState = CS5463_CALI_STATE_RESET;
			}else{
				tCaliState = CS5463_CALI_STATE_STOP;
			}

			break;

		case CS5463_CALI_STATE_RESET:
			tSmartSocketParameter.tCS5463Calib.unValidation = 0xA5A5A5A5;
			if(xSemaphoreTake(xSmartSocketParameterSemaphore, (portTickType)(10000/portTICK_RATE_MS)) == pdTRUE){
				system_param_save_with_protect(GET_USER_DATA_SECTORE(USER_DATA_CONF_PARA),
									&tSmartSocketParameter, sizeof(tSmartSocketParameter));
				xSemaphoreGive(xSmartSocketParameterSemaphore);
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
	tSmartSocketParameter.tConfigure.bCS5463Cali = false;
	user_plug_set_status(PLUG_STATUS_OPEN);
	user_relay_led_output(LED_OFF);
	return 0;
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

os_timer_t tTrendRecord;
void ICACHE_FLASH_ATTR
trend_record_callback(void *arg)
{
	TrendContent_t tValue;
	tValue.fTemperature = fCS5463_T;
	tValue.fCurrent = unCS5463_IRMS;
	tValue.fVoltage = unCS5463_VRMS;
	tValue.fPower = unCS5463_P;
	tValue.unTime = sntp_get_current_timestamp();
    DAT_bTrendRecordAdd(tValue);
}

void initCS5463(void)
{
//	  write_5463(0x5e,0x80,0x00,0x00);
//	   write_5463(0x40,0x00,0x00,0x01);
//	   write_5463(0x4a,0x00,0x0f,0xa0);
//	   write_5463(0x74,0x00,0x00,0x00);
//	   write_5463(0x64,0x80,0x00,0x01);
//
	uint8_t unPara[3];

	unPara[0] = 0xFF;
	unPara[1] = 0xFF;
	unPara[2] = 0xFF;
	CS5463IF_WriteReg(CS5463_CMD_WR_STATUS, unPara, sizeof(unPara));

	unPara[0] = 0;
	unPara[1] = 0;
	unPara[2] = 0x01;
	CS5463IF_WriteReg(CS5463_CMD_WR_CONFIG, unPara, sizeof(unPara));

	unPara[0] = 0;
	unPara[1] = 0x0F;
	unPara[2] = 0xA0;
	CS5463IF_WriteReg(CS5463_CMD_WR_CYCLE_COUNT, unPara, sizeof(unPara));

	unPara[0] = 0;
	unPara[1] = 0;
	unPara[2] = 0;
	CS5463IF_WriteReg(CS5463_CMD_WR_MASK, unPara, sizeof(unPara));

	unPara[0] = 0x80;
	unPara[1] = 0;
	unPara[2] = 0x01;
	CS5463IF_WriteReg(CS5463_CMD_WR_MODE, unPara, sizeof(unPara));

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

	CS5463IF_WriteCmd(CS5463_CMD_START_CNTN_CNVS);

    os_timer_disarm(&tTrendRecord);
    os_timer_setfn(&tTrendRecord, trend_record_callback , NULL);
    os_timer_arm(&tTrendRecord, TREND_RECORD_INTERVAL, 1);

	while(1){
		// Temperature
		CS5463IF_Read(CS5463_CMD_RD_T, unCS5463Data, sizeof(unCS5463Data));
		fCS5463_T = (int8_t)(unCS5463Data[0]);
//		unCS5463_P = 15 + ((float)(unCS5463ReadData[1] % 100))/100;
		vTaskDelay(200/portTICK_RATE_MS);

		// Current
		CS5463IF_Read(CS5463_CMD_RD_IRMS, unCS5463Data, sizeof(unCS5463Data));
		unCS5463_IRMS = (unCS5463Data[0] << 16) | (unCS5463Data[1] << 8) | unCS5463Data[2]; //(float)(*((int16_t*)(unCS5463ReadData)))/MAX_SIGNED_INT_16_VALUE;
		vTaskDelay(200/portTICK_RATE_MS);

		// Voltage
		CS5463IF_Read(CS5463_CMD_RD_VRMS, unCS5463Data, sizeof(unCS5463Data));
		unCS5463_VRMS = (unCS5463Data[0] << 16) | (unCS5463Data[1] << 8) | unCS5463Data[2]; //(float)(*((int16_t*)(unCS5463ReadData)))/MAX_SIGNED_INT_16_VALUE;
		vTaskDelay(200/portTICK_RATE_MS);

		// Status
		CS5463IF_Read(CS5463_CMD_RD_STATUS, unCS5463Data, sizeof(unCS5463Data));
		unCS5463_Status = (unCS5463Data[0] << 16) | (unCS5463Data[1] << 8) | unCS5463Data[2];
		vTaskDelay(200/portTICK_RATE_MS);

		// Power
		CS5463IF_Read(CS5463_CMD_RD_P, unCS5463Data, sizeof(unCS5463Data));
		unCS5463_P = (unCS5463Data[0] << 16) | (unCS5463Data[1] << 8) | unCS5463Data[2];	//((unCS5463ReadData[0] << 16) | (unCS5463ReadData[1] << 8) | unCS5463ReadData[2]);
		vTaskDelay(200/portTICK_RATE_MS);
	}
	os_timer_disarm(&tTrendRecord);
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
	//
uint32_t CS5463_unGetStatus(void)
{
	return unCS5463_Status;
}

uint32_t CS5463_unGetCurrent(void)
{
	return unCS5463_IRMS;
}

uint32_t CS5463_unGetVoltage(void)
{
	return unCS5463_VRMS;
}

uint32_t CS5463_unGetPower(void)
{
	return unCS5463_P;
}

float CS5463_fGetTemperature(void)
{
	return fCS5463_T;
}
