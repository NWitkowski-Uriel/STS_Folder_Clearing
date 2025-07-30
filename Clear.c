#include <TSystem.h>
#include <TString.h>
#include <TSystemDirectory.h>
#include <TSystemFile.h>
#include <TList.h>
#include <TInterpreter.h>
#include <TFile.h>
#include <TCanvas.h>
#include <TPaveText.h>
#include <TLatex.h>
#include <TPie.h>
#include <TLegend.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <iomanip>
#include <ctime>
#include "TSystem.h"
#include "TSystemDirectory.h"
#include <TObjString.h>  
#include <TCollection.h> 

// ===================================================================
// Flag definitions (updated to include all flags from Check* programs)
// ===================================================================
// From CheckLogFiles.c
#define LOG_FLAG_DIR_MISSING         0x01
#define LOG_FLAG_LOG_MISSING         0x02
#define LOG_FLAG_DATA_MISSING        0x04
#define LOG_FLAG_NO_FEB_FILE         0x08
#define LOG_FLAG_FILE_OPEN           0x10
#define LOG_FLAG_DATA_EMPTY          0x20
#define LOG_FLAG_DATA_INVALID        0x40
#define LOG_FLAG_UNEXPECTED_FILES    0x80

// From CheckTrimFiles.c
#define TRIM_FLAG_TRIM_FOLDER_MISSING  0x01
#define TRIM_FLAG_DIR_ACCESS           0x02
#define TRIM_FLAG_ELECTRON_COUNT       0x04
#define TRIM_FLAG_HOLE_COUNT           0x08
#define TRIM_FLAG_FILE_OPEN            0x10
#define TRIM_FLAG_UNEXPECTED_FILES     0x20

// From CheckPscanFiles.c
#define PSCAN_FLAG_PSCAN_FOLDER_MISSING  0x01
#define PSCAN_FLAG_DIR_ACCESS            0x02
#define PSCAN_FLAG_ELECTRON_TXT          0x04
#define PSCAN_FLAG_HOLE_TXT              0x08
#define PSCAN_FLAG_ELECTRON_ROOT         0x10
#define PSCAN_FLAG_HOLE_ROOT             0x20
#define PSCAN_FLAG_FILE_OPEN             0x40
#define PSCAN_FLAG_MODULE_ROOT           0x80
#define PSCAN_FLAG_MODULE_TXT            0x100
#define PSCAN_FLAG_MODULE_PDF            0x200
#define PSCAN_FLAG_UNEXPECTED_FILES      0x400

// From CheckConnFiles.c
#define CONN_FLAG_CONN_FOLDER_MISSING    0x01
#define CONN_FLAG_DIR_ACCESS             0x02
#define CONN_FLAG_ELECTRON_COUNT         0x04
#define CONN_FLAG_HOLE_COUNT             0x08
#define CONN_FLAG_FILE_OPEN              0x10
#define CONN_FLAG_UNEXPECTED_FILES       0x20

typedef int (*CheckFunction)(const char*);

// Global variables
std::vector<std::string> gReportPages;
std::string gGlobalSummary;
int gPassedDirs = 0;
int gFailedDirs = 0;

// Helper class to capture console output
class OutputCapture {
public:
    OutputCapture() {
        oldBuffer = std::cout.rdbuf(buffer.rdbuf());
    }
    ~OutputCapture() {
        std::cout.rdbuf(oldBuffer);
    }
    std::string getOutput() { return buffer.str(); }
    void clear() { buffer.str(""); }

private:
    std::stringstream buffer;
    std::streambuf* oldBuffer;
};

