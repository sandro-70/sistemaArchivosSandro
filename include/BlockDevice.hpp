// BlockDevice.hpp
#ifndef BLOCKDEVICE_HPP
#define BLOCKDEVICE_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <iterator>

class BlockDevice {
public:
struct superBloque{
        uint32_t byteMapStart;
        uint32_t inodeTableStart;
        uint32_t dataBlockStart;
        uint32_t maxInodes;
        uint32_t inodeTableBlocks;
        uint32_t byteMapBlocks;
        uint32_t firstDataBlock;
    };
    struct inode{
    
    uint64_t indices[8];   
    char filename[64];     
    uint32_t fileSize;         
    bool isUsed;
    char padding[3];

    };
    BlockDevice() = default;
    BlockDevice(std::size_t blockSize, std::size_t blockCount, const std::string& filename);
    bool create(const std::string& filename, std::size_t blockSize, std::size_t blockCount);
    void write(std::string& data,std::string& name);
    
    void printInfo();
    std::string read(const std::string& name); 
    std::size_t getBlockCount()const;
    
    std::string getFileName();
    bool open(const std::string& filename);
    bool findDev(const std::string& filename);
    void initializeInodeTable();
    uint32_t getMaxInodes();
    uint32_t calculateBlockOffset(int blocknumber);
    int findEmptyBlock();
    void markUsed(int block);
    void markEmpty(int block);
    inode* fileExists(const std::string& filename);
    inode* findEmptyInode();
    void ls();
    void formatDisk();
    void rm(const std::string& name);
    void copy(const std::string& origin, std::string& receiver);
    std::string hexDump(const std::string& name);
    void updateInode(const std::string& name);
    void overWriteByteMap();
    void overWriteInodeTable();

private:
  
    std::size_t blockSize;
    std::size_t blockCount;
    std::string filename;
    std::vector<uint8_t>blockmap;
    uint32_t maxInodes;
    uint32_t inodeTableStart;
    uint32_t byteMapStart;
    std::vector<inode> inodeTable;
    superBloque superB;
};

#endif // BLOCKDEVICE_HPP
