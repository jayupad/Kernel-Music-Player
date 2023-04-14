#include "ide.h"
#include "ext2.h"
#include "libk.h"
#include "threads.h"
#include "semaphore.h"
#include "future.h"


/*
####################################################################################################
#  Welcome to this Test Case 005:                                                                  #   
#         - Do you have a cahce for Inode?                                                         #
#         - Do you have a big enough cache?                                                        #
#         - Do you have a cache for read_block?                                                    #
#         - Is it at all effcient or do you have a logic error?                                    #
#         - Do you delete properly in these caches?                                                #
#         - If I call your entry count it should not be reading from the disk so many times        #
#            - Since we are read only, it is better to make your public methods not read from disk #
#              instead save values in your stuct and return                                        #
#                                                                                                  #
#                                                                                                  #
#  Notice: This testcase takes 2-3.4 seconds on my end on a high load machine, hence this test     #
#          case has plenty of leanway, and is a kind testcase                                      #
#                                                                                                  #
#                                                                                                  #
#  Main Focus: Test your cache logic to make sure it works and deletes properly.                   #
#                                                                                                  #
####################################################################################################
*/

// Given function by Dr.Gheith 
void show(const char* name, Shared<Node> node, bool show) {

    Debug::printf("*** looking at %s\n",name);

    if (node == nullptr) {
        Debug::printf("***      does not exist\n");
        return;
    } 

    if (node->is_dir()) {
        Debug::printf("***      is a directory\n");
        Debug::printf("***      contains %d entries\n",node->entry_count());
        Debug::printf("***      has %d links\n",node->n_links());
    } else if (node->is_symlink()) {
        Debug::printf("***      is a symbolic link\n");
        auto sz = node->size_in_bytes();
        Debug::printf("***      link size is %d\n",sz);
        auto buffer = new char[sz+1];
        buffer[sz] = 0;
        node->get_symbol(buffer);
        Debug::printf("***       => %s\n",buffer);
    } else if (node->is_file()) {
        Debug::printf("***      is a file\n");
        auto sz = node->size_in_bytes();
        Debug::printf("***      contains %d bytes\n",sz);
        Debug::printf("***      has %d links\n",node->n_links());
        if (show) {
            auto buffer = new char[sz+1];
            buffer[sz] = 0;
            auto cnt = node->read_all(0,sz,buffer);
            CHECK(sz == cnt);
            CHECK(K::strlen(buffer) == cnt);
            // can't just print the string because there is a 1000 character limit
            // on the output string length.
            for (uint32_t i=0; i<cnt; i++) {
                Debug::printf("%c",buffer[i]);
            }
            delete[] buffer;
            Debug::printf("\n");
        }
    } else {
        Debug::printf("***    is of type %d\n",node->get_type());
    }
}



/* Called by one CPU */
void kernelMain(void) {

    // This array can help u test whatever you want to and 
    // not the others so you can make sure which ones pass 
     bool which_test[] = {   
                            true, // Entry count should not do disk reads 
                            true, // You dont read ur whole cache, u index into it 
                            true, // If you have a cache for ur inode 
                            true, // Cache works and isnt like 10 in size (300 ish in size) - also ur find should not disk reads if it doesnt need to 
                            true, // Do u have a cache for read_block 
                            true, // This cache works and doest only store one block at a time 
                          };

    // IDE device #1
    auto ide = Shared<Ide>::make(1);
    
    // We expect to find an ext2 file system there
    auto fs = Shared<Ext2>::make(ide);

   // get "/"
   auto root = fs->root;


   // Proper Delete in Entry-Count and make sure you dont do
   // Disk Reads in Entry Count 
   // You might timeout if you always read from disk in entry count 
    if(which_test[0]) {
        for(int x = 0; x < 1000000; x++) {
            root->entry_count();
        }
   }

   Debug::printf("*** Entry Count is Good\n");
   

   // Cache Required Test Cases 


   // 0. Make sure you dont traverse your whole cache
    if(which_test[1]) {
        for(int x = 0; x < 1000000; x++) {
            fs->find(root, "bee_movie");
        }
   }
   
   Debug::printf("*** Return Null Shared Pointer Fast Enough\n");

   // 1. Make sure you have a cache for Inodes (stress tests)
   // You might out of memory error - implement a cache 
    if(which_test[2]) {
        for(int x = 0; x < 1000000; x++) {
            fs->find(root, "bee_movie.txt");
        }
   }

   Debug::printf("*** Find Cache Exists\n");

   // 2. Tests to see if your cache reasonable size 
   // Made 320 Files to make sure that you cache is not to small 
   // Also ensures proper deletes in your cache otherwise you get out of memory 
   auto stress_test = fs->find(root, "stress_test");

    if(which_test[3]) {
        // Code copied from (https://stackoverflow.com/questions/3982320/convert-integer-to-string-without-access-to-libraries)
        for(int x = 0; x < 500; x++) {
            for(int y = 1; y <= 320; y++) {

                    int num = y;
                    char res[4];
                    int len = 0;
                    for(; num > 0; ++len)
                    {
                        res[len] = num%10+'0';
                        num/=10; 
                    }
                    res[len] = 0; //null-terminating

                    //now we need to reverse res
                    for(int i = 0; i < len/2; ++i)
                    {
                        char c = res[i]; res[i] = res[len-i-1]; res[len-i-1] = c;
                    }

                auto temp = fs->find(stress_test, res);
            }
        }
   }


   Debug::printf("*** Find Cache is big enough\n");

   // 3. Makes sure you have another cache for your get_block
   auto bee_movie = fs->find(root, "bee_movie.txt");

   if(which_test[4]) {
        for(int x = 0; x < 1000; x++) {
            {
                uint32_t start = bee_movie->size_in_bytes() - 26;
                auto sz = bee_movie->size_in_bytes();
                // CHECK(sz >= start);
                auto buffer = new char[sz - start + 1];
                for (uint32_t n = 4; n<sz-start; n++) {
                    buffer[n] = 0;
                    bee_movie->read_all(start,n,buffer);
                    // CHECK(cnt == n);
                }
                delete[] buffer;
            }
        }
   }

    Debug::printf("*** Read Block Cache Exists\n");

   // 4. Your block_cache is of decent size (not too small)
   // This checks to read 57318 bytes 1000 times 
   // Very simple cache, print your cache to make sure 
   // You keep data from the same inode number instead of the 
   // older inodes even if it was more frequently uses
   if(which_test[5]) { 
        for(int x = 0; x < 1000; x++) {
            auto sz = bee_movie->size_in_bytes();
            auto buffer = new char[sz+1];
            buffer[sz] = 0;
            auto cnt = bee_movie->read_all(0,sz,buffer);
            CHECK(sz == cnt);
            CHECK(sz > 0);
            CHECK(K::strlen(buffer) == cnt);
            delete[] buffer;
        }
   }

   Debug::printf("*** Read Block is big enough\n");


}

