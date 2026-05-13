// elec2645 camel games console
// main file for the shared menu and game switching
// each game has its own folder and shared stuff stays in shared

// custom pallette guide
// 0  = black      
// 1  = white       
// 2  = baby blue    
// 3  = brown     
// 4  = camel tan   
// 5  = camel nose 
// 6  = pale cream  
// 7  = light pink   
// 8  = lavender     
// 9  = grey         
// 10 = sage green   
// 11 = red
// 12 = green
// 13 = blue
// 14 = yellow
// 15 = orange

// stm32cubemx files
#include "main.h"
#include "tim.h"       // timer 2 for pwm buzzer control
#include "usart.h"     // serial output
#include "gpio.h"      // gpio control
#include "adc.h"       // adc for joystick input
#include "rng.h"       // rng if a game needs it




// stm32 setup functions
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);

// drivers and helper files
#include "Buzzer.h"    // buzzer control
#include "PWM.h"       // pwm control for led 
#include "LCD.h"       // lcd display
#include "Joystick.h"  // joystick input
#include "Utils.h"     // shared types

// menu and input
#include "Menu.h"      // menu screens
#include "InputHandler.h" // input reading
#include "IntroScreens.h" // startup pixel art screens

#include <stdint.h>
#include <stdio.h>
#include <math.h>

// game entry points
MenuState Game1_Run(void);
MenuState Game2_Run(void);
MenuState Game3_Run(void);

// buzzer on tim2 channel 3
Buzzer_cfg_t buzzer_cfg = {
    .htim = &htim2,
    .channel = TIM_CHANNEL_3,
    .tick_freq_hz = 1000000,  // 1mhz timer clock
    .min_freq_hz = 20,
    .max_freq_hz = 20000,
    .setup_done = 0
};

// lcd setup
ST7789V2_cfg_t cfg0 = {
    .setup_done = 0,
    .spi = SPI2,
    .RST = {.port = GPIOB, .pin = GPIO_PIN_2},
    .BL = {.port = GPIOB, .pin = GPIO_PIN_1},
    .DC = {.port = GPIOB, .pin = GPIO_PIN_11},
    .CS = {.port = GPIOB, .pin = GPIO_PIN_12},
    .MOSI = {.port = GPIOB, .pin = GPIO_PIN_15},
    .SCLK = {.port = GPIOB, .pin = GPIO_PIN_13},
    .dma = {.instance = DMA1, .channel = DMA1_Channel5}
};

// joystick setup
Joystick_cfg_t joystick_cfg = {
    .adc = &hadc1,
    .x_channel = ADC_CHANNEL_1, // a5 on nucleo board
    .y_channel = ADC_CHANNEL_2, // a4 on nucleo board
    .sampling_time = ADC_SAMPLETIME_47CYCLES_5,
    .center_x = JOYSTICK_DEFAULT_CENTER_X,
    .center_y = JOYSTICK_DEFAULT_CENTER_Y,
    .deadzone = JOYSTICK_DEADZONE,
    .setup_done = 0
};

// latest joystick reading
Joystick_t joystick_data;

// pwm setup
PWM_cfg_t pwm_cfg = {
    .htim = &htim4,
    .channel = TIM_CHANNEL_1,
    .tick_freq_hz = 1000000,  // 1mhz timer clock
    .min_freq_hz = 10,
    .max_freq_hz = 50000,
    .setup_done = 0
};

// menu state
MenuSystem menu;

// timer counters
volatile uint32_t g_tim6_ticks = 0;
volatile uint32_t g_tim7_ticks = 0;

// button debounce
#define DEBOUNCE_DELAY 200  // 200ms debounce

// timers available for the games
//
// tim2 is used for the buzzer
// tim4 is used for pwm led stuff
// tim6 is a faster general timer
// tim7 is a slower timer if we need it
//
// these are set up here but only started if a game needs them


// small helper functions

/**
 * @brief Redirect printf to UART for debugging
 */
int _write(int file, char *ptr, int len) {
    HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, HAL_MAX_DELAY);
    return len;
}

