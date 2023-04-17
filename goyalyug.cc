#include "ide.h"
#include "ext2.h"
#include "libk.h"
#include "threads.h"
#include "semaphore.h"
#include "future.h"
#include "pit.h"
#include "wave.h"


uint16_t vendorID;
uint8_t headerType;


uint16_t readConfigDWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, bool b) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    uint16_t tmp = 0;
 
    // Create configuration address as per Figure 1
    address = (uint32_t)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));

    if(b) {
        Debug::printf("Address: %x\n", address);
    }
    
    // Write out the address
    outl(0xCF8, address);
    // Read in the data
    // (offset & 2) * 8) = 0 will choose the first word of the 32-bit register
    tmp = (uint16_t)((inl(0xCFC) >> ((offset & 2) * 8)) & 0xFFFF);
    return tmp;
}

uint32_t readConfigDWord_prive(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, bool b) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    uint32_t tmp = 0;
 
    // Create configuration address as per Figure 1
    address = (uint32_t)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));

    if(b) {
        Debug::printf("Address: %x\n", address);
    }
    
    // Write out the address
    outl(0xCF8, address);
    // Read in the data
    // (offset & 2) * 8) = 0 will choose the first word of the 32-bit register
    tmp = inl(0xCFC); 
    return tmp;
}

// Function to get the Vendor ID of a device
uint16_t getVendorID(uint8_t bus, uint8_t device, uint8_t function) {
    uint32_t val = readConfigDWord(bus, device, function, 0x00, false);
    return val & 0xFFFF;
}

uint16_t getVendorID_priv(uint8_t bus, uint8_t device, uint8_t function) {
    uint32_t val = readConfigDWord(bus, device, function, 0x00, true);
    return val & 0xFFFF;
}

// Function to get the Device ID of a device
uint16_t getDeviceID(uint8_t bus, uint8_t device, uint8_t function) {
    uint32_t val = readConfigDWord(bus, device, function, 0x02, false);
    return (val) & 0xFFFF;
}

// uint16_t getDeviceID(uint8_t bus, uint8_t device, uint8_t function) {
//     uint32_t val = readConfigDWord(bus, device, function, 0x00);
//     return (val>> 16) & 0xFFFF;
// }


// Function to get the Class ID of a device
uint8_t getClassID(uint8_t bus, uint8_t device, uint8_t function) {
    uint32_t val = readConfigDWord(bus, device, function, 0x08, false);
    return (val >> 24) & 0xFF;
}

uint16_t getCommandRegister(uint8_t bus, uint8_t device, uint8_t function) {
    uint32_t val = readConfigDWord(bus, device, function, 0x04, false);
    return val & 0xFFFF;
}

// Function to get the Subclass ID of a device
uint8_t getSubclassID(uint8_t bus, uint8_t device, uint8_t function) {
    uint32_t val = readConfigDWord(bus, device, function, 0x08, false);
    return (val >> 16) & 0xFF;
}

// Function to get the Header Type of a device
uint8_t getHeaderType(uint8_t bus, uint8_t device, uint8_t function) {
    uint32_t val = readConfigDWord(bus, device, function, 0x0C, false);
    return (val >> 16) & 0xFF;
}

// Function to get the Header Type of a device
uint32_t getBarZero(uint8_t bus, uint8_t device, uint8_t function) {
    uint32_t val = readConfigDWord_prive(bus, device, function, 0x10, false);
    return val;
}

void checkFunction(uint8_t bus, uint8_t device, uint8_t function) {
    // Get class ID and subclass ID
    uint8_t classID = getClassID(bus, device, function);
    uint8_t subclassID = getSubclassID(bus, device, function);

    int vId = getVendorID_priv(bus, device, function);
    Debug::printf("vendor id %x\n", vId);
    int dId = getDeviceID(bus, device, function);
    Debug::printf("device id %x\n", dId);
    uint32_t barz = getBarZero(bus, device, function);
    Debug::printf("Bar 0: %x \n", barz);


    // if(vId == 0x8086 && dId == 0x2668) {
    //     uint32_t * command_regis = getCommandRegister(bus, device, function); 
    //     *command_regis = *command_regis | 0x4;
    //     Debug::printf("Command regis: %x\n", command_regis);
    // }



    // Debug::printf("VENDOR ID: %x & DevicID: %x\n", getVendorID(bus, device, function), getDeviceID(bus, device, function));

    // Check if this is an Intel HD device
    if (vendorID == 0x8086 && classID == 0x03 && subclassID == 0x00) {
        Debug::printf("Found Intel HD device at bus %d, device %d, function %d\n", bus, device, function);
    }
}

