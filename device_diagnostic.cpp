#include <iostream>
#include "include/JY8902.h"
#include "include/JY5320Core.h"
#include "include/JY5710.h"

int main()
{
    std::cout << "PCB Fault Detection Device Diagnostic Tool" << std::endl;
    std::cout << "===========================================" << std::endl;
    
    // Test JY8902 DMM Device
    std::cout << "\n1. Testing JY8902 DMM Device..." << std::endl;
    JY8902_DeviceHandle dmmHandle = nullptr;
    int32_t result = JY8902_Open(0, &dmmHandle);
    if (result == 0) {
        std::cout << "   ✓ JY8902 DMM: Connected successfully" << std::endl;
        JY8902_Close(dmmHandle);
    } else {
        std::cout << "   ✗ JY8902 DMM: Failed to connect (Error: " << result << ")" << std::endl;
        
        // Provide specific error information
        switch (result) {
            case -10001:
                std::cout << "     → Device not found or already in use" << std::endl;
                break;
            case -10004:
                std::cout << "     → Device initialization failed - check hardware" << std::endl;
                break;
            case -10005:
                std::cout << "     → Device activation failed - check drivers" << std::endl;
                break;
            case -10006:
                std::cout << "     → Invalid device slot number" << std::endl;
                break;
            default:
                std::cout << "     → Unknown error - check device documentation" << std::endl;
                break;
        }
    }
    
    // Test JY5711 AO Device
    std::cout << "\n2. Testing JY5711 AO Device..." << std::endl;
    JY5710_DeviceHandle aoHandle = nullptr;
    result = JY5710_Open(0, &aoHandle);
    if (result == 0) {
        std::cout << "   ✓ JY5711 AO: Connected successfully" << std::endl;
        JY5710_Close(aoHandle);
    } else {
        std::cout << "   ✗ JY5711 AO: Failed to connect (Error: " << result << ")" << std::endl;
    }
    
    // Test JY5322 DAQ Device (slot 5)
    std::cout << "\n3. Testing JY5322 DAQ Device (slot 5)..." << std::endl;
    JY5320_DeviceHandle daqHandle5322 = nullptr;
    result = JY5320_Open(5, &daqHandle5322);
    if (result == 0) {
        std::cout << "   ✓ JY5322 DAQ (slot 5): Connected successfully" << std::endl;
        JY5320_Close(daqHandle5322);
    } else {
        std::cout << "   ✗ JY5322 DAQ (slot 5): Failed to connect (Error: " << result << ")" << std::endl;
    }
    
    // Test JY5323 DAQ Device (slot 3)
    std::cout << "\n4. Testing JY5323 DAQ Device (slot 3)..." << std::endl;
    JY5320_DeviceHandle daqHandle5323 = nullptr;
    result = JY5320_Open(3, &daqHandle5323);
    if (result == 0) {
        std::cout << "   ✓ JY5323 DAQ (slot 3): Connected successfully" << std::endl;
        JY5320_Close(daqHandle5323);
    } else {
        std::cout << "   ✗ JY5323 DAQ (slot 3): Failed to connect (Error: " << result << ")" << std::endl;
    }
    
    std::cout << "\n===========================================" << std::endl;
    std::cout << "Diagnostic complete. Press Enter to exit..." << std::endl;
    std::cin.get();
    
    return 0;
}
