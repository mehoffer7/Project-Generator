/*
 * copyright (c) 2017 Matthew Oliver
 *
 * This file is part of ShiftMediaProject.
 *
 * ShiftMediaProject is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * ShiftMediaProject is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with ShiftMediaProject; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include "projectGenerator.h"

#include <algorithm>
#include <iterator>
#include <sstream>
#include <utility>

// This can be used to force all detected DCE values to be output to file
// whether they are enabled in current configuration or not
#define FORCEALLDCE 0
const string asDCETags[] = {"ARCH_", "HAVE_", "CONFIG_", "EXTERNAL_", "INTERNAL_", "INLINE_"};

bool ProjectGenerator::outputProjectDCE(const StaticList& vIncludeDirs)
{
    outputLine("  Generating missing DCE symbols (" + m_sProjectName + ")...");
    // Create list of source files to scan
#if !FORCEALLDCE
    StaticList vSearchFiles = m_vCIncludes;
    vSearchFiles.insert(vSearchFiles.end(), m_vCPPIncludes.begin(), m_vCPPIncludes.end());
    vSearchFiles.insert(vSearchFiles.end(), m_vHIncludes.begin(), m_vHIncludes.end());
#else
    StaticList vSearchFiles;
    bool bRecurse = (m_sProjectDir.compare(this->m_ConfigHelper.m_rootDirectory) != 0);
    findFiles(m_sProjectDir + "*.h", vSearchFiles, bRecurse);
    findFiles(m_sProjectDir + "*.c", vSearchFiles, bRecurse);
    findFiles(m_sProjectDir + "*.cpp", vSearchFiles, bRecurse);
#endif
    // Ensure we can add extra items to the list without needing reallocs
    if (vSearchFiles.capacity() < vSearchFiles.size() + 250) {
        vSearchFiles.reserve(vSearchFiles.size() + 250);
    }

    // Check for DCE constructs
    map<string, DCEParams> mFoundDCEUsage;
    set<string> vNonDCEUsage;
    StaticList vPreProcFiles;
    // Search through each included file
    for (auto itFile = vSearchFiles.begin(); itFile < vSearchFiles.end(); ++itFile) {
        // Open the input file
        m_ConfigHelper.makeFileGeneratorRelative(*itFile, *itFile);
        string sFile;
        if (!loadFromFile(*itFile, sFile)) {
            return false;
        }
        bool bRequiresPreProcess = false;
        outputProjectDCEFindFunctions(sFile, *itFile, mFoundDCEUsage, bRequiresPreProcess, vNonDCEUsage);
        if (bRequiresPreProcess) {
            vPreProcFiles.push_back(*itFile);
        }

        // Check if this file includes additional source files
        uint uiFindPos = sFile.find(".c\"");
        while (uiFindPos != string::npos) {
            // Check if this is an include
            uint uiFindPos2 = sFile.rfind("#include \"", uiFindPos);
            if ((uiFindPos2 != string::npos) && (uiFindPos - uiFindPos2 < 50)) {
                // Get the name of the file
                uiFindPos2 += 10;
                uiFindPos += 2;
                string sTemplateFile = sFile.substr(uiFindPos2, uiFindPos - uiFindPos2);
                // check if file contains current project
                uint uiProjName = sTemplateFile.find(m_sProjectName);
                if (uiProjName != string::npos) {
                    sTemplateFile = sTemplateFile.substr(uiProjName + m_sProjectName.length() + 1);
                }
                string sFound;
                string sBack = sTemplateFile;
                sTemplateFile = m_sProjectDir + sBack;
                if (!findFile(sTemplateFile, sFound)) {
                    sTemplateFile = (m_ConfigHelper.m_rootDirectory.length() > 0) ?
                        m_ConfigHelper.m_rootDirectory + '/' + sBack :
                        sBack;
                    if (!findFile(sTemplateFile, sFound)) {
                        sTemplateFile = m_ConfigHelper.m_solutionDirectory + m_sProjectName + '/' + sBack;
                        if (!findFile(sTemplateFile, sFound)) {
                            sTemplateFile = itFile->substr(0, itFile->rfind('/') + 1) + sBack;
                            if (!findFile(sTemplateFile, sFound)) {
                                outputError("Failed to find included file " + sBack);
                                return false;
                            }
                        }
                    }
                }
                // Add the file to the list
                if (find(vSearchFiles.begin(), vSearchFiles.end(), sTemplateFile) == vSearchFiles.end()) {
                    m_ConfigHelper.makeFileProjectRelative(sTemplateFile, sTemplateFile);
                    vSearchFiles.push_back(sTemplateFile);
                }
            }
            // Check for more
            uiFindPos = sFile.find(".c\"", uiFindPos + 1);
        }
    }
#if !FORCEALLDCE
    // Get a list of all files in current project directory (including subdirectories)
    bool bRecurse = (m_sProjectDir != this->m_ConfigHelper.m_rootDirectory);
    vSearchFiles.resize(0);
    findFiles(m_sProjectDir + "*.h", vSearchFiles, bRecurse);
    findFiles(m_sProjectDir + "*.c", vSearchFiles, bRecurse);
    findFiles(m_sProjectDir + "*.cpp", vSearchFiles, bRecurse);
    // Ensure we can add extra items to the list without needing reallocs
    if (vSearchFiles.capacity() < vSearchFiles.size() + 250) {
        vSearchFiles.reserve(vSearchFiles.size() + 250);
    }

    // Check all configurations are enabled early to avoid later lookups of unused functions
    for (auto itDCE = mFoundDCEUsage.begin(); itDCE != mFoundDCEUsage.end();) {
        outputProgramDCEsResolveDefine(itDCE->second.sDefine);
        if (itDCE->second.sDefine == "1") {
            vNonDCEUsage.insert(itDCE->first);
            // remove from the list
            mFoundDCEUsage.erase(itDCE++);
        } else {
            ++itDCE;
        }
    }
#endif

    // Now we need to find the declaration of each function
    map<string, DCEParams> mFoundDCEFunctions;
    map<string, DCEParams> mFoundDCEVariables;
    if (!mFoundDCEUsage.empty()) {
        // Search through each included file
        for (auto itFile = vSearchFiles.begin(); itFile < vSearchFiles.end(); ++itFile) {
            string sFile;
            if (!loadFromFile(*itFile, sFile)) {
                return false;
            }
            for (auto itDCE = mFoundDCEUsage.begin(); itDCE != mFoundDCEUsage.end();) {
                string sReturn;
                bool bIsFunc;
                if (outputProjectDCEsFindDeclarations(sFile, itDCE->first, *itFile, sReturn, bIsFunc)) {
                    // Get the declaration file
                    string sFileName;
                    makePathsRelative(*itFile, m_ConfigHelper.m_rootDirectory, sFileName);
                    if (sFileName.at(0) == '.') {
                        sFileName = sFileName.substr(2);
                    }
                    if (bIsFunc) {
                        mFoundDCEFunctions[sReturn] = {itDCE->second.sDefine, sFileName};
                    } else {
                        mFoundDCEVariables[sReturn] = {itDCE->second.sDefine, sFileName};
                    }

                    // Remove it from the list
                    mFoundDCEUsage.erase(itDCE++);
                } else {
                    // Only increment the iterator when nothing has been found
                    // when we did find something we erased a value from the list so the iterator is still valid
                    ++itDCE;
                }
            }
        }
    }

    // Add any files requiring pre-processing to unfound list
    for (auto itPP = vPreProcFiles.begin(); itPP < vPreProcFiles.end(); ++itPP) {
        mFoundDCEUsage[*itPP] = {"#", *itPP};
    }

    // Check if we failed to find any functions
    if (!mFoundDCEUsage.empty()) {
        vector<string> vIncludeDirs2 = vIncludeDirs;
        string sTempFolder = sTempDirectory + m_sProjectName;
        if (!makeDirectory(sTempDirectory) || !makeDirectory(sTempFolder)) {
            outputError("Failed to create temporary working directory (" + sTempFolder + ")");
            return false;
        }
        // Get all the files that include functions
        map<string, vector<DCEParams>> mFunctionFiles;
        // Remove project dir from start of file
        string sProjectDirCut, sFile;
        makePathsRelative(m_sProjectDir, m_ConfigHelper.m_rootDirectory, sProjectDirCut);
        sProjectDirCut = (sProjectDirCut.find('.') == 0) ? sProjectDirCut.substr(2) : sProjectDirCut;
        for (auto& itDCE : mFoundDCEUsage) {
            // Make source file relative to root
            makePathsRelative(itDCE.second.sFile, m_ConfigHelper.m_rootDirectory, sFile);
            sFile = (sFile.find('.') == 0) ? sFile.substr(2) : sFile;
            uint uiPos = (sProjectDirCut.length() > 0) ? sFile.find(sProjectDirCut) : string::npos;
            uiPos = (uiPos == string::npos) ? 0 : uiPos + sProjectDirCut.length();
            sFile = sTempFolder + '/' + sFile.substr(uiPos);
            if (sFile.find('/', sTempFolder.length() + 1) != string::npos) {
                string sFolder = sFile.substr(0, sFile.rfind('/'));
                if (!makeDirectory(sFolder)) {
                    outputError("Failed to create temporary working sub-directory (" + sFolder + ")");
                    return false;
                }
            }
            // Copy file to local working directory
            if (!copyFile(itDCE.second.sFile, sFile)) {
                outputError(
                    "Failed to copy dce file (" + itDCE.second.sFile + ") to temporary directory (" + sFile + ")");
                return false;
            }
            mFunctionFiles[sFile].push_back({itDCE.second.sDefine, itDCE.first});
        }
        map<string, vector<string>> mDirectoryObjects;
        const string asDCETags2[] = {"if (", "if(", "& ", "&", "| ", "|"};
        for (auto& mFunctionFile : mFunctionFiles) {
            // Get subdirectory
            string sSubFolder;
            if (mFunctionFile.first.find('/', sTempFolder.length() + 1) != string::npos) {
                sSubFolder = m_sProjectDir +
                    mFunctionFile.first.substr(
                        sTempFolder.length() + 1, mFunctionFile.first.rfind('/') - sTempFolder.length() - 1);
                if (find(vIncludeDirs2.begin(), vIncludeDirs2.end(), sSubFolder) == vIncludeDirs2.end()) {
                    // Need to add subdirectory to include list
                    vIncludeDirs2.push_back(sSubFolder);
                }
                sSubFolder = sSubFolder.substr(m_sProjectDir.length());
            }
            mDirectoryObjects[sSubFolder].push_back(mFunctionFile.first);
            // Modify existing tags so that they are still valid after preprocessing
            string sFile;
            if (!loadFromFile(mFunctionFile.first, sFile)) {
                return false;
            }
            for (const auto& asDCETag : asDCETags) {
                for (const auto& uiTag2 : asDCETags2) {
                    const string sSearch = uiTag2 + asDCETag;
                    // Search for all occurrences
                    uint uiFindPos = 0;
                    string sReplace = uiTag2 + "XXX" + asDCETag;
                    while ((uiFindPos = sFile.find(sSearch, uiFindPos)) != string::npos) {
                        sFile.replace(uiFindPos, sSearch.length(), sReplace);
                        uiFindPos += sReplace.length() + 1;
                    }
                }
            }
            if (!writeToFile(mFunctionFile.first, sFile)) {
                return false;
            }
        }
        // Add current directory to include list (must be done last to ensure correct include order)
        if (find(vIncludeDirs2.begin(), vIncludeDirs2.end(), m_sProjectDir) == vIncludeDirs2.end()) {
            vIncludeDirs2.push_back(m_sProjectDir);
        }
        if (!runCompiler(vIncludeDirs2, mDirectoryObjects, 1)) {
            return false;
        }
        // Check the file that the function usage was found in to see if it was declared using macro expansion
        for (auto& mFunctionFile : mFunctionFiles) {
            string sFile = mFunctionFile.first;
            sFile.at(sFile.length() - 1) = 'i';
            if (!loadFromFile(sFile, sFile)) {
                return false;
            }

            // Restore the initial macro names
            for (const auto& asDCETag : asDCETags) {
                for (const auto& uiTag2 : asDCETags2) {
                    const string sSearch = uiTag2 + asDCETag;
                    // Search for all occurrences
                    uint uiFindPos = 0;
                    string sFind = uiTag2 + "XXX" + asDCETag;
                    while ((uiFindPos = sFile.find(sFind, uiFindPos)) != string::npos) {
                        sFile.replace(uiFindPos, sFind.length(), sSearch);
                        uiFindPos += sSearch.length() + 1;
                    }
                }
            }

            // Check for any un-found function usage
            map<string, DCEParams> mNewDCEUsage;
            bool bCanIgnore = false;
            outputProjectDCEFindFunctions(sFile, mFunctionFile.first, mNewDCEUsage, bCanIgnore, vNonDCEUsage);
#if !FORCEALLDCE
            for (auto itDCE = mNewDCEUsage.begin(); itDCE != mNewDCEUsage.end();) {
                outputProgramDCEsResolveDefine(itDCE->second.sDefine);
                if (itDCE->second.sDefine == "1") {
                    vNonDCEUsage.insert(itDCE->first);
                    // remove from the list
                    mNewDCEUsage.erase(itDCE++);
                } else {
                    ++itDCE;
                }
            }
#endif
            for (auto& itDCE2 : mNewDCEUsage) {
                // Add the file to the list
                if (find(mFunctionFile.second.begin(), mFunctionFile.second.end(), itDCE2.first) ==
                    mFunctionFile.second.end()) {
                    mFunctionFile.second.push_back({itDCE2.second.sDefine, itDCE2.first});
                    mFoundDCEUsage[itDCE2.first] = itDCE2.second;
                }
            }

            // Search through each function in the current file
            for (auto itDCE2 = mFunctionFile.second.begin(); itDCE2 < mFunctionFile.second.end(); ++itDCE2) {
                if (itDCE2->sDefine != "#") {
                    string sReturn;
                    bool bIsFunc;
                    if (outputProjectDCEsFindDeclarations(
                            sFile, itDCE2->sFile, mFunctionFile.first, sReturn, bIsFunc)) {
                        // Get the declaration file
                        string sFileName;
                        makePathsRelative(mFunctionFile.first, m_ConfigHelper.m_rootDirectory, sFileName);
                        if (sFileName.at(0) == '.') {
                            sFileName = sFileName.substr(2);
                        }
                        // Add the declaration (ensure not to stomp a function found before needing pre-processing)
                        if (bIsFunc) {
                            if (mFoundDCEFunctions.find(sReturn) == mFoundDCEFunctions.end()) {
                                mFoundDCEFunctions[sReturn] = {itDCE2->sDefine, sFileName};
                            }
                        } else {
                            if (mFoundDCEVariables.find(sReturn) == mFoundDCEVariables.end()) {
                                mFoundDCEVariables[sReturn] = {itDCE2->sDefine, sFileName};
                            }
                        }
                        // Remove the function from list
                        mFoundDCEUsage.erase(itDCE2->sFile);
                    }
                } else {
                    // Remove the function from list as it was just a preproc file
                    mFoundDCEUsage.erase(itDCE2->sFile);
                }
            }
        }

        // Delete the created temp files
        deleteFolder(sTempDirectory);
    }

    // Get any required hard coded values
    map<string, DCEParams> mBuiltDCEFunctions;
    map<string, DCEParams> mBuiltDCEVariables;
    buildProjectDCEs(mBuiltDCEFunctions, mBuiltDCEVariables);
    for (map<string, DCEParams>::iterator itI = mBuiltDCEFunctions.begin(); itI != mBuiltDCEFunctions.end(); ++itI) {
        // Add to found list if not already found
        if (mFoundDCEFunctions.find(itI->first) == mFoundDCEFunctions.end()) {
#if !FORCEALLDCE
            outputProgramDCEsResolveDefine(itI->second.sDefine);
            if (itI->second.sDefine == "1") {
                vNonDCEUsage.insert(itI->first);
            } else {
                mFoundDCEFunctions[itI->first] = itI->second;
            }
#else
            mFoundDCEFunctions[itI->first] = itI->second;
#endif
        }
        // Remove from unfound list
        if (mFoundDCEUsage.find(itI->first) == mFoundDCEUsage.end()) {
            mFoundDCEUsage.erase(itI->first);
        }
    }
    for (map<string, DCEParams>::iterator itI = mBuiltDCEVariables.begin(); itI != mBuiltDCEVariables.end(); ++itI) {
        // Add to found list if not already found
        if (mFoundDCEVariables.find(itI->first) == mFoundDCEVariables.end()) {
            const string sName = itI->first.substr(itI->first.rfind(' ') + 1);
            if (vNonDCEUsage.find(sName) == vNonDCEUsage.end()) {
#if !FORCEALLDCE
                outputProgramDCEsResolveDefine(itI->second.sDefine);
                if (itI->second.sDefine == "1") {
                    vNonDCEUsage.insert(itI->first);
                } else {
                    mFoundDCEVariables[itI->first] = itI->second;
                }
#else
                mFoundDCEVariables[itI->first] = itI->second;
#endif
            }
        }
        // Remove from unfound list
        if (mFoundDCEUsage.find(itI->first) == mFoundDCEUsage.end()) {
            mFoundDCEUsage.erase(itI->first);
        }
    }

    // Check if we failed to find anything (even after using buildDCEs)
    if (!mFoundDCEUsage.empty()) {
        for (auto& itDCE : mFoundDCEUsage) {
            outputInfo("Failed to find function definition for " + itDCE.first + ", " + itDCE.second.sFile);
            // Just output a blank definition and hope it works
            mFoundDCEFunctions["void " + itDCE.first + "()"] = {itDCE.second.sDefine, itDCE.second.sFile};
        }
    }

    // Add definition to new file
    if ((!mFoundDCEFunctions.empty()) || (!mFoundDCEVariables.empty())) {
        vector<DCEParams> vIncludedHeaders;
        string sDCEOutFile;
        // Loop through all functions
        for (map<string, DCEParams>::iterator itDCE = mFoundDCEFunctions.begin(); itDCE != mFoundDCEFunctions.end();
             ++itDCE) {
            bool bUsePreProc = (itDCE->second.sDefine.length() > 1) && (itDCE->second.sDefine != "0");
            if (bUsePreProc) {
                sDCEOutFile += "#if !(" + itDCE->second.sDefine + ")\n";
            }
            if (itDCE->second.sFile.find(".h") != string::npos) {
                // Include header files only once
                if (!bUsePreProc) {
                    itDCE->second.sDefine = "";
                }
                auto vitH = find(vIncludedHeaders.begin(), vIncludedHeaders.end(), itDCE->second.sFile);
                if (vitH == vIncludedHeaders.end()) {
                    vIncludedHeaders.push_back({itDCE->second.sDefine, itDCE->second.sFile});
                } else {
                    outputProgramDCEsCombineDefine(vitH->sDefine, itDCE->second.sDefine, vitH->sDefine);
                }
            }
            // Check to ensure the function correctly declares parameter names.
            string sFunction = itDCE->first;
            uint uiPos = sFunction.find('(');
            uint uiCount = 0;
            while (uiPos != string::npos) {
                uint uiPos2 = sFunction.find(',', uiPos + 1);
                uint uiPosBack = uiPos2;
                uiPos2 = (uiPos2 != string::npos) ? uiPos2 : sFunction.rfind(')');
                uiPos2 = sFunction.find_last_not_of(g_whiteSpace, uiPos2 - 1);
                // Check the type of the last tag in case it is only a type name
                bool bNeedsName = false;
                string param = sFunction.substr(uiPos + 1, uiPos2 - uiPos);
                if (param.back() == '*') {
                    bNeedsName = true;
                } else {
                    // Split parameter string up and ensure there are at least a type and a name
                    istringstream ss(param);
                    vector<string> vTokens{istream_iterator<string>{ss}, istream_iterator<string>{}};
                    if (*vTokens.begin() == "const") {
                        vTokens.erase(vTokens.begin());
                    }
                    if (vTokens.size() >= 2) {
                        if ((vTokens.at(1) == "int") || (vTokens.at(1) == "long")) {
                            vTokens.erase(vTokens.begin());
                        }
                    }
                    if (vTokens.size() < 2) {
                        bNeedsName = true;
                    }
                }
                if (bNeedsName && (param.find("void") != 0)) {
                    ++uiCount;
                    stringstream ss;
                    string sInsert;
                    ss << uiCount;
                    ss >> sInsert;
                    sInsert = " param" + sInsert;
                    sFunction.insert(uiPos2 + 1, sInsert);
                    uiPosBack = (uiPosBack != string::npos) ? uiPosBack + sInsert.length() : uiPosBack;
                }
                // Get next
                uiPos = uiPosBack;
            }
            sDCEOutFile += sFunction + " {";
            // Need to check return type
            string sReturn = sFunction.substr(0, sFunction.find_first_of(g_whiteSpace));
            if (sReturn == "void") {
                sDCEOutFile += "return;";
            } else if (sReturn == "int") {
                sDCEOutFile += "return 0;";
            } else if (sReturn == "unsigned") {
                sDCEOutFile += "return 0;";
            } else if (sReturn == "long") {
                sDCEOutFile += "return 0;";
            } else if (sReturn == "short") {
                sDCEOutFile += "return 0;";
            } else if (sReturn == "float") {
                sDCEOutFile += "return 0.0f;";
            } else if (sReturn == "double") {
                sDCEOutFile += "return 0.0;";
            } else if (sReturn.find('*') != string::npos) {
                sDCEOutFile += "return 0;";
            } else {
                sDCEOutFile += "return *(" + sReturn + "*)(0);";
            }
            sDCEOutFile += "}\n";
            if (bUsePreProc) {
                sDCEOutFile += "#endif\n";
            }
        }

        // Loop through all variables
        for (map<string, DCEParams>::iterator itDCE = mFoundDCEVariables.begin(); itDCE != mFoundDCEVariables.end();
             ++itDCE) {
            bool bUsePreProc = true;
            bool enabled = false;
#if !FORCEALLDCE
            bUsePreProc = (itDCE->second.sDefine.length() > 1) && (itDCE->second.sDefine != "0");
            auto ConfigOpt = m_ConfigHelper.getConfigOptionPrefixed(itDCE->second.sDefine);
            if (ConfigOpt == m_ConfigHelper.m_configValues.end()) {
                // This config option doesn't exist so it potentially requires the header file to be included first
            } else {
                bool bReserved = (m_ConfigHelper.m_replaceList.find(ConfigOpt->m_prefix + ConfigOpt->m_option) !=
                    m_ConfigHelper.m_replaceList.end());
                if (!bReserved) {
                    enabled = (ConfigOpt->m_value == "1");
                }
                bUsePreProc = bUsePreProc || bReserved;
            }
#endif
            // Include only those options that are currently disabled
            if (!enabled) {
                // Only include preprocessor guards if its a reserved option
                if (bUsePreProc) {
                    sDCEOutFile += "#if !(" + itDCE->second.sDefine + ")\n";
                } else {
                    itDCE->second.sDefine = "";
                }
                // Include header files only once
                auto vitH = find(vIncludedHeaders.begin(), vIncludedHeaders.end(), itDCE->second.sFile);
                if (vitH == vIncludedHeaders.end()) {
                    vIncludedHeaders.push_back({itDCE->second.sDefine, itDCE->second.sFile});
                } else {
                    outputProgramDCEsCombineDefine(vitH->sDefine, itDCE->second.sDefine, vitH->sDefine);
                }
                sDCEOutFile += "const " + itDCE->first + " = {0};\n";
                if (bUsePreProc) {
                    sDCEOutFile += "#endif\n";
                }
            }
        }
        string sFinalDCEOutFile = getCopywriteHeader(m_sProjectName + " DCE definitions") + '\n';
        sFinalDCEOutFile += "\n#include \"config.h\"\n#include \"stdint.h\"\n\n";
        // Add all header files (goes backwards to avoid bug in header include order in avcodec between vp9.h and
        // h264pred.h)
        for (auto vitHeaders = vIncludedHeaders.end(); vitHeaders > vIncludedHeaders.begin();) {
            --vitHeaders;
            if (vitHeaders->sDefine.length() > 0) {
                sFinalDCEOutFile += "#if !(" + vitHeaders->sDefine + ")\n";
            }
            sFinalDCEOutFile += "#include \"" + vitHeaders->sFile + "\"\n";
            if (vitHeaders->sDefine.length() > 0) {
                sFinalDCEOutFile += "#endif\n";
            }
        }
        sFinalDCEOutFile += '\n' + sDCEOutFile;

        // Output the new file
        string sOutName = m_ConfigHelper.m_solutionDirectory + '/' + m_sProjectName + '/' + "dce_defs.c";
        writeToFile(sOutName, sFinalDCEOutFile);
        m_ConfigHelper.makeFileProjectRelative(sOutName, sOutName);
        m_vCIncludes.push_back(sOutName);
    }
    return true;
}

void ProjectGenerator::outputProjectDCEFindFunctions(const string& sFile, const string& sFileName,
    map<string, DCEParams>& mFoundDCEUsage, bool& bRequiresPreProcess, set<string>& vNonDCEUsage) const
{
    const string asDCETags2[] = {"if (", "if(", "if ((", "if(("};
    StaticList vFuncIdents = {"ff_"};
    if ((m_sProjectName == "ffmpeg") || (m_sProjectName == "ffplay") || (m_sProjectName == "ffprobe") ||
        (m_sProjectName == "avconv") || (m_sProjectName == "avplay") || (m_sProjectName == "avprobe")) {
        vFuncIdents.push_back("avcodec_");
        vFuncIdents.push_back("avdevice_");
        vFuncIdents.push_back("avfilter_");
        vFuncIdents.push_back("avformat_");
        vFuncIdents.push_back("avutil_");
        vFuncIdents.push_back("av_");
        vFuncIdents.push_back("avresample_");
        vFuncIdents.push_back("postproc_");
        vFuncIdents.push_back("swri_");
        vFuncIdents.push_back("swresample_");
        vFuncIdents.push_back("swscale_");
    } else if (m_sProjectName == "libavcodec") {
        vFuncIdents.push_back("avcodec_");
    } else if (m_sProjectName == "libavdevice") {
        vFuncIdents.push_back("avdevice_");
    } else if (m_sProjectName == "libavfilter") {
        vFuncIdents.push_back("avfilter_");
    } else if (m_sProjectName == "libavformat") {
        vFuncIdents.push_back("avformat_");
    } else if (m_sProjectName == "libavutil") {
        vFuncIdents.push_back("avutil_");
        vFuncIdents.push_back("av_");
    } else if (m_sProjectName == "libavresample") {
        vFuncIdents.push_back("avresample_");
    } else if (m_sProjectName == "libpostproc") {
        vFuncIdents.push_back("postproc_");
    } else if (m_sProjectName == "libswresample") {
        vFuncIdents.push_back("swri_");
        vFuncIdents.push_back("swresample_");
    } else if (m_sProjectName == "libswscale") {
        vFuncIdents.push_back("swscale_");
    }
    struct InternalDCEParams
    {
        DCEParams params;
        vector<uint> vLocations;
    };
    map<string, InternalDCEParams> mInternalList;
    for (const auto& asDCETag : asDCETags) {
        for (unsigned uiTag2 = 0; uiTag2 < sizeof(asDCETags2) / sizeof(string); uiTag2++) {
            const string sSearch = asDCETags2[uiTag2] + asDCETag;

            // Search for all occurrences
            uint uiFindPos = sFile.find(sSearch);
            while (uiFindPos != string::npos) {
                // Get the define tag
                uint uiFindPos2 = sFile.find(')', uiFindPos + sSearch.length());
                uiFindPos = uiFindPos + asDCETags2[uiTag2].length();
                if (uiTag2 >= 2) {
                    --uiFindPos;
                }
                // Skip any '(' found within the parameters itself
                uint uiFindPos3 = sFile.find('(', uiFindPos);
                while ((uiFindPos3 != string::npos) && (uiFindPos3 < uiFindPos2)) {
                    uiFindPos3 = sFile.find('(', uiFindPos3 + 1);
                    uiFindPos2 = sFile.find(')', uiFindPos2 + 1);
                }
                string sDefine = sFile.substr(uiFindPos, uiFindPos2 - uiFindPos);

                // Check if define contains pre-processor tags
                if (sDefine.find("##") != string::npos) {
                    bRequiresPreProcess = true;
                    return;
                }
                outputProjectDCECleanDefine(sDefine);

                // Get the block of code being wrapped
                string sCode;
                uiFindPos = sFile.find_first_not_of(g_whiteSpace, uiFindPos2 + 1);
                if (sFile.at(uiFindPos) == '{') {
                    // Need to get the entire block of code being wrapped
                    uiFindPos2 = sFile.find('}', uiFindPos + 1);
                    // Skip any '{' found within the parameters itself
                    uint uiFindPos5 = sFile.find('{', uiFindPos + 1);
                    while ((uiFindPos5 != string::npos) && (uiFindPos5 < uiFindPos2)) {
                        uiFindPos5 = sFile.find('{', uiFindPos5 + 1);
                        uiFindPos2 = sFile.find('}', uiFindPos2 + 1);
                    }
                } else {
                    // This is a single line of code
                    uiFindPos2 = sFile.find_first_of(g_endLine + ';', uiFindPos + 1);
                    if (sFile.at(uiFindPos2) == ';') {
                        ++uiFindPos2;    // must include the ;
                    } else {
                        // Must check if next line was also an if
                        uint uiFindPos5 = uiFindPos;
                        while ((sFile.at(uiFindPos5) == 'i') && (sFile.at(uiFindPos5 + 1) == 'f')) {
                            // Get the define tag
                            uiFindPos5 = sFile.find('(', uiFindPos5 + 2);
                            uiFindPos2 = sFile.find(')', uiFindPos5 + 1);
                            // Skip any '(' found within the parameters itself
                            uiFindPos3 = sFile.find('(', uiFindPos5 + 1);
                            while ((uiFindPos3 != string::npos) && (uiFindPos3 < uiFindPos2)) {
                                uiFindPos3 = sFile.find('(', uiFindPos3 + 1);
                                uiFindPos2 = sFile.find(')', uiFindPos2 + 1);
                            }
                            uiFindPos5 = sFile.find_first_not_of(g_whiteSpace, uiFindPos2 + 1);
                            if (sFile.at(uiFindPos5) == '{') {
                                // Need to get the entire block of code being wrapped
                                uiFindPos2 = sFile.find('}', uiFindPos5 + 1);
                                // Skip any '{' found within the parameters itself
                                uint uiFindPos6 = sFile.find('{', uiFindPos5 + 1);
                                while ((uiFindPos6 != string::npos) && (uiFindPos6 < uiFindPos2)) {
                                    uiFindPos6 = sFile.find('{', uiFindPos6 + 1);
                                    uiFindPos2 = sFile.find('}', uiFindPos2 + 1);
                                }
                                break;
                            }
                            // This is a single line of code
                            uiFindPos2 = sFile.find_first_of(g_endLine + ';', uiFindPos5 + 1);
                            if (sFile.at(uiFindPos2) == ';') {
                                ++uiFindPos2;    // must include the ;
                                break;
                            }
                        }
                    }
                }
                sCode = sFile.substr(uiFindPos, uiFindPos2 - uiFindPos);
                uint uiFindBack = uiFindPos;

                // Get name of any functions
                for (auto itIdent = vFuncIdents.begin(); itIdent < vFuncIdents.end(); ++itIdent) {
                    uiFindPos = sCode.find(*itIdent);
                    while (uiFindPos != string::npos) {
                        bool bValid = false;
                        // Check if this is a valid function call
                        uint uiFindPos3 = sCode.find_first_of(g_nonName, uiFindPos + 1);
                        if ((uiFindPos3 != 0) && (uiFindPos3 != string::npos)) {
                            uint uiFindPos4 = sCode.find_last_of(g_nonName, uiFindPos3 - 1);
                            uiFindPos4 = (uiFindPos4 == string::npos) ? 0 : uiFindPos4 + 1;
                            // Check if valid function
                            if (uiFindPos4 == uiFindPos) {
                                uiFindPos4 = sCode.find_first_not_of(g_whiteSpace, uiFindPos3);
                                if (sCode.at(uiFindPos4) == '(') {
                                    bValid = true;
                                } else if (sCode.at(uiFindPos4) == ';') {
                                    uiFindPos4 = sCode.find_last_not_of(g_whiteSpace, uiFindPos - 1);
                                    if (sCode.at(uiFindPos4) == '=') {
                                        bValid = true;
                                    }
                                } else if (sCode.at(uiFindPos4) == '#') {
                                    // Check if this is a macro expansion
                                    bRequiresPreProcess = true;
                                    return;
                                }
                            }
                        }
                        if (bValid) {
                            string sAdd = sCode.substr(uiFindPos, uiFindPos3 - uiFindPos);
                            // Check if there are any other DCE conditions
                            string sFuncDefine = sDefine;
                            for (const auto& uiTagB : asDCETags) {
                                for (unsigned uiTag2B = 0; uiTag2B < sizeof(asDCETags2) / sizeof(string); uiTag2B++) {
                                    const string sSearch2 = asDCETags2[uiTag2B] + uiTagB;

                                    // Search for all occurrences
                                    uint uiFindPos7 = sCode.rfind(sSearch2, uiFindPos);
                                    while (uiFindPos7 != string::npos) {
                                        // Get the define tag
                                        uint uiFindPos4 = sCode.find(')', uiFindPos7 + sSearch.length());
                                        uint uiFindPos8 = uiFindPos7 + asDCETags2[uiTag2B].length();
                                        if (uiTag2B >= 2) {
                                            --uiFindPos8;
                                        }
                                        // Skip any '(' found within the parameters itself
                                        uint uiFindPos9 = sCode.find('(', uiFindPos8);
                                        while ((uiFindPos9 != string::npos) && (uiFindPos9 < uiFindPos4)) {
                                            uiFindPos9 = sCode.find('(', uiFindPos9 + 1);
                                            uiFindPos4 = sCode.find(')', uiFindPos4 + 1);
                                        }
                                        string sDefine2 = sCode.substr(uiFindPos8, uiFindPos4 - uiFindPos8);
                                        outputProjectDCECleanDefine(sDefine2);

                                        // Get the block of code being wrapped
                                        string sCode2;
                                        uiFindPos8 = sCode.find_first_not_of(g_whiteSpace, uiFindPos4 + 1);
                                        if (sCode.at(uiFindPos8) == '{') {
                                            // Need to get the entire block of code being wrapped
                                            uiFindPos4 = sCode.find('}', uiFindPos8 + 1);
                                            // Skip any '{' found within the parameters itself
                                            uint uiFindPos5 = sCode.find('{', uiFindPos8 + 1);
                                            while ((uiFindPos5 != string::npos) && (uiFindPos5 < uiFindPos4)) {
                                                uiFindPos5 = sCode.find('{', uiFindPos5 + 1);
                                                uiFindPos4 = sCode.find('}', uiFindPos4 + 1);
                                            }
                                            sCode2 = sCode.substr(uiFindPos8, uiFindPos4 - uiFindPos8);
                                        } else {
                                            // This is a single line of code
                                            uiFindPos4 = sCode.find_first_of(g_endLine, uiFindPos8 + 1);
                                            sCode2 = sCode.substr(uiFindPos8, uiFindPos4 - uiFindPos8);
                                        }

                                        // Check if function is actually effected by this DCE
                                        if (sCode2.find(sAdd) != string::npos) {
                                            // Add the additional define
                                            string sCond = "&&";
                                            if (sDefine2.find_first_of("&|") != string::npos) {
                                                sCond = '(' + sDefine2 + ')' + sCond;
                                            } else {
                                                sCond = sDefine2 + sCond;
                                            }
                                            if (sFuncDefine.find_first_of("&|") != string::npos) {
                                                sCond += '(' + sFuncDefine + ')';
                                            } else {
                                                sCond += sFuncDefine;
                                            }
                                            sFuncDefine = sCond;
                                        }

                                        // Search for next occurrence
                                        uiFindPos7 = sCode.rfind(sSearch, uiFindPos7 - 1);
                                    }
                                }
                            }

                            // Check if not already added
                            auto itFind = mInternalList.find(sAdd);
                            uint uiValuePosition = uiFindBack + uiFindPos;
                            if (itFind == mInternalList.end()) {
                                // Check that another non DCE instance hasn't been found
                                if (vNonDCEUsage.find(sAdd) == vNonDCEUsage.end()) {
                                    mInternalList[sAdd] = {{sFuncDefine, sFileName}, {uiValuePosition}};
                                }
                            } else {
                                string sRetDefine;
                                outputProgramDCEsCombineDefine(itFind->second.params.sDefine, sFuncDefine, sRetDefine);
                                mInternalList[sAdd].params.sDefine = sRetDefine;
                                mInternalList[sAdd].vLocations.push_back(uiValuePosition);
                            }
                        }
                        // Search for next occurrence
                        uiFindPos = sCode.find(*itIdent, uiFindPos + 1);
                    }
                }

                // Search for next occurrence
                uiFindPos = sFile.find(sSearch, uiFindPos2 + 1);
            }
        }
    }

    // Search for usage that is not effected by DCE
    for (auto itIdent = vFuncIdents.begin(); itIdent < vFuncIdents.end(); ++itIdent) {
        uint uiFindPos = sFile.find(*itIdent);
        while (uiFindPos != string::npos) {
            bool bValid = false;
            // Check if this is a valid value
            uint uiFindPos3 = sFile.find_first_of(g_nonName, uiFindPos + 1);
            if (uiFindPos3 != string::npos) {
                uint uiFindPos4 = sFile.find_last_of(g_nonName, uiFindPos3 - 1);
                uiFindPos4 = (uiFindPos4 == string::npos) ? 0 : uiFindPos4 + 1;
                if (uiFindPos4 == uiFindPos) {
                    uiFindPos4 = sFile.find_first_not_of(g_whiteSpace, uiFindPos3);
                    // Check if declared inside a preprocessor block
                    uint uiFindPos5 = sFile.find('#', uiFindPos4 + 1);
                    if ((uiFindPos5 == string::npos) || (sFile.at(uiFindPos5 + 1) != 'e')) {
                        // Check if valid function
                        if (sFile.at(uiFindPos4) == '(') {
                            // Check if function call or declaration (a function call must be inside a function {})
                            uint uiCheck1 = sFile.rfind('{', uiFindPos);
                            if (uiCheck1 != string::npos) {
                                uint uiCheck2 = sFile.rfind('}', uiFindPos);
                                if ((uiCheck2 == string::npos) || (uiCheck1 > uiCheck2)) {
                                    bValid = true;
                                }
                            }
                            // Check if function definition
                            uiCheck1 = sFile.find(')', uiFindPos4 + 1);
                            // Skip any '(' found within the function parameters itself
                            uint uiCheck2 = sFile.find('(', uiFindPos4 + 1);
                            while ((uiCheck2 != string::npos) && (uiCheck2 < uiCheck1)) {
                                uiCheck2 = sFile.find('(', uiCheck2 + 1);
                                uiCheck1 = sFile.find(')', uiCheck1 + 1) + 1;
                            }
                            uiCheck2 = sFile.find_first_not_of(g_whiteSpace, uiCheck1 + 1);
                            if (sFile.at(uiCheck2) == '{') {
                                bValid = true;
                            }
                        } else if (sFile.at(uiFindPos4) == ';') {
                            uiFindPos4 = sFile.find_last_not_of(g_whiteSpace, uiFindPos4 - 1);
                            if (sFile.at(uiFindPos4) == '=') {
                                bValid = true;
                            }
                        } else if (sFile.at(uiFindPos4) == '[') {
                            // Check if function is a table declaration
                            uiFindPos4 = sFile.find(']', uiFindPos4 + 1);
                            uiFindPos4 = sFile.find_first_not_of(g_whiteSpace, uiFindPos4 + 1);
                            if (sFile.at(uiFindPos4) == '=') {
                                bValid = true;
                            }
                        } else if (sFile.at(uiFindPos4) == '=') {
                            bValid = true;
                        }
                    }
                }
            }
            if (bValid) {
                string sAdd = sFile.substr(uiFindPos, uiFindPos3 - uiFindPos);
                // Check if already added
                auto itFind = mInternalList.find(sAdd);
                if (itFind == mInternalList.end()) {
                    vNonDCEUsage.insert(sAdd);
                    // Remove from external list as well
                    // WARNING: Assumes that there is not 2 values with the same name but local visibility in 2
                    // different files
                    if (mFoundDCEUsage.find(sAdd) != mFoundDCEUsage.end()) {
                        mFoundDCEUsage.erase(sAdd);
                    }
                } else {
                    // Check if this location was found in a DCE
                    bool bFound = false;
                    for (auto itI = mInternalList[sAdd].vLocations.begin(); itI < mInternalList[sAdd].vLocations.end();
                         ++itI) {
                        if (*itI == uiFindPos) {
                            bFound = true;
                        }
                    }
                    if (!bFound) {
                        vNonDCEUsage.insert(sAdd);
                        // Remove from external list as well
                        if (mFoundDCEUsage.find(sAdd) != mFoundDCEUsage.end()) {
                            mFoundDCEUsage.erase(sAdd);
                        }
                        // Needs to remove it from internal list as it is also found in non-DCE section
                        mInternalList.erase(itFind);
                    }
                }
            }
            // Search for next occurrence
            uiFindPos = sFile.find(*itIdent, uiFindPos + 1);
        }
    }

    // Add all the found internal DCE values to the return list
    for (auto& itI : mInternalList) {
        auto itFind = mFoundDCEUsage.find(itI.first);
        if (itFind == mFoundDCEUsage.end()) {
            mFoundDCEUsage[itI.first] = itI.second.params;
        } else {
            string sRetDefine;
            outputProgramDCEsCombineDefine(itFind->second.sDefine, itI.second.params.sDefine, sRetDefine);
            mFoundDCEUsage[itI.first] = {sRetDefine, itFind->second.sFile};
        }
    }
}

void ProjectGenerator::outputProgramDCEsResolveDefine(string& sDefine)
{
    // Complex combinations of config options require determining exact values
    uint uiStartTag = sDefine.find_first_not_of(g_preProcessor);
    while (uiStartTag != string::npos) {
        // Get the next tag
        uint uiDiv = sDefine.find_first_of(g_preProcessor, uiStartTag);
        string sTag = sDefine.substr(uiStartTag, uiDiv - uiStartTag);
        // Check if tag is enabled
        auto ConfigOpt = m_ConfigHelper.getConfigOptionPrefixed(sTag);
        if ((ConfigOpt == m_ConfigHelper.m_configValues.end()) ||
            (m_ConfigHelper.m_replaceList.find(ConfigOpt->m_prefix + ConfigOpt->m_option) !=
                m_ConfigHelper.m_replaceList.end())) {
            // This config option doesn't exist but it is potentially included in its corresponding header file
            // Or this is a reserved value
        } else {
            // Replace the option with its value
            sDefine.replace(uiStartTag, uiDiv - uiStartTag, ConfigOpt->m_value);
            uiDiv = sDefine.find_first_of(g_preProcessor, uiStartTag);
        }

        // Get next
        uiStartTag = sDefine.find_first_not_of(g_preProcessor, uiDiv);
    }
    // Process the string to combine values
    findAndReplace(sDefine, "&&", "&");
    findAndReplace(sDefine, "||", "|");

    // Need to search through for !, !=, ==, &&, || in correct order of precendence
    const char acOps[] = {'!', '=', '&', '|'};
    for (char acOp : acOps) {
        uiStartTag = sDefine.find(acOp);
        while (uiStartTag != string::npos) {
            // Check for != or ==
            if (sDefine.at(uiStartTag + 1) == '=') {
                ++uiStartTag;
            }
            // Get right tag
            ++uiStartTag;
            uint uiRight = sDefine.find_first_of(g_preProcessor, uiStartTag);
            // Skip any '(' found within the function parameters itself
            if ((uiRight != string::npos) && (sDefine.at(uiRight) == '(')) {
                uint uiBack = uiRight + 1;
                uiRight = sDefine.find(')', uiBack) + 1;
                uint uiFindPos3 = sDefine.find('(', uiBack);
                while ((uiFindPos3 != string::npos) && (uiFindPos3 < uiRight)) {
                    uiFindPos3 = sDefine.find('(', uiFindPos3 + 1);
                    uiRight = sDefine.find(')', uiRight + 1) + 1;
                }
            }
            string sRight = sDefine.substr(uiStartTag, uiRight - uiStartTag);
            --uiStartTag;

            // Check current operation
            if (sDefine.at(uiStartTag) == '!') {
                if (sRight == "0") {
                    //! 0 = 1
                    sDefine.replace(uiStartTag, uiRight - uiStartTag, 1, '1');
                } else if (sRight == "1") {
                    //! 1 = 0
                    sDefine.replace(uiStartTag, uiRight - uiStartTag, 1, '0');
                } else {
                    //! X = (!X)
                    if (uiRight == string::npos) {
                        sDefine += ')';
                    } else {
                        sDefine.insert(uiRight, 1, ')');
                    }
                    sDefine.insert(uiStartTag, 1, '(');
                    uiStartTag += 2;
                }
            } else {
                // Check for != or ==
                if (sDefine.at(uiStartTag) == '=') {
                    --uiStartTag;
                }
                // Get left tag
                uint uiLeft = sDefine.find_last_of(g_preProcessor, uiStartTag - 1);
                // Skip any ')' found within the function parameters itself
                if ((uiLeft != string::npos) && (sDefine.at(uiLeft) == ')')) {
                    uint uiBack = uiLeft - 1;
                    uiLeft = sDefine.rfind('(', uiBack);
                    uint uiFindPos3 = sDefine.rfind(')', uiBack);
                    while ((uiFindPos3 != string::npos) && (uiFindPos3 > uiLeft)) {
                        uiFindPos3 = sDefine.rfind(')', uiFindPos3 - 1);
                        uiLeft = sDefine.rfind('(', uiLeft - 1);
                    }
                } else {
                    uiLeft = (uiLeft == string::npos) ? 0 : uiLeft + 1;
                }
                string sLeft = sDefine.substr(uiLeft, uiStartTag - uiLeft);

                // Check current operation
                if (acOp == '&') {
                    if ((sLeft == "0") || (sRight == "0")) {
                        // 0&&X or X&&0 == 0
                        sDefine.replace(uiLeft, uiRight - uiLeft, 1, '0');
                        uiStartTag = uiLeft;
                    } else if (sLeft == "1") {
                        // 1&&X = X
                        ++uiStartTag;
                        sDefine.erase(uiLeft, uiStartTag - uiLeft);
                        uiStartTag = uiLeft;
                    } else if (sRight == "1") {
                        // X&&1 = X
                        sDefine.erase(uiStartTag, uiRight - uiStartTag);
                    } else {
                        // X&&X = (X&&X)
                        if (uiRight == string::npos) {
                            sDefine += ')';
                        } else {
                            sDefine.insert(uiRight, 1, ')');
                        }
                        sDefine.insert(uiLeft, 1, '(');
                        uiStartTag += 2;
                    }
                } else if (acOp == '|') {
                    if ((sLeft == "1") || (sRight == "1")) {
                        // 1||X or X||1 == 1
                        sDefine.replace(uiLeft, uiRight - uiLeft, 1, '1');
                        uiStartTag = uiLeft;
                    } else if (sLeft == "0") {
                        // 0||X = X
                        ++uiStartTag;
                        sDefine.erase(uiLeft, uiStartTag - uiLeft);
                        uiStartTag = uiLeft;
                    } else if (sRight == "0") {
                        // X||0 == X
                        sDefine.erase(uiStartTag, uiRight - uiStartTag);
                    } else {
                        // X||X = (X||X)
                        if (uiRight == string::npos) {
                            sDefine += ')';
                        } else {
                            sDefine.insert(uiRight, 1, ')');
                        }
                        sDefine.insert(uiLeft, 1, '(');
                        uiStartTag += 2;
                    }
                } else if (acOp == '!') {
                    if ((sLeft == "1") && (sRight == "1")) {
                        // 1!=1 == 0
                        sDefine.replace(uiLeft, uiRight - uiLeft, 1, '0');
                        uiStartTag = uiLeft;
                    } else if ((sLeft == "1") && (sRight == "0")) {
                        // 1!=0 == 1
                        sDefine.replace(uiLeft, uiRight - uiLeft, 1, '1');
                        uiStartTag = uiLeft;
                    } else if ((sLeft == "0") && (sRight == "1")) {
                        // 0!=1 == 1
                        sDefine.replace(uiLeft, uiRight - uiLeft, 1, '1');
                        uiStartTag = uiLeft;
                    } else {
                        // X!=X = (X!=X)
                        if (uiRight == string::npos) {
                            sDefine += ')';
                        } else {
                            sDefine.insert(uiRight, 1, ')');
                        }
                        sDefine.insert(uiLeft, 1, '(');
                        uiStartTag += 2;
                    }
                } else if (acOp == '=') {
                    if ((sLeft == "1") && (sRight == "1")) {
                        // 1==1 == 1
                        sDefine.replace(uiLeft, uiRight - uiLeft, 1, '1');
                        uiStartTag = uiLeft;
                    } else if ((sLeft == "1") && (sRight == "0")) {
                        // 1==0 == 0
                        sDefine.replace(uiLeft, uiRight - uiLeft, 1, '0');
                        uiStartTag = uiLeft;
                    } else if ((sLeft == "0") && (sRight == "1")) {
                        // 0==1 == 0
                        sDefine.replace(uiLeft, uiRight - uiLeft, 1, '0');
                        uiStartTag = uiLeft;
                    } else {
                        // X==X == (X==X)
                        if (uiRight == string::npos) {
                            sDefine += ')';
                        } else {
                            sDefine.insert(uiRight, 1, ')');
                        }
                        sDefine.insert(uiLeft, 1, '(');
                        uiStartTag += 2;
                    }
                }
            }
            findAndReplace(sDefine, "(0)", "0");
            findAndReplace(sDefine, "(1)", "1");

            // Get next
            uiStartTag = sDefine.find(acOp, uiStartTag);
        }
    }
    // Remove any (RESERV)
    uiStartTag = sDefine.find('(');
    while (uiStartTag != string::npos) {
        uint uiEndTag = sDefine.find(')', uiStartTag);
        ++uiStartTag;
        // Skip any '(' found within the function parameters itself
        uint uiFindPos3 = sDefine.find('(', uiStartTag);
        while ((uiFindPos3 != string::npos) && (uiFindPos3 < uiEndTag)) {
            uiFindPos3 = sDefine.find('(', uiFindPos3 + 1);
            uiEndTag = sDefine.find(')', uiEndTag + 1);
        }
        string sTag = sDefine.substr(uiStartTag, uiEndTag - uiStartTag);
        if ((sTag.find_first_of("&|()") == string::npos) || ((uiStartTag == 1) && (uiEndTag == sDefine.length() - 1))) {
            sDefine.erase(uiEndTag, 1);
            sDefine.erase(--uiStartTag, 1);
        }
        uiStartTag = sDefine.find('(', uiStartTag);
    }
    findAndReplace(sDefine, "&", " && ");
    findAndReplace(sDefine, "|", " || ");
}

bool ProjectGenerator::outputProjectDCEsFindDeclarations(const string& sFile, const string& sFunction,
    const string& /*sFileName*/, string& sRetDeclaration, bool& bIsFunction)
{
    uint uiFindPos = sFile.find(sFunction);
    while (uiFindPos != string::npos) {
        uint uiFindPos4 = sFile.find_first_not_of(g_whiteSpace, uiFindPos + sFunction.length());
        if (sFile.at(uiFindPos4) == '(') {
            // Check if this is a function call or an actual declaration
            uint uiFindPos2 = sFile.find(')', uiFindPos4 + 1);
            if (uiFindPos2 != string::npos) {
                // Skip any '(' found within the function parameters itself
                uint uiFindPos3 = sFile.find('(', uiFindPos4 + 1);
                while ((uiFindPos3 != string::npos) && (uiFindPos3 < uiFindPos2)) {
                    uiFindPos3 = sFile.find('(', uiFindPos3 + 1);
                    uiFindPos2 = sFile.find(')', uiFindPos2 + 1);
                }
                uiFindPos3 = sFile.find_first_not_of(g_whiteSpace, uiFindPos2 + 1);
                // If this is a definition (i.e. '{') then that means no declaration could be found (headers are
                // searched before code files)
                if ((sFile.at(uiFindPos3) == ';') || (sFile.at(uiFindPos3) == '{')) {
                    uiFindPos3 = sFile.find_last_not_of(g_whiteSpace, uiFindPos - 1);
                    if (g_nonName.find(sFile.at(uiFindPos3)) == string::npos) {
                        // Get the return type
                        uiFindPos = sFile.find_last_of(g_whiteSpace, uiFindPos3 - 1);
                        uiFindPos = (uiFindPos == string::npos) ? 0 : uiFindPos + 1;
                        // Return the declaration
                        sRetDeclaration = sFile.substr(uiFindPos, uiFindPos2 - uiFindPos + 1);
                        bIsFunction = true;
                        return true;
                    }
                    if (sFile.at(uiFindPos3) == '*') {
                        // Return potentially contains a pointer
                        --uiFindPos3;
                        uiFindPos3 = sFile.find_last_not_of(g_whiteSpace, uiFindPos3 - 1);
                        if (g_nonName.find(sFile.at(uiFindPos3)) == string::npos) {
                            // Get the return type
                            uiFindPos = sFile.find_last_of(g_whiteSpace, uiFindPos3 - 1);
                            uiFindPos = (uiFindPos == string::npos) ? 0 : uiFindPos + 1;
                            // Return the declaration
                            sRetDeclaration = sFile.substr(uiFindPos, uiFindPos2 - uiFindPos + 1);
                            bIsFunction = true;
                            return true;
                        }
                    }
                }
            }
        } else if (sFile.at(uiFindPos4) == '[') {
            // This is an array/table
            // Check if this is an definition or an declaration
            uint uiFindPos2 = sFile.find(']', uiFindPos4 + 1);
            if (uiFindPos2 != string::npos) {
                // Skip multidimensional array
                while ((uiFindPos2 + 1 < sFile.length()) && (sFile.at(uiFindPos2 + 1) == '[')) {
                    uiFindPos2 = sFile.find(']', uiFindPos2 + 1);
                }
                uint uiFindPos3 = sFile.find_first_not_of(g_whiteSpace, uiFindPos2 + 1);
                if (sFile.at(uiFindPos3) == '=') {
                    uiFindPos3 = sFile.find_last_not_of(g_whiteSpace, uiFindPos - 1);
                    if (g_nonName.find(sFile.at(uiFindPos3)) == string::npos) {
                        // Get the array type
                        uiFindPos = sFile.find_last_of(g_whiteSpace, uiFindPos3 - 1);
                        uiFindPos = (uiFindPos == string::npos) ? 0 : uiFindPos + 1;
                        // Return the definition
                        sRetDeclaration = sFile.substr(uiFindPos, uiFindPos2 - uiFindPos + 1);
                        bIsFunction = false;
                        return true;
                    }
                    if (sFile.at(uiFindPos3) == '*') {
                        // Type potentially contains a pointer
                        --uiFindPos3;
                        uiFindPos3 = sFile.find_last_not_of(g_whiteSpace, uiFindPos3 - 1);
                        if (g_nonName.find(sFile.at(uiFindPos3)) == string::npos) {
                            // Get the array type
                            uiFindPos = sFile.find_last_of(g_whiteSpace, uiFindPos3 - 1);
                            uiFindPos = (uiFindPos == string::npos) ? 0 : uiFindPos + 1;
                            // Return the definition
                            sRetDeclaration = sFile.substr(uiFindPos, uiFindPos2 - uiFindPos + 1);
                            bIsFunction = false;
                            return true;
                        }
                    }
                }
            }
        }

        // Search for next occurrence
        uiFindPos = sFile.find(sFunction, uiFindPos + sFunction.length() + 1);
    }
    return false;
}

