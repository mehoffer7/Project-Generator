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

#include "configGenerator.h"

#include <fstream>
#include <algorithm>
#include <regex>

ConfigGenerator::ConfigGenerator() :
#ifdef _MSC_VER
    m_sToolchain("msvc"),
#elif defined(_WIN32)
    m_sToolchain("mingw"),
#else
    m_sToolchain("gcc"),
#endif
    m_bLibav(false),
    m_sProjectName("FFMPEG"),
    m_bDCEOnly(false),
    m_bUsingExistingConfig(false),
    m_bUseNASM(true)
{
}

bool ConfigGenerator::passConfig(int argc, char** argv)
{
    //Check for initial input arguments
    vector<string> vEarlyArgs;
    buildEarlyConfigArgs(vEarlyArgs);
    for (int i = 1; i < argc; i++) {
        string stOption = string(argv[i]);
        string stCommand = stOption;
        const uint uiPos = stOption.find('=');
        if (uiPos != string::npos){
            stCommand = stOption.substr(0, uiPos);
        }
        if (find(vEarlyArgs.begin(), vEarlyArgs.end(), stCommand) != vEarlyArgs.end()) {
            if (!changeConfig(stOption)) {
                return false;
            }
        }
    }
    if (!passConfigureFile()) {
        return false;
    }
    //Load with default values
    if (!buildDefaultValues()) {
        return false;
    }
    //Pass input arguments
    for (int i = 1; i < argc; i++) {
        //Check that option hasn't already been processed
        string stOption = string(argv[i]);
        if (find(vEarlyArgs.begin(), vEarlyArgs.end(), stOption) == vEarlyArgs.end()) {
            if (!changeConfig(stOption)) {
                return false;
            }
        }
    }
    //Ensure forced values
    if (!buildForcedValues()) {
        return false;
    }
    //Perform validation of values
    if (!passCurrentValues()) {
        return false;
    }
    return true;
}

bool ConfigGenerator::passConfigureFile()
{
    //Generate a new config file by scanning existing build chain files
    outputLine("  Passing configure file...");

    //Setup initial directories
    if (m_sRootDirectory.length() == 0) {
        //Search paths starting in current directory then checking parents
        string sPathList[] = {"./", "../", "./ffmpeg/", "../ffmpeg/", "../../ffmpeg/", "../../../", "../../", "./libav/", "../libav/", "../../libav/"};
        uint uiPathCount = 0;
        uint uiNumPaths = sizeof(sPathList) / sizeof(string);
        for (uiPathCount; uiPathCount < uiNumPaths; uiPathCount++) {
            m_sRootDirectory = sPathList[uiPathCount];
            string sConfigFile = m_sRootDirectory + "configure";
            if (loadFromFile(sConfigFile, m_sConfigureFile, false, false)) {
                break;
            }
        }
        if (uiPathCount == uiNumPaths) {
            outputError("Failed to find a 'configure' file");
            return false;
        }
    } else {
        //Open configure file
        string sConfigFile = m_sRootDirectory + "configure";
        if (!loadFromFile(sConfigFile, m_sConfigureFile, false, false)) {
            outputError("Failed to find a 'configure' file in specified root directory");
            return false;
        }
    }

    //Search for start of config.h file parameters
    uint uiStartPos = m_sConfigureFile.find("#define FFMPEG_CONFIG_H");
    if (uiStartPos == string::npos) {
        //Check if this is instead a libav configure
        uiStartPos = m_sConfigureFile.find("#define LIBAV_CONFIG_H");
        if (uiStartPos == string::npos) {
            outputError("Failed finding config.h start parameters");
            return false;
        }
        m_bLibav = true;
        m_sProjectName = "LIBAV";
    }
    //Move to end of header guard (+1 for new line)
    uiStartPos += 24;

    //Build default value list
    DefaultValuesList mDefaultValues;
    buildFixedValues(mDefaultValues);

    //Get each defined option till EOF
    uiStartPos = m_sConfigureFile.find("#define", uiStartPos);
    uint uiConfigEnd = m_sConfigureFile.find("EOF", uiStartPos);
    if (uiConfigEnd == string::npos) {
        outputError("Failed finding config.h parameters end");
        return false;
    }
    uint uiEndPos = uiConfigEnd;
    while ((uiStartPos != string::npos) && (uiStartPos < uiConfigEnd)) {
        //Skip white space
        uiStartPos = m_sConfigureFile.find_first_not_of(sWhiteSpace, uiStartPos + 7);
        //Get first string
        uiEndPos = m_sConfigureFile.find_first_of(sWhiteSpace, uiStartPos + 1);
        string sConfigName = m_sConfigureFile.substr(uiStartPos, uiEndPos - uiStartPos);
        //Get second string
        uiStartPos = m_sConfigureFile.find_first_not_of(sWhiteSpace, uiEndPos + 1);
        uiEndPos = m_sConfigureFile.find_first_of(sWhiteSpace, uiStartPos + 1);
        string sConfigValue = m_sConfigureFile.substr(uiStartPos, uiEndPos - uiStartPos);
        //Check if the value is a variable
        uint uiStartPos2 = sConfigValue.find('$');
        if (uiStartPos2 != string::npos) {
            //Check if it is a function call
            if (sConfigValue.at(uiStartPos2 + 1) == '(') {
                uiEndPos = m_sConfigureFile.find(')', uiStartPos);
                sConfigValue = m_sConfigureFile.substr(uiStartPos, uiEndPos - uiStartPos + 1);
            }
            //Remove any quotes from the tag if there are any
            uint uiEndPos2 = (sConfigValue.at(sConfigValue.length() - 1) == '"') ? sConfigValue.length() - 1 : sConfigValue.length();
            //Find and replace the value
            DefaultValuesList::iterator mitVal = mDefaultValues.find(sConfigValue.substr(uiStartPos2, uiEndPos2 - uiStartPos2));
            if (mitVal == mDefaultValues.end()) {
                outputError("Unknown configuration operation found (" + sConfigValue.substr(uiStartPos2, uiEndPos2 - uiStartPos2) + ")");
                return false;
            }
            //Check if we need to add the quotes back
            if (sConfigValue.at(0) == '"') {
                //Replace the value with the default option in quotations
                sConfigValue = '"' + mitVal->second + '"';
            } else {
                //Replace the value with the default option
                sConfigValue = mitVal->second;
            }
        }

        //Add to the list
        m_vFixedConfigValues.push_back(ConfigPair(sConfigName, "", sConfigValue));

        //Find next
        uiStartPos = m_sConfigureFile.find("#define", uiEndPos + 1);
    }

    //Find the end of this section
    uiConfigEnd = m_sConfigureFile.find("#endif", uiConfigEnd + 1);
    if (uiConfigEnd == string::npos) {
        outputError("Failed finding config.h header end");
        return false;
    }

    //Get the additional config values
    uiStartPos = m_sConfigureFile.find("print_config", uiEndPos + 3);
    while ((uiStartPos != string::npos) && (uiStartPos < uiConfigEnd)) {
        //Add these to the config list
        //Find prefix
        uiStartPos = m_sConfigureFile.find_first_not_of(sWhiteSpace, uiStartPos + 12);
        uiEndPos = m_sConfigureFile.find_first_of(sWhiteSpace, uiStartPos + 1);
        string sPrefix = m_sConfigureFile.substr(uiStartPos, uiEndPos - uiStartPos);
        //Skip unneeded var
        uiStartPos = m_sConfigureFile.find_first_not_of(sWhiteSpace, uiEndPos + 1);
        uiEndPos = m_sConfigureFile.find_first_of(sWhiteSpace, uiStartPos + 1);

        //Find option list
        uiStartPos = m_sConfigureFile.find_first_not_of(sWhiteSpace, uiEndPos + 1);
        uiEndPos = m_sConfigureFile.find_first_of(sWhiteSpace, uiStartPos + 1);
        string sList = m_sConfigureFile.substr(uiStartPos, uiEndPos - uiStartPos);
        //Strip the variable prefix from start
        sList.erase(0, 1);

        //Create option list
        if (!passConfigList(sPrefix, "", sList)) {
            return false;
        }

        //Check if multiple lines
        uiEndPos = m_sConfigureFile.find_first_not_of(sWhiteSpace, uiEndPos + 1);
        while (m_sConfigureFile.at(uiEndPos) == '\\') {
            //Skip newline
            ++uiEndPos;
            uiStartPos = m_sConfigureFile.find_first_not_of(" \t", uiEndPos + 1);
            //Check for blank line
            if (m_sConfigureFile.at(uiStartPos) == '\n') {
                break;
            }
            uiEndPos = m_sConfigureFile.find_first_of(sWhiteSpace, uiStartPos + 1);
            string sList = m_sConfigureFile.substr(uiStartPos, uiEndPos - uiStartPos);
            //Strip the variable prefix from start
            sList.erase(0, 1);

            //Create option list
            if (!passConfigList(sPrefix, "", sList)) {
                return false;
            }
            uiEndPos = m_sConfigureFile.find_first_not_of(sWhiteSpace, uiEndPos + 1);
        }

        //Get next
        uiStartPos = m_sConfigureFile.find("print_config", uiStartPos + 1);
    }
    //Mark the end of the config list. Any elements added after this are considered temporary and should not be exported
    m_uiConfigValuesEnd = m_vConfigValues.size(); //must be uint in case of realloc
    return true;
}

bool ConfigGenerator::passExistingConfig()
{
    outputLine("  Passing in existing config.h file...");
    //load in config.h from root dir
    string sConfigH;
    string sConfigFile = m_sRootDirectory + "config.h";
    if (!loadFromFile(sConfigFile, sConfigH, false, false)) {
        outputError("Failed opening existing config.h file.");
        outputError("Ensure the requested config.h file is found in the source codes root directory.", false);
        outputError("Or omit the --use-existing-config option." + sConfigFile, false);
        return false;
    }

    //Find the first valid configuration option
    uint uiPos = -1;
    const string asConfigTags[] = {"ARCH_", "HAVE_", "CONFIG_"};
    for (unsigned uiTag = 0; uiTag < sizeof(asConfigTags) / sizeof(string); uiTag++) {
        string sSearch = "#define " + asConfigTags[uiTag];
        uint uiPos2 = sConfigH.find(sSearch);
        uiPos = (uiPos2 < uiPos) ? uiPos2 : uiPos;
    }

    //Loop through each #define tag val and set internal option to val
    while (uiPos != string::npos) {
        uiPos = sConfigH.find_first_not_of(sWhiteSpace, uiPos + 7);
        //Get the tag
        uint uiPos2 = sConfigH.find_first_of(sWhiteSpace, uiPos + 1);
        string sOption = sConfigH.substr(uiPos, uiPos2 - uiPos);

        //Check if the options is valid
        if (!isConfigOptionValidPrefixed(sOption)) {
            //Check if it is a fixed value and skip
            bool bFound = false;
            for (ValuesList::iterator vitOption = m_vFixedConfigValues.begin(); vitOption < m_vFixedConfigValues.end(); vitOption++) {
                if (vitOption->m_sOption.compare(sOption) == 0) {
                    bFound = true;
                    break;
                }
            }
            if (bFound) {
                //Get next
                uiPos = sConfigH.find("#define ", uiPos2 + 1);
                continue;
            }
            outputInfo("Unknown config option (" + sOption + ") found in config.h file.");
            return false;
        }

        //Get the value
        uiPos = sConfigH.find_first_not_of(sWhiteSpace, uiPos2 + 1);
        uiPos2 = sConfigH.find_first_of(sWhiteSpace, uiPos + 1);
        string sValue = sConfigH.substr(uiPos, uiPos2 - uiPos);
        bool bEnable = (sValue.compare("1") == 0);
        if (!bEnable && (sValue.compare("0") != 0)) {
            outputError("Invalid config value (" + sValue + ") for option (" + sOption + ") found in config.h file.");
            return false;
        }

        //Update intern value
        fastToggleConfigValue(sOption, bEnable);

        //Get next
        uiPos = sConfigH.find("#define ", uiPos2 + 1);
    }
    return true;
}

