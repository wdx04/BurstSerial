#include "BurstSerial.h"

// Support up to 10 U(S)ARTs and 2 LPUARTs
#define MAX_SERIAL_COUNT 12

struct SerialDMAInfo
{
    int index; // index in SPITxDMALinks array, U(S)ART starts at 0, LPUARTs starts at 10
    UARTName name;
    void *dmaInstance;
    void *txInstance;
    uint32_t txRequest;
    IRQn_Type txIrqn;
    void *rxInstance;
    uint32_t rxRequest;
    IRQn_Type rxIrqn;
    IRQn_Type uartIrqn;
};

extern UART_HandleTypeDef uart_handlers[];
extern "C" int8_t get_uart_index(UARTName uart_name);

#if defined(STM32G030xx) || defined(STM32G031xx)
static const SerialDMAInfo SPITxDMALinks[] = {
        { 0, UART_1, DMA1, DMA1_Channel5, DMA_REQUEST_USART1_TX, DMA1_Ch4_5_DMAMUX1_OVR_IRQn, 
            DMA1_Channel4, DMA_REQUEST_USART1_RX, DMA1_Ch4_5_DMAMUX1_OVR_IRQn, USART1_IRQn }
};
#endif
#if defined(STM32G070xx) || defined(STM32G071xx)
static const SerialDMAInfo SPITxDMALinks[] = {
        { 0, UART_1, DMA1, DMA1_Channel5, DMA_REQUEST_USART1_TX, DMA1_Ch4_7_DMAMUX1_OVR_IRQn, 
            DMA1_Channel4, DMA_REQUEST_USART1_RX, DMA1_Ch4_7_DMAMUX1_OVR_IRQn, USART1_IRQn }
};
#endif
#if defined(STM32L0)
static const SerialDMAInfo SPITxDMALinks[] = {
        { 0, UART_1, DMA1, DMA1_Channel4, DMA_REQUEST_3, DMA1_Channel4_5_6_7_IRQn, 
            DMA1_Channel5, DMA_REQUEST_3, DMA1_Channel4_5_6_7_IRQn, USART1_IRQn }
};
#endif
#if defined(STM32G0B1xx)
static const SerialDMAInfo SPITxDMALinks[] = {
        { 0, UART_1, DMA1, DMA1_Channel5, DMA_REQUEST_USART1_TX, DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQn, 
            DMA1_Channel4, DMA_REQUEST_USART1_RX, DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQn, USART1_IRQn },
        { 10, LPUART_1, DMA2, DMA2_Channel5, DMA_REQUEST_LPUART1_TX, DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQn, 
            DMA2_Channel4, DMA_REQUEST_LPUART1_RX, DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQn, USART3_4_5_6_LPUART1_IRQn }
};
#endif
#if defined(STM32F1) || defined(STM32F3)
static const SerialDMAInfo SPITxDMALinks[] = {
        { 0, UART_1, DMA1, DMA1_Channel4, 0, DMA1_Channel4_IRQn, 
            DMA1_Channel5, 0, DMA1_Channel5_IRQn, USART1_IRQn },
        { 2, UART_3, DMA1, DMA1_Channel2, 0, DMA1_Channel2_IRQn, 
            DMA1_Channel3, 0, DMA1_Channel3_IRQn, USART3_IRQn }
};
#endif
#if defined(TARGET_STM32F2) || defined(TARGET_STM32F4) || defined(TARGET_STM32F7)
static const SerialDMAInfo SPITxDMALinks[] = {
        { 0, UART_1, DMA2, DMA2_Stream7, DMA_CHANNEL_4, DMA2_Stream7_IRQn, 
            DMA2_Stream5, DMA_CHANNEL_4, DMA2_Stream5_IRQn, USART1_IRQn },
        { 2, UART_3, DMA1, DMA1_Stream3, DMA_CHANNEL_4, DMA1_Stream3_IRQn, 
            DMA1_Stream1, DMA_CHANNEL_4, DMA1_Stream1_IRQn, USART3_IRQn }
};
#endif
#if defined(STM32L4) && !defined(DMAMUX1)
static const SerialDMAInfo SPITxDMALinks[] = {
        { 0, UART_1, DMA1, DMA1_Channel4, DMA_REQUEST_2, DMA1_Channel4_IRQn, 
            DMA1_Channel5, DMA_REQUEST_2, DMA1_Channel5_IRQn, USART1_IRQn }
};
#endif
#if defined(STM32G4)
static const SerialDMAInfo SPITxDMALinks[] = {
        { 0, UART_1, DMA1, DMA1_Channel4, DMA_REQUEST_USART1_TX, DMA1_Channel4_IRQn, 
            DMA1_Channel5, DMA_REQUEST_USART1_RX, DMA1_Channel5_IRQn, USART1_IRQn }
};
#endif
#if defined(STM32L5) || (defined(STM32L4) && defined(DMAMUX1))
static const SerialDMAInfo SPITxDMALinks[] = {
        { 0, UART_1, DMA1, DMA1_Channel4, DMA_REQUEST_USART1_TX, DMA1_Channel4_IRQn, 
            DMA1_Channel5, DMA_REQUEST_USART1_RX, DMA1_Channel5_IRQn, USART1_IRQn }
};
#endif
#if defined(STM32U5)
static const SerialDMAInfo SPITxDMALinks[] = {
        { 0, UART_1, GPDMA1, GPDMA1_Channel11, GPDMA1_REQUEST_USART1_TX, GPDMA1_Channel11_IRQn, 
            GPDMA1_Channel10, GPDMA1_REQUEST_USART1_RX, GPDMA1_Channel10_IRQn, USART1_IRQn }
};
#endif
#ifdef STM32H7
static const SerialDMAInfo SPITxDMALinks[] = {
        { 0, UART_1, DMA2, DMA2_Stream7, DMA_REQUEST_USART1_TX, DMA2_Stream7_IRQn, 
            DMA2_Stream5, DMA_REQUEST_USART1_RX, DMA2_Stream5_IRQn, USART1_IRQn }
};
#endif
static BurstSerial *BurstSerialInstances[MAX_SERIAL_COUNT];

