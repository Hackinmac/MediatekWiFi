#ifndef MEDIATEK_WIFI_H
#define MEDIATEK_WIFI_H

#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOInterruptEventSource.h>

// Mediatek vendor ID
#define MEDIATEK_VENDOR_ID 0x14

// Register offsets
#define MT_HW_CHIPID 0x0000
#define MT_WFDMA0_HOST_INT_ENA 0x1234

class MediatekWifi : public IOPCIDevice {
    OSDeclareDefaultStructors(MediatekWifi)
    
public:
    // Lifecycle methods
    virtual bool start(IOService *provider) override;
    virtual void stop(IOService *provider) override;
    virtual void free() override;
    virtual IOService *probe(IOService *provider, SInt32 *score) override;
    
private:
    // PCI configuration
    bool enablePCI();
    bool disablePCI();
    
    // Memory mapping
    bool mapDeviceMemory();
    bool unmapDeviceMemory();
    
    // Register access
    uint32_t readRegister(uint32_t offset);
    void writeRegister(uint32_t offset, uint32_t value);
    uint32_t readRegisterL1(uint32_t offset);
    void writeRegisterL1(uint32_t offset, uint32_t value);
    
    // Interrupt handling
    bool setupInterrupts();
    void handleInterrupt(IOInterruptEventSource *source, int count);
    
    // Firmware loading
    bool loadFirmware();
    
    // Member variables
    volatile void *hwBase;
    IOMemoryMap *memoryMap;
    IOWorkLoop *workLoop;
    IOInterruptEventSource *intSrc;
    uint32_t chipId;
};

#endif
