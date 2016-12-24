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

#define CS5463_RST_IO_PIN		GPIO_Pin_5
#define CS5463_RST_PIN_NUM		5

void CS5463_Manager(void *pvParameters)
{
	SpiData tSPI_Dat;
	SpiAttr tAttr;   //Set as Master/Sub mode 0 and speed 2MHz
	GPIO_ConfigTypeDef tIO_OutConf;
	uint32_t unCS5463ReadDummyData;	//[] = {CS5463_CMD_SYNC_1, CS5463_CMD_SYNC_1, CS5463_CMD_SYNC_1};

	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U,FUNC_HSPIQ_MISO);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U,FUNC_HSPI_CS0);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U,FUNC_HSPID_MOSI);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U,FUNC_HSPI_CLK);

	tAttr.mode=SpiMode_Master;
	tAttr.subMode=SpiSubMode_0;
	tAttr.speed=SpiSpeed_2MHz;
	tAttr.bitOrder=SpiBitOrder_MSBFirst;
	SPIInit(SpiNum_HSPI,&tAttr);

	tIO_OutConf.GPIO_IntrType = GPIO_PIN_INTR_DISABLE;
	tIO_OutConf.GPIO_Mode = GPIO_Mode_Output;
	tIO_OutConf.GPIO_Pin = CS5463_RST_IO_PIN ;
	tIO_OutConf.GPIO_Pullup = GPIO_PullUp_DIS;
	gpio_config(&tIO_OutConf);

	gpio16_output_conf();
	gpio16_output_set(1);

	GPIO_OUTPUT_SET(CS5463_RST_PIN_NUM, 0);
	vTaskDelay(200/portTICK_RATE_MS);
	GPIO_OUTPUT_SET(CS5463_RST_PIN_NUM, 1);
	vTaskDelay(200/portTICK_RATE_MS);

	tSPI_Dat.cmd = CS5463_CMD_RD_CONFIG;			 ///< Command value
	tSPI_Dat.cmdLen = 1;		   ///< Command byte length
	tSPI_Dat.addr = NULL; 		 ///< Point to address value
	tSPI_Dat.addrLen = 0; 	   ///< Address byte length
	tSPI_Dat.data = &unCS5463ReadDummyData; 		 ///< Point to data buffer
	tSPI_Dat.dataLen = 3; 	   ///< Data byte length.

	while(1){
		gpio16_output_set(0);
		memset(&unCS5463ReadDummyData, CS5463_CMD_SYNC_1, sizeof(unCS5463ReadDummyData));
		SPIMasterRecvData(SpiNum_HSPI, &tSPI_Dat);
		gpio16_output_set(1);
		printf("Data received from CS5463 is %X.\n", unCS5463ReadDummyData);
		vTaskDelay(200/portTICK_RATE_MS);
	}
}

double CS5463_dGetCurrent(void)
{
	printf("user_cs5463_get_current_d\n");
	return 2.34;
}