// Flag decoding functions (updated to include all flags)
std::string decodeLogFlags(int flags) {
    if (flags == 0) return "OK";
    
    std::vector<std::string> messages;
    if (flags & LOG_FLAG_DIR_MISSING)      messages.push_back("DIR_MISSING");
    if (flags & LOG_FLAG_LOG_MISSING)      messages.push_back("LOG_MISSING");
    if (flags & LOG_FLAG_DATA_MISSING)     messages.push_back("DATA_MISSING");
    if (flags & LOG_FLAG_NO_FEB_FILE)      messages.push_back("NO_FEB_FILE");
    if (flags & LOG_FLAG_FILE_OPEN)        messages.push_back("FILE_OPEN");
    if (flags & LOG_FLAG_DATA_EMPTY)       messages.push_back("DATA_EMPTY");
    if (flags & LOG_FLAG_DATA_INVALID)     messages.push_back("DATA_INVALID");
    if (flags & LOG_FLAG_UNEXPECTED_FILES) messages.push_back("UNEXPECTED_FILES");
    
    std::string result;
    for (size_t i = 0; i < messages.size(); ++i) {
        if (i > 0) result += " | ";
        result += messages[i];
    }
    return result;
}

std::string decodeTrimFlags(int flags) {
    if (flags == 0) return "OK";
    
    std::vector<std::string> messages;
    if (flags & TRIM_FLAG_TRIM_FOLDER_MISSING) messages.push_back("TRIM_FILES_FOLDER_MISSING");
    if (flags & TRIM_FLAG_DIR_ACCESS)          messages.push_back("DIR_ACCESS");
    if (flags & TRIM_FLAG_ELECTRON_COUNT)      messages.push_back("ELECTRON_COUNT");
    if (flags & TRIM_FLAG_HOLE_COUNT)          messages.push_back("HOLE_COUNT");
    if (flags & TRIM_FLAG_FILE_OPEN)           messages.push_back("FILE_OPEN");
    if (flags & TRIM_FLAG_UNEXPECTED_FILES)    messages.push_back("UNEXPECTED_FILES");
    
    std::string result;
    for (size_t i = 0; i < messages.size(); ++i) {
        if (i > 0) result += " | ";
        result += messages[i];
    }
    return result;
}

std::string decodePscanFlags(int flags) {
    if (flags == 0) return "OK";
    
    std::vector<std::string> messages;
    if (flags & PSCAN_FLAG_PSCAN_FOLDER_MISSING)  messages.push_back("PSCAN_FILES_FOLDER_MISSING");
    if (flags & PSCAN_FLAG_DIR_ACCESS)            messages.push_back("DIR_ACCESS");
    if (flags & PSCAN_FLAG_ELECTRON_TXT)          messages.push_back("ELECTRON_TXT_COUNT");
    if (flags & PSCAN_FLAG_HOLE_TXT)              messages.push_back("HOLE_TXT_COUNT");
    if (flags & PSCAN_FLAG_ELECTRON_ROOT)         messages.push_back("ELECTRON_ROOT_COUNT");
    if (flags & PSCAN_FLAG_HOLE_ROOT)             messages.push_back("HOLE_ROOT_COUNT");
    if (flags & PSCAN_FLAG_FILE_OPEN)             messages.push_back("FILE_OPEN");
    if (flags & PSCAN_FLAG_MODULE_ROOT)           messages.push_back("MODULE_ROOT");
    if (flags & PSCAN_FLAG_MODULE_TXT)            messages.push_back("MODULE_TXT");
    if (flags & PSCAN_FLAG_MODULE_PDF)            messages.push_back("MODULE_PDF_MISSING");
    if (flags & PSCAN_FLAG_UNEXPECTED_FILES)      messages.push_back("UNEXPECTED_FILES");
    
    std::string result;
    for (size_t i = 0; i < messages.size(); ++i) {
        if (i > 0) result += " | ";
        result += messages[i];
    }
    return result;
}