bool ConfigGenerator::changeConfig(const string & stOption)
{
    if (stOption.compare("--help") == 0) {
        uint uiStart = m_sConfigureFile.find("show_help(){");
        if (uiStart == string::npos) {
            outputError("Failed finding help list in config file");
            return false;
        }
        // Find first 'EOF'
        uiStart = m_sConfigureFile.find("EOF", uiStart) + 2;
        if (uiStart == string::npos) {
            outputError("Incompatible help list in config file");
            return false;
        }
        uint uiEnd = m_sConfigureFile.find("EOF", uiStart);
        string sHelpOptions = m_sConfigureFile.substr(uiStart, uiEnd - uiStart);
        // Search through help options and remove any values not supported
        string sRemoveSections[] = {"Standard options:", "Documentation options:", "Toolchain options:",
            "Advanced options (experts only):", "Developer options (useful when working on FFmpeg itself):", "NOTE:"};
        for (string sSection : sRemoveSections) {
            uiStart = sHelpOptions.find(sSection);
            if (uiStart != string::npos) {
                uiEnd = sHelpOptions.find("\n\n", uiStart + sSection.length() + 1);
                sHelpOptions = sHelpOptions.erase(uiStart, uiEnd - uiStart + 2);
            }
        }
        outputLine(sHelpOptions);
        // Add in custom standard string
        outputLine("Standard options:");
        outputLine("  --prefix=PREFIX          install in PREFIX [../../../msvc/]");
        //outputLine("  --bindir=DIR             install binaries in DIR [PREFIX/bin]");
        //outputLine("  --libdir=DIR             install libs in DIR [PREFIX/lib]");
        //outputLine("  --incdir=DIR             install includes in DIR [PREFIX/include]");
        outputLine("  --rootdir=DIR            location of source configure file [auto]");
        outputLine("  --projdir=DIR            location of output project files [ROOT/SMP]");
        outputLine("  --use-existing-config    use an existing config.h file found in rootdir, ignoring any other passed parameters affecting config");
        // Add in custom toolchain string
        outputLine("Toolchain options:");
        outputLine("  --toolchain=NAME         set tool defaults according to NAME");
        outputLine("  --dce-only               do not output a project and only generate missing DCE files");
        outputLine("  --use-yasm               use YASM instead of the default NASM (this is not advised as it does not support newer instructions)");
        // Add in reserved values
        vector<string> vReservedItems;
        buildReservedValues(vReservedItems);
        outputLine("\nReserved options (auto handled and cannot be set explicitly):");
        for (string sResVal : vReservedItems) {
            outputLine("  " + sResVal);
        }
        return false;
    } else if (stOption.find("--toolchain") == 0) {
        //Check for correct command syntax
        if (stOption.at(11) != '=') {
            outputError("Incorrect toolchain syntax (" + stOption + ")");
            outputError("Excepted syntax (--toolchain=NAME)", false);
            return false;
        }
        //A tool chain has been specified
        string sToolChain = stOption.substr(12);
        if (sToolChain.compare("msvc") == 0) {
            //Don't disable inline as the configure header will auto header guard it out anyway. This allows for changing on the fly afterwards
        } else if (sToolChain.compare("icl") == 0) {
            //Inline asm by default is turned on if icl is detected
        } else {
#ifdef _MSC_VER
            //Only support msvc when built with msvc
            outputError("Unknown toolchain option (" + sToolChain + ")");
            outputError("Excepted toolchains (msvc, icl)", false);
            return false;
#else
            //Only support other toolchains if DCE only
            if (!m_bDCEOnly) {
                outputError("Unknown toolchain option (" + sToolChain + ")");
                outputError("Other toolchains are only supported if --dce-only has already been specified.", false);
                return false;
            } else {
                if ((sToolChain.find("mingw") == string::npos) && (sToolChain.find("gcc") == string::npos)) {
                    outputError("Unknown toolchain option (" + sToolChain + ")");
                    outputError("Excepted toolchains (mingw*, gcc*)", false);
                    return false;
                }
            }
#endif
        }
        m_sToolchain = sToolChain;
    } else if (stOption.find("--prefix") == 0) {
        //Check for correct command syntax
        if (stOption.at(8) != '=') {
            outputError("Incorrect prefix syntax (" + stOption + ")");
            outputError("Excepted syntax (--prefix=PREFIX)", false);
            return false;
        }
        //A output dir has been specified
        string sValue = stOption.substr(9);
        m_sOutDirectory = sValue;
        //Convert '\' to '/'
        replace(m_sOutDirectory.begin(), m_sOutDirectory.end(), '\\', '/');
        //Check if a directory has been passed
        if (m_sOutDirectory.length() == 0) {
            m_sOutDirectory = "./";
        }
        //Check if directory has trailing '/'
        if (m_sOutDirectory.back() != '/') {
            m_sOutDirectory += '/';
        }
    } else if (stOption.find("--rootdir") == 0) {
        //Check for correct command syntax
        if (stOption.at(9) != '=') {
            outputError("Incorrect rootdir syntax (" + stOption + ")");
            outputError("Excepted syntax (--rootdir=DIR)", false);
            return false;
        }
        //A source dir has been specified
        string sValue = stOption.substr(10);
        m_sRootDirectory = sValue;
        //Convert '\' to '/'
        replace(m_sRootDirectory.begin(), m_sRootDirectory.end(), '\\', '/');
        //Check if a directory has been passed
        if (m_sRootDirectory.length() == 0) {
            m_sRootDirectory = "./";
        }
        //Check if directory has trailing '/'
        if (m_sRootDirectory.back() != '/') {
            m_sRootDirectory += '/';
        }
        //rootdir is passed before all other options are set up so must skip any other remaining steps
        return true;
    } else if (stOption.find("--projdir") == 0) {
        //Check for correct command syntax
        if (stOption.at(9) != '=') {
            outputError("Incorrect projdir syntax (" + stOption + ")");
            outputError("Excepted syntax (--projdir=DIR)", false);
            return false;
        }
        //A project dir has been specified
        string sValue = stOption.substr(10);
        m_sSolutionDirectory = sValue;
        //Convert '\' to '/'
        replace(m_sSolutionDirectory.begin(), m_sSolutionDirectory.end(), '\\', '/');
        //Check if a directory has been passed
        if (m_sSolutionDirectory.length() == 0) {
            m_sSolutionDirectory = "./";
        }
        //Check if directory has trailing '/'
        if (m_sSolutionDirectory.back() != '/') {
            m_sSolutionDirectory += '/';
        }
    } else if (stOption.compare("--dce-only") == 0) {
        //This has no parameters and just sets internal value
        m_bDCEOnly = true;
    } else if (stOption.compare("--use-yasm") == 0) {
        //This has no parameters and just sets internal value
        m_bUseNASM = false;
    } else if (stOption.find("--use-existing-config") == 0) {
        //A input config file has been specified
        m_bUsingExistingConfig = true;
    } else if (stOption.find("--list-") == 0) {
        string sOption = stOption.substr(7);
        string sOptionList = sOption;
        if (sOptionList.back() == 's') {
            sOptionList = sOptionList.substr(0, sOptionList.length() - 1);//Remove the trailing s
        }
        transform(sOptionList.begin(), sOptionList.end(), sOptionList.begin(), ::toupper);
        sOptionList += "_LIST";
        vector<string> vList;
        if (!getConfigList(sOptionList, vList)) {
            outputError("Unknown list option (" + sOption + ")");
            outputError("Use --help to get available options", false);
            return false;
        }
        outputLine(sOption + ": ");
        for (vector<string>::iterator itIt = vList.begin(); itIt < vList.end(); itIt++) {
            //cut off any trailing type
            uint uiPos = itIt->rfind('_');
            if (uiPos != string::npos) {
                *itIt = itIt->substr(0, uiPos);
            }
            transform(itIt->begin(), itIt->end(), itIt->begin(), ::tolower);
            outputLine("  " + *itIt);
        }
        return false;
    } else if (stOption.find("--quiet") == 0) {
        setOutputVerbosity(VERBOSITY_ERROR);
    } else if (stOption.find("--loud") == 0) {
        setOutputVerbosity(VERBOSITY_INFO);
    } else {
        bool bEnable;
        string sOption;
        if (stOption.find("--enable-") == 0) {
            bEnable = true;
            //Find remainder of option
            sOption = stOption.substr(9);
        } else if (stOption.find("--disable-") == 0) {
            bEnable = false;
            //Find remainder of option
            sOption = stOption.substr(10);
        } else {
            outputError("Unknown command line option (" + stOption + ")");
            outputError("Use --help to get available options", false);
            return false;
        }

        //Replace any '-'s with '_'
        replace(sOption.begin(), sOption.end(), '-', '_');
        //Check and make sure that a reserved item is not being changed
        vector<string> vReservedItems;
        buildReservedValues(vReservedItems);
        vector<string>::iterator vitTemp = vReservedItems.begin();
        for (vitTemp; vitTemp < vReservedItems.end(); vitTemp++) {
            if (vitTemp->compare(sOption) == 0) {
                outputWarning("Reserved option (" + sOption + ") was passed in command line option (" + stOption + ")");
                outputWarning("This option is reserved and will be ignored", false);
                return true;
            }
        }

        uint uiStartPos = sOption.find('=');
        if (uiStartPos != string::npos) {
            //Find before the =
            string sList = sOption.substr(0, uiStartPos);
            //The actual element name is suffixed by list name (all after the =)
            sOption = sOption.substr(uiStartPos + 1) + "_" + sList;
            //Get the config element
            if (!isConfigOptionValid(sOption)) {
                outputError("Unknown option (" + sOption + ") in command line option (" + stOption + ")");
                outputError("Use --help to get available options", false);
                return false;
            }
            toggleConfigValue(sOption, bEnable);
        } else {
            // Check for changes to entire list
            if (sOption.compare("devices") == 0) {
                //Change INDEV_LIST
                vector<string> vList;
                if (!getConfigList("INDEV_LIST", vList)) {
                    return false;
                }
                vector<string>::iterator vitValues = vList.begin();
                for (vitValues; vitValues < vList.end(); vitValues++) {
                    toggleConfigValue(*vitValues, bEnable);
                }
                //Change OUTDEV_LIST
                vList.resize(0);
                if (!getConfigList("OUTDEV_LIST", vList)) {
                    return false;
                }
                vitValues = vList.begin();
                for (vitValues; vitValues < vList.end(); vitValues++) {
                    toggleConfigValue(*vitValues, bEnable);
                }
            } else if (sOption.compare("programs") == 0) {
                //Change PROGRAM_LIST
                vector<string> vList;
                if (!getConfigList("PROGRAM_LIST", vList)) {
                    return false;
                }
                vector<string>::iterator vitValues = vList.begin();
                for (vitValues; vitValues < vList.end(); vitValues++) {
                    toggleConfigValue(*vitValues, bEnable);
                }
            } else if (sOption.compare("everything") == 0) {
                //Change ALL_COMPONENTS
                vector<string> vList;
                if (!getConfigList("ALL_COMPONENTS", vList)) {
                    return false;
                }
                vector<string>::iterator vitValues = vList.begin();
                for (vitValues; vitValues < vList.end(); vitValues++) {
                    toggleConfigValue(*vitValues, bEnable);
                }
            } else if (sOption.compare("all") == 0) {
                //Change ALL_COMPONENTS
                vector<string> vList;
                if (!getConfigList("ALL_COMPONENTS", vList)) {
                    return false;
                }
                vector<string>::iterator vitValues = vList.begin();
                for (vitValues; vitValues < vList.end(); vitValues++) {
                    toggleConfigValue(*vitValues, bEnable);
                }
                //Change LIBRARY_LIST
                vList.resize(0);
                if (!getConfigList("LIBRARY_LIST", vList)) {
                    return false;
                }
                vitValues = vList.begin();
                for (vitValues; vitValues < vList.end(); vitValues++) {
                    toggleConfigValue(*vitValues, bEnable);
                }
                //Change PROGRAM_LIST
                vList.resize(0);
                if (!getConfigList("PROGRAM_LIST", vList)) {
                    return false;
                }
                vitValues = vList.begin();
                for (vitValues; vitValues < vList.end(); vitValues++) {
                    toggleConfigValue(*vitValues, bEnable);
                }
            } else if (sOption.compare("autodetect") == 0) {
                //Change AUTODETECT_LIBS
                vector<string> vList;
                if (!getConfigList("AUTODETECT_LIBS", vList)) {
                    return false;
                }
                vector<string>::iterator vitValues = vList.begin();
                for (vitValues; vitValues < vList.end(); vitValues++) {
                    toggleConfigValue(*vitValues, bEnable);
                }
            } else {
                //Check if the option is a component
                vector<string> vList;
                getConfigList("COMPONENT_LIST", vList);
                vector<string>::iterator vitComponent = find(vList.begin(), vList.end(), sOption);
                if (vitComponent != vList.end()) {
                    //This is a component
                    string sOption2 = sOption.substr(0, sOption.length() - 1); //Need to remove the s from end
                    //Get the specific list
                    vList.resize(0);
                    transform(sOption2.begin(), sOption2.end(), sOption2.begin(), ::toupper);
                    getConfigList(sOption2 + "_LIST", vList);
                    for (vitComponent = vList.begin(); vitComponent < vList.end(); vitComponent++) {
                        toggleConfigValue(*vitComponent, bEnable);
                    }
                } else {
                    //If not one of above components then check if it exists as standalone option
                    if (!isConfigOptionValid(sOption)) {
                        outputError("Unknown option (" + sOption + ") in command line option (" + stOption + ")");
                        outputError("Use --help to get available options", false);
                        return false;
                    }
                    //Check if this option has a component list
                    string sOption2 = sOption;
                    transform(sOption2.begin(), sOption2.end(), sOption2.begin(), ::toupper);
                    sOption2 += "_COMPONENTS";
                    vList.resize(0);
                    getConfigList(sOption2, vList, false);
                    for (vitComponent = vList.begin(); vitComponent < vList.end(); vitComponent++) {
                        //This is a component
                        sOption2 = vitComponent->substr(0, vitComponent->length() - 1); //Need to remove the s from end
                                                                                        //Get the specific list
                        vector<string> vList2;
                        transform(sOption2.begin(), sOption2.end(), sOption2.begin(), ::toupper);
                        getConfigList(sOption2 + "_LIST", vList2);
                        vector<string>::iterator vitComponent2;
                        for (vitComponent2 = vList2.begin(); vitComponent2 < vList2.end(); vitComponent2++) {
                            toggleConfigValue(*vitComponent2, bEnable);
                        }
                    }
                }
                toggleConfigValue(sOption, bEnable);
            }
        }
    }
    //Add to the internal configuration variable
    ValuesList::iterator vitOption = m_vFixedConfigValues.begin();
    for (vitOption; vitOption < m_vFixedConfigValues.end(); vitOption++) {
        if (vitOption->m_sOption.compare(m_sProjectName + "_CONFIGURATION") == 0) {
            break;
        }
    }
    if (vitOption != m_vFixedConfigValues.end()) { //This will happen when passing early --prefix, --rootdir etc.
        vitOption->m_sValue.resize(vitOption->m_sValue.length() - 1); //Remove trailing "
        if (vitOption->m_sValue.length() > 2) {
            vitOption->m_sValue += ' ';
        }
        vitOption->m_sValue += stOption + "\"";
    }
    return true;
}

