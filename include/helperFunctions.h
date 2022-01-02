/*
 * copyright (c) 2015 Matthew Oliver
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

#ifndef _HELPERFUNCTIONS_H_
#define _HELPERFUNCTIONS_H_

#include <string>
#include <vector>

using namespace std;

#if defined(__x86_64) || defined(_M_X64)
typedef unsigned __int64 uint;
#else
typedef unsigned int uint;
#endif

namespace project_generate {
bool loadFromFile(const string& sFileName, string& sRetString, bool bBinary = false, bool bOutError = true);

bool loadFromResourceFile(int iResourceID, string& sRetString);

bool writeToFile(const string& sFileName, const string& sString, bool bBinary = false);

bool copyResourceFile(int iResourceID, const string & sDestinationFile);

void deleteFile(const string & sDestinationFile);

void deleteFolder(const string & sDestinationFolder);

string getCopywriteHeader(const string& sDecription);

bool makeDirectory(const string& sDirectory);

bool findFile(const string & sFileName, string & sRetFileName);

bool findFiles(const string & sFileSearch, vector<string> & vRetFiles, bool bRecursive = true);

bool findFolders(const string & sFolderSearch, vector<string>& vRetFolders, bool bRecursive = true);

void makePathsRelative(const string& sPath, const string& sMakeRelativeTo, string& sRetPath);

void removeWhiteSpace(string & sString);

void findAndReplace(string & sString, const string & sSearch, const string & sReplace);

const string sEndLine = "\n\r\f\v";
const string sWhiteSpace = " \t" + sEndLine;
const string sOperators = "+-*/=<>;()[]{}!^%|&~\'\"#";
const string sNonName = sOperators + sWhiteSpace;
const string sPreProcessor = "&|()!";
};

using namespace project_generate;

#endif
