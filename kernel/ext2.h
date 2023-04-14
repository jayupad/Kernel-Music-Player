#ifndef _ext2_h_
#define _ext2_h_

#include "ide.h"
#include "atomic.h"
#include "debug.h"
#include "shared.h"
#include "libk.h"
#include "blocking_lock.h"



struct SuperBlock{
    uint32_t num_inodes; 
    uint32_t num_blocks; 
    uint32_t num_blocks_for_superuser;
    uint32_t num_unallocated_blocks; 
    uint32_t num_unallocated_inodes; 
    uint32_t superblock_id; 
    uint32_t block_size_shifter; 
    uint32_t fragement_size_shifter; 
    uint32_t blocks_in_group; 
    uint32_t fragements_in_group;
    uint32_t inodes_in_group;
    uint32_t last_mount_time;
    uint32_t last_written_time;
    uint16_t number_times_volume_mounted; 
    uint16_t number_of_mounts_allowed;
    uint16_t ext2_signature; 
    uint16_t file_system_state; 
    uint16_t error_code_handler; 
    uint16_t minor_portion_of_version; 
    uint32_t last_consistency_check;
    uint32_t interval_forced_consistency_checks;
    uint32_t OS_ID;
    uint32_t major_portion_of_version;
    uint16_t user_id_reserved;
    uint16_t group_id_reserved;
};

struct BGDT_struct {
    uint32_t block_address_block_bitmap; 
    uint32_t block_address_inode_bitmap; 
    uint32_t starting_block_adderess_inode_table;
    uint16_t num_unallocated_blocks_ingroup; 
    uint16_t num_unallocated_inodes_ingroup; 
    uint16_t num_directories_ingroup; 
    char not_needed[14];
}; 

struct inode {
    uint16_t permissions; 
    uint16_t user_id; 
    uint32_t lower_32_bytes_ofsize; 
    uint32_t last_access_time; 
    uint32_t creation_time; 
    uint32_t last_modified_time; 
    uint32_t deletion_time;
    uint16_t group_id; 
    uint16_t num_hard_links; 
    uint32_t num_disk_sectors; 
    uint32_t flags; 
    uint32_t os_specific_value; 
    uint32_t pointers[15];
    uint32_t generation_number; 
    uint32_t file_acl; 
    uint32_t directory_acl; 
    uint32_t block_addy_fragment; 
    // uint32_t * second_os_specific_value[3];
    char not_needed[12];

    ~inode() {

    }
}; 

struct directory_entry {
    uint32_t inode_number; 
    uint16_t size_of_entry; 
    uint8_t name_of_file; 
    uint8_t type_indicator; 
    char name[256];
};

struct file_stuff {
    char * name; // rounding up for life 
    uint32_t inode_number; 

    ~file_stuff() {
        if(name != nullptr) {
            delete name; 
        }
    }
};

class file_node {

    public:
    file_stuff * file; 
    file_node * next;
    Atomic<uint32_t> ref_count{0}; 

    file_node(uint32_t size) {
        file = new file_stuff;
        file->name = new char[size];
        next = nullptr; 
    }

    ~file_node() {
        delete file;
        if(next != nullptr) {
            delete next; 
        }
    }
}; 


struct block_data_meta {
    char * data; 
    uint32_t index; 
    uint32_t counter; 
    uint32_t num; 

    ~block_data_meta() {
        delete data; 
    }
};

class Cache_Block {

public:

    block_data_meta *** my_cache; 
    uint32_t size_of_inner_array; 
    uint32_t bs;
    BlockingLock * bl; 
    

    Cache_Block(uint32_t rows, uint32_t columns, uint32_t block_size) {
        bl = new BlockingLock();
        my_cache = new block_data_meta**[rows];
        for(uint32_t x = 0; x < rows; x++) {
            my_cache[x] = new block_data_meta*[columns / (block_size / 1024)];
            for(uint32_t y = 0; y < (columns / (block_size / 1024)); y++) {
                my_cache[x][y] = nullptr; 
            }
        }
        bs = block_size;

        size_of_inner_array = (columns / (block_size / 1024)); 

    }