bool ConfigGenerator::passCurrentValues()
{
    if (m_bUsingExistingConfig) {
        //Don't output a new config as just use the original
        return passExistingConfig();
    }

    //Correct license variables
    if (getConfigOption("version3")->m_sValue.compare("1") == 0) {
        if (getConfigOption("gpl")->m_sValue.compare("1") == 0) {
            fastToggleConfigValue("gplv3", true);
        } else {
            fastToggleConfigValue("lgplv3", true);
        }
    }

    //Perform full check of all config values
    ValuesList::iterator vitOption = m_vConfigValues.begin();
    for (vitOption; vitOption < m_vConfigValues.end(); vitOption++) {
        if (!passDependencyCheck(vitOption)) {
            return false;
        }
    }

#if defined(OPTIMISE_ENCODERS) || defined(OPTIMISE_DECODERS)
    //Optimise the config values. Based on user input different encoders/decoder can be disabled as there are now better inbuilt alternatives
    OptimisedConfigList mOptimisedDisables;
    buildOptimisedDisables(mOptimisedDisables);
    //Check everything that is disabled based on current configuration
    OptimisedConfigList::iterator vitDisable = mOptimisedDisables.begin();
    bool bDisabledOpt = false;
    for (vitDisable; vitDisable != mOptimisedDisables.end(); vitDisable++) {
        //Check if optimised value is valid for current configuration
        ValuesList::iterator vitDisableOpt = getConfigOption(vitDisable->first);
        if (vitDisableOpt != m_vConfigValues.end()) {
            if (vitDisableOpt->m_sValue.compare("1") == 0) {
                //Disable unneeded items
                vector<string>::iterator vitOptions = vitDisable->second.begin();
                for (vitOptions; vitOptions < vitDisable->second.end(); vitOptions++) {
                    bDisabledOpt = true;
                    toggleConfigValue(*vitOptions, false);
                }
            }
        }
    }
    //It may be possible that the above optimisation pass disables some dependencies of other options.
    // If this happens then a full recheck is performed
    if (bDisabledOpt) {
        vitOption = m_vConfigValues.begin();
        for (vitOption; vitOption < m_vConfigValues.end(); vitOption++) {
            if (!passDependencyCheck(vitOption)) {
                return false;
            }
        }
    }
#endif

    //Check the current options are valid for selected license
    if (getConfigOption("nonfree")->m_sValue.compare("1") != 0) {
        vector<string> vLicenseList;
        //Check for existence of specific license lists
        if (getConfigList("EXTERNAL_LIBRARY_NONFREE_LIST", vLicenseList, false)) {
            for (vector<string>::iterator itI = vLicenseList.begin(); itI < vLicenseList.end(); itI++) {
                if (getConfigOption(*itI)->m_sValue.compare("1") == 0) {
                    outputError("Current license does not allow for option (" + getConfigOption(*itI)->m_sOption + ")");
                    return false;
                }
            }
            //Check for gpl3 lists
            if (getConfigOption("gplv3")->m_sValue.compare("1") != 0) {
                vLicenseList.clear();
                if (getConfigList("EXTERNAL_LIBRARY_GPLV3_LIST", vLicenseList, false)) {
                    for (vector<string>::iterator itI = vLicenseList.begin(); itI < vLicenseList.end(); itI++) {
                        if (getConfigOption(*itI)->m_sValue.compare("1") == 0) {
                            outputError("Current license does not allow for option (" + getConfigOption(*itI)->m_sOption + ")");
                            return false;
                        }
                    }
                }
            }
            //Check for version3 lists
            if ((getConfigOption("lgplv3")->m_sValue.compare("1") != 0) && (getConfigOption("gplv3")->m_sValue.compare("1") != 0)) {
                vLicenseList.clear();
                if (getConfigList("EXTERNAL_LIBRARY_VERSION3_LIST", vLicenseList, false)) {
                    for (vector<string>::iterator itI = vLicenseList.begin(); itI < vLicenseList.end(); itI++) {
                        if (getConfigOption(*itI)->m_sValue.compare("1") == 0) {
                            outputError("Current license does not allow for option (" + getConfigOption(*itI)->m_sOption + ")");
                            return false;
                        }
                    }
                }
            }
            //Check for gpl lists
            if (getConfigOption("gpl")->m_sValue.compare("1") != 0) {
                vLicenseList.clear();
                if (getConfigList("EXTERNAL_LIBRARY_GPL_LIST", vLicenseList, false)) {
                    for (vector<string>::iterator itI = vLicenseList.begin(); itI < vLicenseList.end(); itI++) {
                        if (getConfigOption(*itI)->m_sValue.compare("1") == 0) {
                            outputError("Current license does not allow for option (" + getConfigOption(*itI)->m_sOption + ")");
                            return false;
                        }
                    }
                }
            }
        }
    }
    return true;
}

