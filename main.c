#include "main.h"
#include <stdio.h>
#include <stdint.h>

/* ADC */
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

/* LCD pin definitions */
#define LCD_PORT GPIOB
#define LCD_RS   GPIO_PIN_0
#define LCD_EN   GPIO_PIN_1
#define LCD_D4   GPIO_PIN_10
#define LCD_D5   GPIO_PIN_12
#define LCD_D6   GPIO_PIN_13
#define LCD_D7   GPIO_PIN_14

/* LEDs and relay */
#define GREEN_LED GPIO_PIN_5
#define RED_LED   GPIO_PIN_6
#define RELAY_PIN GPIO_PIN_7

/* Assumes active-low relay */
#define RELAY_ON  GPIO_PIN_RESET
#define RELAY_OFF GPIO_PIN_SET

/* ADC channels */
#define VOLTAGE_CHANNEL ADC_CHANNEL_0
#define CURRENT_CHANNEL ADC_CHANNEL_1

/* Voltage divider: 39k upper, 10k lower */
#define VOLTAGE_FACTOR 4.9f

/* ACS712 divider: 10k upper, 20k lower */
#define ACS712_DIVIDER_FACTOR 1.5f

/* ACS712-20A nominal sensitivity */
#define ACS712_SENSITIVITY 0.100f

/* Example protection limits: change for your load */
#define MIN_VOLTAGE 10.0f
#define MAX_VOLTAGE 15.0f
#define MAX_CURRENT 5.0f

float zero_current_voltage = 2.5f;

/* Function prototypes */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
void Error_Handler(void);

void LCD_Init(void);
void LCD_Command(uint8_t command);
void LCD_Char(char data);
void LCD_String(char *str);
void LCD_SetCursor(uint8_t row, uint8_t col);
void LCD_PrintLine(uint8_t row, char *text);

uint16_t Read_ADC_Channel(uint32_t channel);
float Read_Voltage(void);
float Read_Current(void);

/* ================= LCD FUNCTIONS ================= */

static void LCD_Pulse(void)
{
    HAL_GPIO_WritePin(LCD_PORT, LCD_EN, GPIO_PIN_SET);
    HAL_Delay(1);

    HAL_GPIO_WritePin(LCD_PORT, LCD_EN, GPIO_PIN_RESET);
    HAL_Delay(1);
}