    void getValue(uint32_t indexc, char* buffer, Shared<Ide> ide, inode* inode_meta, uint32_t number) {
        bl->lock();
        auto index_of_set = indexc & 0xF; 

        auto num_to_put = -1; 
        auto num = -1; 
        auto hard_num = -1; 


        for(uint32_t x = 0; x < size_of_inner_array; x++) {
            
            if(my_cache[index_of_set][x] != nullptr) {

                if((my_cache[index_of_set][x])->index == indexc && my_cache[index_of_set][x]->num == number) {
                    (my_cache[index_of_set][x])->counter++;
                    memcpy(buffer, (my_cache[index_of_set][x])->data, bs);
                    bl->unlock();
                    return; 
                } else {
                    if(num == -1 || my_cache[index_of_set][x]->counter < my_cache[index_of_set][num]->counter) { 
                        if(my_cache[index_of_set][x]->index != my_cache[index_of_set][indexc]->index) {
                            hard_num = x; 
                        }
                        num = x; 
                    }
                }
            } else {
                num_to_put = x; 
            }
        }

        if(num_to_put == -1) {
            // delete (my_cache[index_of_set][num]);
            // auto current_item = my_cache[index_of_set][num];
            if(hard_num != -1) {
                num = hard_num; 
            }
            auto current_item = my_cache[index_of_set][num];
            current_item->index = indexc;
            current_item->num = number;
            current_item->counter = 0;
            read_block_private(indexc, current_item->data, ide, inode_meta);
            my_cache[index_of_set][num] = current_item;
            memcpy(buffer, current_item->data, bs);

        } else {
            auto current_item = new block_data_meta;
            current_item->index = indexc;
            current_item->num = number;
            current_item->counter = 0;
            current_item->data = new char[bs];
            read_block_private(indexc, current_item->data, ide, inode_meta);
            my_cache[index_of_set][num_to_put] = current_item;
            memcpy(buffer, current_item->data, bs);
        }

        bl->unlock();

    }

    void read_block_private(uint32_t number, char* buffer, Shared<Ide> ide_life, inode* inode_meta) {

    uint32_t block_size_x = bs;
    if(number <= 11) {
        ide_life->read_all((inode_meta->pointers[number]) * block_size_x, block_size_x, buffer);
    } else if (number <= (block_size_x/4 + 11)) {
        uint32_t * array = new uint32_t[block_size_x/4];
        ide_life->read_all((inode_meta->pointers[12]) * block_size_x, block_size_x, (char*) array);
        number -= 11; 
        ide_life->read_all((array[number-1]) * block_size_x, block_size_x, buffer);
        delete array; 
    } else if (number <= ((block_size_x/4) * (block_size_x/4) + (block_size_x/4 + 11))) {
        uint32_t * array = new uint32_t[block_size_x/4];
        uint32_t * ounter_array = new uint32_t[block_size_x/4];
        ide_life->read_all((inode_meta->pointers[13]) * block_size_x, block_size_x, (char*) array);
        number -= (block_size_x/4 + 11);
        uint32_t index = (number-1) / (block_size_x/4);
        ide_life->read_all((array[index]) * block_size_x, block_size_x, (char*) ounter_array);
        ide_life->read_all((ounter_array[(number-1) % ((block_size_x/4))]) * block_size_x, block_size_x, buffer);
        delete array; 
        delete ounter_array; 
    } else {
        uint32_t * array = new uint32_t[block_size_x/4];
        uint32_t * ounter_array = new uint32_t[block_size_x/4];
        uint32_t * inner = new uint32_t[block_size_x/4];
        ide_life->read_all((inode_meta->pointers[14]) * block_size_x, block_size_x, (char*) array);
        number -= (block_size_x/4 + 11);
        uint32_t index = (number-1) / (block_size_x/4) / (block_size_x/4) ;
        ide_life->read_all((array[index]) * block_size_x, block_size_x, (char*) ounter_array);
        index = (number-1) / (block_size_x/4);
        ide_life->read_all((ounter_array[index]) * block_size_x, block_size_x, (char*) inner);
        ide_life->read_all((inner[(number-1) % ((block_size_x/4))]) * block_size_x, block_size_x, buffer);
        delete array; 
        delete ounter_array;
        delete inner; 
    }
 }


};