std::string decodeConnFlags(int flags) {
    if (flags == 0) return "OK";
    
    std::vector<std::string> messages;
    if (flags & CONN_FLAG_CONN_FOLDER_MISSING)   messages.push_back("CONN_CHECK_FILES_FOLDER_MISSING");
    if (flags & CONN_FLAG_DIR_ACCESS)            messages.push_back("DIR_ACCESS");
    if (flags & CONN_FLAG_ELECTRON_COUNT)        messages.push_back("ELECTRON_COUNT");
    if (flags & CONN_FLAG_HOLE_COUNT)            messages.push_back("HOLE_COUNT");
    if (flags & CONN_FLAG_FILE_OPEN)             messages.push_back("FILE_OPEN");
    if (flags & CONN_FLAG_UNEXPECTED_FILES)      messages.push_back("UNEXPECTED_FILES");
    
    std::string result;
    for (size_t i = 0; i < messages.size(); ++i) {
        if (i > 0) result += " | ";
        result += messages[i];
    }
    return result;
}

void SaveTxtReport(const std::string& filename) {
    std::ofstream out(filename.c_str());
    out << "VALIDATION REPORT\n";
    out << "====================================================\n\n";
    
    for (size_t i = 0; i < gReportPages.size(); i++) {
        out << gReportPages[i] << "\n\n";
    }
    
    out << gGlobalSummary << std::endl;
    out.close();
}

void SaveRootReport(const std::string& filename) {
    TFile file(filename.c_str(), "RECREATE");
    
    for (size_t i = 0; i < gReportPages.size(); i++) {
        std::string name = "Directory_" + std::to_string(i);
        TObjString* obj = new TObjString(gReportPages[i].c_str());
        obj->Write(name.c_str());
        delete obj;
    }
    
    TObjString* summary = new TObjString(gGlobalSummary.c_str());
    summary->Write("GlobalSummary");
    delete summary;
    file.Close();
}

void SavePdfReport(const std::string& filename, const std::string& currentDir) {
    TCanvas canvas("canvas", "Validation Report", 800, 1200);
    canvas.Print((filename + "[").c_str());

    // Title page
    canvas.cd();
    TLatex title;
    title.SetTextSize(0.04);
    title.SetTextAlign(22);
    title.DrawLatexNDC(0.5, 0.7, TString::Format("Validation report for ladder: %s", currentDir.c_str()));
    
    // Get current date and time
    time_t now = time(0);
    struct tm tstruct;
    char buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);
    
    TLatex subtitle;
    subtitle.SetTextSize(0.03);
    subtitle.DrawLatexNDC(0.5, 0.5, TString::Format("Generated on: %s", buf));
    canvas.Print(filename.c_str());
    
    // Directory reports
    for (const auto& page : gReportPages) {
        canvas.Clear();
        
        // Content panel
        TPad* contentPad = (TPad*)canvas.cd(2);
        contentPad->SetPad(0.05, 0.05, 0.95, 0.95);
        contentPad->Draw();
        contentPad->cd();
        
        // Split text into lines
        std::vector<std::string> lines;
        std::istringstream stream(page);
        std::string line;
        while (std::getline(stream, line)) {
            lines.push_back(line);
        }
        
        // Draw formatted text
        double y = 1.;
        double textSize = 0.02;
        
        for (const auto& ln : lines) {
            TText* t = new TText(0.03, y, ln.c_str());
            t->SetTextSize(textSize);
            
            // Color coding
            if (ln.find("FAILED") != std::string::npos) {
                t->SetTextColor(kRed);
            } else if (ln.find("PASSED") != std::string::npos) {
                t->SetTextColor(kGreen+2);
            } else if (ln.find("ERROR") != std::string::npos) {
                t->SetTextColor(kOrange+7);
            } else if (ln.find("DIRECTORY") != std::string::npos) {
                t->SetTextColor(kBlue);
                t->SetTextSize(textSize * 1.1);
            }
            
            t->Draw();
            y -= 0.015;
        }
        
        canvas.Print(filename.c_str());
    }

    // Summary page
    canvas.Clear();
    canvas.Divide(1, 2);
    
    // Summary header
    TPad* summaryHeader = (TPad*)canvas.cd(1);
    summaryHeader->SetPad(0.05, 0.85, 0.95, 0.95);
    summaryHeader->SetFillColor(8);
    summaryHeader->Draw();
    summaryHeader->cd();
    
    TText summaryTitle(0.5, 0.5, "GLOBAL VALIDATION SUMMARY");
    summaryTitle.SetTextAlign(22);
    summaryTitle.SetTextSize(0.4);
    summaryTitle.SetTextColor(kWhite);
    summaryTitle.Draw();
    
    // Summary content
    canvas.cd(2);
    TPad* summaryContent = (TPad*)canvas.cd(2);
    summaryContent->SetPad(0.05, 0.05, 0.95, 0.85);
    summaryContent->Draw();
    summaryContent->cd();
    
    // Pie chart
    if (gPassedDirs > 0 || gFailedDirs > 0) {
        TPie* pie = new TPie("pie", "", 2);
        
        pie->SetEntryVal(0, gPassedDirs);
        pie->SetEntryLabel(0, "Passed");
        pie->SetEntryFillColor(0, kGreen);
        
        pie->SetEntryVal(1, gFailedDirs);
        pie->SetEntryLabel(1, "Failed");
        pie->SetEntryFillColor(1, kRed);
        
        pie->SetCircle(0.5, 0.25, 0.15);
        pie->Draw("r");
    }
    
    // Text statistics
    double y = 0.95;
    std::istringstream summaryStream(gGlobalSummary);
    std::string summaryLine;
    while (std::getline(summaryStream, summaryLine)) {
        TText* t = new TText(0.1, y, summaryLine.c_str());
        t->SetTextSize(0.025);
        
        if (summaryLine.find("Success rate") != std::string::npos) {
            t->SetTextColor(kGreen+2);
            t->SetTextSize(0.025);
        }
        
        t->Draw();
        y -= 0.05;
    }
    
    canvas.Print(filename.c_str());
    canvas.Print((filename + "]").c_str());
}