bool ConfigGenerator::outputConfig()
{
    outputLine("  Outputting config.h...");

    //Create header output
    string sHeader = getCopywriteHeader("Automatically generated configuration values") + '\n';
    string sConfigureFile = sHeader;
    sConfigureFile += "\n#ifndef SMP_CONFIG_H\n";
    sConfigureFile += "#define SMP_CONFIG_H\n";

    //Update the license configuration
    ValuesList::iterator vitOption = m_vFixedConfigValues.begin();
    for (vitOption; vitOption < m_vFixedConfigValues.end(); vitOption++) {
        if (vitOption->m_sOption.compare(m_sProjectName + "_LICENSE") == 0) {
            break;
        }
    }
    if (getConfigOption("nonfree")->m_sValue.compare("1") == 0) {
        vitOption->m_sValue = "\"nonfree and unredistributable\"";
    } else if (getConfigOption("gplv3")->m_sValue.compare("1") == 0) {
        vitOption->m_sValue = "\"GPL version 3 or later\"";
    } else if (getConfigOption("lgplv3")->m_sValue.compare("1") == 0) {
        vitOption->m_sValue = "\"LGPL version 3 or later\"";
    } else if (getConfigOption("gpl")->m_sValue.compare("1") == 0) {
        vitOption->m_sValue = "\"GPL version 2 or later\"";
    } else {
        vitOption->m_sValue = "\"LGPL version 2.1 or later\"";
    }

    //Build inbuilt force replace list
    string header;
    buildReplaceValues(m_mReplaceList, header, m_mASMReplaceList);

    //Ouptut header
    sConfigureFile += header + '\n';

    //Output all fixed config options
    vitOption = m_vFixedConfigValues.begin();
    for (vitOption; vitOption < m_vFixedConfigValues.end(); vitOption++) {
        //Check for forced replacement (only if attribute is not disabled)
        if ((vitOption->m_sValue.compare("0") != 0) && (m_mReplaceList.find(vitOption->m_sOption) != m_mReplaceList.end())) {
            sConfigureFile += m_mReplaceList[vitOption->m_sOption] + '\n';
        } else {
            sConfigureFile += "#define " + vitOption->m_sOption + ' ' + vitOption->m_sValue + '\n';
        }
    }

    //Create ASM config file
    string sHeader2 = sHeader;
    sHeader2.replace(sHeader2.find(" */", sHeader2.length() - 4), 3, ";******");
    size_t ulFindPos = sHeader2.find("/*");
    sHeader2.replace(ulFindPos, 2, ";******");
    while ((ulFindPos = sHeader2.find(" *", ulFindPos)) != string::npos) {
        sHeader2.replace(ulFindPos, 2, ";* ");
        ulFindPos += 3;
    }
    string sASMConfigureFile = sHeader2 + '\n';

    //Output all internal options
    vitOption = m_vConfigValues.begin();
    for (vitOption; vitOption < m_vConfigValues.begin() + m_uiConfigValuesEnd; vitOption++) {
        string sTagName = vitOption->m_sPrefix + vitOption->m_sOption;
        //Check for forced replacement (only if attribute is not disabled)
        string sAddConfig;
        if ((vitOption->m_sValue.compare("0") != 0) && (m_mReplaceList.find(sTagName) != m_mReplaceList.end())) {
            sAddConfig = m_mReplaceList[sTagName];
        } else {
            sAddConfig = "#define " + sTagName + ' ' + vitOption->m_sValue;
        }
        sConfigureFile += sAddConfig + '\n';
        if ((vitOption->m_sValue.compare("0") != 0) && (m_mASMReplaceList.find(sTagName) != m_mASMReplaceList.end())) {
            sASMConfigureFile += m_mASMReplaceList[sTagName] + '\n';
        } else {
            sASMConfigureFile += "%define " + sTagName + ' ' + vitOption->m_sValue + '\n';
        }
    }

    //Output end header guard
    sConfigureFile += "#endif /* SMP_CONFIG_H */\n";
    //Write output files
    string sConfigFile = m_sSolutionDirectory + "config.h";
    if (!writeToFile(sConfigFile, sConfigureFile)) {
        outputError("Failed opening output configure file (" + sConfigFile + ")");
        return false;
    }
    sConfigFile = m_sSolutionDirectory + "config.asm";
    if (!writeToFile(sConfigFile, sASMConfigureFile)) {
        outputError("Failed opening output asm configure file (" + sConfigFile + ")");
        return false;
    }

    //Output avconfig.h
    outputLine("  Outputting avconfig.h...");
    if (!makeDirectory(m_sSolutionDirectory + "libavutil")) {
        outputError("Failed creating local libavutil directory");
        return false;
    }

    //Output header guard
    string sAVConfigFile = sHeader + '\n';
    sAVConfigFile += "#ifndef SMP_LIBAVUTIL_AVCONFIG_H\n";
    sAVConfigFile += "#define SMP_LIBAVUTIL_AVCONFIG_H\n";

    //avconfig.h currently just uses HAVE_LIST_PUB to define its values
    vector<string> vAVConfigList;
    if (!getConfigList("HAVE_LIST_PUB", vAVConfigList)) {
        outputError("Failed finding HAVE_LIST_PUB needed for avconfig.h generation");
        return false;
    }
    for (vector<string>::iterator vitAVC = vAVConfigList.begin(); vitAVC < vAVConfigList.end(); vitAVC++) {
        ValuesList::iterator vitOption = getConfigOption(*vitAVC);
        sAVConfigFile += "#define AV_HAVE_" + vitOption->m_sOption + ' ' + vitOption->m_sValue + '\n';
    }
    sAVConfigFile += "#endif /* SMP_LIBAVUTIL_AVCONFIG_H */\n";
    sConfigFile = m_sSolutionDirectory + "libavutil/avconfig.h";
    if (!writeToFile(sConfigFile, sAVConfigFile)) {
        outputError("Failed opening output avconfig file (" + sAVConfigFile + ")");
        return false;
    }

    //Output ffversion.h
    outputLine("  Outputting ffversion.h...");
    //Open VERSION file and get version string
    string sVersionDefFile = m_sRootDirectory + "RELEASE";
    ifstream ifVersionDefFile(sVersionDefFile);
    if (!ifVersionDefFile.is_open()) {
        outputError("Failed opening output version file (" + sVersionDefFile + ")");
        return false;
    }
    //Load first line into string
    string sVersion;
    getline(ifVersionDefFile, sVersion);
    ifVersionDefFile.close();

    //Output header
    string sVersionFile = sHeader + '\n';

    //Output info
    sVersionFile += "#ifndef SMP_LIBAVUTIL_FFVERSION_H\n#define SMP_LIBAVUTIL_FFVERSION_H\n#define FFMPEG_VERSION \"";
    sVersionFile += sVersion;
    sVersionFile += "\"\n#endif /* SMP_LIBAVUTIL_FFVERSION_H */\n";
    //Open output file
    sConfigFile = m_sSolutionDirectory + "libavutil/ffversion.h";
    if (!writeToFile(sConfigFile, sVersionFile)) {
        outputError("Failed opening output version file (" + sVersionFile + ")");
        return false;
    }

    //Output enabled components lists
    uint uiStart = m_sConfigureFile.find("print_enabled_components ");
    while (uiStart != string::npos) {
        //Get file name input parameter
        uiStart = m_sConfigureFile.find_first_not_of(sWhiteSpace, uiStart + 24);
        uint uiEnd = m_sConfigureFile.find_first_of(sWhiteSpace, uiStart + 1);
        string sFile = m_sConfigureFile.substr(uiStart, uiEnd - uiStart);
        //Get struct name input parameter
        uiStart = m_sConfigureFile.find_first_not_of(sWhiteSpace, uiEnd + 1);
        uiEnd = m_sConfigureFile.find_first_of(sWhiteSpace, uiStart + 1);
        string sStruct = m_sConfigureFile.substr(uiStart, uiEnd - uiStart);
        //Get list name input parameter
        uiStart = m_sConfigureFile.find_first_not_of(sWhiteSpace, uiEnd + 1);
        uiEnd = m_sConfigureFile.find_first_of(sWhiteSpace, uiStart + 1);
        string sName = m_sConfigureFile.substr(uiStart, uiEnd - uiStart);
        //Get config list input parameter
        uiStart = m_sConfigureFile.find_first_not_of(sWhiteSpace, uiEnd + 1);
        uiEnd = m_sConfigureFile.find_first_of(sWhiteSpace, ++uiStart); //skip preceding '$'
        string sList = m_sConfigureFile.substr(uiStart, uiEnd - uiStart);
        if (!passEnabledComponents(sFile, sStruct, sName, sList)) {
            return false;
        }
        uiStart = m_sConfigureFile.find("print_enabled_components ", uiEnd + 1);
    }

    return true;
}

void ConfigGenerator::deleteCreatedFiles()
{
    if (!m_bUsingExistingConfig) {
        //Delete any previously generated files
        vector<string> vExistingFiles;
        findFiles(m_sSolutionDirectory + "config.h", vExistingFiles, false);
        findFiles(m_sSolutionDirectory + "config.asm", vExistingFiles, false);
        findFiles(m_sSolutionDirectory + "libavutil/avconfig.h", vExistingFiles, false);
        findFiles(m_sSolutionDirectory + "libavutil/ffversion.h", vExistingFiles, false);
        for (vector<string>::iterator itIt = vExistingFiles.begin(); itIt < vExistingFiles.end(); itIt++) {
            deleteFile(*itIt);
        }
    }
}

void ConfigGenerator::makeFileProjectRelative(const string & sFileName, string & sRetFileName)
{
    string sPath;
    string sFile = sFileName;
    uint uiPos = sFile.rfind('/');
    if (uiPos != string::npos) {
        ++uiPos;
        sPath = sFileName.substr(0, uiPos);
        sFile = sFileName.substr(uiPos);
    }
    makePathsRelative(sPath, m_sSolutionDirectory, sRetFileName);
    //Check if relative to project dir
    if (sRetFileName.find("./") == 0) {
        sRetFileName = sRetFileName.substr(2);
    }
    sRetFileName += sFile;
}

void ConfigGenerator::makeFileGeneratorRelative(const string & sFileName, string & sRetFileName)
{
    string sPath;
    string sFile = sFileName;
    uint uiPos = sFile.rfind('/');
    if (uiPos != string::npos) {
        ++uiPos;
        sPath = sFileName.substr(0, uiPos);
        sFile = sFileName.substr(uiPos);
    }
    makePathsRelative(m_sSolutionDirectory + sPath, "./", sRetFileName);
    //Check if relative to current dir
    if (sRetFileName.find("./") == 0) {
        sRetFileName = sRetFileName.substr(2);
    }
    sRetFileName += sFile;
}