// A wrapper around an i-node
class Node : public BlockIO { // we implement BlockIO because we
                              // represent data

public:
    Atomic<uint32_t> ref_count{0};
    // i-number of this node
    uint32_t number;
    inode* inode_meta; 
    SuperBlock* super_block; 
    Shared<Ide> ide_life; 
    uint32_t block_size_x; 
    uint32_t number_of_entries;
    file_stuff * directory; 
    BGDT_struct * bgdt_array_node;
    char * sym_link_name; 
    file_node * head; 
    Cache_Block * block_cache_e; 
    uint32_t entries_counted; 
    Node(uint32_t block_size, uint32_t number_e, SuperBlock* temp , Shared<Ide> ide, BGDT_struct * bgdt_array, Cache_Block * block_cache) : BlockIO(block_size) {
        block_cache_e = block_cache;
        block_size_x = block_size;
        super_block = temp;
        ide_life = ide; 
        inode_meta = new inode;
        bgdt_array_node = bgdt_array;
        head = new file_node(0);
        number = number_e;

        uint32_t block_group_number = get_block_group_number(number);
        uint32_t index = get_block_group_index(number);

        ide->read(bgdt_array_node[block_group_number].starting_block_adderess_inode_table * block_size + index * 128, *inode_meta);

        // Extracting the file name 
        if(is_dir()) {
            entries_counted = private_entry_count();
        }

        if(is_symlink()) {
            sym_link_name = new char[size_in_bytes() + 1];
            if(inode_meta->lower_32_bytes_ofsize < 60) {
                memcpy(sym_link_name,&(inode_meta->pointers),size_in_bytes());
                sym_link_name[size_in_bytes()] = '\0';
            } else {
                ide_life->read_all(inode_meta->pointers[0] * block_size_x, size_in_bytes(), sym_link_name);
                sym_link_name[size_in_bytes()] = '\0';
            }
        }
    }

    virtual ~Node() {}

    // How many bytes does this i-node represent
    //    - for a file, the size of the file
    //    - for a directory, implementation dependent
    //    - for a symbolic link, the length of the name
    uint32_t size_in_bytes() override {
        if(is_file()) {
            return inode_meta->lower_32_bytes_ofsize;
        } else if(is_dir()) {
            return inode_meta->lower_32_bytes_ofsize; // TODO: fix this 
        } else if(is_symlink()) {
            return inode_meta->lower_32_bytes_ofsize; 
        }

        return 0;
    }

    // read the given block (panics if the block number is not valid)
    // remember that block size is defined by the file system not the device
    void read_block(uint32_t number, char* buffer) override;

    // returns the ext2 type of the node
    uint32_t get_type() {

        if((inode_meta->permissions & 0xF000) == 0x4000) {
            return 2; 
        } else if((inode_meta->permissions & 0xF000) == 0x8000) {
            return 1; 
        } else if((inode_meta->permissions & 0xF000) == 0xA000) {
            return 7;
        }

        return 0;
    }

    // true if this node is a directory
    bool is_dir() {
        // Debug::printf("%d", inode_meta->permissions);
        return (inode_meta->permissions & 0xF000) == 0x4000;
    }

    // true if this node is a file
    bool is_file() {
        return (inode_meta->permissions & 0xF000) == 0x8000;
    }

    // true if this node is a symbolic link
    bool is_symlink() {
        return (inode_meta->permissions & 0xF000) == 0xA000;
    }

    // If this node is a symbolic link, fill the buffer with
    // the name the link referes to.
    //
    // Panics if the node is not a symbolic link
    //
    // The buffer needs to be at least as big as the the value
    // returned by size_in_byte()
    void get_symbol(char* buffer) {
        if(!is_symlink()) {
            Debug::panic("get Symbol is not symlink \n");
        }

        memcpy(buffer,&(*sym_link_name),size_in_bytes());
    }

    // Returns the number of hard links to this node
    uint32_t n_links() {
        return inode_meta->num_hard_links;
    }

    // Returns the number of entries in a directory node
    //
    // Panics if not a directory
    uint32_t entry_count() {

         if(!is_dir()) {
            Debug::panic("bruh entry count\n");
        }

        return entries_counted;

    }


