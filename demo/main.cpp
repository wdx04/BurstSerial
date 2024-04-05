#include "mbed.h"
#include <cstdint>
#include <vector>
#include "BurstSerial.h"

#define USE_USBSERIAL 0
#define BURSTSERIAL_ECHO 1

EventQueue *event_queue;
BurstSerial serial(PA_9, PA_10, 921600);

#if USE_USBSERIAL
#include "USBSerial.h"
USBSerial pc(true);
#define PC_PRINTF pc.printf
#define PC_PUTS pc.puts
#else
#define PC_PRINTF printf
#define PC_PUTS puts
#endif

DigitalOut led(LED1);

void flip_led()
{
    led = !led;
}

void serial_callback(bool is_idle)
{
    static char buffer[BurstSerial::user_buffer_size + 1];
#if USE_RX_EVENT_TYPE
    if(is_idle)
    {
        uint32_t size = serial.user_buffer.pop((uint8_t*)buffer, BurstSerial::user_buffer_size);
#if BURSTSERIAL_ECHO
        serial.dma_write((uint8_t*)buffer, size);
#else
        buffer[size] = 0;
        PC_PUTS(buffer);
#endif       
    }
#else
    static uint32_t rx_pos = 0;
    uint32_t size = serial.user_buffer.pop((uint8_t*)(buffer + rx_pos), BurstSerial::user_buffer_size - rx_pos);
    rx_pos += size;
    if(rx_pos > 0 && buffer[rx_pos - 1] == '\n')
    {
        #if BURSTSERIAL_ECHO
        serial.dma_write((uint8_t*)buffer, rx_pos);
        #else
        buffer[rx_pos] = 0;
        PC_PUTS(buffer);
        #endif
        rx_pos = 0;
    }
#endif
}

int main()
{
    sleep_manager_lock_deep_sleep();

    PC_PUTS("Application started!");

    event_queue = mbed_event_queue();

    // Initialize LEDs
    led = 0;

    serial.set_rx_callback(&serial_callback);
    if(serial.dma_init())
    {
        PC_PUTS("DMA Serial init OK.");
    }
    else
    {
        PC_PUTS("DMA Serial init Failed.");
    }

    event_queue->call_every(1s, &flip_led);
    event_queue->dispatch_forever();
}