bool ConfigGenerator::getConfigList(const string & sList, vector<string> & vReturn, bool bForce, uint uiCurrentFilePos)
{
    //Find List name in file (searches backwards so that it finds the closest definition to where we currently are)
    //   This is in case a list is redefined
    uint uiStart = m_sConfigureFile.rfind(sList + "=", uiCurrentFilePos);
    //Need to ensure this is the correct list
    while ((uiStart != string::npos) && (m_sConfigureFile.at(uiStart - 1) != '\n')) {
        uiStart = m_sConfigureFile.rfind(sList + "=", uiStart - 1);
    }
    if (uiStart == string::npos) {
        if (bForce) {
            outputError("Failed finding config list (" + sList + ")");
        }
        return false;
    }
    uiStart += sList.length() + 1;
    //Check if this is a list or a function
    char cEndList = '\n';
    if (m_sConfigureFile.at(uiStart) == '"') {
        cEndList = '"';
        ++uiStart;
    } else if (m_sConfigureFile.at(uiStart) == '\'') {
        cEndList = '\'';
        ++uiStart;
    }
    //Get start of tag
    uiStart = m_sConfigureFile.find_first_not_of(sWhiteSpace, uiStart);
    while (m_sConfigureFile.at(uiStart) != cEndList) {
        //Check if this is a function
        uint uiEnd;
        if ((m_sConfigureFile.at(uiStart) == '$') && (m_sConfigureFile.at(uiStart + 1) == '(')) {
            //Skip $(
            uiStart += 2;
            //Get function name
            uiEnd = m_sConfigureFile.find_first_of(sWhiteSpace, uiStart + 1);
            string sFunction = m_sConfigureFile.substr(uiStart, uiEnd - uiStart);
            //Check if this is a known function
            if (sFunction.compare("find_things") == 0) {
                //Get first parameter
                uiStart = m_sConfigureFile.find_first_not_of(sWhiteSpace, uiEnd + 1);
                uiEnd = m_sConfigureFile.find_first_of(sWhiteSpace, uiStart + 1);
                string sParam1 = m_sConfigureFile.substr(uiStart, uiEnd - uiStart);
                //Get second parameter
                uiStart = m_sConfigureFile.find_first_not_of(sWhiteSpace, uiEnd + 1);
                uiEnd = m_sConfigureFile.find_first_of(sWhiteSpace, uiStart + 1);
                string sParam2 = m_sConfigureFile.substr(uiStart, uiEnd - uiStart);
                //Get file name
                uiStart = m_sConfigureFile.find_first_not_of(sWhiteSpace, uiEnd + 1);
                uiEnd = m_sConfigureFile.find_first_of(sWhiteSpace + ")", uiStart + 1);
                string sParam3 = m_sConfigureFile.substr(uiStart, uiEnd - uiStart);
                //Call function find_things
                if (!passFindThings(sParam1, sParam2, sParam3, vReturn)) {
                    return false;
                }
                //Make sure the closing ) is not included
                uiEnd = (m_sConfigureFile.at(uiEnd) == ')') ? uiEnd + 1 : uiEnd;
            } else if (sFunction.compare("find_things_extern") == 0) {
                //Get first parameter
                uiStart = m_sConfigureFile.find_first_not_of(sWhiteSpace, uiEnd + 1);
                uiEnd = m_sConfigureFile.find_first_of(sWhiteSpace, uiStart + 1);
                string sParam1 = m_sConfigureFile.substr(uiStart, uiEnd - uiStart);
                //Get second parameter
                uiStart = m_sConfigureFile.find_first_not_of(sWhiteSpace, uiEnd + 1);
                uiEnd = m_sConfigureFile.find_first_of(sWhiteSpace, uiStart + 1);
                string sParam2 = m_sConfigureFile.substr(uiStart, uiEnd - uiStart);
                //Get file name
                uiStart = m_sConfigureFile.find_first_not_of(sWhiteSpace, uiEnd + 1);
                uiEnd = m_sConfigureFile.find_first_of(sWhiteSpace + ")", uiStart + 1);
                string sParam3 = m_sConfigureFile.substr(uiStart, uiEnd - uiStart);
                //Check for optional 4th argument
                string sParam4;
                if ((m_sConfigureFile.at(uiEnd) != ')') && (m_sConfigureFile.at(m_sConfigureFile.find_first_not_of(sWhiteSpace, uiEnd)) != ')')) {
                    uiStart = m_sConfigureFile.find_first_not_of(sWhiteSpace, uiEnd + 1);
                    uiEnd = m_sConfigureFile.find_first_of(sWhiteSpace + ")", uiStart + 1);
                    sParam4 = m_sConfigureFile.substr(uiStart, uiEnd - uiStart);
                }
                //Call function find_things
                if (!passFindThingsExtern(sParam1, sParam2, sParam3, sParam4, vReturn)) {
                    return false;
                }
                //Make sure the closing ) is not included
                uiEnd = (m_sConfigureFile.at(uiEnd) == ')') ? uiEnd + 1 : uiEnd;
            } else if (sFunction.compare("add_suffix") == 0) {
                //Get first parameter
                uiStart = m_sConfigureFile.find_first_not_of(sWhiteSpace, uiEnd + 1);
                uiEnd = m_sConfigureFile.find_first_of(sWhiteSpace, uiStart + 1);
                string sParam1 = m_sConfigureFile.substr(uiStart, uiEnd - uiStart);
                //Get second parameter
                uiStart = m_sConfigureFile.find_first_not_of(sWhiteSpace, uiEnd + 1);
                uiEnd = m_sConfigureFile.find_first_of(sWhiteSpace + ")", uiStart + 1);
                string sParam2 = m_sConfigureFile.substr(uiStart, uiEnd - uiStart);
                //Call function add_suffix
                if (!passAddSuffix(sParam1, sParam2, vReturn)) {
                    return false;
                }
                //Make sure the closing ) is not included
                uiEnd = (m_sConfigureFile.at(uiEnd) == ')') ? uiEnd + 1 : uiEnd;
            } else if (sFunction.compare("filter_out") == 0) {
                //This should filter out occurrence of first parameter from the list passed in the second
                uint uiStartSearch = uiStart - sList.length() - 5; //ensure search is before current instance of list
                //Get first parameter
                uiStart = m_sConfigureFile.find_first_not_of(sWhiteSpace, uiEnd + 1);
                uiEnd = m_sConfigureFile.find_first_of(sWhiteSpace, uiStart + 1);
                string sParam1 = m_sConfigureFile.substr(uiStart, uiEnd - uiStart);
                //Get second parameter
                uiStart = m_sConfigureFile.find_first_not_of(sWhiteSpace, uiEnd + 1);
                uiEnd = m_sConfigureFile.find_first_of(sWhiteSpace + ")", uiStart + 1);
                string sParam2 = m_sConfigureFile.substr(uiStart, uiEnd - uiStart);
                //Call function add_suffix
                if (!passFilterOut(sParam1, sParam2, vReturn, uiStartSearch)) {
                    return false;
                }
                //Make sure the closing ) is not included
                uiEnd = (m_sConfigureFile.at(uiEnd) == ')') ? uiEnd + 1 : uiEnd;
            } else if (sFunction.compare("find_filters_extern") == 0) {
                //Get file name
                uiStart = m_sConfigureFile.find_first_not_of(sWhiteSpace, uiEnd + 1);
                uiEnd = m_sConfigureFile.find_first_of(sWhiteSpace + ")", uiStart + 1);
                string sParam = m_sConfigureFile.substr(uiStart, uiEnd - uiStart);
                //Call function find_filters_extern
                if (!passFindFiltersExtern(sParam, vReturn)) {
                    return false;
                }
                //Make sure the closing ) is not included
                uiEnd = (m_sConfigureFile.at(uiEnd) == ')') ? uiEnd + 1 : uiEnd;
            } else {
                outputError("Unknown list function (" + sFunction + ") found in list (" + sList + ")");
                return false;
            }
        } else {
            uiEnd = m_sConfigureFile.find_first_of(sWhiteSpace + cEndList, uiStart + 1);
            //Get the tag
            string sTag = m_sConfigureFile.substr(uiStart, uiEnd - uiStart);
            //Check the type of tag
            if (sTag.at(0) == '$') {
                //Strip the identifier
                sTag.erase(0, 1);
                //Recursively pass
                if (!getConfigList(sTag, vReturn, bForce, uiEnd)) {
                    return false;
                }
            } else {
                //Directly add the identifier
                vReturn.push_back(sTag);
            }
        }
        uiStart = m_sConfigureFile.find_first_not_of(sWhiteSpace, uiEnd);
        //If this is not specified as a list then only a '\' will allow for more than 1 line
        if ((cEndList == '\n') && (m_sConfigureFile.at(uiStart) != '\\')) {
            break;
        }
    }
    return true;
}

bool ConfigGenerator::passFindThings(const string & sParam1, const string & sParam2, const string & sParam3, vector<string> & vReturn, vector<string> * vReturnExterns)
{
    //Need to find and open the specified file
    string sFile = m_sRootDirectory + sParam3;
    string sFindFile;
    if (!loadFromFile(sFile, sFindFile)) {
        return false;
    }
    string sDecl;

    //Find the search pattern in the file
    uint uiStart = sFindFile.find(sParam2);
    while (uiStart != string::npos) {
        //Find the start of the tag (also as ENCDEC should be treated as both DEC+ENC we skip that as well)
        uiStart = sFindFile.find_first_of(sWhiteSpace + "(", uiStart + 1);
        //Skip any filling white space
        uiStart = sFindFile.find_first_not_of(" \t", uiStart);
        //Check if valid
        if (sFindFile.at(uiStart) != '(') {
            //Get next
            uiStart = sFindFile.find(sParam2, uiStart + 1);
            continue;
        }
        ++uiStart;
        //Find end of tag
        uint uiEnd = sFindFile.find_first_of(sWhiteSpace + ",);", uiStart);
        if (sFindFile.at(uiEnd) != ',') {
            //Get next
            uiStart = sFindFile.find(sParam2, uiEnd + 1);
            continue;
        }
        //Get the tag string
        string sTag = sFindFile.substr(uiStart, uiEnd - uiStart);
        //Check to make sure this is a definition not a macro declaration
        if (sTag.compare("X") == 0) {
            if ((vReturnExterns != NULL) && (sDecl.length() == 0)) {
                //Get the first occurance of extern then till ; as that gives naming for export as well as type
                uiStart = sFindFile.find("extern ", uiEnd + 1) + 7;
                uiEnd = sFindFile.find(';', uiStart + 1);
                sDecl = sFindFile.substr(uiStart, uiEnd - uiStart);
                uiStart = sDecl.find("##");
                while (uiStart != string::npos) {
                    char cReplace = '@';
                    if (sDecl.at(uiStart + 2) == 'y') {
                        cReplace = '$';
                    }
                    sDecl.replace(uiStart, ((sDecl.length() - uiStart) > 3) ? 5 : 3, 1, cReplace);
                    uiStart = sDecl.find("##");
                }
            }

            //Get next
            uiStart = sFindFile.find(sParam2, uiEnd + 1);
            continue;
        }
        //Get second tag
        uiStart = sFindFile.find_first_not_of(" \t", uiEnd + 1);
        uiEnd = sFindFile.find_first_of(sWhiteSpace + ",);", uiStart);
        if ((sFindFile.at(uiEnd) != ')') && (sFindFile.at(uiEnd) != ',')) {
            //Get next
            uiStart = sFindFile.find(sParam2, uiEnd + 1);
            continue;
        }
        string sTag2 = sFindFile.substr(uiStart, uiEnd - uiStart);
        if (vReturnExterns == NULL) {
            //Check that both tags match
            transform(sTag2.begin(), sTag2.end(), sTag2.begin(), ::toupper);
            if (sTag2.compare(sTag) != 0) {
                //This is somewhat incorrect as the official configuration will always take the second tag
                //  and create a config option out of it. This is actually incorrect as the source code itself
                //  only uses the first parameter as the config option.
                swap(sTag, sTag2);
            }
        }
        transform(sTag.begin(), sTag.end(), sTag.begin(), ::tolower);
        //Add any requested externs
        if (vReturnExterns != NULL) {
            //Create new extern by replacing tag with found one
            uiStart = 0;
            string sDecTag = sDecl;
            while ((uiStart = sDecTag.find('@', uiStart)) != std::string::npos) {
                sDecTag.replace(uiStart, 1, sTag2);
                uiStart += sTag2.length();
            }
            //Get any remaining tags and add to extern
            if (sDecTag.find('$') != string::npos) {
                //Get third tag
                uiStart = sFindFile.find_first_not_of(" \t", uiEnd + 1);
                uiEnd = sFindFile.find_first_of(sWhiteSpace + ",);", uiStart);
                if ((sFindFile.at(uiEnd) != ')') && (sFindFile.at(uiEnd) != ',')) {
                    //Get next
                    uiStart = sFindFile.find(sParam2, uiEnd + 1);
                    continue;
                }
                string sTag3 = sFindFile.substr(uiStart, uiEnd - uiStart);
                //Replace second tag
                uiStart = 0;
                while ((uiStart = sDecTag.find('$', uiStart)) != std::string::npos) {
                    sDecTag.replace(uiStart, 1, sTag3);
                    uiStart += sTag3.length();
                }
            }

            //Add to the list
            vReturnExterns->push_back(sDecTag);
        }
        sTag = sTag + "_" + sParam1;
        //Add the new value to list
        vReturn.push_back(sTag);
        //Get next
        uiStart = sFindFile.find(sParam2, uiEnd + 1);
    }
    return true;
}