BurstSerial::BurstSerial(PinName tx, PinName rx, int bauds, PinName direction)
    : SerialBase(tx, rx, bauds), shared_queue(mbed_event_queue())
{
    if(direction != PinName::NC)
    {
        p_direction = std::make_unique<DigitalOut>(direction);
        p_direction->write(0);
    }
}

BurstSerial::~BurstSerial()
{
    dma_uninit();
}

bool BurstSerial::dma_init()
{
    if(dma_initialized)
    {
        return true;
    }
    #if defined(STM32F3) || defined(STM32H7)
    UARTName uart_name = _serial.uart;
    #else
    UARTName uart_name = _serial.serial.uart;
    #endif
    int uart_index = get_uart_index(uart_name);
    uart_handle = &uart_handlers[uart_index];
    for(const SerialDMAInfo &info: SPITxDMALinks)
    {
        if(info.name == uart_name)
        {
            #if defined(__HAL_RCC_DMAMUX1_CLK_ENABLE)
            __HAL_RCC_DMAMUX1_CLK_ENABLE();
            #endif            
            #ifdef DMA1
            if(info.dmaInstance == DMA1) __HAL_RCC_DMA1_CLK_ENABLE();
            #endif
            #ifdef DMA2
            if(info.dmaInstance == DMA2) __HAL_RCC_DMA2_CLK_ENABLE();
            #endif
            #ifdef GPDMA1
            if(info.dmaInstance == GPDMA1) __HAL_RCC_GPDMA1_CLK_ENABLE();
            #endif
            #if defined(STM32G0) || defined(STM32L0) || defined(STM32L4) || defined(STM32G4) || defined(STM32L5) || defined(STM32U5)
            hdma_tx.Instance = reinterpret_cast<DMA_Channel_TypeDef *>(info.txInstance);
            hdma_tx.Init.Request = info.txRequest;
            #endif
            #if defined(STM32H7)
            hdma_tx.Instance = reinterpret_cast<DMA_Stream_TypeDef *>(info.txInstance);
            hdma_tx.Init.Request = info.txRequest;
            #endif
            #if defined(STM32F1) || defined(STM32F3)
            hdma_tx.Instance = reinterpret_cast<DMA_Channel_TypeDef *>(info.txInstance);
            #endif
            #if defined(TARGET_STM32F2) || defined(TARGET_STM32F4) || defined(TARGET_STM32F7)
            hdma_tx.Instance = reinterpret_cast<DMA_Stream_TypeDef *>(info.txInstance);
            hdma_tx.Init.Channel = info.txRequest;
            #endif
            hdma_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
            hdma_tx.Init.Mode = DMA_NORMAL;
            #if !defined(TARGET_STM32U5)
            hdma_tx.Init.PeriphInc = DMA_PINC_DISABLE;
            hdma_tx.Init.MemInc = DMA_MINC_ENABLE;
            hdma_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
            hdma_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
            hdma_tx.Init.Priority = DMA_PRIORITY_LOW;
            #else
            hdma_tx.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
            hdma_tx.Init.SrcInc = DMA_SINC_INCREMENTED;
            hdma_tx.Init.DestInc = DMA_DINC_FIXED;
            hdma_tx.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_BYTE;
            hdma_tx.Init.DestDataWidth = DMA_DEST_DATAWIDTH_BYTE;
            hdma_tx.Init.Priority = DMA_LOW_PRIORITY_LOW_WEIGHT;
            hdma_tx.Init.SrcBurstLength = 1;
            hdma_tx.Init.DestBurstLength = 1;
            hdma_tx.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0|DMA_DEST_ALLOCATED_PORT0;
            hdma_tx.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
            #endif

            if (HAL_DMA_Init(&hdma_tx) != HAL_OK)
            {
                return false;
            }
            __HAL_LINKDMA(uart_handle, hdmatx, hdma_tx);

            #if defined(STM32G0) || defined(STM32L0) || defined(STM32L4) || defined(STM32G4) || defined(STM32L5)
            hdma_rx.Instance = reinterpret_cast<DMA_Channel_TypeDef *>(info.rxInstance);
            hdma_rx.Init.Request = info.rxRequest;
            #endif
            #if defined(STM32F1) || defined(STM32F3)
            hdma_rx.Instance = reinterpret_cast<DMA_Channel_TypeDef *>(info.rxInstance);
            #endif
            #if defined(STM32H7)
            hdma_rx.Instance = reinterpret_cast<DMA_Stream_TypeDef *>(info.rxInstance);
            hdma_rx.Init.Request = info.rxRequest;
            #endif            
            #if defined(TARGET_STM32F2) || defined(TARGET_STM32F4) || defined(TARGET_STM32F7)
            hdma_rx.Instance = reinterpret_cast<DMA_Stream_TypeDef *>(info.rxInstance);
            hdma_rx.Init.Channel = info.rxRequest;
            #endif
            #if !defined(TARGET_STM32U5)
            hdma_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
            hdma_rx.Init.PeriphInc = DMA_PINC_DISABLE;
            hdma_rx.Init.MemInc = DMA_MINC_ENABLE;
            hdma_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
            hdma_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
            hdma_rx.Init.Mode = DMA_CIRCULAR;
            hdma_rx.Init.Priority = DMA_PRIORITY_HIGH;
            if (HAL_DMA_Init(&hdma_rx) != HAL_OK)
            {
                return false;
            }
            __HAL_LINKDMA(uart_handle, hdmarx, hdma_rx);
            #else
            DMA_NodeConfTypeDef NodeConfig;
            NodeConfig.NodeType = DMA_GPDMA_LINEAR_NODE;
            NodeConfig.Init.Request = info.rxRequest;
            NodeConfig.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
            NodeConfig.Init.Direction = DMA_PERIPH_TO_MEMORY;
            NodeConfig.Init.SrcInc = DMA_SINC_FIXED;
            NodeConfig.Init.DestInc = DMA_DINC_INCREMENTED;
            NodeConfig.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_BYTE;
            NodeConfig.Init.DestDataWidth = DMA_DEST_DATAWIDTH_BYTE;
            NodeConfig.Init.SrcBurstLength = 1;
            NodeConfig.Init.DestBurstLength = 1;
            NodeConfig.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0|DMA_DEST_ALLOCATED_PORT0;
            NodeConfig.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
            NodeConfig.Init.Mode = DMA_NORMAL;
            NodeConfig.TriggerConfig.TriggerPolarity = DMA_TRIG_POLARITY_MASKED;
            NodeConfig.DataHandlingConfig.DataExchange = DMA_EXCHANGE_NONE;
            NodeConfig.DataHandlingConfig.DataAlignment = DMA_DATA_RIGHTALIGN_ZEROPADDED;
            if (HAL_DMAEx_List_BuildNode(&NodeConfig, &dma_rx_node) != HAL_OK)
            {
                return false;
            }
            if (HAL_DMAEx_List_InsertNode(&dma_rx_list, NULL, &dma_rx_node) != HAL_OK)
            {
                return false;
            }
            if (HAL_DMAEx_List_SetCircularMode(&dma_rx_list) != HAL_OK)
            {
                return false;
            }
            hdma_rx.Instance = reinterpret_cast<DMA_Channel_TypeDef *>(info.rxInstance);
            hdma_rx.InitLinkedList.Priority = DMA_LOW_PRIORITY_HIGH_WEIGHT;
            hdma_rx.InitLinkedList.LinkStepMode = DMA_LSM_FULL_EXECUTION;
            hdma_rx.InitLinkedList.LinkAllocatedPort = DMA_LINK_ALLOCATED_PORT0;
            hdma_rx.InitLinkedList.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
            hdma_rx.InitLinkedList.LinkedListMode = DMA_LINKEDLIST_CIRCULAR;
            if (HAL_DMAEx_List_Init(&hdma_rx) != HAL_OK)
            {
                return false;
            }
            if (HAL_DMAEx_List_LinkQ(&hdma_rx, &dma_rx_list) != HAL_OK)
            {
                return false;
            }
            __HAL_LINKDMA(uart_handle, hdmarx, hdma_rx);
            if (HAL_DMA_ConfigChannelAttributes(&hdma_rx, DMA_CHANNEL_NPRIV) != HAL_OK)
            {
                return false;
            }
            #endif

            /* DMA interrupt configuration */
            HAL_NVIC_SetPriority(info.rxIrqn, 0, 0);
            HAL_NVIC_EnableIRQ(info.rxIrqn);            
            if(info.rxIrqn != info.txIrqn)
            {
                HAL_NVIC_SetPriority(info.txIrqn, 5, 0);
                HAL_NVIC_EnableIRQ(info.txIrqn);            
            }
            HAL_NVIC_SetPriority(info.uartIrqn, 0, 1);
            HAL_NVIC_EnableIRQ(info.uartIrqn);
            BurstSerialInstances[info.index] = this;
            dma_initialized = HAL_UARTEx_ReceiveToIdle_DMA(uart_handle, &rx_buffer[0], rx_buffer_size) == HAL_OK;
            return dma_initialized;
        }
    }
    return false;
}

