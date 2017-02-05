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

static int8_t fCS5463_T;
static uint32_t unCS5463_I;
static uint32_t unCS5463_Status;
static float fCS5463_V;
static float fCS5463_P;

//FLASH_SECTOR_SIZE

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
	writeCS5463SPI(CS5463_CMD_SYNC_1);
	writeCS5463SPI(CS5463_CMD_SYNC_1);
	writeCS5463SPI(CS5463_CMD_SYNC_1);
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
	tValue.fCurrent = unCS5463_I;
	tValue.fVoltage = fCS5463_V;
	tValue.fPower = fCS5463_P;
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

void CS5463_Manager(void *pvParameters)
{
	uint8_t unCS5463ReadData[3];	//[] = {CS5463_CMD_SYNC_1, CS5463_CMD_SYNC_1, CS5463_CMD_SYNC_1};

	CS5463IF_Init();

	GPIO_OUTPUT_SET(CS5463_RST_PIN_NUM, 0);
	vTaskDelay(500/portTICK_RATE_MS);
	GPIO_OUTPUT_SET(CS5463_RST_PIN_NUM, 1);
	vTaskDelay(500/portTICK_RATE_MS);

	CS5463IF_WriteCmd(CS5463_CMD_START_CNTN_CNVS);

    os_timer_disarm(&tTrendRecord);
    os_timer_setfn(&tTrendRecord, trend_record_callback , NULL);
    os_timer_arm(&tTrendRecord, TREND_RECORD_INTERVAL, 1);

	while(1){
		// Temperature
		CS5463IF_Read(CS5463_CMD_RD_T, unCS5463ReadData, sizeof(unCS5463ReadData));
		fCS5463_T = (int8_t)(unCS5463ReadData[0]);
		fCS5463_P = 15 + ((float)(unCS5463ReadData[1] % 100))/100;
		vTaskDelay(200/portTICK_RATE_MS);

		// Current
		CS5463IF_Read(CS5463_CMD_RD_I, unCS5463ReadData, sizeof(unCS5463ReadData));
		unCS5463_I = ((unCS5463ReadData[0] << 16) | (unCS5463ReadData[1] << 8) | unCS5463ReadData[2]); //(float)(*((int16_t*)(unCS5463ReadData)))/MAX_SIGNED_INT_16_VALUE;
		vTaskDelay(200/portTICK_RATE_MS);

		// Voltage
		CS5463IF_Read(CS5463_CMD_RD_V, unCS5463ReadData, sizeof(unCS5463ReadData));
		fCS5463_V = (float)(*((int16_t*)(unCS5463ReadData)))/MAX_SIGNED_INT_16_VALUE;	//((unCS5463ReadData[0] << 16) | (unCS5463ReadData[1] << 8) | unCS5463ReadData[2]);
		vTaskDelay(200/portTICK_RATE_MS);

		// Status
		CS5463IF_Read(CS5463_CMD_RD_STATUS, unCS5463ReadData, sizeof(unCS5463ReadData));
		unCS5463_Status = ((unCS5463ReadData[0] << 16) | (unCS5463ReadData[1] << 8) | unCS5463ReadData[2]);
		vTaskDelay(200/portTICK_RATE_MS);

		// Power
//		CS5463IF_Read(CS5463_CMD_RD_P, unCS5463ReadData, sizeof(unCS5463ReadData));
//		fCS5463_P = *((int16_t*)(unCS5463ReadData))/MAX_SIGNED_INT_16_VALUE;	//((unCS5463ReadData[0] << 16) | (unCS5463ReadData[1] << 8) | unCS5463ReadData[2]);
		vTaskDelay(200/portTICK_RATE_MS);
	}
}
	//
uint32_t CS5463_unGetStatus(void)
{
	return unCS5463_Status;
}

uint32_t CS5463_unGetCurrent(void)
{
	return unCS5463_I;
}

float CS5463_fGetVoltage(void)
{
	return fCS5463_V;
}

float CS5463_fGetPower(void)
{
	return fCS5463_P;
}

float CS5463_fGetTemperature(void)
{
	return fCS5463_T;
}