bool ConfigGenerator::passFindThingsExtern(const string & sParam1, const string & sParam2, const string & sParam3, const string & sParam4, vector<string>& vReturn)
{
    //Need to find and open the specified file
    string sFile = m_sRootDirectory + sParam3;
    string sFindFile;
    if (!loadFromFile(sFile, sFindFile)) {
        return false;
    }

    //Find the search pattern in the file
    string sStartSearch = "extern ";
    uint uiStart = sFindFile.find(sStartSearch);
    while (uiStart != string::npos) {
        uiStart += sStartSearch.length();
        //Skip any occurrence of 'const'
        if ((sFindFile.at(uiStart) == 'c') && (sFindFile.find("const ", uiStart) == uiStart)) {
            uiStart += 6;
        }
        //Check for search tag
        uiStart = sFindFile.find_first_not_of(sWhiteSpace, uiStart);
        if ((sFindFile.at(uiStart) != sParam2.at(0)) || (sFindFile.find(sParam2, uiStart) != uiStart)) {
            //Get next
            uiStart = sFindFile.find(sStartSearch, uiStart + 1);
            continue;
        }
        uiStart += sParam2.length() + 1;
        uiStart = sFindFile.find_first_not_of(sWhiteSpace, uiStart);
        //Check for function start
        if ((sFindFile.at(uiStart) != 'f') || (sFindFile.find("ff_", uiStart) != uiStart)) {
            //Get next
            uiStart = sFindFile.find(sStartSearch, uiStart + 1);
            continue;
        }
        uiStart += 3;
        //Find end of tag
        uint uiEnd = sFindFile.find_first_of(sWhiteSpace + ",();[]", uiStart);
        uint uiEnd2 = sFindFile.find("_" + sParam1, uiStart);
        uiEnd = (uiEnd2 < uiEnd) ? uiEnd2 : uiEnd;
        if ((sFindFile.at(uiEnd) != '_') || (uiEnd2 != uiEnd)) {
            //Get next
            uiStart = sFindFile.find(sStartSearch, uiEnd + 1);
            continue;
        }
        //Get the tag string
        uiEnd += 1 + sParam1.length();
        string sTag = sFindFile.substr(uiStart, uiEnd - uiStart);
        //Check for any 4th value replacements
        if (sParam4.length() > 0) {
            uint uiRep = sTag.find("_" + sParam1);
            sTag.replace(uiRep, uiRep + 1 + sParam1.length(), "_" + sParam4);
        }
        //Add the new value to list
        transform(sTag.begin(), sTag.end(), sTag.begin(), ::tolower);
        vReturn.push_back(sTag);
        //Get next
        uiStart = sFindFile.find(sStartSearch, uiEnd + 1);
    }
    return true;
}

bool ConfigGenerator::passFindFiltersExtern(const string & sParam1, vector<string>& vReturn)
{
    // s/^extern AVFilter ff_([avfsinkrc]{2,5})_([a-zA-Z0-9_]+);/\2_filter/p
    //Need to find and open the specified file
    const string sFile = m_sRootDirectory + sParam1;
    string sFindFile;
    if (!loadFromFile(sFile, sFindFile)) {
        return false;
    }

    //Find the search pattern in the file
    const string sSearch = "extern AVFilter ff_";
    uint uiStart = sFindFile.find(sSearch);
    while (uiStart != string::npos) {
        //Find the start and end of the tag
        uiStart += sSearch.length();
        //Find end of tag
        const uint uiEnd = sFindFile.find_first_of(sWhiteSpace + ",();", uiStart);
        //Get the tag string
        string sTag = sFindFile.substr(uiStart, uiEnd - uiStart);
        //Get first part
        uiStart = sTag.find("_");
        if (uiStart == string::npos) {
            //Get next
            uiStart = sFindFile.find(sSearch, uiEnd + 1);
            continue;
        }
        const string sFirst = sTag.substr(0, uiStart);
        if (sFirst.find_first_not_of("avfsinkrc") != string::npos) {
            //Get next
            uiStart = sFindFile.find(sSearch, uiEnd + 1);
            continue;
        }
        //Get second part
        sTag = sTag.substr(++uiStart);
        transform(sTag.begin(), sTag.end(), sTag.begin(), ::tolower);
        sTag = sTag + "_filter";
        //Add the new value to list
        vReturn.push_back(sTag);
        //Get next
        uiStart = sFindFile.find(sSearch, uiEnd + 1);
    }
    return true;
}

bool ConfigGenerator::passAddSuffix(const string & sParam1, const string & sParam2, vector<string> & vReturn, uint uiCurrentFilePos)
{
    //Convert the first parameter to upper case
    string sParam1Upper = sParam1;
    transform(sParam1Upper.begin(), sParam1Upper.end(), sParam1Upper.begin(), ::toupper);
    //Erase the $ from variable
    string sParam2Cut = sParam2.substr(1, sParam2.length() - 1);
    //Just call getConfigList
    vector<string> vTemp;
    if (getConfigList(sParam2Cut, vTemp, true, uiCurrentFilePos)) {
        //Update with the new suffix and add to the list
        vector<string>::iterator vitList = vTemp.begin();
        for (vitList; vitList < vTemp.end(); vitList++) {
            vReturn.push_back(*vitList + sParam1Upper);
        }
        return true;
    }
    return false;
}

bool ConfigGenerator::passFilterOut(const string & sParam1, const string & sParam2, vector<string> & vReturn, uint uiCurrentFilePos)
{
    //Remove the "'" from the front and back of first parameter
    string sParam1Cut = sParam1.substr(1, sParam1.length() - 2);
    //Erase the $ from variable2
    string sParam2Cut = sParam2.substr(1, sParam2.length() - 1);
    //Get the list
    if (getConfigList(sParam2Cut, vReturn, true, uiCurrentFilePos)) {
        vector<string>::iterator vitCheckItem = vReturn.begin();
        for (vitCheckItem; vitCheckItem < vReturn.end(); vitCheckItem++) {
            if (vitCheckItem->compare(sParam1Cut) == 0) {
                vReturn.erase(vitCheckItem);
                //assume only appears once in list
                break;
            }
        }
        return true;
    }
    return false;
}

bool ConfigGenerator::passFullFilterName(const string & sParam1, string & sReturn)
{
    // sed -n "s/^extern AVFilter ff_\([avfsinkrc]\{2,5\}\)_$1;/\1_$1/p"
    //Need to find and open the specified file
    const string sFile = m_sRootDirectory + "libavfilter/allfilters.c";
    string sFindFile;
    if (!loadFromFile(sFile, sFindFile)) {
        return false;
    }

    //Find the search pattern in the file
    const string sSearch = "extern AVFilter ff_";
    uint uiStart = sFindFile.find(sSearch);
    while (uiStart != string::npos) {
        //Find the start and end of the tag
        uiStart += sSearch.length();
        //Find end of tag
        const uint uiEnd = sFindFile.find_first_of(sWhiteSpace + ",();", uiStart);
        //Get the tag string
        string sTag = sFindFile.substr(uiStart, uiEnd - uiStart);
        //Get first part
        uiStart = sTag.find("_");
        if (uiStart == string::npos) {
            //Get next
            uiStart = sFindFile.find(sSearch, uiEnd + 1);
            continue;
        }
        const string sFirst = sTag.substr(0, uiStart);
        if (sFirst.find_first_not_of("avfsinkrc") != string::npos) {
            //Get next
            uiStart = sFindFile.find(sSearch, uiEnd + 1);
            continue;
        }
        //Get second part
        string sSecond = sTag.substr(++uiStart);
        transform(sSecond.begin(), sSecond.end(), sSecond.begin(), ::tolower);
        if (sSecond.compare(sParam1) == 0) {
            sReturn = sTag;
            transform(sReturn.begin(), sReturn.end(), sReturn.begin(), ::tolower);
            return true;
        }
        //Get next
        uiStart = sFindFile.find(sSearch, uiEnd + 1);
    }
    return true;
}

bool ConfigGenerator::passConfigList(const string & sPrefix, const string & sSuffix, const string & sList)
{
    vector<string> vList;
    if (getConfigList(sList, vList)) {
        //Loop through each member of the list and add it to internal list
        vector<string>::iterator vitList = vList.begin();
        for (vitList; vitList < vList.end(); vitList++) {
            //Directly add the identifier
            string sTag = *vitList;
            transform(sTag.begin(), sTag.end(), sTag.begin(), ::toupper);
            sTag = sTag + sSuffix;
            m_vConfigValues.push_back(ConfigPair(sTag, sPrefix, ""));
        }
        return true;
    }
    return false;
}

