# BurstSerial
Mbed OS Serial(UART) DMA Driver for STM32

This driver is created for high-speed serial communication(1Mbit/s and up).

By default USART1 is enabled for G03x/G07x/G0B1/G4/F1/F2/F3/F4/F7/L0/L4/L4+/L5/U5/H7 MCUs, other U(SART) peripherals can be enabled by adding the SPITxDMALinks entries and UART/DMA interrupt handlers

RS485 flow control is supported with any GPIO as the Drive Enable pin.

Note: This library uses relatively newer HAL APIs such as HAL_UARTEx_ReceiveToIdle_DMA and HAL_UARTEx_GetRxEventType. 
      A manual update of STM32Cube firmware package is required for some models.

API Usage: 

```C++
void serial_callback(bool is_idle)
{
    static char buffer[BurstSerial::user_buffer_size + 1];
    static uint32_t rx_pos = 0;
    // pop received data from ring buffer
    uint32_t size = serial.user_buffer.pop((uint8_t*)(buffer + rx_pos), 
        BurstSerial::user_buffer_size - rx_pos);
    rx_pos += size;
    // check for end of message
    if(rx_pos > 0 && buffer[rx_pos - 1] == '\n')
    {
        serial.dma_write((uint8_t*)buffer, rx_pos);
        rx_pos = 0;
    }
}

// construct with TX, RX and baudrate
BurstSerial serial(PA_9, PA_10, 921600);
// lock deep sleep because DMA won't work in stop mode
sleep_manager_lock_deep_sleep();
// set RX callback function
serial.set_rx_callback(&serial_callback);
// initialize DMA and start receiving
serial.dma_init();
```
