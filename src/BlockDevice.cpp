#include<iostream>
#include<unordered_set>
#include<sstream>
#include<string>
#include<vector>
#include<unordered_map>
#include<fstream>
#include"BlockDevice.hpp"
#include<filesystem>
#include<cstring>
#include <iomanip>
namespace fs = std::filesystem;


/*
Estructura del Block Device:
-Blocksize
-BlockCount
-Contenido

Agregar:
-SuperBloque
-Mapa de Bloques Libres
-Tabla de Inodos


*/
//Bloque 0: Superbloque
//Bloque 1: Mapa de Bloques Libres

    
    BlockDevice::BlockDevice(std::size_t blockSize = 0, std::size_t blockCount = 0, const std::string& filename = "")
        : blockSize(blockSize), blockCount(blockCount), filename(filename) {}
    
    //CREATE DEVICE
     bool BlockDevice::create(const std::string& filename, std::size_t blockSize, std::size_t blockCount) {
        //Initialize SuperBlock
        this->blockSize = blockSize;
        this->blockCount = blockCount;
        std::cout << "Debug: Starting create function" << std::endl;
        std::cout << "Block Size: " << blockSize << ", Block Count: " << blockCount << std::endl;

        
        if (blockSize > SIZE_MAX / blockCount) {
    std::cerr << "Overflow in block size and block count multiplication" << std::endl;
    return false;
        }
        std::cout << "Block Size: " << blockSize << " Block Count: " << blockCount;

        //round up
        std::cout << "Debug: byteMapBlocks calculation" << std::endl;
        int byteMapBlocks = (blockCount + blockSize - 1) / blockSize;
        std::cout << "byteMapBlocks: " << byteMapBlocks << std::endl;

        superB.byteMapBlocks = byteMapBlocks;
        superB.byteMapStart = calculateBlockOffset(1);
        
        superB.inodeTableStart = calculateBlockOffset(1 + byteMapBlocks);
        
        superB.maxInodes = getMaxInodes();
        std::cout << "byteMapBlocks: " << byteMapBlocks << std::endl;
        superB.dataBlockStart = calculateBlockOffset(1 + byteMapBlocks + superB.inodeTableBlocks);
        superB.firstDataBlock = 1 + byteMapBlocks + superB.inodeTableBlocks;

        //Initialize bytemap
        
        blockmap.resize(blockCount,0);
        int reservedBlocks = 2 + byteMapBlocks + superB.inodeTableBlocks;
        for(int i = 0; i < reservedBlocks; i++){
            markUsed(i);
        }
        for(int i = reservedBlocks; i < blockmap.size();i++){
            markEmpty(i);
        }
        
        //Inode Table
        inodeTable.reserve(superB.maxInodes);
        

        //Create file
        std::size_t totalSize = (blockSize * blockCount) + sizeof(blockSize) + sizeof(blockCount);
        std::ofstream outFile(filename, std::ios::binary);

        if (!outFile) {
            std::cerr << "Error creating file: " << filename << std::endl;
            return false;
        }
        
        
        //Header
        outFile.write(reinterpret_cast<const char*>(&blockSize), sizeof(blockSize));
        outFile.write(reinterpret_cast<const char*>(&blockCount), sizeof(blockCount));
        std::streampos dataOffset = sizeof(blockSize) + sizeof(blockCount); 
        std::streampos blockStart = dataOffset;

        outFile.seekp(blockStart);
        
        //Write superblock on disk
        outFile.write(reinterpret_cast<char*>(&superB.byteMapStart),sizeof(superB.byteMapStart));
        outFile.write(reinterpret_cast<char*>(&superB.inodeTableStart),sizeof(superB.inodeTableStart));
        outFile.write(reinterpret_cast<char*>(&superB.maxInodes),sizeof(superB.maxInodes));
        outFile.write(reinterpret_cast<char*>(&superB.dataBlockStart),sizeof(superB.dataBlockStart));
        outFile.write(reinterpret_cast<char*>(&superB.byteMapBlocks),sizeof(superB.byteMapBlocks));
        outFile.write(reinterpret_cast<char*>(&superB.inodeTableBlocks),sizeof(superB.inodeTableBlocks));
        outFile.write(reinterpret_cast<char*>(&superB.firstDataBlock),sizeof(superB.firstDataBlock));

        //bytemap on disk
        outFile.seekp(superB.byteMapStart);
        outFile.write(reinterpret_cast<char*>(blockmap.data()),blockmap.size());
        
        //inode table on disk
        initializeInodeTable();

        outFile.seekp(totalSize - 1); 
        outFile.write("", 1);          
        outFile.close();
        
        std::cout << "Block device " << filename << " created with " << blockCount << " blocks of " << blockSize << " bytes." << std::endl;
        return true;
    }

    //WRITE DATA ON FILE
    void BlockDevice::write(std::string& data, std::string& file) {
    // Open the file in binary mode
    std::ofstream outFile(filename, std::ios::binary | std::ios::in | std::ios::out);
    if (!outFile) {
        std::cerr << "Error opening file for writing: " << file << std::endl;
        return;
    }

    // Check if the file already exists in the inode table
    for (auto& inode : inodeTable) {
        if (inode.isUsed && std::string(inode.filename) == file) {
            // Append data to the existing file
            if (inode.fileSize + data.size() > blockSize * 8) {
                std::cerr << "Not enough space to append data. Truncating to fit.\n";
                data = data.substr(0, blockSize * 8 - inode.fileSize);
            }

            // Calculate the block to start writing
            std::streampos blockStart = sizeof(blockSize) + sizeof(blockCount) + (inode.indices[0] * blockSize) + inode.fileSize;

            outFile.seekp(blockStart);
            outFile.write(data.c_str(), data.size());

            // Update inode metadata
            inode.fileSize += data.size();

            outFile.close();
            std::cout << "Appended data to file: " << file << "\n";
            return;
        }
    }

    // If the file doesn't exist, create a new one
    int blockNumber = findEmptyBlock();
    if (blockNumber == -1) {
        std::cerr << "No empty blocks available.\n";
        return;
    }

    // Set up inode
    inode* newInode = findEmptyInode();
    std::memset(newInode->filename, '\0', sizeof(newInode->filename));
    std::strncpy(newInode->filename, file.c_str(), sizeof(newInode->filename) - 1);
    newInode->fileSize = 0;
    newInode->isUsed = true;
    newInode->indices[0] = blockNumber;

    // Write data to the block
    std::streampos dataOffset = sizeof(blockSize) + sizeof(blockCount); 
    std::streampos blockStart = dataOffset + static_cast<std::streamoff>(blockNumber * blockSize);


    outFile.seekp(blockStart);
    std::string clearData(blockSize, '\0'); 
    outFile.write(clearData.c_str(), blockSize);
    outFile.seekp(blockStart);

    if (data.size() > blockSize*8) {
        std::cerr << "Data exceeds block size. Truncating to fit.\n";
        outFile.write(data.c_str(), blockSize*8);  
        newInode->fileSize += blockSize*8;
    } else {
        outFile.write(data.c_str(), data.size());
        newInode->fileSize += data.size();
    }
    //roundUp
    //int blocksOccupied = (newInode->fileSize+blockSize-1)/blockSize;
    for(int i = 0;i < 8;i++){
        newInode->indices[i] = blockNumber;
        markUsed(blockNumber);
        blockNumber++;
        
    }
    // Add new inode to the table
    

    outFile.close();
    overWriteByteMap();
    overWriteInodeTable();
    std::cout << "Wrote data to new file: " << file << "\n"
              << "On block number: " << blockNumber - 8 << "\n";
}

     //READ FILE
   std::string BlockDevice::read(const std::string& name) {

    
    std::ifstream inFile(filename, std::ios::binary);
    if (!inFile) {
        std::cerr << "Error opening file for reading: " << filename << std::endl;
        return "";
    }
    inode* inode = fileExists(name);
    if(inode == nullptr || !inode->isUsed){
        std::cout << "The file does not exist";
        return "";
    }

    int blockNumber = inode->indices[0];
    std::streampos dataOffset = sizeof(blockSize) + sizeof(blockCount); 
    std::streampos blockStart = dataOffset + static_cast<std::streamoff>(blockNumber * blockSize);


    
    inFile.seekg(blockStart);

   
    char* buffer = new char[blockSize];
    inFile.read(buffer, blockSize);

   
    if (!inFile) {
        std::cerr << "Error reading data from block " << blockNumber << std::endl;
        delete[] buffer;
        return "";
    }


    std::cout << "Data read from block " << blockNumber << ":\n";
    std::string data(buffer, blockSize);

    
    delete[] buffer;
    return data;
}
    //LOAD BINARY FILE INTO MEMORY
   bool BlockDevice::open(const std::string& filename) {
    std::cout << "Finding device..." << std::endl;
    
    if (findDev(filename)) {
        std::cout << "Device found: " << filename << std::endl;

        std::ifstream inFile(filename, std::ios::binary);
        if (!inFile) {
            std::cerr << "Error opening file for reading: " << filename << std::endl;
            return false;
        }
        std::cout << "Opening file..." << std::endl;

        std::size_t blockSize;
        std::size_t blockCount;

        // HEADER
        inFile.read(reinterpret_cast<char*>(&blockSize), sizeof(blockSize));
        inFile.read(reinterpret_cast<char*>(&blockCount), sizeof(blockCount));
        
        if (inFile.fail()) {
            std::cerr << "Error reading block size or count from file" << std::endl;
            return false;
        }

        this->filename = filename;
        this->blockSize = blockSize;
        this->blockCount = blockCount;
        std::cout << "Block Count: " << blockCount << std::endl;

        blockmap.resize(blockCount, 0);

        // SUPERBLOQUE
        inFile.seekg(calculateBlockOffset(0));
        inFile.read(reinterpret_cast<char*>(&superB.byteMapStart), sizeof(superB.byteMapStart));
        inFile.read(reinterpret_cast<char*>(&superB.inodeTableStart), sizeof(superB.inodeTableStart));
        inFile.read(reinterpret_cast<char*>(&superB.maxInodes), sizeof(superB.maxInodes));
        inFile.read(reinterpret_cast<char*>(&superB.dataBlockStart), sizeof(superB.dataBlockStart));
        inFile.read(reinterpret_cast<char*>(&superB.byteMapBlocks), sizeof(superB.byteMapBlocks));
        inFile.read(reinterpret_cast<char*>(&superB.inodeTableBlocks), sizeof(superB.inodeTableBlocks));
        inFile.read(reinterpret_cast<char*>(&superB.firstDataBlock), sizeof(superB.firstDataBlock));

        // BYTEMAP
        inFile.seekg(superB.byteMapStart);
        inFile.read(reinterpret_cast<char*>(blockmap.data()), blockCount);
        if (inFile.fail()) {
            std::cerr << "Error reading byte map" << std::endl;
            return false;
        }

        // TABLA DE INODOS
        int inodeTableLength = superB.inodeTableBlocks * 136;
        inodeTable.resize(superB.maxInodes);
        std::cout << "Inode table blocks: " << superB.inodeTableBlocks << std::endl;
        inFile.seekg(superB.inodeTableStart);
        for(int i = 0;i < inodeTable.size();i++){
            inode tempInode;
            inFile.read(reinterpret_cast<char*>(&tempInode),sizeof(inode));
            inodeTable.at(i) = tempInode;
        }
        
        if (inFile.fail()) {
            std::cerr << "Error reading inode table" << std::endl;
            return false;
        }

        std::cout << "Block device opened: " << filename << std::endl;
        std::cout << "Block size: " << blockSize << std::endl;
        std::cout << "Block count: " << blockCount << std::endl;
        return true;
    } else {
        std::cerr << "Device not found: " << filename << std::endl;
        return false;
    }
}


    void BlockDevice::formatDisk(){

    int blockStart = superB.firstDataBlock;
    for(int i = blockStart; i < blockCount;i++){
    markEmpty(i);
    std::cout << "Block number: " << i << " marked as empty";
    }
    for(auto& inode: inodeTable){
        inode.isUsed = false;
    }
    

    //overWrite bytemap
    overWriteByteMap();
    overWriteInodeTable();
    std::cout << "Disk formatted";
    }

    void BlockDevice::ls(){
    for (auto& inode : inodeTable) {
    if(inode.isUsed){
        std::cout << inode.filename << std::endl;
    }
    }
}
    void BlockDevice::rm(const std::string& name){
for(auto& inode: inodeTable){
    if(name == inode.filename){
        inode.isUsed = false;
        for(int i = 0; i < 8; i ++){
            markEmpty(inode.indices[i]);
        }
        overWriteByteMap();
        overWriteInodeTable();
        std::cout << "File removed: " << inode.filename;
        return;
    }
}
std::cout << "File not found";
}