// button interrupt callback
/**
 * @brief HAL callback for external interrupts (button presses)
 * 
 * This simply sets a flag that the main loop will check once per frame
 * in InputHandler_Read(). This provides debouncing and clean separation
 * between interrupt handling and main game logic.
 * 
 * Based on Unit_3_4_Event_Trigger pattern.
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);  // implemented in inputhandler.c

/**
 * @brief HAL callback for timer period elapsed interrupts
 *
 * TIM6/TIM7 periodic interrupts are handled centrally here.
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM6) {
    g_tim6_ticks++;
  }
  else if (htim->Instance == TIM7) {
    g_tim7_ticks++;
  }
}

static void Play_Intro_Tone(Buzzer_Note_t note, uint32_t duration_ms)
{
    buzzer_note(&buzzer_cfg, note, 12);
    HAL_Delay(duration_ms);
    buzzer_off(&buzzer_cfg);
    HAL_Delay(35);
}

static void Play_Intro_Tune(void)
{
    Play_Intro_Tone(NOTE_E5, 90);
    Play_Intro_Tone(NOTE_G5, 90);
    Play_Intro_Tone(NOTE_A5, 120);
    Play_Intro_Tone(NOTE_E5, 160);
}

void Show_Studio_Splash(void)
{
    LCD_Fill_Buffer(2);
    LCD_Draw_Sprite(0, 20, INTRO_SCREEN_HEIGHT, INTRO_SCREEN_WIDTH, intro_studio_sprite);
    LCD_Refresh(&cfg0);
    Play_Intro_Tune();
    HAL_Delay(1150);
}


void Show_Title_Screen(void)
{
    LCD_Fill_Buffer(2);
    LCD_Draw_Sprite(0, 20, INTRO_SCREEN_HEIGHT, INTRO_SCREEN_WIDTH, intro_title_sprite);
    LCD_Refresh(&cfg0);
    HAL_Delay(1800);
}

static void LED1_Set(uint8_t r, uint8_t g, uint8_t b)
{
    // led1 wiring on my board pa9 green pc7 blue pc8 red
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, r ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, g ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, b ? GPIO_PIN_SET : GPIO_PIN_RESET);

    // led2 just follows the same colour
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, r ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, g ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, b ? GPIO_PIN_SET : GPIO_PIN_RESET);
}



// main function
/**
  * @brief  The application entry point - Menu System with Simple Game Loop
  * @retval int
  */
int main(void)
{
    /* MCU Configuration */
    HAL_Init();
    SystemClock_Config();
    PeriphCommonClock_Config();

    /* Initialize peripherals */
    MX_GPIO_Init();
    LED1_Set(1, 1, 1);

    MX_USART2_UART_Init();
    MX_ADC1_Init();  // adc for joystick
    MX_RNG_Init();   // rng if needed
    
    // lcd first because it uses some gpio pins
    LCD_init(&cfg0);

    // my custom palette for the camel theme
    LCD_Set_Palette(PALETTE_CUSTOM);

    // buzzer timer
    MX_TIM2_Init();
    buzzer_init(&buzzer_cfg);

    // tim4 comes after lcd to avoid the pb6 conflict
    MX_TIM4_Init();
    
    // general purpose timers
    // tim6 is the faster timer
    // tim7 is the slower timer
    MX_TIM6_Init();
    MX_TIM7_Init();

    // start tim6 so there is a regular tick available
    HAL_TIM_Base_Start_IT(&htim6);

    // tim7 is there if we need a slower tick later
    // hal_tim_base_start_it(&htim7)
  
    // button handling
    Input_Init();
    
    // joystick
    Joystick_Init(&joystick_cfg);
    
    // clear screen
    LCD_Fill_Buffer(0);
    LCD_Refresh(&cfg0);

    // shared startup screens
    Show_Studio_Splash();
    Show_Title_Screen();
        
    // pwm for led control
    PWM_Init(&pwm_cfg);
    PWM_SetFreq(&pwm_cfg, 1000);
    PWM_SetDuty(&pwm_cfg, 50);  // start at 50 percent
    
    // make sure ld2 starts off
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  


    // start the menu
    Menu_Init(&menu);
    
    printf("Menu system initialized. Press BT3 to select.\n");

    // menu and games run until they return the next state
    MenuState current_state = MENU_STATE_HOME;
    
    while (1)
    {
        switch (current_state) {
            case MENU_STATE_HOME:
                // menu returns the chosen game
                current_state = Menu_Run(&menu);
                break;
                
            case MENU_STATE_GAME_1:
                // game 1 returns here when it exits
                current_state = Game1_Run();
                break;
                
            case MENU_STATE_GAME_2:
                // game 2 returns here when it exits
                current_state = Game2_Run();
                break;
                
            case MENU_STATE_GAME_3:
                // game 3 returns here when it exits
                current_state = Game3_Run();
                break;
                
            default:
                // fall back to the menu if something odd happens
                current_state = MENU_STATE_HOME;
                break;
        }
    }
    
    return 0;
}


// game files
//   - game_1/Game_1.c
//   - game_2/Game_2.c  
//   - game_3/Game_3.c
//
// game 1 and game 2 are separate with common menu and lcd code in shared





// stm32 generated functions below
// leaving these alone unless something really needs changing


/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RNG|RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCCLKSOURCE_PLLSAI1;
  PeriphClkInit.RngClockSelection = RCC_RNGCLKSOURCE_PLLSAI1;
  PeriphClkInit.PLLSAI1.PLLSAI1Source = RCC_PLLSOURCE_HSI;
  PeriphClkInit.PLLSAI1.PLLSAI1M = 1;
  PeriphClkInit.PLLSAI1.PLLSAI1N = 8;
  PeriphClkInit.PLLSAI1.PLLSAI1P = RCC_PLLP_DIV7;
  PeriphClkInit.PLLSAI1.PLLSAI1Q = RCC_PLLQ_DIV4;
  PeriphClkInit.PLLSAI1.PLLSAI1R = RCC_PLLR_DIV2;
  PeriphClkInit.PLLSAI1.PLLSAI1ClockOut = RCC_PLLSAI1_48M2CLK|RCC_PLLSAI1_ADC1CLK;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}



/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
