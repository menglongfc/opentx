/*
 * Authors (alphabetical order)
 * - Andre Bernet <bernet.andre@gmail.com>
 * - Andreas Weitl
 * - Bertrand Songis <bsongis@gmail.com>
 * - Bryan J. Rentoul (Gruvin) <gruvin@gmail.com>
 * - Cameron Weeks <th9xer@gmail.com>
 * - Erez Raviv
 * - Gabriel Birkus
 * - Jean-Pierre Parisy
 * - Karl Szmutny
 * - Michael Blandford
 * - Michal Hlavinka
 * - Pat Mackenzie
 * - Philip Moss
 * - Rob Thomson
 * - Romolo Manfredini <romolo.manfredini@gmail.com>
 * - Thomas Husterer
 *
 * opentx is based on code named
 * gruvin9x by Bryan J. Rentoul: http://code.google.com/p/gruvin9x/,
 * er9x by Erez Raviv: http://code.google.com/p/er9x/,
 * and the original (and ongoing) project by
 * Thomas Husterer, th9x: http://code.google.com/p/th9x/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "../../opentx.h"

uint8_t uart3Mode = UART_MODE_NONE;
Fifo<512> uart3TxFifo;
extern Fifo<512> telemetryFifo;
extern Fifo<32> sbusFifo;

void uart3Setup(unsigned int baudrate)
{
  USART_InitTypeDef USART_InitStructure;
  GPIO_InitTypeDef GPIO_InitStructure;

  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIO_UART3, ENABLE);

  GPIO_PinAFConfig(GPIO_UART3, GPIO_PinSource_UART3_RX, GPIO_AF_UART3);
  GPIO_PinAFConfig(GPIO_UART3, GPIO_PinSource_UART3_TX, GPIO_AF_UART3);

  GPIO_InitStructure.GPIO_Pin = GPIO_PIN_UART3_TX | GPIO_PIN_UART3_RX;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIO_UART3, &GPIO_InitStructure);
  
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART3, ENABLE);
  
  USART_InitStructure.USART_BaudRate = baudrate;
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  USART_InitStructure.USART_Parity = USART_Parity_No;
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
  
  USART_Init(UART3, &USART_InitStructure);
  USART_Cmd(UART3, ENABLE);

  USART_ITConfig(UART3, USART_IT_RXNE, ENABLE);
  USART_ITConfig(UART3, USART_IT_TXE, DISABLE);

  NVIC_SetPriority(USART3_IRQn, 7);
  NVIC_EnableIRQ(USART3_IRQn);
}

void uart3Init(unsigned int mode, unsigned int protocol)
{
  USART_DeInit(USART3);

  uart3Mode = false;

  switch (mode) {
    case UART_MODE_TELEMETRY_MIRROR:
      uart3Setup(FRSKY_SPORT_BAUDRATE);
      break;
#if defined(DEBUG)
    case UART_MODE_DEBUG:
      uart3Setup(DEBUG_BAUDRATE);
      break;
#endif
    case UART_MODE_TELEMETRY:
      if (protocol == PROTOCOL_FRSKY_D_SECONDARY) {
        uart3Setup(FRSKY_D_BAUDRATE);
      }
      break;
  }

  uart3Mode = mode;
}

void uart3Putc(const char c)
{
  uart3TxFifo.push(c);
  USART_ITConfig(UART3, USART_IT_TXE, ENABLE);
}

#if defined(DEBUG)
void debugPutc(const char c)
{
  if (uart3Mode == UART_MODE_DEBUG) {
    uart3Putc(c);
  }
}
#endif

void uart3SbusInit()
{
  uart3Setup(100000);
  USART3->CR1 |= USART_CR1_M | USART_CR1_PCE ;
}

void uart3Stop()
{
  USART_DeInit(USART3);
}

#if !defined(SIMU)
extern "C" void USART3_IRQHandler(void)
{
  // Send
  if (USART_GetITStatus(UART3, USART_IT_TXE) != RESET) {
    uint8_t txchar;
    if (uart3TxFifo.pop(txchar)) {
      /* Write one byte to the transmit data register */
      USART_SendData(UART3, txchar);
    }
    else {
      USART_ITConfig(UART3, USART_IT_TXE, DISABLE);
    }
  }

  // Receive
  uint32_t status = USART3->SR;
  while (status & (USART_FLAG_RXNE | USART_FLAG_ERRORS)) {
    uint8_t data = USART3->DR;

    if (!(status & USART_FLAG_ERRORS)) {
      switch (uart3Mode) {
        case UART_MODE_TELEMETRY:
          telemetryFifo.push(data);
          break;
        case UART_MODE_SBUS_TRAINER:
          sbusFifo.push(data);
          break;
      }
    }

    status = USART3->SR;
  }
}
#endif