void checkDevice(uint8_t bus, uint8_t device) {
    uint8_t function = 0;

    vendorID = getVendorID(bus, device, function);
    if (vendorID == 0xFFFF) return; // Device doesn't exist
    checkFunction(bus, device, function);
    headerType = getHeaderType(bus, device, function);
    if( (headerType & 0x80) != 0) {
        // It's a multi-function device, so check remaining functions
        for (function = 1; function < 8; function++) {
            if (getVendorID(bus, device, function) != 0xFFFF) {
                checkFunction(bus, device, function);
            }
        }
    }
}


void checkAllBuses(void) {
    uint16_t bus;
    uint8_t device;

    for (bus = 0; bus < 256; bus++) {
        for (device = 0; device < 32; device++) {
            checkDevice(bus, device);
        }
    }
}

uint32_t get_response(char *base) {

    while((((*(uint32_t *) (base + 0x68)) & 0x2) >> 1) != 1) {
        Debug::printf("Response Not Ready\n");
    }

    ASSERT((((*(uint32_t *) (base + 0x68)) & 0x2) >> 1) == 1);

    Debug::printf("Response is ready to be retrieved, reading from: %x\n", (base + 0x64));

    // reset IRV
    (*(uint32_t *) (base + 0x68)) = (*(uint32_t *) (base + 0x68)) & 0xFFFFFFFD; 

    return *((uint32_t *) (base + 0x64));

}

bool send_command(uint32_t codec, uint32_t node, uint32_t command, uint32_t data, char *base) {

    Debug::printf("Send Command\n");

    codec = (codec & 0xf) << 28; 
    node = (node & 0xff) << 20; 
    command = (command & 0xfff) << 8; 
    data = data & 0xff; 

    uint32_t final_command = codec + node + command + data; 

    uint32_t * command_addy = &final_command; 

    ASSERT(*command_addy == final_command);

    Debug::printf("Setting ICB bit to 0\n");
    // Set ICB to 0 
    (*(uint32_t *)(base + 0x68)) = (*(uint32_t *)(base + 0x68)) & 0xFFFFFFFE; // (setting bit 0 to 0)

    // ASSERT((*(uint32_t *) (base + 0x68) & 0x1) == 0);

    Debug::printf("Setting IRV bit to 0\n");
    // Set IRV to 0 
    (*(uint32_t *)(base + 0x68)) = (*(uint32_t *)(base + 0x68)) & 0xFFFFFFFD; // (setting bit 1 to 0)

    // ASSERT(((*(uint32_t *) (base + 0x68) & 0x2) >> 1) == 0);

    Debug::printf("Setting ICOI to final command\n");
    // Sending request To ICOI

    Debug::printf("Setting ICOI ~ FINAL COMMAND: %x\n", *command_addy);
    Debug::printf("Setting ICOI ~ Mem Addy: %x\n", *(uint32_t *) (base + 0x60));

    // memcpy((void *)(base + 0x60), (void *)command_addy, 4);
    Debug::printf("Writing too: %x\n", (base + 0x60));
    *(uint32_t *)(base + 0x60) = final_command; 

    Debug::printf("Setting ICOI ~ FINAL COMMAND ~ AFTER: %x\n", *command_addy);
    Debug::printf("Setting ICOI ~ Mem Addy ~ AFTER: %x\n", *(uint32_t *) (base + 0x60));

    ASSERT(*(uint32_t *) (base + 0x60) == final_command);

    Debug::printf("Setting ICB bit to 1 ~ command is valid\n");
    // Set ICB to 1
    (*(uint32_t *) (base + 0x68)) = (*(uint32_t *) (base + 0x68)) | 0x1; // (setting bit 0 to 1)

    // ASSERT((*(uint32_t *) (base + 0x68) & 0x1) == 1);

    return true; 
}


