#include "capacitive_moisture.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_log.h"
#include "esp_system.h"

int    m_valuePct=0 ;               
int    m_valueVRead=0;

int getMoisture()  {  return m_valuePct;    }


void CM_ErrorHandler(int response)
{
	switch(response) {
		case CM_SIGNAL_ERROR:
			ESP_LOGI( "CM", "Signal error\n" );
			break;

		case CM_OK:
			break;

		default :
			ESP_LOGI( "CM", "Unknown error\n" );
	}
}

int readCM()
{
  gpio_pad_select_gpio(MOISTURE_SENSOR_PIN);
  gpio_set_direction(MOISTURE_SENSOR_PIN, GPIO_MODE_INPUT );		

  adc1_config_width(ADC_WIDTH_BIT_12);  // Đặt độ phân giải ADC là 12 bit
  adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN_DB_11);  // Đặt độ giảm áp cho kênh ADC  
  m_valueVRead = adc1_get_raw(ADC_CHANNEL);
  m_valuePct = 100 - ((m_valueVRead - CALIBRATEMIN)*100/(CALIBRATEMAX - CALIBRATEMIN));
	if  (m_valuePct > 100) m_valuePct = 100;
  printf(" Read= %d", m_valueVRead);
  printf(" valuePct= %d\n", m_valuePct);
  	if (m_valueVRead>0)  return CM_OK;
    else return CM_SIGNAL_ERROR;
}