void ProjectGenerator::outputProjectDCECleanDefine(string& sDefine)
{
    const string asTagReplace[] = {"EXTERNAL", "INTERNAL", "INLINE"};
    const string asTagReplaceRemove[] = {"_FAST", "_SLOW"};
    // There are some macro tags that require conversion
    for (const auto& uiRep : asTagReplace) {
        string sSearch = uiRep + '_';
        uint uiFindPos = 0;
        while ((uiFindPos = sDefine.find(sSearch, uiFindPos)) != string::npos) {
            uint uiFindPos4 = sDefine.find_first_of('(', uiFindPos + 1);
            uint uiFindPosBack = uiFindPos;
            uiFindPos += sSearch.length();
            string sTagPart = sDefine.substr(uiFindPos, uiFindPos4 - uiFindPos);
            // Remove conversion values
            for (const auto& uiRem : asTagReplaceRemove) {
                uint uiFindRem = 0;
                while ((uiFindRem = sTagPart.find(uiRem, uiFindRem)) != string::npos) {
                    sTagPart.erase(uiFindRem, uiRem.length());
                }
            }
            sTagPart = "HAVE_" + sTagPart + '_' + uiRep;
            uint uiFindPos6 = sDefine.find_first_of(')', uiFindPos4 + 1);
            // Skip any '(' found within the parameters itself
            uint uiFindPos5 = sDefine.find('(', uiFindPos4 + 1);
            while ((uiFindPos5 != string::npos) && (uiFindPos5 < uiFindPos6)) {
                uiFindPos5 = sDefine.find('(', uiFindPos5 + 1);
                uiFindPos6 = sDefine.find(')', uiFindPos6 + 1);
            }
            // Update tag with replacement
            uint uiRepLength = uiFindPos6 - uiFindPosBack + 1;
            sDefine.replace(uiFindPosBack, uiRepLength, sTagPart);
            uiFindPos = uiFindPosBack + sTagPart.length();
        }
    }

    // Check if the tag contains multiple conditionals
    removeWhiteSpace(sDefine);
    uint uiStartTag = sDefine.find_first_not_of(g_preProcessor);
    while (uiStartTag != string::npos) {
        // Check if each conditional is valid
        bool bValid = false;
        for (const auto& asDCETag : asDCETags) {
            if (sDefine.find(asDCETag, uiStartTag) == uiStartTag) {
                // We have found a valid additional tag
                bValid = true;
                break;
            }
        }
        if (!bValid) {
            // Get right tag
            uint uiRight = sDefine.find_first_of(g_preProcessor, uiStartTag);
            // Skip any '(' found within the function parameters itself
            if ((uiRight != string::npos) && (sDefine.at(uiRight) == '(')) {
                uint uiBack = uiRight + 1;
                uiRight = sDefine.find(')', uiBack) + 1;
                uint uiFindPos3 = sDefine.find('(', uiBack);
                while ((uiFindPos3 != string::npos) && (uiFindPos3 < uiRight)) {
                    uiFindPos3 = sDefine.find('(', uiFindPos3 + 1);
                    uiRight = sDefine.find(')', uiRight + 1) + 1;
                }
            }
            if (!bValid && (isupper(sDefine.at(uiStartTag)) != 0)) {
                string sRight(sDefine.substr(uiStartTag, uiRight - uiStartTag));
                if ((sRight.find("AV_") != 0) && (sRight.find("FF_") != 0)) {
                    outputInfo("Found unknown macro in DCE condition " + sRight);
                }
            }
            while ((uiStartTag != 0) && ((sDefine.at(uiStartTag - 1) == '(') && (sDefine.at(uiRight) == ')'))) {
                // Remove the ()'s
                --uiStartTag;
                ++uiRight;
            }
            if ((uiStartTag == 0) || ((sDefine.at(uiStartTag - 1) == '(') && (sDefine.at(uiRight) != ')'))) {
                // Trim operators after tag instead of before it
                uiRight = sDefine.find_first_not_of("|&!=", uiRight + 1);    // Must not search for ()'s
            } else {
                // Trim operators before tag
                uiStartTag = sDefine.find_last_not_of("|&!=", uiStartTag - 1) + 1;    // Must not search for ()'s
            }
            sDefine.erase(uiStartTag, uiRight - uiStartTag);
            uiStartTag = sDefine.find_first_not_of(g_preProcessor, uiStartTag);
        } else {
            uiStartTag = sDefine.find_first_of(g_preProcessor, uiStartTag + 1);
            uiStartTag =
                (uiStartTag != string::npos) ? sDefine.find_first_not_of(g_preProcessor, uiStartTag + 1) : uiStartTag;
        }
    }
}

void ProjectGenerator::outputProgramDCEsCombineDefine(const string& sDefine, const string& sDefine2, string& sRetDefine)
{
    if (sDefine != sDefine2) {
        // Check if either string contains the other
        if ((sDefine.find(sDefine2) != string::npos) || (sDefine2.length() == 0)) {
            // Keep the existing one
            sRetDefine = sDefine;
        } else if ((sDefine2.find(sDefine) != string::npos) || (sDefine.length() == 0)) {
            // Use the new one in place of the original
            sRetDefine = sDefine2;
        } else {
            // Add the additional define
            string sRet = "||";
            if (sDefine.find('&') != string::npos) {
                sRet = '(' + sDefine + ')' + sRet;
            } else {
                sRet = sDefine + sRet;
            }
            if (sDefine2.find('&') != string::npos) {
                sRet += '(' + sDefine2 + ')';
            } else {
                sRet += sDefine2;
            }
            sRetDefine = sRet;
        }
    } else {
        sRetDefine = sDefine;
    }
}