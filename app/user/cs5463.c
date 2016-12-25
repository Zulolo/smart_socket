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

int8_t nCS5463_Temperature;

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

void CS5463_Manager(void *pvParameters)
{
	uint8_t unCS5463ReadData[3];	//[] = {CS5463_CMD_SYNC_1, CS5463_CMD_SYNC_1, CS5463_CMD_SYNC_1};

	CS5463IF_Init();

	GPIO_OUTPUT_SET(CS5463_RST_PIN_NUM, 0);
	vTaskDelay(500/portTICK_RATE_MS);
	GPIO_OUTPUT_SET(CS5463_RST_PIN_NUM, 1);
	vTaskDelay(500/portTICK_RATE_MS);

	CS5463IF_WriteCmd(CS5463_CMD_START_CNTN_CNVS);
	while(1){
		CS5463IF_Read(CS5463_CMD_RD_T, unCS5463ReadData, sizeof(unCS5463ReadData));
		nCS5463_Temperature = (int8_t)(unCS5463ReadData[0]);
		if (unCS5463ReadData[0] > 28){

		}
		vTaskDelay(200/portTICK_RATE_MS);
	}
}

double CS5463_dGetCurrent(void)
{
	printf("user_cs5463_get_current_d\n");
	return 2.34;
}
