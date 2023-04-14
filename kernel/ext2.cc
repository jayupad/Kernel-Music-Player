#include "ext2.h"
#include "debug.h"
#include "libk.h"

// Ext2

Ext2::Ext2(Shared<Ide> ide) {
    super_block = new SuperBlock; 
    // inode_cache = create_cache(5, 7, 32, 0);

    cache = new Cache(32, 10);
    
    // bgdt = new BGDT_struct; 
    ide->read_all(1024, sizeof(SuperBlock), (char*)super_block);

    uint32_t block_size = ((1 << 10) << super_block->block_size_shifter); 
    block_cache = new Cache_Block(16, 16, block_size);
    uint32_t start_bgdt = block_size; 
    if(block_size == 1024) { 
        start_bgdt = block_size * 2; 
    }


    // based on blocks math 
    uint32_t num_groups = (super_block->num_blocks / super_block->blocks_in_group);
    num_groups += (super_block->num_blocks % super_block->blocks_in_group) > 0 ? 1 : 0;

    // based on group math 
    uint32_t num_groups_inodes = (super_block->num_inodes / super_block->inodes_in_group);
    num_groups_inodes += (super_block->num_inodes % super_block->inodes_in_group) > 0 ? 1 : 0;

    ASSERT(num_groups_inodes == num_groups);

    // Array of BGDT_struct - replicates Block Group Descriptor Table 
    bgdt_array = new BGDT_struct[num_groups];

    // Gives info for each of these values
    for(uint32_t x = 0; x < num_groups; x++) {
        ide->read_all(start_bgdt + (x * sizeof(BGDT_struct)), sizeof(BGDT_struct), (char *) &bgdt_array[x]);
    }

    root = Shared<Node>::make(((1 << 10) << super_block->block_size_shifter), 2, super_block, ide, bgdt_array, block_cache);

}

Shared<Node> Ext2::find(Shared<Node> dir, const char* name) {

    if(!dir->is_dir()) {
        Debug::printf("non find calling firect\n");
    }

   file_node * temp = dir->head->next; 

    while(temp != nullptr) {
        if(K::streq(temp->file->name, name)) {
            return cache->getValue(temp->file->inode_number, temp, dir, super_block, block_cache);
        }
        temp = temp->next; 
    }

    return Shared<Node>(); 
}



void Node::read_block(uint32_t numbere, char* buffer) {
    block_cache_e->getValue(numbere, buffer, ide_life, inode_meta, number);
}