bool send_comman_extended(uint32_t codec, uint32_t node, uint32_t command, uint32_t data, char *base) {

    Debug::printf("Send Command\n");

    codec = (codec & 0xf) << 28; 
    node = (node & 0xff) << 20; 
    command = (command & 0xf) << 16; 
    data = data & 0xffff; 

    uint32_t final_command = codec + node + command + data; 

    uint32_t * command_addy = &final_command; 

    ASSERT(*command_addy == final_command);

    Debug::printf("Setting ICB bit to 0\n");
    // Set ICB to 0 
    (*(uint32_t *)(base + 0x68)) = (*(uint32_t *)(base + 0x68)) & 0xFFFFFFFE; // (setting bit 0 to 0)

    // ASSERT((*(uint32_t *) (base + 0x68) & 0x1) == 0);

    Debug::printf("Setting IRV bit to 0\n");
    // Set IRV to 0 
    (*(uint32_t *)(base + 0x68)) = (*(uint32_t *)(base + 0x68)) & 0xFFFFFFFD; // (setting bit 1 to 0)

    // ASSERT(((*(uint32_t *) (base + 0x68) & 0x2) >> 1) == 0);

    Debug::printf("Setting ICOI to final command\n");
    // Sending request To ICOI

    Debug::printf("Setting ICOI ~ FINAL COMMAND: %x\n", *command_addy);
    Debug::printf("Setting ICOI ~ Mem Addy: %x\n", *(uint32_t *) (base + 0x60));

    // memcpy((void *)(base + 0x60), (void *)command_addy, 4);
    Debug::printf("Writing too: %x\n", (base + 0x60));
    *(uint32_t *)(base + 0x60) = final_command; 

    Debug::printf("Setting ICOI ~ FINAL COMMAND ~ AFTER: %x\n", *command_addy);
    Debug::printf("Setting ICOI ~ Mem Addy ~ AFTER: %x\n", *(uint32_t *) (base + 0x60));

    ASSERT(*(uint32_t *) (base + 0x60) == final_command);

    Debug::printf("Setting ICB bit to 1 ~ command is valid\n");
    // Set ICB to 1
    (*(uint32_t *) (base + 0x68)) = (*(uint32_t *) (base + 0x68)) | 0x1; // (setting bit 0 to 1)

    // ASSERT((*(uint32_t *) (base + 0x68) & 0x1) == 1);

    return true; 
}

void ready_to_play(WaveParser file, char * base, WaveParser wave_file) {

    char * base_addy_plus_x = (char *) (base + (0x80 + 4 * 0x20)); 

    // SDnBDL Lower Set Up 
    Debug::printf("Addy: %x\n",wave_file.b_entries);
    uint32_t entries = (uint32_t) wave_file.b_entries;
    *(uint32_t *)(base_addy_plus_x + 0x18) = entries; 
    ASSERT((*(uint32_t *)(base_addy_plus_x + 0x18)) == entries);
    Debug::printf("Addy ~ Entries: %x\n",entries);

    // LVI -> 15 
        // *(uint16_t *)(LVI) = 15; 
    char * LVI = base_addy_plus_x + 12; 
    uint16_t num = 15; 
    uint16_t * value = &num; 
    memcpy(LVI, value, 2);
    ASSERT(*(uint16_t *)(base_addy_plus_x + 0xC) == 15);
    Debug::printf("LVI: %d\n", *(uint16_t *)(base_addy_plus_x + 0xC));

    // SDnCBL -> 16 * 4096 
    char * CBL = (base_addy_plus_x + 0x8); 
    uint32_t CBL_num = 65536; 
    uint32_t * CBL_value = &CBL_num; 
    *(uint32_t *)(CBL) = *CBL_value;
    Debug::printf("Value of CBL: %d and CLB Num: %d\n", *CBL_value, CBL_num); 
    Debug::printf("CBL: %d\n", *(uint32_t *)((base_addy_plus_x + 0x8)));
    ASSERT(*(uint32_t *)((base_addy_plus_x + 0x8)) == (4096*16));
    

    // SDnFMT -> 
    char * FMT = (base_addy_plus_x + 0x12); 

    uint16_t current_FMT = *(uint16_t *)(FMT);

    Debug::printf("Current FMT: %x\n", current_FMT);

    current_FMT = current_FMT & 0x8080; 
    current_FMT += 0x500; 
    Debug::printf("After FMT: %x\n", current_FMT);
    *(uint16_t *)(FMT) = current_FMT;


    Debug::printf("FMT: %x\n", *(uint16_t *)((base_addy_plus_x + 0x12)));
    ASSERT(*(uint16_t *)((base_addy_plus_x + 0x12)) == (1280));
}

