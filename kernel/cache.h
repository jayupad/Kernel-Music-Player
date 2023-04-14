#include "ext2.h"

class Cache {

public:

    Shared<Node> *** my_cache; 
    uint32_t size_of_inner_array; 

    Cache(uint32_t rows, uint32_t columns) {

        my_cache = new Shared<Node>**[rows];
        for(uint32_t x = 0; x < rows; x++) {
            my_cache[x] = new Shared<Node>*[columns];
            for(uint32_t y = 0; y < columns; y++) {
                my_cache[x][y] = nullptr; 
            }
        }

        size_of_inner_array = columns; 

    }

    Shared<Node> getValue(uint32_t index, file_node * temp, Shared<Node> dir, SuperBlock * super_block) {
        auto index_of_set = index & 0x1F; 

        // auto data_set = my_cache[index_of_set]; 

 

        auto num_to_put = -1; 

        for(uint32_t x = 0; x < size_of_inner_array; x++) {
            if(my_cache[index_of_set][x] != nullptr) {
                if(my_cache[index_of_set][x]->number == index) {
                    Debug::printf("I found it in my cache\n");
                    return (my_cache[index_of_set][x]);
                }
            } else {
                num_to_put = x; 
            }
        }

        if(num_to_put == -1) {
            // To Change 
            delete my_cache[index_of_set][0];
            my_cache[index_of_set][0] = new Shared<Node>::make(((1 << 10) << super_block->block_size_shifter), temp->file->inode_number, super_block, dir->ide_life, dir->bgdt_array_node);
            return (my_cache[index_of_set][0]);
        } else {
            my_cache[index_of_set][num_to_put] = new Shared<Node>::make(((1 << 10) << super_block->block_size_shifter), temp->file->inode_number, super_block, dir->ide_life, dir->bgdt_array_node);
            return (my_cache[index_of_set][num_to_put]);
        }
    }

};
