#include <iostream>
#include <fstream>
#include "TSystem.h"
#include "TSystemDirectory.h"
#include "TFile.h"

// Define flag constants using bitmask
#define FLAG_FOLDER_MISSING        0x01   // Target directory not found
#define FLAG_PSCAN_FOLDER_MISSING  0x02   // pscan_files directory missing
#define FLAG_DIR_ACCESS_ERROR      0x04   // Error accessing directory
#define FLAG_ELECTRON_TXT          0x08   // Incorrect electron txt file count
#define FLAG_HOLE_TXT              0x10   // Incorrect hole txt file count
#define FLAG_ELECTRON_ROOT         0x20   // Incorrect electron root file count
#define FLAG_HOLE_ROOT             0x40   // Incorrect hole root file count
#define FLAG_FILE_OPEN_ERROR       0x80   // File opening error

int CheckPscanFiles(const char* targetDir) {
    int resultFlags = 0;  // Initialize flag container

    // Get current working directory
    TString currentDir = gSystem->pwd();
    
    // Construct full path to target directory
    TString fullTargetPath = TString::Format("%s/%s", currentDir.Data(), targetDir);
    
    // Construct full path to pscan_files directory
    TString pscanDirPath = TString::Format("%s/%s/pscan_files", currentDir.Data(), targetDir);

    // Verify target directory exists
    if (gSystem->AccessPathName(fullTargetPath, kFileExists)) {
        std::cerr << "\n===== CRITICAL ERROR =====" << std::endl;
        std::cerr << "Target directory '" << targetDir << "' does not exist!" << std::endl;
        std::cerr << "Current location: " << currentDir << std::endl;
        std::cerr << "Expected path:    " << fullTargetPath << std::endl;
        
        std::cout << "\n===== Final Flags Status =====" << std::endl;
        std::cout << "FLAG 0 (Folder exists):  1" << std::endl;
        std::cout << "FLAG 1 (Electron txt):   1" << std::endl;
        std::cout << "FLAG 2 (Hole txt):       1" << std::endl;
        std::cout << "FLAG 3 (Electron root):  1" << std::endl;
        std::cout << "FLAG 4 (Hole root):      1" << std::endl;
        std::cout << "\nSummary: [TARGET FOLDER MISSING]" << std::endl;
        
        resultFlags |= FLAG_FOLDER_MISSING;  // Set missing directory flag
        return resultFlags;  // Return immediately with flags
    }
    
    // Verify pscan_files directory exists
    if (gSystem->AccessPathName(pscanDirPath, kFileExists)) {
        std::cerr << "\n===== CRITICAL ERROR =====" << std::endl;
        std::cerr << "Directory 'pscan_files' does not exist!" << std::endl;
        std::cerr << "Target folder: " << fullTargetPath << std::endl;
        std::cerr << "Expected path: " << pscanDirPath << std::endl;
        
        std::cout << "\n===== Final Flags Status =====" << std::endl;
        std::cout << "FLAG 0 (Folder exists):  1" << std::endl;
        std::cout << "FLAG 1 (Electron txt):   1" << std::endl;
        std::cout << "FLAG 2 (Hole txt):       1" << std::endl;
        std::cout << "FLAG 3 (Electron root):  1" << std::endl;
        std::cout << "FLAG 4 (Hole root):      1" << std::endl;
        std::cout << "\nSummary: [PSCAN_FILES FOLDER MISSING]" << std::endl;
        
        resultFlags |= FLAG_PSCAN_FOLDER_MISSING;  // Set missing pscan_files flag
        return resultFlags;  // Return immediately with flags
    }

    // Access pscan_files directory contents
    TSystemDirectory pscanDir("pscan_files", pscanDirPath);
    TList* files = pscanDir.GetListOfFiles();
    
    if (!files) {
        std::cerr << "Error: Could not read directory contents: " << pscanDirPath << std::endl;
        
        std::cout << "\n===== Final Flags Status =====" << std::endl;
        std::cout << "FLAG 0 (Folder access):  1" << std::endl;
        std::cout << "FLAG 1 (Electron txt):   1" << std::endl;
        std::cout << "FLAG 2 (Hole txt):       1" << std::endl;
        std::cout << "FLAG 3 (Electron root):  1" << std::endl;
        std::cout << "FLAG 4 (Hole root):      1" << std::endl;
        std::cout << "\nSummary: [DIRECTORY ACCESS ERROR]" << std::endl;
        
        resultFlags |= FLAG_DIR_ACCESS_ERROR;  // Set directory access flag
        return resultFlags;  // Return immediately with flags
    }

    int electronTxtCount = 0;   // Counter for electron txt files
    int holeTxtCount = 0;       // Counter for hole txt files
    int electronRootCount = 0;   // Counter for electron root files
    int holeRootCount = 0;       // Counter for hole root files
    bool openErrors = false;     // Track file opening issues

    // Iterate through all files in directory
    TSystemFile* file;
    TIter next(files);
    while ((file = (TSystemFile*)next())) {
        TString fileName = file->GetName();
        
        // Skip directories and special files (. and ..)
        if (file->IsDirectory() || fileName == "." || fileName == "..") continue;
        
        TString filePath = TString::Format("%s/%s", pscanDirPath.Data(), fileName.Data());

        // Process electron text files (ending with '_elect.txt')
        if (fileName.EndsWith("_elect.txt")) {
            electronTxtCount++;
            std::ifstream f_test(filePath.Data());
            if (!f_test.is_open()) {
                std::cerr << "Error: Cannot open electron txt file: " << filePath << std::endl;
                openErrors = true;
            } else {
                f_test.close();
            }
        }
        // Process hole text files (ending with '_holes.txt')
        else if (fileName.EndsWith("_holes.txt")) {
            holeTxtCount++;
            std::ifstream f_test(filePath.Data());
            if (!f_test.is_open()) {
                std::cerr << "Error: Cannot open hole txt file: " << filePath << std::endl;
                openErrors = true;
            } else {
                f_test.close();
            }
        }
        // Process electron ROOT files (ending with '_elect.root')
        else if (fileName.EndsWith("_elect.root")) {
            electronRootCount++;
            TFile* rootFile = TFile::Open(filePath, "READ");
            if (!rootFile || rootFile->IsZombie()) {
                std::cerr << "Error: Cannot open electron root file: " << filePath << std::endl;
                openErrors = true;
            }
            if (rootFile) rootFile->Close();
        }
        // Process hole ROOT files (ending with '_holes.root')
        else if (fileName.EndsWith("_holes.root")) {
            holeRootCount++;
            TFile* rootFile = TFile::Open(filePath, "READ");
            if (!rootFile || rootFile->IsZombie()) {
                std::cerr << "Error: Cannot open hole root file: " << filePath << std::endl;
                openErrors = true;
            }
            if (rootFile) rootFile->Close();
        }
    }

    // Calculate validation flags based on file counts
    bool flag_electron_txt = (electronTxtCount != 8);
    bool flag_hole_txt = (holeTxtCount != 8);
    bool flag_electron_root = (electronRootCount != 8);
    bool flag_hole_root = (holeRootCount != 8);

    // Update result flags based on validation
    if (flag_electron_txt) resultFlags |= FLAG_ELECTRON_TXT;
    if (flag_hole_txt) resultFlags |= FLAG_HOLE_TXT;
    if (flag_electron_root) resultFlags |= FLAG_ELECTRON_ROOT;
    if (flag_hole_root) resultFlags |= FLAG_HOLE_ROOT;
    if (openErrors) resultFlags |= FLAG_FILE_OPEN_ERROR;

    // Generate validation report
    std::cout << "\n===== Validation Report =====" << std::endl;
    std::cout << "Current location:      " << currentDir << std::endl;
    std::cout << "Target directory:      " << targetDir << std::endl;
    std::cout << "Full target path:      " << fullTargetPath << std::endl;
    std::cout << "Pscan files location: " << pscanDirPath << std::endl;
    std::cout << "Folder status:         EXISTS | FLAG 0: OK" << std::endl;
    
    std::cout << "Electron txt:  " << electronTxtCount << "/8 | "
              << "FLAG 1: " << (flag_electron_txt ? "TRIGGERED" : "OK") 
              << (electronTxtCount < 8 ? " (MISSING)" : (electronTxtCount > 8 ? " (EXTRA)" : "")) << std::endl;
              
    std::cout << "Hole txt:      " << holeTxtCount << "/8 | "
              << "FLAG 2: " << (flag_hole_txt ? "TRIGGERED" : "OK")
              << (holeTxtCount < 8 ? " (MISSING)" : (holeTxtCount > 8 ? " (EXTRA)" : "")) << std::endl;
              
    std::cout << "Electron root: " << electronRootCount << "/8 | "
              << "FLAG 3: " << (flag_electron_root ? "TRIGGERED" : "OK")
              << (electronRootCount < 8 ? " (MISSING)" : (electronRootCount > 8 ? " (EXTRA)" : "")) << std::endl;
              
    std::cout << "Hole root:     " << holeRootCount << "/8 | "
              << "FLAG 4: " << (flag_hole_root ? "TRIGGERED" : "OK")
              << (holeRootCount < 8 ? " (MISSING)" : (holeRootCount > 8 ? " (EXTRA)" : "")) << std::endl;
              
    std::cout << "File access:    " << (openErrors ? "ERRORS" : "OK") << std::endl;
              
    std::cout << "\n===== Final Flags Status =====" << std::endl;
    std::cout << "FLAG 0 (Folder exists):   " << 0 << std::endl;
    std::cout << "FLAG 1 (Electron txt):    " << flag_electron_txt << std::endl;
    std::cout << "FLAG 2 (Hole txt):        " << flag_hole_txt << std::endl;
    std::cout << "FLAG 3 (Electron root):   " << flag_electron_root << std::endl;
    std::cout << "FLAG 4 (Hole root):       " << flag_hole_root << std::endl;
    
    std::cout << "\nSummary: ";
    if (!flag_electron_txt && !flag_hole_txt && 
        !flag_electron_root && !flag_hole_root && !openErrors) {
        std::cout << "ALL CHECKS PASSED";
    } else {
        if (flag_electron_txt) std::cout << "[ELECTRON TXT] ";
        if (flag_hole_txt) std::cout << "[HOLE TXT] ";
        if (flag_electron_root) std::cout << "[ELECTRON ROOT] ";
        if (flag_hole_root) std::cout << "[HOLE ROOT] ";
        if (openErrors) std::cout << "[FILE ACCESS]";
    }
    std::cout << std::endl;

    return resultFlags;  // Return combined flags as bitmask
}