void BlockDevice::copy(const std::string& host,  std::string& receiver){
    inode* hostFile = fileExists(host);
    inode* receiverFile = fileExists(receiver);
    if(hostFile && receiverFile){
        std::string data = read(host);
        write(data,receiver);
    }else{
        std::cout << "File(s) not found";
    }
}


std::string BlockDevice::hexDump(const std::string& filename) {
    // Read the file content into a string
    std::string dataToHex = read(filename);
    
    if (dataToHex.empty()) {
        return "";  // Return empty if no data was read
    }

    // Use a stringstream to build the hex dump
    std::stringstream hexStream;
    
    // Loop through each character in the data string
    for (size_t i = 0; i < dataToHex.size(); ++i) {
        // Print each byte in hexadecimal format
        hexStream << std::setw(2) << std::setfill('0') << std::hex << (static_cast<unsigned int>(dataToHex[i]) & 0xFF) << " ";
    }

    // Return the hex dump as a string
    return hexStream.str();
}

    u_int32_t BlockDevice::calculateBlockOffset(int blocknumber){
        int dataOffset = sizeof(blockSize) + sizeof(blockCount);
        return (blocknumber * blockSize) + dataOffset;
    }

    //SET BYTEMAP BLOCK AS USED
    void BlockDevice::markUsed(int block){
        blockmap.at(block) = 1;

    }
    //SET BYTEMAP BLOCK AS EMPTY
    void BlockDevice::markEmpty(int block){
        blockmap.at(block) = 0;
    }

    //INODE TABLE
    void BlockDevice::initializeInodeTable(){
       
        std::ofstream outFile(filename, std::ios::binary);
        for(auto& inode:inodeTable){
            inode.isUsed = false;
            inode.fileSize = 0;
            std::fill(std::begin(inode.filename), std::end(inode.filename), '\0');
            std::fill(std::begin(inode.indices), std::end(inode.indices), 0);
        }
        outFile.seekp(inodeTableStart);
        outFile.write(reinterpret_cast<const char*>(inodeTable.data()), inodeTable.size() * sizeof(inode)); 
    }

    //CALCULATE NUMBER OF INODES
   uint32_t BlockDevice::getMaxInodes(){
    //rounddown
    int inodesperBlock = blockSize/136;
    //roundup
    int inodeBlocks = (blockCount + inodesperBlock -1)/inodesperBlock;
    superB.inodeTableBlocks = inodeBlocks;
    return inodeBlocks*inodesperBlock;
   }

  //GET EMPTY INODE FROM TABLE
   BlockDevice::inode* BlockDevice::findEmptyInode(){
    for(auto& inode:inodeTable){
        if(!inode.isUsed)return &inode;
    }
    return nullptr;
   }


    //CHECK IF FILE EXISTS
   BlockDevice::inode* BlockDevice::fileExists(const std::string& filename){
   int i=0;
    for(auto& inode:inodeTable){
        
        //std::cout << "inode number: " << i << "name: "<< inode.filename;
        if(inode.filename == filename){return &inode;}
        i++;
    }
    return nullptr;
   }
   

    //PRINT
    void BlockDevice::printInfo(){
        std::cout << "Block device: " << filename << std::endl;
        std::cout << "Block size: " << blockSize << std::endl;
        std::cout << "Block count: " << blockCount << std::endl;
    }


    std::size_t BlockDevice::getBlockCount() const {
        return blockCount;
    }

    std::string BlockDevice::getFileName(){
        return filename;
    }

    

    bool BlockDevice::findDev(const std::string& filename) {
    fs::path currentDir = fs::current_path(); // Get the current directory path
    for (const auto& entry : fs::directory_iterator(currentDir)) {
        if (entry.is_regular_file() && entry.path().filename() == filename) {
            std::cout << "File found: " << entry.path() << std::endl;
            return true;
        }
    }
    
    return false;
}

int BlockDevice::findEmptyBlock() {
    for (size_t i = 0; i < blockmap.size(); ++i) { 
        if (blockmap[i] == 0) {
            return i; 
        }
    }
    return -1; 
}




void BlockDevice::overWriteByteMap(){
    std::ofstream outFile(filename, std::ios::binary | std::ios::in | std::ios::out);
    if (!outFile) {
        //std::cerr << "Error opening file for writing: " << file << std::endl;
        return;
    }
    outFile.seekp(superB.byteMapStart);
    outFile.write(reinterpret_cast<char*>(blockmap.data()),blockmap.size());
    outFile.close();
}
void BlockDevice::overWriteInodeTable(){
    std::ofstream outFile(filename, std::ios::binary | std::ios::in | std::ios::out);
    if (!outFile) {
        //std::cerr << "Error opening file for writing: " << file << std::endl;
        return;
    }
    outFile.seekp(superB.inodeTableStart);
    outFile.write(reinterpret_cast<const char*>(inodeTable.data()), inodeTable.size() * sizeof(inode));
    outFile.close();
}