static void LCD_Send4Bit(uint8_t data)
{
    HAL_GPIO_WritePin(LCD_PORT, LCD_D4,
        (data & 0x01U) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    HAL_GPIO_WritePin(LCD_PORT, LCD_D5,
        (data & 0x02U) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    HAL_GPIO_WritePin(LCD_PORT, LCD_D6,
        (data & 0x04U) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    HAL_GPIO_WritePin(LCD_PORT, LCD_D7,
        (data & 0x08U) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    LCD_Pulse();
}

static void LCD_Send(uint8_t data, uint8_t rs)
{
    HAL_GPIO_WritePin(LCD_PORT, LCD_RS,
        rs ? GPIO_PIN_SET : GPIO_PIN_RESET);

    LCD_Send4Bit(data >> 4);
    LCD_Send4Bit(data & 0x0F);
}

void LCD_Command(uint8_t command)
{
    LCD_Send(command, 0);
    HAL_Delay(2);
}

void LCD_Char(char data)
{
    LCD_Send((uint8_t)data, 1);
}

void LCD_String(char *str)
{
    while (*str != '\0')
    {
        LCD_Char(*str++);
    }
}

void LCD_SetCursor(uint8_t row, uint8_t col)
{
    uint8_t address;

    address = (row == 0U)
            ? (uint8_t)(0x80U + col)
            : (uint8_t)(0xC0U + col);

    LCD_Command(address);
}

void LCD_Init(void)
{
    HAL_Delay(50);

    HAL_GPIO_WritePin(LCD_PORT, LCD_RS, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_PORT, LCD_EN, GPIO_PIN_RESET);

    LCD_Send4Bit(0x03);
    HAL_Delay(5);

    LCD_Send4Bit(0x03);
    HAL_Delay(2);

    LCD_Send4Bit(0x03);
    HAL_Delay(2);

    LCD_Send4Bit(0x02);

    LCD_Command(0x28); /* 4-bit, 2-line */
    LCD_Command(0x0C); /* Display ON */
    LCD_Command(0x06); /* Cursor increment */
    LCD_Command(0x01); /* Clear display */

    HAL_Delay(5);
}

void LCD_PrintLine(uint8_t row, char *text)
{
    uint8_t i;

    LCD_SetCursor(row, 0);

    for (i = 0; i < 16U; i++)
    {
        if (text[i] != '\0')
        {
            LCD_Char(text[i]);
        }
        else
        {
            LCD_Char(' ');
        }
    }
}

/* ================= ADC FUNCTIONS ================= */

uint16_t Read_ADC_Channel(uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    uint16_t adc_value = 0;

    sConfig.Channel = channel;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    __HAL_ADC_CLEAR_FLAG(&hadc1, ADC_FLAG_EOC | ADC_FLAG_STRT);

    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_ADC_Start(&hadc1) == HAL_OK)
    {
        if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK)
        {
            adc_value = (uint16_t)HAL_ADC_GetValue(&hadc1);
        }

        HAL_ADC_Stop(&hadc1);
    }

    return adc_value;
}

float Read_Voltage(void)
{
	 uint32_t adc_sum = 0;
	    uint16_t adc_value;
	    float adc_voltage;
	    float input_voltage;
	    uint8_t i;

	    /* Read 10 ADC samples */
	    for (i = 0; i < 10; i++)
	    {
	        adc_sum += Read_ADC_Channel(VOLTAGE_CHANNEL);
	        HAL_Delay(2);
	    }

	    /* Calculate average ADC value */
	    adc_value = (uint16_t)(adc_sum / 10U);

	    /* Convert ADC value into voltage */
	    adc_voltage = ((float)adc_value * 3.3f) / 4095.0f;

	    /* Calculate actual input voltage */
	    input_voltage = adc_voltage * VOLTAGE_FACTOR;


    return input_voltage;
}

float Read_Current(void)
{
    uint16_t adc_value;
    float adc_voltage;
    float sensor_voltage;
    float current;

    adc_value = Read_ADC_Channel(CURRENT_CHANNEL);

    adc_voltage = ((float)adc_value * 3.3f) / 4095.0f;

    /* Convert divider output to original ACS712 output */
    sensor_voltage = adc_voltage * ACS712_DIVIDER_FACTOR;

    current = (sensor_voltage - zero_current_voltage)
              / ACS712_SENSITIVITY;

    /* Display absolute current */
    if (current < 0.0f)
    {
        current = -current;
    }

    return current;
}

/* Display one decimal place without printf float support */
static void Format_Measurements(char *line,
                                float voltage,
                                float current)
{
    int voltage10 = (int)(voltage * 10.0f + 0.5f);
    int current10 = (int)(current * 10.0f + 0.5f);

    int v_whole = voltage10 / 10;
    int v_decimal = voltage10 % 10;

    int i_whole = current10 / 10;
    int i_decimal = current10 % 10;

    /*
     * LCD row maximum is 16 characters.
     * Example: V:12.3V I:1.5A
     */
    snprintf(line, 17, "V:%2d.%1dV I:%1d.%1dA",
             v_whole, v_decimal,
             i_whole, i_decimal);
}

/* ================= MAIN ================= */

int main(void)
{
    float voltage;
    float current;

    uint8_t under_voltage;
    uint8_t over_voltage;
    uint8_t over_current;
    uint8_t fault;

    char line1[17];
    char line2[17];

    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_ADC1_Init();

    /* LCD startup */
    LCD_Init();

    /* Relay starts OFF */
    HAL_GPIO_WritePin(GPIOA, RELAY_PIN, RELAY_OFF);

    HAL_GPIO_WritePin(GPIOA, GREEN_LED, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, RED_LED, GPIO_PIN_RESET);

    LCD_PrintLine(0, "DC POWER SYSTEM");
    LCD_PrintLine(1, "Calibrating...");

    /*
     * Disconnect/disable the load during startup calibration.
     * Read 100 current sensor samples at zero current.
     */
    {
        uint32_t sum = 0;
        uint16_t i;

        for (i = 0; i < 100U; i++)
        {
            sum += Read_ADC_Channel(CURRENT_CHANNEL);
            HAL_Delay(5);
        }

        zero_current_voltage =
            (((float)sum / 100.0f) * 3.3f / 4095.0f)
            * ACS712_DIVIDER_FACTOR;
    }

    LCD_PrintLine(0, "Calibration OK");
    LCD_PrintLine(1, "Monitoring...");
    HAL_Delay(1000);

    while (1)
    {
        voltage = Read_Voltage();
        current = Read_Current();

        under_voltage = (voltage < MIN_VOLTAGE);
        over_voltage = (voltage > MAX_VOLTAGE);
        over_current = (current > MAX_CURRENT);

        fault = under_voltage || over_voltage || over_current;

        /* Always display measured values on row 1 */
        Format_Measurements(line1, voltage, current);
        LCD_PrintLine(0, line1);

        if (fault == 0U)
        {
            /* NORMAL */
            HAL_GPIO_WritePin(GPIOA, RELAY_PIN, RELAY_ON);
            HAL_GPIO_WritePin(GPIOA, GREEN_LED, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOA, RED_LED, GPIO_PIN_RESET);

            LCD_PrintLine(1, "STATUS: NORMAL");
        }
        else
        {
            /* FAULT: relay OFF, red LED ON */
            HAL_GPIO_WritePin(GPIOA, RELAY_PIN, RELAY_OFF);
            HAL_GPIO_WritePin(GPIOA, GREEN_LED, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOA, RED_LED, GPIO_PIN_SET);

            if (over_current)
            {
                LCD_PrintLine(1, "OVERCURRENT!");
            }
            else if (under_voltage)
            {
                LCD_PrintLine(1, "UNDER VOLTAGE");
            }
            else
            {
                LCD_PrintLine(1, "OVER VOLTAGE");
            }
        }

        HAL_Delay(500);
    }
}

/* ================= CLOCK CONFIGURATION ================= */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();

    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue =
        RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct,
                            FLASH_LATENCY_0) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ================= ADC1 INITIALIZATION ================= */

static void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge =
        ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;

    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    sConfig.Channel = ADC_CHANNEL_0;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;

    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ================= DMA INITIALIZATION ================= */

static void MX_DMA_Init(void)
{
    /*
     * DMA is not used by this program.
     * Keep this function so the generated main structure remains
     * compatible with the existing project.
     */
    __HAL_RCC_DMA2_CLK_ENABLE();
}

/* ================= GPIO INITIALIZATION ================= */

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* Safe initial output levels */
    HAL_GPIO_WritePin(GPIOA,
                      GREEN_LED | RED_LED,
                      GPIO_PIN_RESET);

    HAL_GPIO_WritePin(GPIOA, RELAY_PIN, RELAY_OFF);

    HAL_GPIO_WritePin(GPIOB,
                      LCD_RS | LCD_EN |
                      LCD_D4 | LCD_D5 | LCD_D6 | LCD_D7,
                      GPIO_PIN_RESET);

    /* PA0 and PA1: analog input pins */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PA5, PA6, PA7: LED and relay outputs */
    GPIO_InitStruct.Pin = GREEN_LED | RED_LED | RELAY_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* LCD outputs */
    GPIO_InitStruct.Pin = LCD_RS | LCD_EN |
                          LCD_D4 | LCD_D5 | LCD_D6 | LCD_D7;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/* ================= ERROR HANDLER ================= */

void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
        /* Stop here if a HAL initialization error occurs */
    }
}

#ifdef USE_FULL_ASSERT

void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}

#endif
