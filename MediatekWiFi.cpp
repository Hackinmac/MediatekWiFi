#include "MediatekWifi.h"

// Register this class with the kernel
OSDefineMetaClassAndStructors(MediatekWifi, IOPCIDevice);

#pragma mark - Lifecycle Methods

bool MediatekWifi::start(IOService *provider) {
    IOLog("[MediatekWifi] === START CALLED ===\n");
    
    // Step 1: Call parent's start - MUST DO THIS
    if (!IOPCIDevice::start(provider)) {
        IOLog("[MediatekWifi] ERROR: Parent start failed\n");
        return false;
    }
    IOLog("[MediatekWifi] Parent IOPCIDevice::start() successful\n");
    
    // Step 2: Enable PCI device (memory access, bus master)
    if (!enablePCI()) {
        IOLog("[MediatekWifi] ERROR: Failed to enable PCI\n");
        stop(provider);
        return false;
    }
    IOLog("[MediatekWifi] PCI enabled\n");
    
    // Step 3: Map device memory into our address space
    if (!mapDeviceMemory()) {
        IOLog("[MediatekWifi] ERROR: Failed to map device memory\n");
        stop(provider);
        return false;
    }
    IOLog("[MediatekWifi] Device memory mapped\n");
    
    // Step 4: Create work loop for event handling
    workLoop = IOWorkLoop::workLoop();
    if (!workLoop) {
        IOLog("[MediatekWifi] ERROR: Failed to create work loop\n");
        stop(provider);
        return false;
    }
    IOLog("[MediatekWifi] Work loop created\n");
    
    // Step 5: Read chip ID to verify device is working
    chipId = readRegisterL1(MT_HW_CHIPID);
    IOLog("[MediatekWifi] Chip ID: 0x%x\n", chipId);
    
    if (chipId == 0 || chipId == 0xFFFFFFFF) {
        IOLog("[MediatekWifi] ERROR: Invalid chip ID (0x%x) - device not responding\n", chipId);
        stop(provider);
        return false;
    }
    
    // Step 6: Setup interrupt handling
    if (!setupInterrupts()) {
        IOLog("[MediatekWifi] WARNING: Failed to setup interrupts (non-fatal, will try polling)\n");
        // Don't fail - we can poll instead
    } else {
        IOLog("[MediatekWifi] Interrupts configured\n");
    }
    
    // Step 7: Load firmware
    if (!loadFirmware()) {
        IOLog("[MediatekWifi] ERROR: Failed to load firmware\n");
        stop(provider);
        return false;
    }
    IOLog("[MediatekWifi] Firmware loaded\n");
    
    IOLog("[MediatekWifi] === DRIVER LOADED SUCCESSFULLY ===\n");
    return true;
}

void MediatekWifi::stop(IOService *provider) {
    IOLog("[MediatekWifi] === STOP CALLED ===\n");
    
    // Disable interrupts
    if (intSrc) {
        intSrc->disable();
        intSrc->release();
        intSrc = NULL;
    }
    
    // Release work loop
    if (workLoop) {
        workLoop->release();
        workLoop = NULL;
    }
    
    // Unmap device memory
    unmapDeviceMemory();
    
    // Disable PCI
    disablePCI();
    
    // Call parent's stop - MUST DO THIS
    IOPCIDevice::stop(provider);
    
    IOLog("[MediatekWifi] === DRIVER STOPPED ===\n");
}

void MediatekWifi::free() {
    IOLog("[MediatekWifi] Free called\n");
    IOPCIDevice::free();
}

#pragma mark - PCI Configuration

bool MediatekWifi::enablePCI() {
    uint16_t cmd = configRead16(kIOPCIConfigCommand);
    IOLog("[MediatekWifi] Original PCI command: 0x%04x\n", cmd);
    
    // Enable memory space access
    cmd |= kIOPCICommandMemorySpace;
    
    // Enable bus master (device can initiate DMA)
    cmd |= kIOPCICommandBusMaster;
    
    configWrite16(kIOPCIConfigCommand, cmd);
    
    uint16_t newCmd = configRead16(kIOPCIConfigCommand);
    IOLog("[MediatekWifi] New PCI command: 0x%04x\n", newCmd);
    
    return true;
}

bool MediatekWifi::disablePCI() {
    configWrite16(kIOPCIConfigCommand, 0);
    return true;
}

#pragma mark - Memory Mapping

bool MediatekWifi::mapDeviceMemory() {
    // Get BAR0 (Base Address Register 0) - contains MMIO address
    IOMemoryDescriptor *descriptor = getDeviceMemoryWithIndex(0);
    if (!descriptor) {
        IOLog("[MediatekWifi] ERROR: Could not get device memory\n");
        return false;
    }
    
    IOLog("[MediatekWifi] Device memory descriptor: length=%llu\n", descriptor->getLength());
    
    // Map it into kernel address space
    memoryMap = descriptor->map();
    if (!memoryMap) {
        IOLog("[MediatekWifi] ERROR: Failed to map memory\n");
        descriptor->release();
        return false;
    }
    
    // Get virtual address (address we can use in our code)
    hwBase = (volatile void *)memoryMap->getVirtualAddress();
    
    IOLog("[MediatekWifi] Device memory mapped:\n");
    IOLog("[MediatekWifi]   Physical: 0x%llx\n", memoryMap->getPhysicalAddress());
    IOLog("[MediatekWifi]   Virtual:  %p\n", hwBase);
    IOLog("[MediatekWifi]   Size:     0x%llx\n", memoryMap->getLength());
    
    return true;
}