void BurstSerial::dma_uninit()
{
    if(dma_initialized)
    {
        #if defined(STM32F3) || defined(STM32H7)
        UARTName uart_name = _serial.uart;
        #else
        UARTName uart_name = _serial.serial.uart;
        #endif
        for(const SerialDMAInfo &info: SPITxDMALinks)
        {
            if(info.name == uart_name)
            {
                HAL_NVIC_DisableIRQ(info.rxIrqn);            
                if(info.rxIrqn != info.txIrqn)
                {
                    HAL_NVIC_DisableIRQ(info.txIrqn);            
                }
                HAL_NVIC_DisableIRQ(info.uartIrqn);
                HAL_DMA_DeInit(&hdma_rx);
                HAL_DMA_DeInit(&hdma_tx);
                BurstSerialInstances[info.index] = nullptr;
                break;
            }
        }
        dma_initialized = false;
        return;
    }
}

bool BurstSerial::write(const uint8_t* data, uint16_t size)
{
    #if defined (__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
        uint32_t alignedAddr = (uint32_t)data &  ~0x1F;
        SCB_CleanDCache_by_Addr((uint32_t*)alignedAddr, size + ((uint32_t)data - alignedAddr));
    #endif
    if(p_direction)
    {
        p_direction->write(1);
    }
    return HAL_UART_Transmit_DMA(uart_handle, (uint8_t*)data, size) == HAL_OK;
}