bool ConfigGenerator::passEnabledComponents(const string & sFile, const string & sStruct, const string & sName, const string & sList)
{
    outputLine("  Outputting enabled components file " + sFile + "...");

    string sOutput;
    //Add copywrite header
    string sNameNice = sName;
    replace(sNameNice.begin(), sNameNice.end(), '_', ' ');
    sOutput += getCopywriteHeader("Available items from " + sNameNice);
    sOutput += '\n';

    //Output header
    sOutput += "static const " + sStruct + " *" + sName + "[] = {\n";

    //Output each element of desired list
    vector<string> vList;
    if (!getConfigList(sList, vList)) {
        return false;
    }

    //Check if using newer static filter list
    bool bStaticFilterList = false;
    if ((sName.compare("filter_list") == 0) && ((m_sConfigureFile.find("full_filter_name()") != string::npos) || (m_sConfigureFile.find("$full_filter_name_$") != string::npos))) {
        bStaticFilterList = true;
    }

    for (vector<string>::iterator vitList = vList.begin(); vitList < vList.end(); vitList++) {
        ValuesList::iterator vitOption = getConfigOption(*vitList);
        if (vitOption->m_sValue.compare("1") == 0) {
            string sOptionLower = vitOption->m_sOption;
            transform(sOptionLower.begin(), sOptionLower.end(), sOptionLower.begin(), ::tolower);
            //Check for device type replacements
            if (sName.compare("indev_list") == 0) {
                uint uiFind = sOptionLower.find("_indev");
                if (uiFind != string::npos) {
                    sOptionLower.resize(uiFind);
                    sOptionLower += "_demuxer";
                }
            } else if (sName.compare("outdev_list") == 0) {
                uint uiFind = sOptionLower.find("_outdev");
                if (uiFind != string::npos) {
                    sOptionLower.resize(uiFind);
                    sOptionLower += "_muxer";
                }
            } else if (bStaticFilterList) {
                uint uiFind = sOptionLower.find("_filter");
                if (uiFind != string::npos) {
                    sOptionLower.resize(uiFind);
                }
                if (!passFullFilterName(sOptionLower, sOptionLower)) {
                    continue;
                }
            }
            // Check if option requires replacement
            const DefaultValuesList::iterator replaced = m_mReplaceList.find(vitOption->m_sPrefix + vitOption->m_sOption);
            if (replaced != m_mReplaceList.end()) {
                //Since this is a replaced option we need to wrap it in its config preprocessor
                sOutput += "#if " + vitOption->m_sPrefix + vitOption->m_sOption + '\n';
                sOutput += "    &ff_" + sOptionLower + ",\n";
                sOutput += "#endif\n";
            } else {
                sOutput += "    &ff_" + sOptionLower + ",\n";
            }
        }
    }
    if (bStaticFilterList) {
        sOutput += "    &ff_asrc_abuffer,\n";
        sOutput += "    &ff_vsrc_buffer,\n";
        sOutput += "    &ff_asink_abuffer,\n";
        sOutput += "    &ff_vsink_buffer,\n";
    }
    sOutput += "    NULL };";

    //Open output file
    writeToFile(m_sSolutionDirectory + sFile, sOutput);
    return true;
}

bool ConfigGenerator::fastToggleConfigValue(const string & sOption, bool bEnable)
{
    //Simply find the element in the list and change its setting
    string sOptionUpper = sOption; //Ensure it is in upper case
    transform(sOptionUpper.begin(), sOptionUpper.end(), sOptionUpper.begin(), ::toupper);
    //Find in internal list
    bool bRet = false;
    ValuesList::iterator vitOption = m_vConfigValues.begin();
    for (vitOption; vitOption < m_vConfigValues.end(); vitOption++) //Some options appear more than once with different prefixes
    {
        if (vitOption->m_sOption.compare(sOptionUpper) == 0) {
            vitOption->m_sValue = (bEnable) ? "1" : "0";
            bRet = true;
        }
    }
    return bRet;
}

bool ConfigGenerator::toggleConfigValue(const string & sOption, bool bEnable, bool bRecursive)
{
    string sOptionUpper = sOption; //Ensure it is in upper case
    transform(sOptionUpper.begin(), sOptionUpper.end(), sOptionUpper.begin(), ::toupper);
    //Find in internal list
    bool bRet = false;
    ValuesList::iterator vitOption = m_vConfigValues.begin();
    for (vitOption; vitOption < m_vConfigValues.end(); vitOption++) //Some options appear more than once with different prefixes
    {
        if (vitOption->m_sOption.compare(sOptionUpper) == 0) {
            bRet = true;
            if (!vitOption->m_bLock) {
                //Lock the item to prevent cyclic conditions
                vitOption->m_bLock = true;
                if (bEnable && (vitOption->m_sValue.compare("1") != 0)) {
                    //Need to convert the name to lower case
                    string sOptionLower = sOption;
                    transform(sOptionLower.begin(), sOptionLower.end(), sOptionLower.begin(), ::tolower);
                    string sCheckFunc = sOptionLower + "_select";
                    vector<string> vCheckList;
                    if (getConfigList(sCheckFunc, vCheckList, false)) {
                        vector<string>::iterator vitCheckItem = vCheckList.begin();
                        for (vitCheckItem; vitCheckItem < vCheckList.end(); vitCheckItem++) {
                            toggleConfigValue(*vitCheckItem, true, true);
                        }
                    }

                    //If enabled then all of these should then be enabled
                    sCheckFunc = sOptionLower + "_suggest";
                    vCheckList.resize(0);
                    if (getConfigList(sCheckFunc, vCheckList, false)) {
                        vector<string>::iterator vitCheckItem = vCheckList.begin();
                        for (vitCheckItem; vitCheckItem < vCheckList.end(); vitCheckItem++) {
                            toggleConfigValue(*vitCheckItem, true, true); //Weak check
                        }
                    }

                    //Check for any hard dependencies that must be enabled
                    vector<string> vForceEnable;
                    buildForcedEnables(sOptionLower, vForceEnable);
                    vector<string>::iterator vitForcedItem = vForceEnable.begin();
                    for (vitForcedItem; vitForcedItem < vForceEnable.end(); vitForcedItem++) {
                        toggleConfigValue(*vitForcedItem, true, true);
                    }
                } else if (!bEnable && (vitOption->m_sValue.compare("0") != 0)) {
                    //Need to convert the name to lower case
                    string sOptionLower = sOption;
                    transform(sOptionLower.begin(), sOptionLower.end(), sOptionLower.begin(), ::tolower);
                    //Check for any hard dependencies that must be disabled
                    vector<string> vForceDisable;
                    buildForcedDisables(sOptionLower, vForceDisable);
                    vector<string>::iterator vitForcedItem = vForceDisable.begin();
                    for (vitForcedItem; vitForcedItem < vForceDisable.end(); vitForcedItem++) {
                        toggleConfigValue(*vitForcedItem, false, true);
                    }
                }
                //Change the items value
                vitOption->m_sValue = (bEnable) ? "1" : "0";
                //Unlock item
                vitOption->m_bLock = false;
            }
        }
    }
    if (!bRet) {
        DependencyList mAdditionalDependencies;
        buildAdditionalDependencies(mAdditionalDependencies);
        DependencyList::iterator mitDep = mAdditionalDependencies.find(sOption);
        if (bRecursive) {
            //Ensure this is not already set
            if (mitDep == mAdditionalDependencies.end()) {
                //Some options are passed in recursively that do not exist in internal list
                // However there dependencies should still be processed
                string sOptionUpper = sOption;
                transform(sOptionUpper.begin(), sOptionUpper.end(), sOptionUpper.begin(), ::toupper);
                m_vConfigValues.push_back(ConfigPair(sOptionUpper, "", ""));
                outputInfo("Unlisted config dependency found (" + sOption + ")");
            }
        } else {
            if (mitDep == mAdditionalDependencies.end()) {
                outputError("Unknown config option (" + sOption + ")");
                return false;
            }
        }
    }
    return true;
}

ConfigGenerator::ValuesList::iterator ConfigGenerator::getConfigOption(const string & sOption)
{
    //Ensure it is in upper case
    string sOptionUpper = sOption;
    transform(sOptionUpper.begin(), sOptionUpper.end(), sOptionUpper.begin(), ::toupper);
    //Find in internal list
    ValuesList::iterator vitValues = m_vConfigValues.begin();
    for (vitValues; vitValues < m_vConfigValues.end(); vitValues++) {
        if (vitValues->m_sOption.compare(sOptionUpper) == 0) {
            return vitValues;
        }
    }
    return vitValues;
}

ConfigGenerator::ValuesList::iterator ConfigGenerator::getConfigOptionPrefixed(const string & sOption)
{
    //Ensure it is in upper case
    string sOptionUpper = sOption;
    transform(sOptionUpper.begin(), sOptionUpper.end(), sOptionUpper.begin(), ::toupper);
    //Find in internal list
    ValuesList::iterator vitValues = m_vConfigValues.begin();
    for (vitValues; vitValues < m_vConfigValues.end(); vitValues++) {
        if (sOptionUpper.compare(vitValues->m_sPrefix + vitValues->m_sOption) == 0) {
            return vitValues;
        }
    }
    return vitValues;
}

bool ConfigGenerator::isConfigOptionEnabled(const string & sOption)
{
    const ValuesList::iterator vitOpt = getConfigOption(sOption);
    return (vitOpt != m_vConfigValues.end()) && (vitOpt->m_sValue.compare("1") == 0);
}

bool ConfigGenerator::isConfigOptionValid(const string& sOption)
{
    const ValuesList::iterator vitOpt = getConfigOption(sOption);
    return vitOpt != m_vConfigValues.end();
}

bool ConfigGenerator::isConfigOptionValidPrefixed(const string& sOption)
{
    const ValuesList::iterator vitOpt = getConfigOptionPrefixed(sOption);
    return vitOpt != m_vConfigValues.end();
}

bool ConfigGenerator::isASMEnabled()
{
    if (isConfigOptionValidPrefixed("HAVE_X86ASM") || isConfigOptionValidPrefixed("HAVE_YASM")) {
        return true;
    }
    return false;
}

bool ConfigGenerator::getMinWindowsVersion(uint & uiMajor, uint & uiMinor)
{
    const string sSearch = "cppflags -D_WIN32_WINNT=0x";
    uint uiPos = m_sConfigureFile.find(sSearch);
    uint uiMajorT = 10; //Initially set minimum version to Win 10
    uint uiMinorT = 0;
    bool bFound = false;
    while (uiPos != string::npos) {
        uiPos += sSearch.length();
        uint uiEndPos = m_sConfigureFile.find_first_of(sNonName, uiPos);
        //Check if valid version tag
        if ((uiEndPos - uiPos) != 4) {
            outputInfo("Unknown windows version string found (" + sSearch + ")");
        } else {
            const string sVersionMajor = m_sConfigureFile.substr(uiPos, 2);
            //Convert to int from hex string
            uint uiVersionMajor = stoul(sVersionMajor, 0, 16);
            //Check if new version is less than current
            if (uiVersionMajor <= uiMajorT) {
                const string sVersionMinor = m_sConfigureFile.substr(uiPos + 2, 2);
                uint uiVersionMinor = stoul(sVersionMinor, 0, 16);
                if ((uiVersionMajor < uiMajorT) || (uiVersionMinor < uiMinorT)) {
                    //Update best found version
                    uiMajorT = uiVersionMajor;
                    uiMinorT = uiVersionMinor;
                    bFound = true;
                }
            }
        }
        //Get next
        uiPos = m_sConfigureFile.find(sSearch, uiEndPos + 1);
    }
    if (bFound) {
        uiMajor = uiMajorT;
        uiMinor = uiMinorT;
    }
    return bFound;
}

