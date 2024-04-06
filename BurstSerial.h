#pragma once

// Serial(UART) DMA Driver for STM32
// By default USART1 is enabled for G03x/G07x/G0B1/G4/F1/F2/F3/F4/F7/L0/L4/L4+/L5/U5/H7 MCUs
// Other U(SART) peripherals can be enabled by adding the SPITxDMALinks entries and UART/DMA interrupt handlers
// RS485 flow control is supported with any GPIO as the Drive Enable pin
// Note 1: this library uses relatively newer HAL APIs such as HAL_UARTEx_ReceiveToIdle_DMA and HAL_UARTEx_GetRxEventType
//       a manual update of STM32Cube firmware package is required for most models
// Note 2: LPUART peripherals on some low power models may run into overrun(ORE) errors

#include "mbed.h"
#include <memory>

#define USE_RX_EVENT_TYPE 0

class BurstSerial : 
    public SerialBase
{
public:
    constexpr static uint32_t rx_buffer_size = 128;
    constexpr static uint32_t user_buffer_size = 256;

    CircularBuffer<uint8_t, user_buffer_size> user_buffer;

    BurstSerial(PinName tx, PinName rx, int bauds = 115200, PinName direction = PinName::NC);
    
    ~BurstSerial();

    // initialize DMA, return true on success
    bool dma_init();
    
    // deinitialize DMA, automatically called in destructor
    void dma_uninit();

    // send data using DMA transfer
    bool write(const uint8_t* data, uint16_t size);
    bool write(const char *data);

    // set callback function when new data are received
    // the callback function will be called inside mbed_event_queue()
    // caution: the is_idle param may be unreliable with high baud rates(4Mbps or higher)
    // note: if mbed_event_queue()->->dispatch_forever() is called in the main() function, 
    //       configuration option "events.shared-dispatch-from-application" must be set to true
    void set_rx_callback(mbed::Callback<void(bool/*is_idle*/)> callback_)
    {
        rx_callback = callback_;
    }

    // set callback function when data transmition is complete
    // the callback function will be called inside mbed_event_queue()
    // note: if mbed_event_queue()->->dispatch_forever() is called in the main() function, 
    //       configuration option "events.shared-dispatch-from-application" must be set to true
    void set_tx_callback(mbed::Callback<void()> callback_)
    {
        tx_callback = callback_;
    }

private:
    alignas(32U)  uint8_t rx_buffer[rx_buffer_size];
    UART_HandleTypeDef * uart_handle;
    DMA_HandleTypeDef hdma_rx = { 0 };
    DMA_HandleTypeDef hdma_tx = { 0 };
    #if defined(STM32U5)
    DMA_NodeTypeDef dma_rx_node;
    DMA_QListTypeDef dma_rx_list;
    #endif
    uint16_t dma_initialized = 0;
    uint16_t rx_buffer_index = 0;
    mbed::Callback<void(bool)> rx_callback;
    mbed::Callback<void()> tx_callback;
    std::unique_ptr<DigitalOut> p_direction;
    EventQueue *shared_queue;

public:
    void on_uart_received(uint16_t size, bool is_idle);

    void on_uart_tx_complete();

    void on_dma_interrupt_tx();

    void on_dma_interrupt_rx();

    void on_uart_interrupt();
};