bool MediatekWifi::unmapDeviceMemory() {
    if (memoryMap) {
        memoryMap->release();
        memoryMap = NULL;
        hwBase = NULL;
    }
    return true;
}

#pragma mark - Register Access

uint32_t MediatekWifi::readRegister(uint32_t offset) {
    if (!hwBase) {
        IOLog("[MediatekWifi] ERROR: hwBase is NULL\n");
        return 0xDEADBEEF;
    }
    
    volatile uint32_t *addr = (volatile uint32_t *)((uintptr_t)hwBase + offset);
    uint32_t value = *addr;
    
    IOLog("[MediatekWifi] Read  [0x%04x] = 0x%08x\n", offset, value);
    return value;
}

void MediatekWifi::writeRegister(uint32_t offset, uint32_t value) {
    if (!hwBase) {
        IOLog("[MediatekWifi] ERROR: hwBase is NULL\n");
        return;
    }
    
    volatile uint32_t *addr = (volatile uint32_t *)((uintptr_t)hwBase + offset);
    *addr = value;
    
    IOLog("[MediatekWifi] Write [0x%04x] = 0x%08x\n", offset, value);
}

// L1 register access - requires remapping (special Mediatek thing)
uint32_t MediatekWifi::readRegisterL1(uint32_t offset) {
    // Simplified: just do a regular read for now
    // In real driver, this would handle L1 remapping
    return readRegister(offset);
}

void MediatekWifi::writeRegisterL1(uint32_t offset, uint32_t value) {
    // Simplified: just do a regular write for now
    writeRegister(offset, value);
}

#pragma mark - Interrupt Handling

bool MediatekWifi::setupInterrupts() {
    if (!workLoop) {
        IOLog("[MediatekWifi] ERROR: No work loop\n");
        return false;
    }
    
    // Create interrupt event source
    intSrc = IOInterruptEventSource::interruptEventSource(
        this,
        OSMemberFunctionCast(IOInterruptEventSource::Action, 
                            this, 
                            &MediatekWifi::handleInterrupt),
        this,
        0  // First interrupt
    );
    
    if (!intSrc) {
        IOLog("[MediatekWifi] ERROR: Could not create interrupt source\n");
        return false;
    }
    
    // Add to work loop
    if (workLoop->addEventSource(intSrc) != kIOReturnSuccess) {
        IOLog("[MediatekWifi] ERROR: Could not add interrupt to work loop\n");
        intSrc->release();
        intSrc = NULL;
        return false;
    }
    
    // Enable interrupts
    intSrc->enable();
    
    IOLog("[MediatekWifi] Interrupts enabled\n");
    return true;
}

void MediatekWifi::handleInterrupt(IOInterruptEventSource *source, int count) {
    IOLog("[MediatekWifi] Interrupt! (count=%d)\n", count);
    
    // Read interrupt status register
    uint32_t intStatus = readRegister(MT_WFDMA0_HOST_INT_ENA);
    IOLog("[MediatekWifi] Interrupt status: 0x%08x\n", intStatus);
    
    // In a real driver, we'd:
    // 1. Check which queues have data
    // 2. Process RX packets
    // 3. Handle TX completion
    // 4. Clear interrupt flags
}

#pragma mark - Firmware Loading

bool MediatekWifi::loadFirmware() {
    IOLog("[MediatekWifi] === Loading Firmware ===\n");
    
    // In a real implementation:
    // 1. Find firmware file in bundle
    // 2. Allocate DMA buffer
    // 3. Copy firmware to buffer
    // 4. Send MCU command to load
    // 5. Wait for completion
    
    // For now, just log
    IOLog("[MediatekWifi] Firmware loading (stubbed)\n");
    
    return true;
}

#pragma mark - Device Detection

IOService *MediatekWifi::probe(IOService *provider, SInt32 *score) {
    IOLog("[MediatekWifi] Probe called\n");
    
    IOPCIDevice *pciDev = OSDynamicCast(IOPCIDevice, provider);
    if (!pciDev) {
        return NULL;
    }
    
    uint16_t vendorId = pciDev->configRead16(kIOPCIConfigVendorID);
    uint16_t deviceId = pciDev->configRead16(kIOPCIConfigDeviceID);
    
    IOLog("[MediatekWifi] Found device: Vendor=0x%04x Device=0x%04x\n", 
          vendorId, deviceId);
    
    // Return self if this is a Mediatek device
    if (vendorId == MEDIATEK_VENDOR_ID) {
        *score = 1000;  // High priority
        return this;
    }
    
    return NULL;
}