bool ConfigGenerator::passDependencyCheck(const ValuesList::iterator vitOption)
{
    //Need to convert the name to lower case
    string sOptionLower = vitOption->m_sOption;
    transform(sOptionLower.begin(), sOptionLower.end(), sOptionLower.begin(), ::tolower);

    //Get list of additional dependencies
    DependencyList mAdditionalDependencies;
    buildAdditionalDependencies(mAdditionalDependencies);

    //Check if disabled
    if (vitOption->m_sValue.compare("1") != 0) {
        //Enabled if any of these
        string sCheckFunc = sOptionLower + "_if_any";
        vector<string> vCheckList;
        if (getConfigList(sCheckFunc, vCheckList, false)) {
            vector<string>::iterator vitCheckItem = vCheckList.begin();
            for (vitCheckItem; vitCheckItem < vCheckList.end(); vitCheckItem++) {
                //Check if this is a not !
                bool bToggle = false;
                if (vitCheckItem->at(0) == '!') {
                    vitCheckItem->erase(0);
                    bToggle = true;
                }
                bool bEnabled;
                ValuesList::iterator vitTemp = getConfigOption(*vitCheckItem);
                if (vitTemp == m_vConfigValues.end()) {
                    DependencyList::iterator mitDep = mAdditionalDependencies.find(*vitCheckItem);
                    if (mitDep == mAdditionalDependencies.end()) {
                        outputInfo("Unknown option in ifa dependency (" + *vitCheckItem + ") for option (" + sOptionLower + ")");
                        bEnabled = false;
                    } else {
                        bEnabled = mitDep->second ^ bToggle;
                    }
                } else {
                    //Check if this variable has been initialized already
                    if (vitTemp > vitOption) {
                        if (!passDependencyCheck(vitTemp)) {
                            return false;
                        }
                    }
                    bEnabled = (vitTemp->m_sValue.compare("1") == 0) ^ bToggle;
                }
                if (bEnabled) {
                    //If any deps are enabled then enable
                    toggleConfigValue(sOptionLower, true);
                    break;
                }
            }
        }
    }
    //Check if still disabled
    if (vitOption->m_sValue.compare("1") != 0) {
        //Should be enabled if all of these
        string sCheckFunc = sOptionLower + "_if";
        vector<string> vCheckList;
        if (getConfigList(sCheckFunc, vCheckList, false)) {
            vector<string>::iterator vitCheckItem = vCheckList.begin();
            bool bAllEnabled = true;
            for (vitCheckItem; vitCheckItem < vCheckList.end(); vitCheckItem++) {
                //Check if this is a not !
                bool bToggle = false;
                if (vitCheckItem->at(0) == '!') {
                    vitCheckItem->erase(0);
                    bToggle = true;
                }
                ValuesList::iterator vitTemp = getConfigOption(*vitCheckItem);
                if (vitTemp == m_vConfigValues.end()) {
                    DependencyList::iterator mitDep = mAdditionalDependencies.find(*vitCheckItem);
                    if (mitDep == mAdditionalDependencies.end()) {
                        outputInfo("Unknown option in if dependency (" + *vitCheckItem + ") for option (" + sOptionLower + ")");
                        bAllEnabled = false;
                    } else {
                        bAllEnabled = mitDep->second ^ bToggle;
                    }
                } else {
                    //Check if this variable has been initialized already
                    if (vitTemp > vitOption) {
                        if (!passDependencyCheck(vitTemp)) {
                            return false;
                        }
                    }
                    bAllEnabled = (vitTemp->m_sValue.compare("1") == 0) ^ bToggle;
                }
                if (!bAllEnabled) { break; }
            }
            if (bAllEnabled) {
                //If all deps are enabled then enable
                toggleConfigValue(sOptionLower, true);
            }
        }
    }
    //Perform dependency check if enabled
    if (vitOption->m_sValue.compare("1") == 0) {
        //The following are the needed dependencies that must be enabled
        string sCheckFunc = sOptionLower + "_deps";
        vector<string> vCheckList;
        if (getConfigList(sCheckFunc, vCheckList, false)) {
            vector<string>::iterator vitCheckItem = vCheckList.begin();
            for (vitCheckItem; vitCheckItem < vCheckList.end(); vitCheckItem++) {
                //Check if this is a not !
                bool bToggle = false;
                if (vitCheckItem->at(0) == '!') {
                    vitCheckItem->erase(0, 1);
                    bToggle = true;
                }
                bool bEnabled;
                ValuesList::iterator vitTemp = getConfigOption(*vitCheckItem);
                if (vitTemp == m_vConfigValues.end()) {
                    DependencyList::iterator mitDep = mAdditionalDependencies.find(*vitCheckItem);
                    if (mitDep == mAdditionalDependencies.end()) {
                        outputInfo("Unknown option in dependency (" + *vitCheckItem + ") for option (" + sOptionLower + ")");
                        bEnabled = false;
                    } else {
                        bEnabled = mitDep->second ^ bToggle;
                    }
                } else {
                    //Check if this variable has been initialized already
                    if (vitTemp > vitOption) {
                        if (!passDependencyCheck(vitTemp)) {
                            return false;
                        }
                    }
                    bEnabled = (vitTemp->m_sValue.compare("1") == 0) ^ bToggle;
                }
                //If not all deps are enabled then disable
                if (!bEnabled) {
                    toggleConfigValue(sOptionLower, false);
                    break;
                }
            }
        }
    }
    //Perform dependency check if still enabled
    if (vitOption->m_sValue.compare("1") == 0) {
        //Any 1 of the following dependencies are needed
        string sCheckFunc = sOptionLower + "_deps_any";
        vector<string> vCheckList;
        if (getConfigList(sCheckFunc, vCheckList, false)) {
            vector<string>::iterator vitCheckItem = vCheckList.begin();
            bool bAnyEnabled = false;
            for (vitCheckItem; vitCheckItem < vCheckList.end(); vitCheckItem++) {
                //Check if this is a not !
                bool bToggle = false;
                if (vitCheckItem->at(0) == '!') {
                    vitCheckItem->erase(0);
                    bToggle = true;
                }
                ValuesList::iterator vitTemp = getConfigOption(*vitCheckItem);
                if (vitTemp == m_vConfigValues.end()) {
                    DependencyList::iterator mitDep = mAdditionalDependencies.find(*vitCheckItem);
                    if (mitDep == mAdditionalDependencies.end()) {
                        outputInfo("Unknown option in any dependency (" + *vitCheckItem + ") for option (" + sOptionLower + ")");
                        bAnyEnabled = false;
                    } else {
                        bAnyEnabled = mitDep->second ^ bToggle;
                    }
                } else {
                    //Check if this variable has been initialized already
                    if (vitTemp > vitOption) {
                        if (!passDependencyCheck(vitTemp)) {
                            return false;
                        }
                    }
                    bAnyEnabled = (vitTemp->m_sValue.compare("1") == 0) ^ bToggle;
                }
                if (bAnyEnabled) { break; }
            }
            if (!bAnyEnabled) {
                //If not a single dep is enabled then disable
                toggleConfigValue(sOptionLower, false);
            }
        }
    }
    //Perform dependency check if still enabled
    if (vitOption->m_sValue.compare("1") == 0) {
        //If conflict items are enabled then this one must be disabled
        string sCheckFunc = sOptionLower + "_conflict";
        vector<string> vCheckList;
        if (getConfigList(sCheckFunc, vCheckList, false)) {
            vector<string>::iterator vitCheckItem = vCheckList.begin();
            bool bAnyEnabled = false;
            for (vitCheckItem; vitCheckItem < vCheckList.end(); vitCheckItem++) {
                //Check if this is a not !
                bool bToggle = false;
                if (vitCheckItem->at(0) == '!') {
                    vitCheckItem->erase(0);
                    bToggle = true;
                }
                ValuesList::iterator vitTemp = getConfigOption(*vitCheckItem);
                if (vitTemp == m_vConfigValues.end()) {
                    DependencyList::iterator mitDep = mAdditionalDependencies.find(*vitCheckItem);
                    if (mitDep == mAdditionalDependencies.end()) {
                        outputInfo("Unknown option in conflict dependency (" + *vitCheckItem + ") for option (" + sOptionLower + ")");
                        bAnyEnabled = false;
                    } else {
                        bAnyEnabled = mitDep->second ^ bToggle;
                    }
                } else {
                    //Check if this variable has been initialized already
                    if (vitTemp > vitOption) {
                        if (!passDependencyCheck(vitTemp)) {
                            return false;
                        }
                    }
                    bAnyEnabled = (vitTemp->m_sValue.compare("1") == 0) ^ bToggle;
                }
                if (bAnyEnabled) { break; }
            }
            if (bAnyEnabled) {
                //If a single conflict is enabled then disable
                toggleConfigValue(sOptionLower, false);
            }
        }
    }
    //Perform dependency check if still enabled
    if (vitOption->m_sValue.compare("1") == 0) {
        //All select items are enabled when this item is enabled. If one of them has since been disabled then so must this one
        string sCheckFunc = sOptionLower + "_select";
        vector<string> vCheckList;
        if (getConfigList(sCheckFunc, vCheckList, false)) {
            vector<string>::iterator vitCheckItem = vCheckList.begin();
            for (vitCheckItem; vitCheckItem < vCheckList.end(); vitCheckItem++) {
                ValuesList::iterator vitTemp = getConfigOption(*vitCheckItem);
                if (vitTemp == m_vConfigValues.end()) {
                    DependencyList::iterator mitDep = mAdditionalDependencies.find(*vitCheckItem);
                    if (mitDep == mAdditionalDependencies.end()) {
                        outputInfo("Unknown option in select dependency (" + *vitCheckItem + ") for option (" + sOptionLower + ")");
                    } if (!mitDep->second) {
                        //If any deps are disabled then disable
                        toggleConfigValue(sOptionLower, false);
                    }
                    continue;
                }
                //Check if this variable has been initialized already
                if (vitTemp > vitOption) {
                    // Enable it if it is not currently initialised
                    if (vitTemp->m_sValue.length() == 0) {
                        string sOptionLower2 = vitTemp->m_sOption;
                        transform(sOptionLower2.begin(), sOptionLower2.end(), sOptionLower2.begin(), ::tolower);
                        toggleConfigValue(sOptionLower2, true);
                    }
                    if (!passDependencyCheck(vitTemp)) {
                        return false;
                    }
                }
                if (vitTemp->m_sValue.compare("0") == 0) {
                    //If any deps are disabled then disable
                    toggleConfigValue(sOptionLower, false);
                    break;
                }
            }
        }
    }
    //Enable any required deps if still enabled
    if (vitOption->m_sValue.compare("1") == 0) {
        string sCheckFunc = sOptionLower + "_select";
        vector<string> vCheckList;
        if (getConfigList(sCheckFunc, vCheckList, false)) {
            vector<string>::iterator vitCheckItem = vCheckList.begin();
            for (vitCheckItem; vitCheckItem < vCheckList.end(); vitCheckItem++) {
                toggleConfigValue(*vitCheckItem, true);
            }
        }

        //If enabled then all of these should then be enabled (if not already forced disabled)
        sCheckFunc = sOptionLower + "_suggest";
        vCheckList.resize(0);
        if (getConfigList(sCheckFunc, vCheckList, false)) {
            vector<string>::iterator vitCheckItem = vCheckList.begin();
            for (vitCheckItem; vitCheckItem < vCheckList.end(); vitCheckItem++) {
                //Only enable if not forced to disable
                ValuesList::iterator vitTemp = getConfigOption(*vitCheckItem);
                if ((vitTemp != m_vConfigValues.end()) && (vitTemp->m_sValue.compare("0") != 0)) {
                    toggleConfigValue(*vitCheckItem, true); //Weak check
                }
            }
        }
    } else {
        //Ensure the option is not in an uninitialised state
        toggleConfigValue(sOptionLower, false);
    }
    return true;
}