#include <iostream>
#include <unordered_set>
#include <sstream>
#include <string>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include "BlockDevice.hpp" 
namespace fs = std::filesystem;

bool isOpen=false;



void commandHandling(const std::string& command,BlockDevice& device) {
    std::unordered_set<std::string> commands = {"create", "open", "info", "write","close","format","ls","cat","hexdump","copyout","copyin","rm"};

    std::istringstream stream(command);
    std::string type;
    stream >> type;

    if (commands.find(type) == commands.end()) {
        std::cout << "Invalid command\nAvailable commands:\ncreate\nopen\ninfo\nwrite\nclose\nformat\nls\ncat\nhexdump\ncopyout\ncopyin ";
        return;
    }
    //WORKING
    else if (type == "create") {
        std::string filename;
        std::size_t block_size, block_count;
        if (stream >> filename >> block_size >> block_count) {
            if(!device.findDev(filename)){
                std::cout << "Block Size: " << block_size << "Block count: " << block_count;
                device.create(filename, block_size, block_count);
                std::cout << "The device has been created\n";
            }else{
                std::cout << "Device already exists\n";
            }
            
                
            
        } else {
            std::cout << "Invalid parameters for create\n";
        }
        return;
    }
    //WORKING
    else if (type == "open") {
        if(isOpen){
            std::cout << "A device is currently open\n";
            return;
        }
        std::string filename;
        if (stream >> filename) {
            if (device.open(filename)) {
                std::cout << "Device " << filename << " opened successfully.\n";
                
                isOpen=true;
            } else {
                std::cout << "Device not found.\n";
            }
        }
        return;
    }
    //WORKING
    else if (type == "info") {
        if (isOpen) {
            device.printInfo();
        } else {
            std::cout << "No device is currently open\n";
        }
        return;
    }
    //WORKING
    else if (type == "write") {
    if (isOpen) {
        std::string text, filename;

       
        stream >> std::quoted(text) >> std::quoted(filename);

        if (!text.empty() && !filename.empty()) {
            int blockNumber = device.findEmptyBlock(); // Find an empty block
            
            // Write the data to the device
            device.write(text, filename);

            std::cout << "Writing to " << filename << "...\n";
        } else {
            std::cout << "Invalid arguments for write command. Usage: write \"text\" \"filename\"\n";
        }
    } else {
        std::cout << "No device is currently open\n";
    }
    return;
}

    //WORKING
    else if (type == "cat") {
        if (isOpen) {
            std::string file;
            stream >> std::quoted(file);
            std::cout << "Reading from " << device.getFileName() << "...\n";
            int blockNumber;
            if (!file.empty()) {
                if(blockNumber>device.getBlockCount()-1){
                std::cout << "El dispositivo solamente dispone de " << device.getBlockCount() << " bloques\n";
                return;
            }
               std::string contents = device.read(file);
               std::cout << contents << "\n";
            } else {
                std::cout << "Invalid block number\n";
            }
        } else {
            std::cout << "No device is currently open\n";
        }
        return;
    }
    //WORKING
    else if (type == "close") {
        if (isOpen) {
            std::cout << "Closing device " << device.getFileName() << "...\n";
            isOpen = false; 
        } else {
            std::cout << "No device is currently open\n";
        }
        return;
    }
    //NOT WORKING YET
    else if (type == "format"){
        if (isOpen) {
            std::cout << "Formatting device " << device.getFileName() << "...\n";
            device.formatDisk();
         } else {
            std::cout << "No device is currently open\n";
        }
        return;
    }
    //WORKING
    else if(type == "ls"){
         if (isOpen) {
            std::cout << "Listing files from: " << device.getFileName() << "...\n";
            device.ls();
         } else {
            std::cout << "No device is currently open\n";
        }
        return;
    }
    //WORKING
    else if (type == "hexdump") {
    if (isOpen) {
        std::string filename;

        
        stream >> std::quoted(filename);

        if (!filename.empty()) {
            std::cout << "Hexdump of " << filename << ":\n";

            // Call the hexDump function, passing the filename
            std::string contents = device.hexDump(filename);
            std::cout << contents <<"\n";
        } else {
            std::cout << "Invalid filename provided. Usage: hexdump \"filename\"\n";
        }
    } else {
        std::cout << "No device is currently open.\n";
    }
    return;
}

    //NOT WORKING YET
    else if(type == "copyout"){
         if (isOpen) {
            std::string file1, file2;

       
        stream >> std::quoted(file1) >> std::quoted(file2);

        if (!file1.empty() && !file2.empty()) {
            device.copy(file1,file2);
            std::cout << "Copying out " << device.getFileName() << "...\n";
        
        }
            
            
         } else {
            std::cout << "No device is currently open\n";
        }
        return;
    }
    //NOT WORKING YET
    else if(type == "copyin"){
         if (isOpen) {

            std::string file1, file2;

       
        stream >> std::quoted(file1) >> std::quoted(file2);

        if (!file1.empty() && !file2.empty()) {
            device.copy(file2,file1);
            std::cout << "Copying in " << device.getFileName() << "...\n";
        
        }
            
            //device.copyIn();
         } else {
            std::cout << "No device is currently open\n";
        }
        return;
    }
    // WORKING
    else if(type == "rm"){
         if (isOpen) {
            std::string filename;

        
        stream >> std::quoted(filename);

        if (!filename.empty()) {
            device.rm(filename);
            std::cout << "Removing file " << device.getFileName() << "...\n";
        }
            
            //device.remove();
          //  device.rm();
         } else {
            std::cout << "No device is currently open\n";
        }
        return;
    }

    
}

int main() {
    BlockDevice device;
    while (true) {
        std::string command;
        std::cout << "Enter a command:\n";
        std::getline(std::cin, command);
        
        commandHandling(command,device);
    }
}