bool BurstSerial::write(const char *data)
{
    int data_len = strlen(data);
    return write((const uint8_t*)data, (uint16_t)data_len);
}

void BurstSerial::on_uart_received(uint16_t size, bool is_idle)
{
    if (size != rx_buffer_index)
    {
#if defined (__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
        SCB_InvalidateDCache_by_Addr((uint32_t*)rx_buffer, rx_buffer_size);
#endif
        if (size > rx_buffer_index)
        {
            uint16_t received_chars = size - rx_buffer_index;
            user_buffer.push(&rx_buffer[rx_buffer_index],  received_chars);
        }
        else
        {
            uint16_t received_chars = rx_buffer_size - rx_buffer_index;
            if(received_chars > 0)
            {
                user_buffer.push(&rx_buffer[rx_buffer_index],  received_chars);
            }
            if (size > 0)
            {
                user_buffer.push(&rx_buffer[0],  size);
            }
        }
        rx_buffer_index = size;
        if(rx_callback)
        {
            shared_queue->call(rx_callback, is_idle);
        }
    }
}

void BurstSerial::on_uart_tx_complete()
{
    if(p_direction)
    {
        p_direction->write(0);
    }
    if(tx_callback)
    {
        shared_queue->call(tx_callback);
    }
}

void BurstSerial::on_dma_interrupt_tx()
{
    HAL_DMA_IRQHandler(&hdma_tx);
}

