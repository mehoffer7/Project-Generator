/*
 * copyright (c) 2014 Matthew Oliver
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

#include <algorithm>

bool ConfigGenerator::buildDefaultValues()
{
    //Set any unset project values
    if (m_sSolutionDirectory.length() == 0) {
        if (!m_bDCEOnly) {
            m_sSolutionDirectory = m_sRootDirectory + "SMP/";
        } else {
            m_sSolutionDirectory = m_sRootDirectory;
        }
    }
    if (m_sOutDirectory.length() == 0) {
        m_sOutDirectory = "../../../msvc/";
    }

    // configurable options
    vector<string> vList;
    if (!getConfigList("PROGRAM_LIST", vList)) {
        return false;
    }
    //Enable all programs
    vector<string>::iterator vitValues = vList.begin();
    for (vitValues; vitValues < vList.end(); vitValues++) {
        toggleConfigValue(*vitValues, true);
    }
    //Enable all libraries
    vList.resize(0);
    if (!getConfigList("LIBRARY_LIST", vList)) {
        return false;
    }
    vitValues = vList.begin();
    for (vitValues; vitValues < vList.end(); vitValues++) {
        if (!m_bLibav && vitValues->compare("avresample") != 0) {
            toggleConfigValue(*vitValues, true);
        }
    }
    //Enable all components
    vList.resize(0);
    vector<string> vList2;
    if (!getConfigList("COMPONENT_LIST", vList)) {
        return false;
    }
    vitValues = vList.begin();
    for (vitValues; vitValues < vList.end(); vitValues++) {
        toggleConfigValue(*vitValues, true);
        //Get the corresponding list and enable all member elements as well
        vitValues->resize(vitValues->length() - 1); //Need to remove the s from end
        transform(vitValues->begin(), vitValues->end(), vitValues->begin(), ::toupper);
        //Get the specific list
        vList2.resize(0);
        getConfigList(*vitValues + "_LIST", vList2);
        for (vector<string>::iterator vitComponent = vList2.begin(); vitComponent < vList2.end(); vitComponent++) {
            toggleConfigValue(*vitComponent, true);
        }
    }

    fastToggleConfigValue("runtime_cpudetect", true);
    fastToggleConfigValue("safe_bitstream_reader", true);
    fastToggleConfigValue("static", true);
    fastToggleConfigValue("shared", true);
    fastToggleConfigValue("swscale_alpha", true);

    //Enable x86 hardware architectures
    fastToggleConfigValue("x86", true);
    fastToggleConfigValue("i686", true);
    fastToggleConfigValue("fast_cmov", true);
    fastToggleConfigValue("x86_32", true);
    fastToggleConfigValue("x86_64", true);
    //Enable x86 extensions
    vList.resize(0);
    if (!getConfigList("ARCH_EXT_LIST_X86", vList)) {
        return false;
    }
    vitValues = vList.begin();
    for (vitValues; vitValues < vList.end(); vitValues++) {
        fastToggleConfigValue(*vitValues, true);
        //Also enable _EXTERNAL and _INLINE
        fastToggleConfigValue(*vitValues + "_EXTERNAL", true);
        fastToggleConfigValue(*vitValues + "_INLINE", true);
    }

    //Default we enable asm
    fastToggleConfigValue("yasm", true);
    fastToggleConfigValue("x86asm", true);
    if (m_bUseNASM) {
        //NASM doesn't support cpunop
        fastToggleConfigValue("cpunop", false);
        fastToggleConfigValue("cpunop_external", false);
    } else {
        //Yasm doesn't support avx512
        fastToggleConfigValue("avx512", false);
        fastToggleConfigValue("avx512_external", false);
        //Yasm does have cpunop
        fastToggleConfigValue("cpunop", true);
    }

    //msvc specific options
    fastToggleConfigValue("w32threads", true);
    fastToggleConfigValue("atomics_win32", true);

    //math functions
    vList.resize(0);
    if (!getConfigList("MATH_FUNCS", vList)) {
        return false;
    }
    vitValues = vList.begin();
    for (vitValues; vitValues < vList.end(); vitValues++) {
        fastToggleConfigValue(*vitValues, true);
    }

    fastToggleConfigValue("access", true);
    fastToggleConfigValue("aligned_malloc", true);
    fastToggleConfigValue("clock_gettime", false);
    fastToggleConfigValue("closesocket", true);
    fastToggleConfigValue("CommandLineToArgvW", true);
    fastToggleConfigValue("CoTaskMemFree", true);
    fastToggleConfigValue("CryptGenRandom", true);
    fastToggleConfigValue("direct_h", true);
    fastToggleConfigValue("d3d11_h", true);
    fastToggleConfigValue("dxgidebug_h", true);
    fastToggleConfigValue("dxva_h", true);
    fastToggleConfigValue("ebp_available", true);
    fastToggleConfigValue("ebx_available", true);
    fastToggleConfigValue("fast_clz", true);
    fastToggleConfigValue("flt_lim", true);
    fastToggleConfigValue("getaddrinfo", true);
    fastToggleConfigValue("getopt", false);
    fastToggleConfigValue("GetProcessAffinityMask", true);
    fastToggleConfigValue("GetProcessMemoryInfo", true);
    fastToggleConfigValue("GetProcessTimes", true);
    fastToggleConfigValue("GetSystemTimeAsFileTime", true);
    fastToggleConfigValue("io_h", true);
    fastToggleConfigValue("inline_asm_labels", true);
    fastToggleConfigValue("isatty", true);
    fastToggleConfigValue("kbhit", true);
    fastToggleConfigValue("LoadLibrary", true);
    fastToggleConfigValue("libc_msvcrt", true);
    fastToggleConfigValue("local_aligned_32", true);
    fastToggleConfigValue("local_aligned_16", true);
    fastToggleConfigValue("local_aligned_8", true);
    fastToggleConfigValue("local_aligned", true);
    fastToggleConfigValue("malloc_h", true);
    fastToggleConfigValue("MapViewOfFile", true);
    fastToggleConfigValue("MemoryBarrier", true);
    fastToggleConfigValue("mm_empty", true);
    fastToggleConfigValue("PeekNamedPipe", true);
    fastToggleConfigValue("rdtsc", true);
    fastToggleConfigValue("rsync_contimeout", true);
    fastToggleConfigValue("SetConsoleTextAttribute", true);
    fastToggleConfigValue("SetConsoleCtrlHandler", true);
    fastToggleConfigValue("setmode", true);
    fastToggleConfigValue("Sleep", true);
    fastToggleConfigValue("CONDITION_VARIABLE_Ptr", true);
    fastToggleConfigValue("socklen_t", true);
    fastToggleConfigValue("struct_addrinfo", true);
    fastToggleConfigValue("struct_group_source_req", true);
    fastToggleConfigValue("struct_ip_mreq_source", true);
    fastToggleConfigValue("struct_ipv6_mreq", true);
    fastToggleConfigValue("struct_pollfd", true);
    fastToggleConfigValue("struct_sockaddr_in6", true);
    fastToggleConfigValue("struct_sockaddr_storage", true);
    fastToggleConfigValue("unistd_h", true);
    fastToggleConfigValue("VirtualAlloc", true);
    fastToggleConfigValue("Audioclient_h", true);
    fastToggleConfigValue("windows_h", true);
    fastToggleConfigValue("winsock2_h", true);
    fastToggleConfigValue("wglgetprocaddress", true);

    fastToggleConfigValue("dos_paths", true);
    fastToggleConfigValue("dxva2api_cobj", true);
    fastToggleConfigValue("dxva2_lib", true);

    fastToggleConfigValue("aligned_stack", true);
    fastToggleConfigValue("pragma_deprecated", true);
    fastToggleConfigValue("inline_asm", true);
    fastToggleConfigValue("frame_thread_encoder", true);
    fastToggleConfigValue("xmm_clobbers", true);

    //Additional (must be explicitly disabled)
    fastToggleConfigValue("dct", true);
    fastToggleConfigValue("dwt", true);
    fastToggleConfigValue("error_resilience", true);
    fastToggleConfigValue("faan", true);
    fastToggleConfigValue("faandct", true);
    fastToggleConfigValue("faanidct", true);
    fastToggleConfigValue("fast_unaligned", true);
    fastToggleConfigValue("lsp", true);
    fastToggleConfigValue("lzo", true);
    fastToggleConfigValue("mdct", true);
    fastToggleConfigValue("network", true);
    fastToggleConfigValue("rdft", true);
    fastToggleConfigValue("fft", true);
    fastToggleConfigValue("pixelutils", true);

    //Disable all external libs until explicitly enabled
    vList.resize(0);
    if (getConfigList("EXTERNAL_LIBRARY_LIST", vList)) {
        vector<string>::iterator vitValues = vList.begin();
        for (vitValues; vitValues < vList.end(); vitValues++) {
            toggleConfigValue(*vitValues, false);
        }
    }

    //Disable all hwaccels until explicitly enabled
    vList.resize(0);
    if (getConfigList("HWACCEL_LIBRARY_LIST", vList)) {
        vector<string>::iterator vitValues = vList.begin();
        for (vitValues; vitValues < vList.end(); vitValues++) {
            toggleConfigValue(*vitValues, false);
        }
    }

    //Check if auto detection is enabled
    ValuesList::iterator itAutoDet = ConfigGenerator::getConfigOption("autodetect");
    if ((itAutoDet == m_vConfigValues.end()) || (itAutoDet->m_sValue.compare("0") != 0)) {
        //Enable all the auto detected libs
        vList.resize(0);
        if (getConfigList("AUTODETECT_LIBS", vList)) {
            string sFileName;
            vector<string>::iterator vitValues = vList.begin();
            for (vitValues; vitValues < vList.end(); vitValues++) {
                bool bEnable;
                //Handle detection of various libs
                if (vitValues->compare("alsa") == 0) {
                    bEnable = false;
                } else if (vitValues->compare("amf") == 0) {
                    makeFileGeneratorRelative(m_sOutDirectory + "include/AMF/core/Factory.h", sFileName);
                    bEnable = findFile(sFileName, sFileName);
                } else if (vitValues->compare("appkit") == 0) {
                    bEnable = false;
                } else if (vitValues->compare("bzlib") == 0) {
                    makeFileGeneratorRelative(m_sOutDirectory + "include/bzlib.h", sFileName);
                    bEnable = findFile(sFileName, sFileName);
                } else if (vitValues->compare("iconv") == 0) {
                    makeFileGeneratorRelative(m_sOutDirectory + "include/iconv.h", sFileName);
                    bEnable = findFile(sFileName, sFileName);
                } else if (vitValues->compare("jack") == 0) {
                    bEnable = false;
                } else if (vitValues->compare("libxcb") == 0) {
                    bEnable = false;
                } else if (vitValues->compare("libxcb_shm") == 0) {
                    bEnable = false;
                } else if (vitValues->compare("libxcb_shape") == 0) {
                    bEnable = false;
                } else if (vitValues->compare("libxcb_xfixes") == 0) {
                    bEnable = false;
                } else if (vitValues->compare("lzma") == 0) {
                    makeFileGeneratorRelative(m_sOutDirectory + "include/lzma.h", sFileName);
                    bEnable = findFile(sFileName, sFileName);
                } else if (vitValues->compare("schannel") == 0) {
                    bEnable = true;
                } else if (vitValues->compare("sdl2") == 0) {
                    makeFileGeneratorRelative(m_sOutDirectory + "include/SDL/SDL.h", sFileName);
                    bEnable = findFile(sFileName, sFileName);
                } else if (vitValues->compare("securetransport") == 0) {
                    bEnable = false;
                } else if (vitValues->compare("sndio") == 0) {
                    bEnable = false;
                } else if (vitValues->compare("xlib") == 0) {
                    bEnable = false;
                } else if (vitValues->compare("zlib") == 0) {
                    makeFileGeneratorRelative(m_sOutDirectory + "include/zlib.h", sFileName);
                    bEnable = findFile(sFileName, sFileName);
                } else if (vitValues->compare("amf") == 0) {
                    makeFileGeneratorRelative(m_sOutDirectory + "include/AMF/core/Version.h", sFileName);
                    bEnable = findFile(sFileName, sFileName);
                } else if (vitValues->compare("audiotoolbox") == 0) {
                    bEnable = false;
                } else if (vitValues->compare("crystalhd") == 0) {
                    bEnable = false;
                } else if (vitValues->compare("cuda") == 0) {
                    bEnable = findFile(m_sRootDirectory + "compat/cuda/dynlink_cuda.h", sFileName);
                    if (!bEnable) {
                        makeFileGeneratorRelative(m_sOutDirectory + "include/ffnvcodec/dynlink_cuda.h", sFileName);
                        bEnable = findFile(sFileName, sFileName);
                    }
                } else if (vitValues->compare("cuvid") == 0) {
                    bEnable = findFile(m_sRootDirectory + "compat/cuda/dynlink_cuda.h", sFileName);
                    if (!bEnable) {
                        makeFileGeneratorRelative(m_sOutDirectory + "include/ffnvcodec/dynlink_cuda.h", sFileName);
                        bEnable = findFile(sFileName, sFileName);
                    }
                } else if (vitValues->compare("d3d11va") == 0) {
                    bEnable = true;
                } else if (vitValues->compare("dxva2") == 0) {
                    bEnable = true;
                } else if (vitValues->compare("ffnvcodec") == 0) {
                    makeFileGeneratorRelative(m_sOutDirectory + "include/ffnvcodec/dynlink_cuda.h", sFileName);
                    bEnable = findFile(sFileName, sFileName);
                } else if (vitValues->compare("nvdec") == 0) {
                    bEnable = (findFile(m_sRootDirectory + "compat/cuda/dynlink_loader.h", sFileName) &&
                               findFile(m_sRootDirectory + "compat/cuda/dynlink_cuda.h", sFileName));
                    if (!bEnable) {
                        makeFileGeneratorRelative(m_sOutDirectory + "include/ffnvcodec/dynlink_loader.h", sFileName);
                        bEnable = findFile(sFileName, sFileName);
                    }
                } else if (vitValues->compare("nvenc") == 0) {
                    bEnable = findFile(m_sRootDirectory + "compat/nvenc/nvEncodeAPI.h", sFileName);
                    if (!bEnable) {
                        makeFileGeneratorRelative(m_sOutDirectory + "include/ffnvcodec/nvEncodeAPI.h", sFileName);
                        bEnable = findFile(sFileName, sFileName);
                    }
                } else if (vitValues->compare("opencl") == 0) {
                    makeFileGeneratorRelative(m_sOutDirectory + "include/cl/cl.h", sFileName);
                    bEnable = findFile(sFileName, sFileName);
                } else if (vitValues->compare("vaapi") == 0) {
                    bEnable = false;
                } else if (vitValues->compare("vda") == 0) {
                    bEnable = false;
                } else if (vitValues->compare("vdpau") == 0) {
                    bEnable = false;
                } else if (vitValues->compare("videotoolbox_hwaccel") == 0) {
                    bEnable = false;
                } else if (vitValues->compare("v4l2_m2m") == 0) {
                    bEnable = false;
                } else if (vitValues->compare("xvmc") == 0) {
                    bEnable = false;
                } else if (vitValues->compare("pthreads") == 0) {
                    bEnable = false;
                } else if (vitValues->compare("os2threads") == 0) {
                    bEnable = false;
                } else if (vitValues->compare("w32threads") == 0) {
                    bEnable = true;
                } else if (vitValues->compare("avfoundation") == 0) {
                    bEnable = false;
                } else if (vitValues->compare("coreimage") == 0) {
                    bEnable = false;
                } else if (vitValues->compare("videotoolbox") == 0) {
                    bEnable = false;
                } else {
                    //This is an unknown option
                    outputInfo("Found unknown auto detected option " + *vitValues);
                    //Just disable
                    bEnable = false;
                }
                toggleConfigValue(*vitValues, bEnable);
            }
            fastToggleConfigValue("autodetect", true);
        } else {
            //If no auto list then just use hard enables
            fastToggleConfigValue("bzlib", true);
            fastToggleConfigValue("iconv", true);
            fastToggleConfigValue("lzma", true);
            fastToggleConfigValue("schannel", true);
            fastToggleConfigValue("sdl", true);
            fastToggleConfigValue("sdl2", true);
            fastToggleConfigValue("zlib", true);

            //Enable hwaccels by default.
            fastToggleConfigValue("d3d11va", true);
            fastToggleConfigValue("dxva2", true);

            string sFileName;
            if (findFile(m_sRootDirectory + "compat/cuda/dynlink_cuda.h", sFileName)) {
                fastToggleConfigValue("cuda", true);
                fastToggleConfigValue("cuvid", true);
            }
            if (findFile(m_sRootDirectory + "compat/nvenc/nvEncodeAPI.h", sFileName)) {
                fastToggleConfigValue("nvenc", true);
            }
        }
    }

    return buildForcedValues();
}

bool ConfigGenerator::buildForcedValues()
{
    //Additional options set for Intel compiler specific inline asm
    fastToggleConfigValue("inline_asm_nonlocal_labels", false);
    fastToggleConfigValue("inline_asm_direct_symbol_refs", false);
    fastToggleConfigValue("inline_asm_non_intel_mnemonic", false);

    fastToggleConfigValue("xlib", false); //enabled by default but is linux only so we force disable
    fastToggleConfigValue("qtkit", false);
    fastToggleConfigValue("avfoundation", false);
    fastToggleConfigValue("mmal", false);
    fastToggleConfigValue("libdrm", false);
    fastToggleConfigValue("libv4l2", false);

    //values that are not correctly handled by configure
    fastToggleConfigValue("coreimage_filter", false);
    fastToggleConfigValue("coreimagesrc_filter", false);

    return true;
}

void ConfigGenerator::buildFixedValues(DefaultValuesList & mFixedValues)
{
    mFixedValues.clear();
    mFixedValues["$(c_escape $FFMPEG_CONFIGURATION)"] = "";
    mFixedValues["$(c_escape $LIBAV_CONFIGURATION)"] = "";
    mFixedValues["$(c_escape $license)"] = "lgpl";
    mFixedValues["$(eval c_escape $datadir)"] = ".";
    mFixedValues["$(c_escape ${cc_ident:-Unknown compiler})"] = "msvc";
    mFixedValues["$_restrict"] = "__restrict";
    mFixedValues["$restrict_keyword"] = "__restrict";
    mFixedValues["${extern_prefix}"] = "";
    mFixedValues["$build_suffix"] = "";
    mFixedValues["$SLIBSUF"] = "";
    mFixedValues["$sws_max_filter_size"] = "256";
}

void ConfigGenerator::buildReplaceValues(DefaultValuesList & mReplaceValues, DefaultValuesList & mASMReplaceValues)
{
    mReplaceValues.clear();
    //Add to config.h only list
    mReplaceValues["CC_IDENT"] = "#if defined(__INTEL_COMPILER)\n\
#   define CC_IDENT \"icl\"\n\
#else\n\
#   define CC_IDENT \"msvc\"\n\
#endif";
    mReplaceValues["EXTERN_PREFIX"] = "#if defined(__x86_64) || defined(_M_X64)\n\
#   define EXTERN_PREFIX \"\"\n\
#else\n\
#   define EXTERN_PREFIX \"_\"\n\
#endif";
    mReplaceValues["EXTERN_ASM"] = "#if defined(__x86_64) || defined(_M_X64)\n\
#   define EXTERN_ASM\n\
#else\n\
#   define EXTERN_ASM _\n\
#endif";
    mReplaceValues["SLIBSUF"] = "#if defined(_USRDLL) || defined(_WINDLL)\n\
#   define SLIBSUF \".dll\"\n\
#else\n\
#   define SLIBSUF \".lib\"\n\
#endif";

    mReplaceValues["ARCH_X86_32"] = "#if defined(__x86_64) || defined(_M_X64)\n\
#   define ARCH_X86_32 0\n\
#else\n\
#   define ARCH_X86_32 1\n\
#endif";
    mReplaceValues["ARCH_X86_64"] = "#if defined(__x86_64) || defined(_M_X64)\n\
#   define ARCH_X86_64 1\n\
#else\n\
#   define ARCH_X86_64 0\n\
#endif";
    mReplaceValues["CONFIG_SHARED"] = "#if defined(_USRDLL) || defined(_WINDLL)\n\
#   define CONFIG_SHARED 1\n\
#else\n\
#   define CONFIG_SHARED 0\n\
#endif";
    mReplaceValues["CONFIG_STATIC"] = "#if defined(_USRDLL) || defined(_WINDLL)\n\
#   define CONFIG_STATIC 0\n\
#else\n\
#   define CONFIG_STATIC 1\n\
#endif";
    mReplaceValues["HAVE_ALIGNED_STACK"] = "#if defined(__x86_64) || defined(_M_X64)\n\
#   define HAVE_ALIGNED_STACK 1\n\
#else\n\
#   define HAVE_ALIGNED_STACK 0\n\
#endif";
    mReplaceValues["HAVE_FAST_64BIT"] = "#if defined(__x86_64) || defined(_M_X64)\n\
#   define HAVE_FAST_64BIT 1\n\
#else\n\
#   define HAVE_FAST_64BIT 0\n\
#endif";
    mReplaceValues["HAVE_INLINE_ASM"] = "#if defined(__INTEL_COMPILER)\n\
#   define HAVE_INLINE_ASM 1\n\
#else\n\
#   define HAVE_INLINE_ASM 0\n\
#endif";
    mReplaceValues["HAVE_MM_EMPTY"] = "#if defined(__INTEL_COMPILER) || ARCH_X86_32\n\
#   define HAVE_MM_EMPTY 1\n\
#else\n\
#   define HAVE_MM_EMPTY 0\n\
#endif";
    mReplaceValues["HAVE_STRUCT_POLLFD"] = "#if !defined(_WIN32_WINNT) || _WIN32_WINNT >= 0x0600\n\
#   define HAVE_STRUCT_POLLFD 1\n\
#else\n\
#   define HAVE_STRUCT_POLLFD 0\n\
#endif";
    mReplaceValues["CONFIG_D3D11VA"] = "#ifdef _WIN32\n\
#include <sdkddkver.h>\n\
#endif\n\
#if defined(NTDDI_WIN8)\n\
#   define CONFIG_D3D11VA 1\n\
#else\n\
#   define CONFIG_D3D11VA 0\n\
#endif";
    mReplaceValues["CONFIG_VP9_D3D11VA_HWACCEL"] = "#ifdef _WIN32\n\
#include <sdkddkver.h>\n\
#endif\n\
#if defined(NTDDI_WIN10_TH2)\n\
#   define CONFIG_VP9_D3D11VA_HWACCEL 1\n\
#else\n\
#   define CONFIG_VP9_D3D11VA_HWACCEL 0\n\
#endif";
    mReplaceValues["CONFIG_VP9_D3D11VA2_HWACCEL"] = "#ifdef _WIN32\n\
#include <sdkddkver.h>\n\
#endif\n\
#if defined(NTDDI_WIN10_TH2)\n\
#   define CONFIG_VP9_D3D11VA2_HWACCEL 1\n\
#else\n\
#   define CONFIG_VP9_D3D11VA2_HWACCEL 0\n\
#endif";
    mReplaceValues["CONFIG_VP9_DXVA2_HWACCEL"] = "#ifdef _WIN32\n\
#include <sdkddkver.h>\n\
#endif\n\
#if defined(NTDDI_WIN10_TH2)\n\
#   define CONFIG_VP9_DXVA2_HWACCEL 1\n\
#else\n\
#   define CONFIG_VP9_DXVA2_HWACCEL 0\n\
#endif";
    mReplaceValues["HAVE_OPENCL_D3D11"] = "#ifdef _WIN32\n\
#include <sdkddkver.h>\n\
#endif\n\
#if defined(NTDDI_WIN8)\n\
#   define HAVE_OPENCL_D3D11 1\n\
#else\n\
#   define HAVE_OPENCL_D3D11 0\n\
#endif";

    //Build replace values for all x86 inline asm
    vector<string> vInlineList;
    getConfigList("ARCH_EXT_LIST_X86", vInlineList);
    for (vector<string>::iterator vitIt = vInlineList.begin(); vitIt < vInlineList.end(); vitIt++) {
        transform(vitIt->begin(), vitIt->end(), vitIt->begin(), ::toupper);
        string sName = "HAVE_" + *vitIt + "_INLINE";
        mReplaceValues[sName] = "#define " + sName + " ARCH_X86 && HAVE_INLINE_ASM";
    }

    //Sanity checks for inline asm (Needed as some code only checks availability and not inline_asm)
    mReplaceValues["HAVE_EBP_AVAILABLE"] = "#if HAVE_INLINE_ASM && !defined(_DEBUG)\n\
#   define HAVE_EBP_AVAILABLE 1\n\
#else\n\
#   define HAVE_EBP_AVAILABLE 0\n\
#endif";
    mReplaceValues["HAVE_EBX_AVAILABLE"] = "#if HAVE_INLINE_ASM && !defined(_DEBUG)\n\
#   define HAVE_EBX_AVAILABLE 1\n\
#else\n\
#   define HAVE_EBX_AVAILABLE 0\n\
#endif";

    //Add any values that may depend on a replace value from above^
    DefaultValuesList mNewReplaceValues;
    ValuesList::iterator vitOption = m_vConfigValues.begin();
    string sSearchSuffix[] = {"_deps", "_select"};
    for (vitOption; vitOption < m_vConfigValues.begin() + m_uiConfigValuesEnd; vitOption++) {
        string sTagName = vitOption->m_sPrefix + vitOption->m_sOption;
        //Check for forced replacement (only if attribute is not disabled)
        if ((vitOption->m_sValue.compare("0") != 0) && (mReplaceValues.find(sTagName) != mReplaceValues.end())) {
            //Already exists in list so can skip
            continue;
        } else {
            if (vitOption->m_sValue.compare("1") == 0) {
                //Check if it depends on a replace value
                string sOptionLower = vitOption->m_sOption;
                transform(sOptionLower.begin(), sOptionLower.end(), sOptionLower.begin(), ::tolower);
                for (int iSuff = 0; iSuff < (sizeof(sSearchSuffix) / sizeof(sSearchSuffix[0])); iSuff++) {
                    string sCheckFunc = sOptionLower + sSearchSuffix[iSuff];
                    vector<string> vCheckList;
                    if (getConfigList(sCheckFunc, vCheckList, false)) {
                        string sAddConfig;
                        bool bReservedDeps = false;
                        vector<string>::iterator vitCheckItem = vCheckList.begin();
                        for (vitCheckItem; vitCheckItem < vCheckList.end(); vitCheckItem++) {
                            //Check if this is a not !
                            bool bToggle = false;
                            if (vitCheckItem->at(0) == '!') {
                                vitCheckItem->erase(0, 1);
                                bToggle = true;
                            }
                            ValuesList::iterator vitTemp = getConfigOption(*vitCheckItem);
                            if (vitTemp != m_vConfigValues.end()) {
                                string sReplaceCheck = vitTemp->m_sPrefix + vitTemp->m_sOption;
                                transform(sReplaceCheck.begin(), sReplaceCheck.end(), sReplaceCheck.begin(), ::toupper);
                                DefaultValuesList::iterator mitDep = mReplaceValues.find(sReplaceCheck);
                                if (mitDep != mReplaceValues.end()) {
                                    sAddConfig += " " + sReplaceCheck;
                                    if (bToggle)
                                        sAddConfig = '!' + sAddConfig;
                                    bReservedDeps = true;
                                }
                                if (bToggle ^ (vitTemp->m_sValue.compare("1") == 0)) {
                                    //Check recursively if dep has any deps that are reserved types
                                    sOptionLower = vitTemp->m_sOption;
                                    transform(sOptionLower.begin(), sOptionLower.end(), sOptionLower.begin(), ::tolower);
                                    for (int iSuff2 = 0; iSuff2 < (sizeof(sSearchSuffix) / sizeof(sSearchSuffix[0])); iSuff2++) {
                                        sCheckFunc = sOptionLower + sSearchSuffix[iSuff2];
                                        vector<string> vCheckList2;
                                        if (getConfigList(sCheckFunc, vCheckList2, false)) {
                                            uint uiCPos = vitCheckItem - vCheckList.begin();
                                            //Check if not already in list
                                            vector<string>::iterator vitCheckItem2 = vCheckList2.begin();
                                            for (vitCheckItem2; vitCheckItem2 < vCheckList2.end(); vitCheckItem2++) {
                                                //Check if this is a not !
                                                bool bToggle2 = bToggle;
                                                if (vitCheckItem2->at(0) == '!') {
                                                    vitCheckItem2->erase(0, 1);
                                                    bToggle2 = !bToggle2;
                                                }
                                                string sCheckVal = *vitCheckItem2;
                                                if (bToggle2)
                                                    sCheckVal = '!' + sCheckVal;
                                                if (find(vCheckList.begin(), vCheckList.end(), sCheckVal) == vCheckList.end()) {
                                                    vCheckList.push_back(sCheckVal);
                                                }
                                            }
                                            //update iterator position
                                            vitCheckItem = vCheckList.begin() + uiCPos;
                                        }
                                    }
                                }
                            }
                        }

                        if (bReservedDeps) {
                            //Add to list
                            mNewReplaceValues[sTagName] = "#define " + sTagName + sAddConfig;
                        }
                    }
                }
            }
        }
    }
    for (DefaultValuesList::iterator mitI = mNewReplaceValues.begin(); mitI != mNewReplaceValues.end(); mitI++) {
        //Add them to the returned list (done here so that any checks above that test if it is reserved only operate on the unmodified original list)
        mReplaceValues[mitI->first] = mitI->second;
    }

    //Add to config.asm only list
    if (m_bUseNASM) {
        mASMReplaceValues["ARCH_X86_32"] = "%if __BITS__ = 64\n\
%define ARCH_X86_32 0\n\
%elif __BITS__ = 32\n\
%define ARCH_X86_32 1\n\
%define PREFIX\n\
%endif";
        mASMReplaceValues["ARCH_X86_64"] = "%if __BITS__ = 64\n\
%define ARCH_X86_64 1\n\
%elif __BITS__ = 32\n\
%define ARCH_X86_64 0\n\
%endif";
        mASMReplaceValues["HAVE_ALIGNED_STACK"] = "%if __BITS__ = 64\n\
%define HAVE_ALIGNED_STACK 1\n\
%elif __BITS__ = 32\n\
%define HAVE_ALIGNED_STACK 0\n\
%endif";
        mASMReplaceValues["HAVE_FAST_64BIT"] = "%if __BITS__ = 64\n\
%define HAVE_FAST_64BIT 1\n\
%elif __BITS__ = 32\n\
%define HAVE_FAST_64BIT 0\n\
%endif";
    } else {
        mASMReplaceValues["ARCH_X86_32"] = "%ifidn __OUTPUT_FORMAT__,x64\n\
%define ARCH_X86_32 0\n\
%elifidn __OUTPUT_FORMAT__,win64\n\
%define ARCH_X86_32 0\n\
%elifidn __OUTPUT_FORMAT__,win32\n\
%define ARCH_X86_32 1\n\
%define PREFIX\n\
%endif";
        mASMReplaceValues["ARCH_X86_64"] = "%ifidn __OUTPUT_FORMAT__,x64\n\
%define ARCH_X86_64 1\n\
%elifidn __OUTPUT_FORMAT__,win64\n\
%define ARCH_X86_64 1\n\
%elifidn __OUTPUT_FORMAT__,win32\n\
%define ARCH_X86_64 0\n\
%endif";
        mASMReplaceValues["HAVE_ALIGNED_STACK"] = "%ifidn __OUTPUT_FORMAT__,x64\n\
%define HAVE_ALIGNED_STACK 1\n\
%elifidn __OUTPUT_FORMAT__,win64\n\
%define HAVE_ALIGNED_STACK 1\n\
%elifidn __OUTPUT_FORMAT__,win32\n\
%define HAVE_ALIGNED_STACK 0\n\
%endif";
        mASMReplaceValues["HAVE_FAST_64BIT"] = "%ifidn __OUTPUT_FORMAT__,x64\n\
%define HAVE_FAST_64BIT 1\n\
%elifidn __OUTPUT_FORMAT__,win64\n\
%define HAVE_FAST_64BIT 1\n\
%elifidn __OUTPUT_FORMAT__,win32\n\
%define HAVE_FAST_64BIT 0\n\
%endif";
    }
}

void ConfigGenerator::buildReservedValues(vector<string> & vReservedItems)
{
    vReservedItems.resize(0);
    //The following are reserved values that are automatically handled and can not be set explicitly
    vReservedItems.push_back("x86_32");
    vReservedItems.push_back("x86_64");
    vReservedItems.push_back("xmm_clobbers");
    vReservedItems.push_back("shared");
    vReservedItems.push_back("static");
    vReservedItems.push_back("aligned_stack");
    vReservedItems.push_back("fast_64bit");
    vReservedItems.push_back("mm_empty");
    vReservedItems.push_back("ebp_available");
    vReservedItems.push_back("ebx_available");
    vReservedItems.push_back("debug");
    vReservedItems.push_back("hardcoded_tables"); //Not supported
    vReservedItems.push_back("small");
    vReservedItems.push_back("lto");
    vReservedItems.push_back("pic");
}

void ConfigGenerator::buildAdditionalDependencies(DependencyList & mAdditionalDependencies)
{
    mAdditionalDependencies.clear();
    mAdditionalDependencies["capCreateCaptureWindow"] = true;
    mAdditionalDependencies["const_nan"] = true;
    mAdditionalDependencies["CreateDIBSection"] = true;
    mAdditionalDependencies["dv1394"] = false;
    mAdditionalDependencies["DXVA_PicParams_HEVC"] = true;
    mAdditionalDependencies["DXVA_PicParams_VP9"] = true;
    mAdditionalDependencies["dxva2api_h"] = true;
    mAdditionalDependencies["fork"] = false;
    mAdditionalDependencies["jack_jack_h"] = false;
    mAdditionalDependencies["IBaseFilter"] = true;
    mAdditionalDependencies["ID3D11VideoDecoder"] = true;
    mAdditionalDependencies["ID3D11VideoContext"] = true;
    mAdditionalDependencies["libcrystalhd_libcrystalhd_if_h"] = false;
    mAdditionalDependencies["linux_fb_h"] = false;
    mAdditionalDependencies["linux_videodev_h"] = false;
    mAdditionalDependencies["linux_videodev2_h"] = false;
    mAdditionalDependencies["LoadLibrary"] = true;
    mAdditionalDependencies["parisc64"] = false;
    mAdditionalDependencies["DXVA2_ConfigPictureDecode"] = true;
    mAdditionalDependencies["snd_pcm_htimestamp"] = false;
    mAdditionalDependencies["va_va_h"] = false;
    mAdditionalDependencies["vdpau_vdpau_h"] = false;
    mAdditionalDependencies["vdpau_vdpau_x11_h"] = false;
    mAdditionalDependencies["vfw32"] = true;
    mAdditionalDependencies["vfwcap_defines"] = true;
    mAdditionalDependencies["VideoDecodeAcceleration_VDADecoder_h"] = false;
    mAdditionalDependencies["X11_extensions_Xvlib_h"] = false;
    mAdditionalDependencies["X11_extensions_XvMClib_h"] = false;
    mAdditionalDependencies["x264_csp_bgr"] = isConfigOptionEnabled("libx264");
    bool bCuvid = isConfigOptionEnabled("cuvid");
    mAdditionalDependencies["CUVIDH264PICPARAMS"] = bCuvid;
    mAdditionalDependencies["CUVIDHEVCPICPARAMS"] = bCuvid;
    mAdditionalDependencies["CUVIDVC1PICPARAMS"] = bCuvid;
    mAdditionalDependencies["CUVIDVP9PICPARAMS"] = bCuvid;
    mAdditionalDependencies["VAEncPictureParameterBufferH264"] = false;
    mAdditionalDependencies["videotoolbox_encoder"] = false;
    mAdditionalDependencies["VAEncPictureParameterBufferHEVC"] = false;
    mAdditionalDependencies["VAEncPictureParameterBufferJPEG"] = false;
    mAdditionalDependencies["VAEncPictureParameterBufferMPEG2"] = false;
    mAdditionalDependencies["VAEncPictureParameterBufferVP8"] = false;
    mAdditionalDependencies["VAEncPictureParameterBufferVP9"] = false;
    mAdditionalDependencies["ole32"] = true;
    mAdditionalDependencies["shell32"] = true;
    mAdditionalDependencies["wincrypt"] = true;
    mAdditionalDependencies["psapi"] = true;
    mAdditionalDependencies["user32"] = true;
    mAdditionalDependencies["qtkit"] = false;
    mAdditionalDependencies["coreservices"] = false;
    mAdditionalDependencies["corefoundation"] = false;
    mAdditionalDependencies["corevideo"] = false;
    mAdditionalDependencies["coremedia"] = false;
    mAdditionalDependencies["coregraphics"] = false;
    mAdditionalDependencies["applicationservices"] = false;
    mAdditionalDependencies["libdl"] = false;
    mAdditionalDependencies["libm"] = false;
    mAdditionalDependencies["libvorbisenc"] = isConfigOptionEnabled("libvorbis");
    if (getConfigOption("atomics_native") == m_vConfigValues.end()) {
        mAdditionalDependencies["atomics_native"] = true;
    }
}

void ConfigGenerator::buildOptimisedDisables(OptimisedConfigList & mOptimisedDisables)
{
    //This used is to return prioritised version of different config options
    //  For instance If enabling the decoder from an passed in library that is better than the inbuilt one
    //  then simply disable the inbuilt so as to avoid unnecessary compilation

    mOptimisedDisables.clear();
    //From trac.ffmpeg.org/wiki/GuidelinesHighQualityAudio
    //Dolby Digital: ac3
    //Dolby Digital Plus: eac3
    //MP2: libtwolame, mp2
    //Windows Media Audio 1: wmav1
    //Windows Media Audio 2: wmav2
    //LC-AAC: libfdk_aac, libfaac, aac, libvo_aacenc
    //HE-AAC: libfdk_aac, libaacplus
    //Vorbis: libvorbis, vorbis
    //MP3: libmp3lame, libshine
    //Opus: libopus
    //libopus >= libvorbis >= libfdk_aac > libmp3lame > libfaac >= eac3/ac3 > aac > libtwolame > vorbis > mp2 > wmav2/wmav1 > libvo_aacenc

#ifdef OPTIMISE_ENCODERS
    mOptimisedDisables["LIBTWOLAME_ENCODER"].push_back("MP2_ENCODER");
    mOptimisedDisables["LIBFDK_AAC_ENCODER"].push_back("LIBFAAC_ENCODER");
    mOptimisedDisables["LIBFDK_AAC_ENCODER"].push_back("AAC_ENCODER");
    mOptimisedDisables["LIBFDK_AAC_ENCODER"].push_back("LIBVO_AACENC_ENCODER");
    mOptimisedDisables["LIBFDK_AAC_ENCODER"].push_back("LIBAACPLUS_ENCODER");
    mOptimisedDisables["LIBFAAC_ENCODER"].push_back("AAC_ENCODER");
    mOptimisedDisables["LIBFAAC_ENCODER"].push_back("LIBVO_AACENC_ENCODER");
    mOptimisedDisables["AAC_ENCODER"].push_back("LIBVO_AACENC_ENCODER");
    mOptimisedDisables["LIBVORBIS_ENCODER"].push_back("VORBIS_ENCODER");
    mOptimisedDisables["LIBMP3LAME_ENCODER"].push_back("LIBSHINE_ENCODER");
    mOptimisedDisables["LIBOPENJPEG_ENCODER"].push_back("JPEG2000_ENCODER");//???
    mOptimisedDisables["LIBUTVIDEO_ENCODER"].push_back("UTVIDEO_ENCODER");//???
    mOptimisedDisables["LIBWAVPACK_ENCODER"].push_back("WAVPACK_ENCODER");//???
#endif

#ifdef OPTIMISE_DECODERS
    mOptimisedDisables["LIBGSM_DECODER"].push_back("GSM_DECODER");//???
    mOptimisedDisables["LIBGSM_MS_DECODER"].push_back("GSM_MS_DECODER");//???
    mOptimisedDisables["LIBNUT_MUXER"].push_back("NUT_MUXER");
    mOptimisedDisables["LIBNUT_DEMUXER"].push_back("NUT_DEMUXER");
    mOptimisedDisables["LIBOPENCORE_AMRNB_DECODER"].push_back("AMRNB_DECODER");//???
    mOptimisedDisables["LIBOPENCORE_AMRWB_DECODER"].push_back("AMRWB_DECODER");//???
    mOptimisedDisables["LIBOPENJPEG_DECODER"].push_back("JPEG2000_DECODER");//???
    mOptimisedDisables["LIBSCHROEDINGER_DECODER"].push_back("DIRAC_DECODER");
    mOptimisedDisables["LIBUTVIDEO_DECODER"].push_back("UTVIDEO_DECODER");//???
    mOptimisedDisables["VP8_DECODER"].push_back("LIBVPX_VP8_DECODER");//Inbuilt native decoder is apparently faster
    mOptimisedDisables["VP9_DECODER"].push_back("LIBVPX_VP9_DECODER");
    mOptimisedDisables["OPUS_DECODER"].push_back("LIBOPUS_DECODER");//??? Not sure which is better
#endif
}

#define CHECKFORCEDENABLES( Opt ) { if( getConfigOption( Opt ) != m_vConfigValues.end( ) ){ vForceEnable.push_back( Opt ); } }

void ConfigGenerator::buildForcedEnables(string sOptionLower, vector<string> & vForceEnable)
{
    if (sOptionLower.compare("fontconfig") == 0) {
        CHECKFORCEDENABLES("libfontconfig");
    } else if (sOptionLower.compare("dxva2") == 0) {
        CHECKFORCEDENABLES("dxva2_lib");
    } else if (sOptionLower.compare("libcdio") == 0) {
        CHECKFORCEDENABLES("cdio_paranoia_paranoia_h");
    } else if (sOptionLower.compare("libmfx") == 0) {
        CHECKFORCEDENABLES("qsv");
    } else if (sOptionLower.compare("dcadec") == 0) {
        CHECKFORCEDENABLES("struct_dcadec_exss_info_matrix_encoding");
    } else if (sOptionLower.compare("sdl") == 0) {
        fastToggleConfigValue("sdl2", true); //must use fastToggle to prevent infinite cycle
    } else if (sOptionLower.compare("sdl2") == 0) {
        fastToggleConfigValue("sdl", true); //must use fastToggle to prevent infinite cycle
    } else if (sOptionLower.compare("libvorbis") == 0) {
        CHECKFORCEDENABLES("libvorbisenc");
    } else if (sOptionLower.compare("opencl") == 0) {
        CHECKFORCEDENABLES("opencl_d3d11");
        CHECKFORCEDENABLES("opencl_dxva2");
    } else if (sOptionLower.compare("ffnvcodec") == 0) {
        CHECKFORCEDENABLES("cuda");
    } else if (sOptionLower.compare("cuda") == 0) {
        CHECKFORCEDENABLES("ffnvcodec");
    }
}

void ConfigGenerator::buildForcedDisables(string sOptionLower, vector<string> & vForceDisable)
{
    if (sOptionLower.compare("sdl") == 0) {
        fastToggleConfigValue("sdl2", false); //must use fastToggle to prevent infinite cycle
    } else if (sOptionLower.compare("sdl2") == 0) {
        fastToggleConfigValue("sdl", false); //must use fastToggle to prevent infinite cycle
    } else {
        // Currently disable values are exact opposite of the corresponding enable ones
        buildForcedEnables(sOptionLower, vForceDisable);
    }
}

void ConfigGenerator::buildEarlyConfigArgs(vector<string> & vEarlyArgs)
{
    vEarlyArgs.resize(0);
    vEarlyArgs.push_back("--rootdir");
    vEarlyArgs.push_back("--projdir");
    vEarlyArgs.push_back("--prefix");
    vEarlyArgs.push_back("--loud");
    vEarlyArgs.push_back("--quiet");
    vEarlyArgs.push_back("--autodetect");
    vEarlyArgs.push_back("--use-yasm");
}

void ConfigGenerator::buildObjects(const string & sTag, vector<string> & vObjects)
{
    if (sTag.compare("COMPAT_OBJS") == 0) {
        vObjects.push_back("msvcrt/snprintf"); //msvc only provides _snprintf which does not conform to snprintf standard
        vObjects.push_back("strtod"); //msvc contains a strtod but it does not handle NaN's correctly
        vObjects.push_back("getopt");
    } else if (sTag.compare("EMMS_OBJS__yes_") == 0) {
        if (this->getConfigOption("MMX_EXTERNAL")->m_sValue.compare("1") == 0) {
            vObjects.push_back("x86/emms"); //asm emms is not required in 32b but is for 64bit unless with icl
        }
    }
}