void turnOnDevice(uint32_t *base_u) {
    
    while(*(base_u + 2) == 0) {
        Debug::printf("It's not on Yet, %x\n", *(base_u + 2));
    }

    Debug::printf("DEVICE IS TURNED ON: %x\n", (*(base_u + 2)));

    volatile uint32_t help = 0; 

    while(help  < 10000) {
        help += 1; 
    }
}

void kernelMain(void) {


    // 0x8000200

    // PCI controll register -> command register -> outl (addy we want to read) -> inl to read what it responds
    outl(0xCF8, 0x80002004);
    uint32_t Status_Command_Regis =  inl(0xCFC) | 0x4;
    outl(0xCF8, 0x80002004);
    outl(0xCFC, Status_Command_Regis);

    // Base Adderress 
    char *base = (char *) 0xfebf0000;
    uint32_t *base_u = (uint32_t *) 0xfebf0000;

    // CRST Bit being flipped 
    *(base_u + 2) = *(base_u + 2) | 0x01;

    Debug::printf("After flipping bit: GCTL: %x\n", *(base_u + 2));

    // VMAJ
    ASSERT(*((base + 3)) == 1);
    
    // VMIN
    ASSERT(*((base + 2)) == 0);

    Debug::printf("After flipping bit: GCAP: %x\n", *base_u);

    // Esuring the Device is being Turned On 
    turnOnDevice(base_u);

    auto ide = Shared<Ide>::make(1);
    
    // We expect to find an ext2 file system there
    auto fs = Shared<Ext2>::make(ide);

    Debug::printf("*** block size is %d\n",fs->get_block_size());
    Debug::printf("*** inode size is %d\n",fs->get_inode_size());
   
    auto root = fs->root;

    auto hello = fs->find(root,"swift_r8b_extra_compressed (1).wav");

    WaveParser wave_file = WaveParser(hello);

    Debug::printf("Addy: %x\n",wave_file.b_entries);

    // uint16_t GCAP = * (uint16_t *)base; 
    // Debug::printf("GCAP: %x\n", GCAP);
    // Debug::printf("Value ISS:%d\n", (GCAP >> 8) & 0xF); // 4

    ready_to_play(wave_file, base, wave_file);


    // Send SetPinWidgetControl to Node 3 ~ 0x707
    // 01000000

    // ~ Gets the inital value that is there 
    send_command(0, 3, 0xf07, 0, base);

    uint32_t current_pinCntl = get_response(base);
    current_pinCntl |= 0x40; 
    send_command(0, 3, 0x707, current_pinCntl, base); 

    // Send SetStreamChannel to Node 2 ~ 0x706 
    // 0x706 
    // 0001
    // 0000
    send_command(0, 2, 0x706, 0x10, base);

    // SetAmplifierGain 
    send_comman_extended(0, 2, 0x3, 0xB035, base);

    // 1 0 1 1 0 0 0 0 "0" '......' 
    // B035
    send_comman_extended(0, 2, 0x2, 0x500, base);

    // SDnCTL -> Set RUN -> 1 
    char * base_addy_plus_x = (char *) (base + (0x80 + 4 * 0x20)); 
    char * SDnCTL = (base_addy_plus_x); 
    *((uint32_t*)SDnCTL) = (*((uint32_t*)SDnCTL) + 0x100000);
    *((uint32_t*)SDnCTL) = (*((uint32_t*)SDnCTL) + 0x2);
    Debug::printf("SDnCTL: %x\n", (*((uint32_t*)SDnCTL)));

   // DPLBASE ~ Sanity Check ~ FAIL ~ RIP ~ WE ARE INSANE
   while(true) {
       Debug::printf("Value of SDnLPIB: %x\n", *(uint32_t *)(base_addy_plus_x + 0x4));
   }

}