void BurstSerial::on_dma_interrupt_rx()
{
    HAL_DMA_IRQHandler(&hdma_rx);
}

void BurstSerial::on_uart_interrupt()
{
    HAL_UART_IRQHandler(uart_handle);
}

extern "C" void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
#if USE_RX_EVENT_TYPE
    bool is_idle = HAL_UARTEx_GetRxEventType(huart) == HAL_UART_RXEVENT_IDLE;
#else
    bool is_idle = true;
#endif
    if(huart->Instance == USART1)
    {
        BurstSerialInstances[0]->on_uart_received(size, is_idle);
        return;
    }
#ifdef USART2
    if(huart->Instance == USART2)
    {
        BurstSerialInstances[1]->on_uart_received(size, is_idle);
        return;
    }
#endif
#ifdef USART3
    if(huart->Instance == USART3)
    {
        BurstSerialInstances[2]->on_uart_received(size, is_idle);
        return;
    }
#endif
#ifdef LPUART1
    if(huart->Instance == LPUART1)
    {
        BurstSerialInstances[10]->on_uart_received(size, is_idle);
        return;
    }
#endif
}

extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART1)
    {
        BurstSerialInstances[0]->on_uart_tx_complete();
        return;
    }
#ifdef USART2
    if(huart->Instance == USART2)
    {
        BurstSerialInstances[1]->on_uart_tx_complete();
        return;
    }
#endif
#ifdef USART3
    if(huart->Instance == USART3)
    {
        BurstSerialInstances[2]->on_uart_tx_complete();
        return;
    }
#endif
#ifdef LPUART1
    if(huart->Instance == LPUART1)
    {
        BurstSerialInstances[10]->on_uart_tx_complete();
        return;
    }
#endif
}

#if defined(STM32G030xx) || defined(STM32G031xx)
extern "C" void DMA1_Ch4_5_DMAMUX1_OVR_IRQHandler(void)
{
    BurstSerialInstances[0]->on_dma_interrupt_rx();
    BurstSerialInstances[0]->on_dma_interrupt_tx();
}
#endif