    uint32_t private_entry_count() {
         if(!is_dir()) {
            Debug::panic("bruh entry count\n");
        }

        

        uint32_t counter = 0; 

        file_node * temp = head; 

        char * block_data = new char[block_size_x];
        directory_entry * ded = new directory_entry; 

        uint32_t num_blocks = inode_meta->lower_32_bytes_ofsize / (block_size_x);
        num_blocks += (inode_meta->lower_32_bytes_ofsize % block_size_x) > 0 ? 1 : 0;

        for(uint32_t x = 0; x < num_blocks; x++) {
            read_block(x, block_data);
            uint32_t offset = 0; 
            uint32_t max_offset = block_size_x; 
            while(offset < max_offset) {

                while(offset % 4 != 0) {
                    offset++;
                }

                ded->inode_number = * (uint32_t *) (block_data + offset);
                ded->size_of_entry = * (uint16_t *) (block_data + 4 + offset);
                ded->name_of_file = * (uint8_t *) (block_data + 6 + offset);
                ded->type_indicator = * (uint8_t *) (block_data + 7 + offset);

                // Debug::printf("Inode Number: %d\n", ded->inode_number);

                

                if(ded->inode_number != 0) {
                    counter++;
                    temp->next = new file_node(ded->name_of_file + 1);
                    temp->next->file->inode_number = ded->inode_number;
                    for(int i = 0; i < ded->name_of_file; i++) {
                        temp->next->file->name[i] = *(block_data + 8 + offset + i);
                    }
                    temp->next->file->name[ded->name_of_file] = '\0';
                    temp = temp->next; 
                }
                 offset += ded->size_of_entry;
            }
        }

        number_of_entries = counter; 
        delete block_data; 
        delete ded;
        return counter; 
    }

    int get_block_group_number(uint32_t inode_number) {
        return (inode_number - 1) / super_block->num_inodes;
    }

    int get_block_group_index(uint32_t inode_number) {
        return (inode_number - 1) % (super_block->inodes_in_group);
    }

    
};

struct temp_node {
    Shared<Node> node; 
    uint32_t counter; 
};

class Cache {

public:

    temp_node *** my_cache; 
    uint32_t size_of_inner_array; 
    BlockingLock * bl; 

    Cache(uint32_t rows, uint32_t columns) {

        my_cache = new temp_node**[rows];
        for(uint32_t x = 0; x < rows; x++) {
            my_cache[x] = new temp_node*[columns];
            for(uint32_t y = 0; y < columns; y++) {
                my_cache[x][y] = nullptr; 
            }
        }

        bl = new BlockingLock();
        size_of_inner_array = columns; 

    }

    Shared<Node> getValue(uint32_t index, file_node * temp, Shared<Node> dir, SuperBlock * super_block, Cache_Block * block_cache) {
        bl->lock();
        auto index_of_set = index & 0x1F; 

        auto num_to_put = -1; 
        auto num = -1; 

        for(uint32_t x = 0; x < size_of_inner_array; x++) {
            if(my_cache[index_of_set][x] != nullptr) {
                if((my_cache[index_of_set][x])->node->number == index) {
                    (my_cache[index_of_set][x])->counter++;
                    bl->unlock();
                    return (my_cache[index_of_set][x])->node;
                } else {
                    if(num == -1 ||  my_cache[index_of_set][num]->counter > my_cache[index_of_set][x]->counter) {
                        num = x; 
                    }
                }
            } else {
                num_to_put = x; 
            }
        }

        if(num_to_put == -1) {
            delete (my_cache[index_of_set][num]);
        } else {
            num = num_to_put; 
        }
        temp_node * stuff = new temp_node; 
        stuff->node = Shared<Node>::make(((1 << 10) << super_block->block_size_shifter), temp->file->inode_number, super_block, dir->ide_life, dir->bgdt_array_node, block_cache);
        stuff->counter = 0; 
        my_cache[index_of_set][num] = stuff; 
        bl->unlock();
        return stuff->node;
    }

};



// This class encapsulates the implementation of the Ext2 file system
class Ext2 {
public:
   
public:
    Shared<Node> root;
    Atomic<uint32_t> ref_count{0};
    SuperBlock* super_block; 
    BGDT_struct * bgdt_array;
    Cache * cache; 
    Cache_Block * block_cache; 
    // cache_t * inode_cache; 


    // Mount an existing file system residing on the given device
    // Panics if the file system is invalid
    Ext2(Shared<Ide> ide);

    // Returns the block size of the file system. Doesn't have
    // to match that of the underlying device
    uint32_t get_block_size() {
        return (1 << 10) << super_block->block_size_shifter;
    }

    // Returns the actual size of an i-node. Ext2 specifies that
    // an i-node will have a minimum size of 128B but could have
    // more bytes for extended attributes
    uint32_t get_inode_size() {
        return 128;
    }

    // If the given node is a directory, return a reference to the
    // node linked to that name in the directory.
    //
    // Returns a null reference if "name" doesn't exist in the directory
    //
    // Panics if "dir" is not a directory
    Shared<Node> find(Shared<Node> dir, const char* name);
    void printSuperBlock();

};

#endif

