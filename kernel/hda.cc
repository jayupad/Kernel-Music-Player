#include "ide.h"
#include "ext2.h"
#include "libk.h"
#include "threads.h"
#include "semaphore.h"
#include "future.h"
#include "pit.h"

class HDA {
private:
    uint32_t *base_u;
    char *base;

    struct Codec {
        uint32_t vendorID;
        uint32_t deviceID;
        uint32_t revisionID;
        uint32_t subsystemID;
        uint32_t subsystemVendorID;
        uint32_t capabilities;
        uint32_t reserved[2];
    };

    struct CodecList {
        uint32_t codec;
        uint32_t prev;
        uint32_t next;
    };

    struct Node {
        uint32_t widgetCap;
        uint32_t connectionList;
        uint32_t pinWidget;
        uint32_t pinConfig;
        uint32_t verb;
    };

    CodecList codecListStart;
    Codec codec;
    Node node;

public:
    HDA() {
        base = (char *) 0xfebf0000;
        base_u = (uint32_t *) 0xfebf0000;

        // Flip bit 0 of register GCTL to enable the HDA controller
        *(base_u + 2) = *(base_u + 2) | 0x01;

        // Wait until the controller is ready
        while(*(base_u + 2) == 0) {}

        // Get the start of the codec list
        uint32_t codecListPtr = *(base_u + 64);
        codecListStart = *(CodecList *) (base + codecListPtr);
    }

    void findCodecs() {
        Debug::printf("Codec list start: %x\n", codecListStart.codec);

        // Find all codecs in the list
        uint32_t codecPtr = codecListStart.codec;
        while (codecPtr != 0) {
            codec = *(Codec *) (base + codecPtr);

            Debug::printf("Codec: %x:%x:%x:%x:%x:%x\n",
                          codec.vendorID, codec.deviceID, codec.revisionID,
                          codec.subsystemID, codec.subsystemVendorID,
                          codec.capabilities);

            // Move to the next codec in the list
            codecPtr = codecListStart.next;
            codecListStart = *(CodecList *) (base + codecListStart.next);
        }
    }

    void findNodes() {
        Debug::printf("Finding nodes...\n");

        // Traverse the node tree to find all nodes
        uint32_t rootNodePtr = *(base_u + 27);
        traverseNode(rootNodePtr);
    }

    void traverseNode(uint32_t nodePtr) {
        // Read the widget capabilities of the current node
        node = *(Node *) (base + nodePtr);
        uint32_t widgetCap = node.widgetCap;

        // If this is a pin complex, print its configuration
        if ((widgetCap & 0xf000) == 0x4000) {
            Debug::printf("Pin Complex: %x\n", node.pinConfig);
        }

        // Traverse child nodes
        uint32_t childListPtr = node.connectionList;
        while (childListPtr != 0) {
            uint32_t childPtr = *(base_u + childListPtr);
            traverseNode(childPtr);
            childListPtr += 4;
        }

        // Traverse sibling nodes
        uint32_t siblingListPtr = node.pinWidget;
        while (siblingListPtr != 0) {
            uint32_t siblingPtr = *(base_u + siblingListPtr);
            traverseNode(siblingPtr);
            siblingListPtr += 4;
        }
    }
};