void Clear() {
    OutputCapture capture;
    gReportPages.clear();
    gGlobalSummary.clear();
    gPassedDirs = 0;
    gFailedDirs = 0;

    // Get current directory name
    TString currentDir = gSystem->BaseName(gSystem->WorkingDirectory());
    
    // Get list of directories
    TSystemDirectory dir(".", ".");
    TList* files = dir.GetListOfFiles();
    if (!files) {
        std::cerr << "Error: Could not get file list!" << std::endl;
        return;
    }

    std::vector<TString> dirNames;
    TIter next(files);
    TSystemFile* file;
    TObject* obj;
    while ((obj = next())) {
        file = dynamic_cast<TSystemFile*>(obj);
        if (!file) continue;
        
        TString name = file->GetName();
        if (name == "." || name == ".." || name.BeginsWith(".")) continue;

        if (file->IsDirectory()) {
            dirNames.push_back(name);
        }
    }
    delete files;

    if (dirNames.empty()) {
        std::cout << "No directories found!" << std::endl;
        return;
    }

    // Check available programs
    bool useExecutableLog = (gSystem->AccessPathName("CheckLogFiles") == 0);
    bool useSourceLog = (gSystem->AccessPathName("CheckLogFiles.c") == 0);
    bool logAvailable = useExecutableLog || useSourceLog;

    bool useExecutableTrim = (gSystem->AccessPathName("CheckTrimFiles") == 0);
    bool useSourceTrim = (gSystem->AccessPathName("CheckTrimFiles.c") == 0);
    bool trimAvailable = useExecutableTrim || useSourceTrim;

    bool useExecutablePscan = (gSystem->AccessPathName("CheckPscanFiles") == 0);
    bool useSourcePscan = (gSystem->AccessPathName("CheckPscanFiles.c") == 0);
    bool pscanAvailable = useExecutablePscan || useSourcePscan;
    
    bool useExecutableConn = (gSystem->AccessPathName("CheckConnFiles") == 0);
    bool useSourceConn = (gSystem->AccessPathName("CheckConnFiles.c") == 0);
    bool connAvailable = useExecutableConn || useSourceConn;

    if (!logAvailable && !trimAvailable && !pscanAvailable && !connAvailable) {
        std::cerr << "Error: No valid programs found!" << std::endl;
        return;
    }

    // Prepare function pointers
    CheckFunction logFuncPtr = nullptr;
    CheckFunction trimFuncPtr = nullptr;
    CheckFunction pscanFuncPtr = nullptr;
    CheckFunction connFuncPtr = nullptr;

    if (useSourceLog) {
        gInterpreter->ProcessLine(".L CheckLogFiles.c+");
        logFuncPtr = (CheckFunction)gInterpreter->ProcessLine("CheckLogFiles;");
        if (!logFuncPtr) {
            std::cerr << "Error loading CheckLogFiles function!" << std::endl;
            logAvailable = false;
        }
    }

    if (useSourceTrim) {
        gInterpreter->ProcessLine(".L CheckTrimFiles.c+");
        trimFuncPtr = (CheckFunction)gInterpreter->ProcessLine("CheckTrimFiles;");
        if (!trimFuncPtr) {
            std::cerr << "Error loading CheckTrimFiles function!" << std::endl;
            trimAvailable = false;
        }
    }
    
    if (useSourcePscan) {
        gInterpreter->ProcessLine(".L CheckPscanFiles.c+");
        pscanFuncPtr = (CheckFunction)gInterpreter->ProcessLine("CheckPscanFiles;");
        if (!pscanFuncPtr) {
            std::cerr << "Error loading CheckPscanFiles function!" << std::endl;
            pscanAvailable = false;
        }
    }
    
    if (useSourceConn) {
        gInterpreter->ProcessLine(".L CheckConnFiles.c+");
        connFuncPtr = (CheckFunction)gInterpreter->ProcessLine("CheckConnFiles;");
        if (!connFuncPtr) {
            std::cerr << "Error loading CheckConnFiles function!" << std::endl;
            connAvailable = false;
        }
    }

    int totalDirs = dirNames.size();

    // Main validation loop
    for (auto& dir : dirNames) {
        capture.clear();
        
        int logFlags = 0;
        int trimFlags = 0;
        int pscanFlags = 0;
        int connFlags = 0;
        bool dirPassed = true;

        std::cout << "\n\n====================================================" << std::endl;
        std::cout << "VALIDATING DIRECTORY: " << dir << std::endl;
        std::cout << "====================================================" << std::endl;

        // Run CheckLogFiles
        if (logAvailable) {
            if (useExecutableLog) {
                TString command = TString::Format("./CheckLogFiles \"%s\"", dir.Data());
                std::cout << "\n[1/4] EXEC: " << command << std::endl;
                logFlags = gSystem->Exec(command);
            } else {
                TString command = TString::Format(".x CheckLogFiles.c+(\"%s\")", dir.Data());
                std::cout << "\n[1/4] EXEC: " << command << std::endl;
                if (logFuncPtr) logFlags = logFuncPtr(dir.Data());
            }
            if (logFlags != 0) dirPassed = false;
        }
        
        // Run CheckTrimFiles
        if (trimAvailable) {
            if (useExecutableTrim) {
                TString command = TString::Format("./CheckTrimFiles \"%s\"", dir.Data());
                std::cout << "\n[2/4] EXEC: " << command << std::endl;
                trimFlags = gSystem->Exec(command);
            } else {
                TString command = TString::Format(".x CheckTrimFiles.c+(\"%s\")", dir.Data());
                std::cout << "\n[2/4] EXEC: " << command << std::endl;
                if (trimFuncPtr) trimFlags = trimFuncPtr(dir.Data());
            }
            if (trimFlags != 0) dirPassed = false;
        }

        // Run CheckPscanFiles
        if (pscanAvailable) {
            if (useExecutablePscan) {
                TString command = TString::Format("./CheckPscanFiles \"%s\"", dir.Data());
                std::cout << "\n[3/4] EXEC: " << command << std::endl;
                pscanFlags = gSystem->Exec(command);
            } else {
                TString command = TString::Format(".x CheckPscanFiles.c+(\"%s\")", dir.Data());
                std::cout << "\n[3/4] EXEC: " << command << std::endl;
                if (pscanFuncPtr) pscanFlags = pscanFuncPtr(dir.Data());
            }
            if (pscanFlags != 0) dirPassed = false;
        }
        
        // Run CheckConnFiles
        if (connAvailable) {
            if (useExecutableConn) {
                TString command = TString::Format("./CheckConnFiles \"%s\"", dir.Data());
                std::cout << "\n[4/4] EXEC: " << command << std::endl;
                connFlags = gSystem->Exec(command);
            } else {
                TString command = TString::Format(".x CheckConnFiles.c+(\"%s\")", dir.Data());
                std::cout << "\n[4/4] EXEC: " << command << std::endl;
                if (connFuncPtr) connFlags = connFuncPtr(dir.Data());
            }
            if (connFlags != 0) dirPassed = false;
        }

        // Generate directory report
        std::cout << "\n====================================================" << std::endl;
        std::cout << "VALIDATION REPORT FOR: " << dir << std::endl;
        std::cout << "====================================================" << std::endl;
        
        if (logAvailable) {
            std::cout << "CheckLogFiles:  " << decodeLogFlags(logFlags) << std::endl;
        } else {
            std::cout << "CheckLogFiles:  SKIPPED" << std::endl;
        }

        if (trimAvailable) {
            std::cout << "CheckTrimFiles:  " << decodeTrimFlags(trimFlags) << std::endl;
        } else {
            std::cout << "CheckTrimFiles:  SKIPPED" << std::endl;
        }
        
        if (pscanAvailable) {
            std::cout << "CheckPscanFiles: " << decodePscanFlags(pscanFlags) << std::endl;
        } else {
            std::cout << "CheckPscanFiles: SKIPPED" << std::endl;
        }
        
        if (connAvailable) {
            std::cout << "CheckConnFiles:  " << decodeConnFlags(connFlags) << std::endl;
        } else {
            std::cout << "CheckConnFiles:  SKIPPED" << std::endl;
        }
        
        std::cout << "\nDIRECTORY STATUS: " 
                  << (dirPassed ? "PASSED" : "FAILED") 
                  << std::endl;
        std::cout << "====================================================\n" << std::endl;

        // Update statistics
        if (dirPassed) gPassedDirs++;
        else gFailedDirs++;

        // Store this directory's report
        gReportPages.push_back(capture.getOutput());
    }

    // Generate global summary
    std::stringstream summary;
    summary << "\n\n====================================================" << std::endl;
    summary << "LADDER " << currentDir  << " VALIDATION SUMMARY" << std::endl;
    summary << "====================================================" << std::endl;
    summary << "Scanned directories: " << totalDirs << std::endl;
    summary << "Passed:             " << gPassedDirs << std::endl;
    summary << "Failed:             " << gFailedDirs << std::endl;
    summary << "Success rate:       " << std::fixed << std::setprecision(1) 
            << (totalDirs > 0 ? (100.0 * gPassedDirs / totalDirs) : 0) 
            << "%" << std::endl;
    summary << "====================================================" << std::endl;
    
    gGlobalSummary = summary.str();
    std::cout << gGlobalSummary << std::endl;

    // Save reports with current directory name
    std::string reportBaseName = "ValidationReport_" + std::string(currentDir.Data());
    SaveTxtReport(reportBaseName + ".txt");
    SaveRootReport(reportBaseName + ".root");
    SavePdfReport(reportBaseName + ".pdf", currentDir.Data());
}