#if defined(STM32G070xx) || defined(STM32G071xx)
extern "C" void DMA1_Ch4_7_DMAMUX1_OVR_IRQHandler(void)
{
    BurstSerialInstances[0]->on_dma_interrupt_rx();
    BurstSerialInstances[0]->on_dma_interrupt_tx();
}
#endif

#if defined(STM32G0B1xx)
extern "C" void DMA1_Ch4_7_DMA2_Ch1_5_DMAMUX1_OVR_IRQHandler(void)
{
    if(BurstSerialInstances[0] != nullptr)
    {
        BurstSerialInstances[0]->on_dma_interrupt_rx();
        BurstSerialInstances[0]->on_dma_interrupt_tx();
    }
    if(BurstSerialInstances[10] != nullptr)
    {
        BurstSerialInstances[10]->on_dma_interrupt_rx();
        BurstSerialInstances[10]->on_dma_interrupt_tx();
    }
}
#endif

#if defined(STM32L0)
extern "C" void DMA1_Channel4_5_6_7_IRQHandler(void)
{
    BurstSerialInstances[0]->on_dma_interrupt_rx();
    BurstSerialInstances[0]->on_dma_interrupt_tx();
}
#endif

#if defined(STM32F1) || defined(STM32F3) || defined(STM32L4) && !defined(DMAMUX1) || defined(STM32G4)
extern "C" void DMA1_Channel2_IRQHandler(void)
{
    BurstSerialInstances[2]->on_dma_interrupt_tx();
}

extern "C" void DMA1_Channel3_IRQHandler(void)
{
    BurstSerialInstances[2]->on_dma_interrupt_rx();
}

extern "C" void DMA1_Channel4_IRQHandler(void)
{
    BurstSerialInstances[0]->on_dma_interrupt_tx();
}

extern "C" void DMA1_Channel5_IRQHandler(void)
{
    BurstSerialInstances[0]->on_dma_interrupt_rx();
}
#endif

#if defined(STM32L5) || (defined(STM32L4) && defined(DMAMUX1))
extern "C" void DMA1_Channel4_IRQHandler(void)
{
    BurstSerialInstances[0]->on_dma_interrupt_tx();
}

extern "C" void DMA1_Channel5_IRQHandler(void)
{
    BurstSerialInstances[0]->on_dma_interrupt_rx();
}
#endif

#if defined(STM32U5)
extern "C" void GPDMA1_Channel11_IRQHandler(void)
{
    BurstSerialInstances[0]->on_dma_interrupt_tx();
}

extern "C" void GPDMA1_Channel10_IRQHandler(void)
{
    BurstSerialInstances[0]->on_dma_interrupt_rx();
}
#endif

#if defined(TARGET_STM32F2) || defined(TARGET_STM32F4) || defined(TARGET_STM32F7) || defined(STM32H7)
extern "C" void DMA2_Stream7_IRQHandler(void)
{
    BurstSerialInstances[0]->on_dma_interrupt_tx();
}

extern "C" void DMA2_Stream5_IRQHandler(void)
{
    BurstSerialInstances[0]->on_dma_interrupt_rx();
}
#endif

#if defined(TARGET_STM32F4)
extern "C" void DMA1_Stream3_IRQHandler(void)
{
    BurstSerialInstances[2]->on_dma_interrupt_tx();
}

extern "C" void DMA1_Stream1_IRQHandler(void)
{
    BurstSerialInstances[2]->on_dma_interrupt_rx();
}
#endif

extern "C" void USART1_IRQHandler(void)
{
    BurstSerialInstances[0]->on_uart_interrupt();
}

#ifdef USART2
extern "C" void USART2_IRQHandler(void)
{
    BurstSerialInstances[1]->on_uart_interrupt();
}
#endif

#ifdef USART3
extern "C" void USART3_IRQHandler(void)
{
    BurstSerialInstances[2]->on_uart_interrupt();
}
#endif

#if defined(STM32G0B1xx)
extern "C" void USART3_4_5_6_LPUART1_IRQHandler(void)
{
    if(BurstSerialInstances[10] != nullptr)
    {
        BurstSerialInstances[10]->on_uart_interrupt();
    }
}
#endif
