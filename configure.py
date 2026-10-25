#!/usr/bin/env python3

###
# Generates build files for the project.
# This file also includes the project configuration,
# such as compiler flags and the object matching status.
#
# Usage:
#   python3 configure.py
#   ninja
#
# Append --help to see available options.
###

import argparse
import sys
from pathlib import Path
from typing import Any, Dict, List

from tools.project import (
    Object,
    ProgressCategory,
    ProjectConfig,
    calculate_progress,
    generate_build,
    is_windows,
)

# Game versions
DEFAULT_VERSION = 0
VERSIONS = [
    "HAFE",  # Forecast Channel USA/NTSC
]

parser = argparse.ArgumentParser()
parser.add_argument(
    "mode",
    choices=["configure", "progress"],
    default="configure",
    help="script mode (default: configure)",
    nargs="?",
)
parser.add_argument(
    "-v",
    "--version",
    choices=VERSIONS,
    type=str.upper,
    default=VERSIONS[DEFAULT_VERSION],
    help="version to build",
)
parser.add_argument(
    "--build-dir",
    metavar="DIR",
    type=Path,
    default=Path("build"),
    help="base build directory (default: build)",
)
parser.add_argument(
    "--binutils",
    metavar="BINARY",
    type=Path,
    help="path to binutils (optional)",
)
parser.add_argument(
    "--compilers",
    metavar="DIR",
    type=Path,
    help="path to compilers (optional)",
)
parser.add_argument(
    "--map",
    action="store_true",
    help="generate map file(s)",
)
parser.add_argument(
    "--debug",
    action="store_true",
    help="build with debug info (non-matching)",
)
if not is_windows():
    parser.add_argument(
        "--wrapper",
        metavar="BINARY",
        type=Path,
        help="path to wibo or wine (optional)",
    )
parser.add_argument(
    "--dtk",
    metavar="BINARY | DIR",
    type=Path,
    help="path to decomp-toolkit binary or source (optional)",
)
parser.add_argument(
    "--objdiff",
    metavar="BINARY | DIR",
    type=Path,
    help="path to objdiff-cli binary or source (optional)",
)
parser.add_argument(
    "--sjiswrap",
    metavar="EXE",
    type=Path,
    help="path to sjiswrap.exe (optional)",
)
parser.add_argument(
    "--ninja",
    metavar="BINARY",
    type=Path,
    help="path to ninja binary (optional)",
)
parser.add_argument(
    "--verbose",
    action="store_true",
    help="print verbose output",
)
parser.add_argument(
    "--non-matching",
    dest="non_matching",
    action="store_true",
    help="builds equivalent (but non-matching) or modded objects",
)
parser.add_argument(
    "--warn",
    dest="warn",
    type=str,
    choices=["all", "off", "error"],
    help="how to handle warnings",
)
parser.add_argument(
    "--no-progress",
    dest="progress",
    action="store_false",
    help="disable progress calculation",
)
args = parser.parse_args()

config = ProjectConfig()
config.version = str(args.version)
version_num = VERSIONS.index(config.version)

# Apply arguments
config.build_dir = args.build_dir
config.dtk_path = args.dtk
config.objdiff_path = args.objdiff
config.binutils_path = args.binutils
config.compilers_path = args.compilers
config.generate_map = args.map
config.non_matching = args.non_matching
config.sjiswrap_path = args.sjiswrap
config.ninja_path = args.ninja
config.progress = args.progress
if not is_windows():
    config.wrapper = args.wrapper
# Don't build asm unless we're --non-matching
if not config.non_matching:
    config.asm_dir = None

# Tool versions
config.binutils_tag = "2.42-2"
config.compilers_tag = "20251118"
config.dtk_tag = "v1.8.3"
config.objdiff_tag = "v3.6.1"
config.sjiswrap_tag = "v1.2.2"
config.wibo_tag = "1.0.3"

# Project
config.config_path = Path("config") / config.version / "config.yml"
config.check_sha_path = Path("config") / config.version / "build.sha1"
config.asflags = [
    "-mgekko",
    "--strip-local-absolute",
    "-I include",
    f"-I build/{config.version}/include",
    f"--defsym BUILD_VERSION={version_num}",
]
config.ldflags = [
    "-fp hardware",
    "-nodefaults",
]
if args.debug:
    config.ldflags.append("-g")  # Or -gdwarf-2 for Wii linkers
if args.map:
    config.ldflags.append("-mapunused")
    # config.ldflags.append("-listclosure") # For Wii linkers

# Use for any additional files that should cause a re-configure when modified
config.reconfig_deps = []

# Optional numeric ID for decomp.me preset
# Can be overridden in libraries or objects
config.scratch_preset_id = None

# Base flags, common to most GC/Wii games.
# Generally leave untouched, with overrides added below.
cflags_base = [
    "-nodefaults",
    "-proc gekko",
    "-align powerpc",
    "-enum int",
    "-fp hardware",
    "-Cpp_exceptions off",
    "-O4,p",
    "-inline auto",
    '-pragma "cats off"',
    '-pragma "warn_notinlined off"',
    "-maxerrors 1",
    "-nosyspath",
    "-RTTI off",
    "-fp_contract on",
    "-str reuse",
    "-i include",
    "-i include/MSL_C/include",
    "-i include/decomp",
    "-ir include/MetroTRK",
    "-ir include/revolution/BTE",
    f"-i build/{config.version}/include",
    f"-DBUILD_VERSION={version_num}",
    f"-DVERSION_{config.version}",
    "-DREVOLUTION"
]

# Debug flags
if args.debug:
    # Or -sym dwarf-2 for Wii compilers
    cflags_base.extend(["-sym dwarf-2", "-DDEBUG=1"])
else:
    cflags_base.append("-DNDEBUG=1")

# Warning flags
if args.warn == "all":
    cflags_base.append("-W all")
elif args.warn == "off":
    cflags_base.append("-W off")
elif args.warn == "error":
    cflags_base.append("-W error")

# Metrowerks library flags
cflags_runtime = [
    *cflags_base,
    "-use_lmw_stmw on",
    "-str reuse,pool,readonly",
    "-gccinc",
    "-common off",
    "-inline auto",
]

# RVL SDK flags
cflags_rvl = [
    *cflags_base,
    "-enc SJIS",
    "-fp_contract off",
    "-ipa file",
]

# Channel (game code) flags
cflags_channel = [
    "-i include/nw4r_compat",
    *cflags_base,
    "-enc SJIS",
    "-inline noauto",
    "-fp_contract off",
    "-i include/channel",
]

cflags_nw4r = [
    "-i include/nw4r_compat",
    "-enc SJIS",
    *cflags_base,
    "-ipa file",
    "-fp_contract off",
]

# MetroTRK flags
cflags_trk = [
    "-nodefaults",
    "-proc gekko",
    "-align powerpc",
    "-enum int",
    "-fp hardware",
    "-Cpp_exceptions off",
    "-O4,p",
    "-inline deferred,auto",
    '-pragma "cats off"',
    '-pragma "warn_notinlined off"',
    "-maxerrors 1",
    "-nosyspath",
    "-RTTI off",
    "-fp_contract on",
    "-str reuse,readonly",
    "-use_lmw_stmw on",
    "-sdata 0",
    "-sdata2 0",
    "-i include",
    "-i include/MSL_C/include",
    "-i include/MetroTRK",
    f"-i build/{config.version}/include",
    "-DMETRO_TRK",
    "-D__REGISTER=register",
    "-D__OSInterruptHandler=OSInterruptHandler",
    f"-DBUILD_VERSION={version_num}",
    f"-DVERSION_{config.version}",
]

# MSL_C flags
cflags_msl = [
    "-nodefaults",
    "-proc gekko",
    "-align powerpc",
    "-enum int",
    "-fp hardware",
    "-O4,p",
    "-inline auto",
    '-pragma "cats off"',
    '-pragma "warn_notinlined off"',
    "-maxerrors 1",
    "-nosyspath",
    "-RTTI off",
    "-str reuse,pool,readonly",
    "-enc SJIS",
    "-ipa file",
    "-use_lmw_stmw on",
    "-i include",
    "-i include/MSL_C/include",
    "-i include/MetroTRK",
    f"-i build/{config.version}/include",
    f"-DBUILD_VERSION={version_num}",
    f"-DVERSION_{config.version}",
]
cflags_vcmv = [
    "-i include/nw4r_compat",
    *cflags_base,
    "-Cpp_exceptions on",
    "-enc SJIS",
    "-inline auto",
    "-fp_contract off",
    "-i include/vcmv",
]

# REL flags
cflags_rel = [
    *cflags_base,
    "-sdata 0",
    "-sdata2 0",
]

config.linker_version = "GC/3.0a5.2"


# Helper function for Dolphin libraries
def DolphinLib(lib_name: str, objects: List[Object]) -> Dict[str, Any]:
    return {
        "lib": lib_name,
        "mw_version": "GC/1.2.5n",
        "cflags": cflags_base,
        "progress_category": "sdk",
        "objects": objects,
    }


# Helper function for REL script objects
def Rel(lib_name: str, objects: List[Object]) -> Dict[str, Any]:
    return {
        "lib": lib_name,
        "mw_version": "GC/1.3.2",
        "cflags": cflags_rel,
        "progress_category": "game",
        "objects": objects,
    }


Matching = True                   # Object matches and should be linked
NonMatching = False               # Object does not match and should not be linked
Equivalent = config.non_matching  # Object should be linked when configured with --non-matching


# Object is only matching for specific versions
def MatchingFor(*versions):
    return config.version in versions


config.warn_missing_config = True
config.warn_missing_source = False
config.libs = [
    {
        "lib": "Runtime.PPCEABI.H",
        "mw_version": config.linker_version,
        "cflags": cflags_runtime,
        "progress_category": "sdk",  # str | List[str]
        "objects": [
            Object(Matching, "Runtime.PPCEABI.H/global_destructor_chain.c"),
            Object(Matching, "Runtime.PPCEABI.H/__init_cpp_exceptions.cpp"),
            Object(Matching, "Runtime.PPCEABI.H/__mem.c"),
        ],
    },
    {
        "lib": "RVL_SDK",
        "mw_version": "GC/3.0a5",
        "cflags": cflags_rvl,
        "progress_category": "sdk",  # str | List[str]
        "objects": [
            # AI
            Object(Matching, "revolution/AI/ai.c"),

            # ARC
            Object(NonMatching, "revolution/ARC/arc.c"),

            # AX
            Object(NonMatching, "revolution/AX/AX.c"),
            Object(Matching, "revolution/AX/AXAlloc.c"),
            Object(Matching, "revolution/AX/AXAux.c"),
            Object(Matching, "revolution/AX/AXCL.c"),
            Object(Matching, "revolution/AX/AXComp.c"),
            Object(Matching, "revolution/AX/AXOut.c"),
            Object(Matching, "revolution/AX/AXProf.c"),
            Object(Matching, "revolution/AX/AXSPB.c"),
            Object(NonMatching, "revolution/AX/AXVPB.c"),
            Object(Matching, "revolution/AX/DSPCode.c"),

            # AXFX
            Object(Matching, "revolution/AXFX/AXFXHooks.c"),
            Object(Matching, "revolution/AXFX/AXFXReverbHi.c"),
            Object(NonMatching, "revolution/AXFX/AXFXReverbHiExp.c"),

            # BASE
            Object(Matching, "revolution/BASE/PPCArch.c"),

            # BTE/bta/dm
            Object(Matching, "revolution/BTE/bta/dm/bta_dm_act.c"),
            Object(Matching, "revolution/BTE/bta/dm/bta_dm_api.c"),
            Object(Matching, "revolution/BTE/bta/dm/bta_dm_cfg.c"),
            Object(Matching, "revolution/BTE/bta/dm/bta_dm_main.c"),
            Object(Matching, "revolution/BTE/bta/dm/bta_dm_pm.c"),

            # BTE/bta/hh
            Object(Matching, "revolution/BTE/bta/hh/bta_hh_act.c"),
            Object(Matching, "revolution/BTE/bta/hh/bta_hh_api.c"),
            Object(Matching, "revolution/BTE/bta/hh/bta_hh_cfg.c"),
            Object(Matching, "revolution/BTE/bta/hh/bta_hh_main.c"),
            Object(Matching, "revolution/BTE/bta/hh/bta_hh_utils.c"),

            # BTE/bta/sys
            Object(Matching, "revolution/BTE/bta/sys/bd.c"),
            Object(Matching, "revolution/BTE/bta/sys/bta_sys_cfg.c"),
            Object(Matching, "revolution/BTE/bta/sys/bta_sys_conn.c"),
            Object(Matching, "revolution/BTE/bta/sys/bta_sys_main.c"),
            Object(Matching, "revolution/BTE/bta/sys/ptim.c"),
            Object(Matching, "revolution/BTE/bta/sys/utl.c"),

            # BTE/btif/co
            Object(NonMatching, "revolution/BTE/btif/co/bta_dm_co.c"),
            Object(NonMatching, "revolution/BTE/btif/co/bta_hh_co.c"),

            # BTE/gki/common
            Object(Matching, "revolution/BTE/gki/common/gki_buffer.c"),
            Object(Matching, "revolution/BTE/gki/common/gki_time.c"),

            # BTE/hci/src
            Object(Matching, "revolution/BTE/hci/src/hcisu_h2.c"),

            # BTE/main
            Object(Matching, "revolution/BTE/main/bte_hcisu.c"),
            Object(Matching, "revolution/BTE/main/bte_init.c"),
            Object(Matching, "revolution/BTE/main/bte_logmsg.c"),
            Object(Matching, "revolution/BTE/main/bte_main.c"),

            # BTE/rvl
            Object(Matching, "revolution/BTE/rvl/gki_ppc.c"),
            # uusb_ppc.c only reproduces the original register allocation in
            # uusb_WriteCtrlDataCB when built with the GC/3.0a5.2 compiler.
            Object(Matching, "revolution/BTE/rvl/uusb_ppc.c", mw_version="GC/3.0a5.2"),

            # BTE/stack/btm
            Object(Matching, "revolution/BTE/stack/btm/btm_acl.c"),
            Object(Matching, "revolution/BTE/stack/btm/btm_dev.c"),
            Object(Matching, "revolution/BTE/stack/btm/btm_devctl.c"),
            Object(Matching, "revolution/BTE/stack/btm/btm_inq.c"),
            Object(Matching, "revolution/BTE/stack/btm/btm_main.c"),
            Object(Matching, "revolution/BTE/stack/btm/btm_pm.c"),
            Object(Matching, "revolution/BTE/stack/btm/btm_sco.c"),
            Object(Matching, "revolution/BTE/stack/btm/btm_sec.c"),

            # BTE/stack/btu
            Object(Matching, "revolution/BTE/stack/btu/btu_hcif.c"),
            Object(Matching, "revolution/BTE/stack/btu/btu_init.c"),
            Object(Matching, "revolution/BTE/stack/btu/btu_task.c"),

            # BTE/wbt
            Object(Matching, "revolution/BTE/wbt_ext.c"),

            # BTE/stack/gap
            Object(Matching, "revolution/BTE/stack/gap/gap_api.c"),
            Object(Matching, "revolution/BTE/stack/gap/gap_conn.c"),
            Object(Matching, "revolution/BTE/stack/gap/gap_utils.c"),

            # BTE/stack/hcic
            Object(Matching, "revolution/BTE/stack/hcic/hcicmds.c", mw_version="GC/3.0a5.2"),

            # BTE/stack/hid
            Object(Matching, "revolution/BTE/stack/hid/hidd_api.c"),
            Object(Matching, "revolution/BTE/stack/hid/hidd_conn.c"),
            Object(Matching, "revolution/BTE/stack/hid/hidd_mgmt.c"),
            Object(Matching, "revolution/BTE/stack/hid/hidd_pm.c"),
            Object(Matching, "revolution/BTE/stack/hid/hidh_api.c"),
            Object(Matching, "revolution/BTE/stack/hid/hidh_conn.c"),

            # BTE/stack/l2cap
            Object(Matching, "revolution/BTE/stack/l2cap/l2c_api.c"),
            Object(Matching, "revolution/BTE/stack/l2cap/l2c_csm.c"),
            Object(Matching, "revolution/BTE/stack/l2cap/l2c_link.c"),
            Object(Matching, "revolution/BTE/stack/l2cap/l2c_main.c"),
            Object(Matching, "revolution/BTE/stack/l2cap/l2c_utils.c"),

            # BTE/stack/rfcomm
            Object(Matching, "revolution/BTE/stack/rfcomm/port_api.c"),
            Object(Matching, "revolution/BTE/stack/rfcomm/port_rfc.c"),
            Object(Matching, "revolution/BTE/stack/rfcomm/port_utils.c"),
            Object(Matching, "revolution/BTE/stack/rfcomm/rfc_l2cap_if.c"),
            Object(Matching, "revolution/BTE/stack/rfcomm/rfc_mx_fsm.c"),
            Object(Matching, "revolution/BTE/stack/rfcomm/rfc_port_fsm.c"),
            Object(Matching, "revolution/BTE/stack/rfcomm/rfc_port_if.c"),
            Object(Matching, "revolution/BTE/stack/rfcomm/rfc_ts_frames.c"),
            Object(Matching, "revolution/BTE/stack/rfcomm/rfc_utils.c"),

            # BTE/stack/sdp
            Object(Matching, "revolution/BTE/stack/sdp/sdp_api.c"),
            Object(Matching, "revolution/BTE/stack/sdp/sdp_db.c"),
            Object(Matching, "revolution/BTE/stack/sdp/sdp_discovery.c", extra_cflags=[
                "-Iinclude/revolution/BTE/gki/platform",
                "-Iinclude/revolution/BTE/gki/common",
                "-Iinclude/revolution/BTE/include",
                "-Iinclude/revolution/BTE/stack/include",
                "-Iinclude/revolution/BTE/stack/sdp",
            ]),
            Object(Matching, "revolution/BTE/stack/sdp/sdp_main.c"),
            Object(Matching, "revolution/BTE/stack/sdp/sdp_server.c"),
            Object(Matching, "revolution/BTE/stack/sdp/sdp_utils.c"),

            # CNT
            Object(Matching, "revolution/CNT/cnt.c"),

            # CX
            Object(NonMatching, "revolution/CX/CXUncomp.c"),

            # DB
            Object(Matching, "revolution/DB/db.c"),

            # DSP
            Object(Matching, "revolution/DSP/dsp.c"),
            Object(Matching, "revolution/DSP/dsp_debug.c"),
            Object(Matching, "revolution/DSP/dsp_task.c"),

            # DVD
            Object(Matching, "revolution/DVD/dvd.c"),
            Object(Matching, "revolution/DVD/dvd_broadway.c"),
            Object(Matching, "revolution/DVD/dvderror.c"),
            Object(Matching, "revolution/DVD/dvdFatal.c"),
            Object(Matching, "revolution/DVD/dvdfs.c"),
            Object(Matching, "revolution/DVD/dvdidutils.c"),
            Object(Matching, "revolution/DVD/dvdqueue.c"),

            # ESP
            Object(NonMatching, "revolution/ESP/esp.c"),

            # EUART
            Object(Matching, "revolution/EUART/euart.c"),

            # EXI
            Object(NonMatching, "revolution/EXI/EXIBios.c"),
            Object(Matching, "revolution/EXI/EXICommon.c"),
            Object(Matching, "revolution/EXI/EXIUart.c"),

            # FS
            Object(NonMatching, "revolution/FS/fs.c"),

            # GX
            Object(Matching, "revolution/GX/GXAttr.c"),
            Object(Matching, "revolution/GX/GXBump.c"),
            Object(Matching, "revolution/GX/GXDisplayList.c"),
            Object(NonMatching, "revolution/GX/GXDraw.c"),
            Object(NonMatching, "revolution/GX/GXFifo.c"),
            Object(NonMatching, "revolution/GX/GXFrameBuf.c"),
            Object(Matching, "revolution/GX/GXGeometry.c"),
            Object(NonMatching, "revolution/GX/GXInit.c"),
            Object(Matching, "revolution/GX/GXLight.c"),
            Object(NonMatching, "revolution/GX/GXMisc.c"),
            Object(NonMatching, "revolution/GX/GXPerf.c"),
            Object(NonMatching, "revolution/GX/GXPixel.c"),
            Object(NonMatching, "revolution/GX/GXTev.c"),
            Object(NonMatching, "revolution/GX/GXTexture.c"),
            Object(Matching, "revolution/GX/GXTransform.c"),

            # HBM
            Object(NonMatching, "revolution/HBM/HBMBase.cpp"),
            Object(NonMatching, "revolution/HBM/HBMGui.cpp"),
            Object(NonMatching, "revolution/HBM/HBMController.cpp"),

            # IPC
            Object(NonMatching, "revolution/IPC/ipcclt.c"),
            Object(Matching, "revolution/IPC/ipcMain.c"),
            Object(Matching, "revolution/IPC/ipcProfile.c"),
            Object(Matching, "revolution/IPC/memory.c"),

            # MEM
            Object(Matching, "revolution/MEM/mem_allocator.c"),
            Object(NonMatching, "revolution/MEM/mem_expHeap.c"),
            Object(Matching, "revolution/MEM/mem_frameHeap.c"),
            Object(NonMatching, "revolution/MEM/mem_heapCommon.c"),
            Object(Matching, "revolution/MEM/mem_list.c"),
            Object(NonMatching, "revolution/MEM/mem_unitHeap.c"),

            # MTX
            Object(NonMatching, "revolution/MTX/mtx.c"),
            Object(Matching, "revolution/MTX/mtx44.c"),
            Object(Matching, "revolution/MTX/mtxvec.c"),
            Object(Matching, "revolution/MTX/vec.c"),

            # NAND
            Object(NonMatching, "revolution/NAND/nand.c"),
            Object(NonMatching, "revolution/NAND/NANDCore.c"),
            Object(NonMatching, "revolution/NAND/NANDOpenClose.c"),

            # NdevExi2AD
            Object(Matching, "revolution/NdevExi2AD/DebuggerDriver.c"),
            Object(Matching, "revolution/NdevExi2AD/exi2.c"),

            # NET
            Object(NonMatching, "revolution/NET/nettime.c"),
            Object(NonMatching, "revolution/NET/NETVersion.c"),

            # NWC24
            Object(NonMatching, "revolution/NWC24/NWC24Config.c"),
            Object(NonMatching, "revolution/NWC24/NWC24DateParser.c"),
            Object(NonMatching, "revolution/NWC24/NWC24Download.c"),
            Object(Matching, "revolution/NWC24/NWC24FileApi.c"),
            Object(NonMatching, "revolution/NWC24/NWC24FriendList.c"),
            Object(Matching, "revolution/NWC24/NWC24Ipc.c"),
            Object(Matching, "revolution/NWC24/NWC24Manage.c"),
            Object(NonMatching, "revolution/NWC24/NWC24MBoxCtrl.c"),
            Object(NonMatching, "revolution/NWC24/NWC24Mime.c"),
            Object(NonMatching, "revolution/NWC24/NWC24Schedule.c"),
            Object(NonMatching, "revolution/NWC24/NWC24SecretFList.c"),
            Object(NonMatching, "revolution/NWC24/NWC24StdApi.c"),
            Object(NonMatching, "revolution/NWC24/NWC24System.c"),
            Object(NonMatching, "revolution/NWC24/NWC24Time.c"),
            Object(NonMatching, "revolution/NWC24/NWC24Utils.c"),

            # OS
            Object(Matching, "revolution/OS/__ppc_eabi_init.c"),
            Object(Matching, "revolution/OS/__start.c"),
            Object(Matching, "revolution/OS/OS.c"),
            Object(Matching, "revolution/OS/OSAlarm.c"),
            Object(Matching, "revolution/OS/OSAlloc.c"),
            Object(Matching, "revolution/OS/OSArena.c"),
            Object(Matching, "revolution/OS/OSAudioSystem.c"),
            Object(Matching, "revolution/OS/OSCache.c"),
            Object(Matching, "revolution/OS/OSContext.c"),
            Object(Matching, "revolution/OS/OSError.c"),
            Object(NonMatching, "revolution/OS/OSExec.c"),
            Object(Matching, "revolution/OS/OSFatal.c"),
            Object(NonMatching, "revolution/OS/OSFont.c"),
            Object(Matching, "revolution/OS/OSInterrupt.c"),
            Object(Matching, "revolution/OS/OSIpc.c"),
            Object(NonMatching, "revolution/OS/OSLink.c"),
            Object(Matching, "revolution/OS/OSMemory.c"),
            Object(NonMatching, "revolution/OS/OSMessage.c"),
            Object(Matching, "revolution/OS/OSMutex.c"),
            Object(Matching, "revolution/OS/OSNandboot.c"),
            Object(Matching, "revolution/OS/OSNet.c"),
            Object(Matching, "revolution/OS/OSPlayRecord.c"),
            Object(Matching, "revolution/OS/OSPlayTime.c"),
            Object(NonMatching, "revolution/OS/OSReset.c"),
            Object(Matching, "revolution/OS/OSRtc.c"),
            Object(Matching, "revolution/OS/OSStateFlags.c"),
            Object(Matching, "revolution/OS/OSStateTM.c"),
            Object(Matching, "revolution/OS/OSSync.c"),
            Object(Matching, "revolution/OS/OSThread.c"),
            Object(Matching, "revolution/OS/OSTime.c"),
            Object(Matching, "revolution/OS/OSUtf.c"),

            # PAD
            Object(Matching, "revolution/PAD/Pad.c"),

            # SC
            Object(Matching, "revolution/SC/scapi.c"),
            Object(Matching, "revolution/SC/scapi_prdinfo.c"),
            Object(Matching, "revolution/SC/scsystem.c"),

            # SI
            Object(NonMatching, "revolution/SI/SIBios.c"),
            Object(NonMatching, "revolution/SI/SISamplingRate.c"),

            # TPL
            Object(Matching, "revolution/TPL/TPL.c"),

            # USB
            Object(Matching, "revolution/USB/usb.c"),

            # VF
            Object(Matching, "revolution/VF/pf_clib.c"),
            Object(Matching, "revolution/VF/pf_code.c"),
            Object(Matching, "revolution/VF/pf_service.c"),
            Object(Matching, "revolution/VF/pf_str.c"),
            Object(Matching, "revolution/VF/pf_w_clib.c"),
            Object(Matching, "revolution/VF/pf_driver.c"),
            Object(Matching, "revolution/VF/pdm_bpb.c"),
            Object(Matching, "revolution/VF/pdm_disk.c"),
            Object(Matching, "revolution/VF/pdm_partition.c"),
            Object(Matching, "revolution/VF/pdm_mbr.c"),
            Object(Matching, "revolution/VF/pdm_dskmng.c"),
            Object(Matching, "revolution/VF/pf_cache.c"),
            Object(Matching, "revolution/VF/pf_cluster.c"),
            Object(Matching, "revolution/VF/pf_dir.c"),
            Object(Matching, "revolution/VF/pf_dir_api.c"),
            Object(Matching, "revolution/VF/pf_entry.c"),
            Object(Matching, "revolution/VF/pf_entry_iterator.c"),
            Object(Matching, "revolution/VF/pf_fat.c"),
            Object(Matching, "revolution/VF/pf_fat12.c"),
            Object(Matching, "revolution/VF/pf_fat16.c"),
            Object(Matching, "revolution/VF/pf_fat32.c"),
            Object(Matching, "revolution/VF/pf_fatfs.c"),
            Object(Matching, "revolution/VF/pf_file.c"),
            Object(Matching, "revolution/VF/pf_path.c"),
            Object(Matching, "revolution/VF/pf_sector.c"),
            Object(Matching, "revolution/VF/pf_volume.c"),
            Object(Matching, "revolution/VF/pf_cp932.c"),
            Object(Matching, "revolution/VF/pf_api_util.c"),
            Object(Matching, "revolution/VF/pf_attach.c"),
            Object(Matching, "revolution/VF/pf_detach.c"),
            Object(Matching, "revolution/VF/pf_errnum.c"),
            Object(Matching, "revolution/VF/pf_fclose.c"),
            Object(Matching, "revolution/VF/pf_finfo.c"),
            Object(Matching, "revolution/VF/pf_fopen.c"),
            Object(Matching, "revolution/VF/pf_format.c"),
            Object(Matching, "revolution/VF/pf_fread.c"),
            Object(Matching, "revolution/VF/pf_fseek.c"),
            Object(Matching, "revolution/VF/pf_fsfirst.c"),
            Object(Matching, "revolution/VF/pf_fsnext.c"),
            Object(Matching, "revolution/VF/pf_fwrite.c"),
            Object(Matching, "revolution/VF/pf_getdev.c"),
            Object(Matching, "revolution/VF/pf_init_prfile2.c"),
            Object(Matching, "revolution/VF/pf_mkdir.c"),
            Object(Matching, "revolution/VF/pf_remove.c"),
            Object(Matching, "revolution/VF/pf_unmount.c"),
            Object(Matching, "revolution/VF/pf_filelock.c"),
            Object(Matching, "revolution/VF/pf_system.c"),
            Object(Matching, "revolution/VF/d_vf.c"),
            Object(Matching, "revolution/VF/d_vf_sys.c"),
            Object(Matching, "revolution/VF/d_hash.c"),
            Object(Matching, "revolution/VF/d_time.c"),
            Object(Matching, "revolution/VF/d_common.c"),
            Object(Matching, "revolution/VF/nand_drv.c"),
            Object(Matching, "revolution/VF/sd_drv.c"),

            # VI
            Object(NonMatching, "revolution/VI/i2c.c"),
            Object(NonMatching, "revolution/VI/vi.c"),
            Object(NonMatching, "revolution/VI/vi3in1.c"),

            # WENC
            Object(NonMatching, "revolution/WENC/wenc.c"),

            # WPAD
            Object(Matching, "revolution/WPAD/WPAD.c", mw_version="GC/3.0a5.2"),
            Object(Matching, "revolution/WPAD/WPADHIDParser.c", mw_version="GC/3.0a5.2"),
            Object(Matching, "revolution/WPAD/WPADEncrypt.c", mw_version="GC/3.0a5.2"),
            Object(Matching, "revolution/WPAD/debug_msg.c"),
            Object(Matching, "revolution/WPAD/WPADMem.c"),

            # KPAD
            Object(Matching, "revolution/KPAD/KPAD.c", mw_version="GC/3.0a5.2"),

            # WUD
            Object(Matching, "revolution/WUD/debug_msg.c"),
            # WUD.c only reproduces the retail register allocation with 3.0a5.2
            Object(Matching, "revolution/WUD/WUD.c", mw_version="GC/3.0a5.2"),
            Object(Matching, "revolution/WUD/WUDHidHost.c"),
        ],
    },
    {
        "lib": "MSL_C",
        "mw_version": "GC/3.0a3",
        "cflags": cflags_msl,
        "progress_category": "mslc",
        "objects": [
            Object(Matching, "MSL_C/GCN_Mem_Alloc.c"),
            Object(Matching, "MSL_C/alloc.c"),
            Object(Matching, "MSL_C/ansi_files.c"),
            Object(Matching, "MSL_C/ansi_fp.c"),
            Object(Matching, "MSL_C/arith.c"),
            Object(Matching, "MSL_C/bsearch.c"),
            Object(Matching, "MSL_C/buffer_io.c"),
            Object(Matching, "MSL_C/ctype.c"),
            Object(Matching, "MSL_C/direct_io.c"),
            Object(Matching, "MSL_C/errno.c"),
            Object(Matching, "MSL_C/file_io.c"),
            Object(Matching, "MSL_C/FILE_POS.c"),
            Object(Matching, "MSL_C/float.c"),
            Object(Matching, "MSL_C/locale.c"),
            Object(Matching, "MSL_C/mbstring.c"),
            Object(Matching, "MSL_C/mem.c"),
            Object(Matching, "MSL_C/mem_funcs.c"),
            Object(Matching, "MSL_C/math_api.c"),
            Object(Matching, "MSL_C/misc_io.c"),
            Object(Matching, "MSL_C/printf.c", mw_version="GC/3.0a5.2"),
            Object(Matching, "MSL_C/qsort.c"),
            Object(Matching, "MSL_C/rand.c"),
            Object(Matching, "MSL_C/scanf.c"),
            Object(Matching, "MSL_C/signal.c"),
            Object(Matching, "MSL_C/string.c"),
            Object(Matching, "MSL_C/strtold.c"),
            Object(Matching, "MSL_C/strtoul.c"),
            Object(Matching, "MSL_C/wctype.c"),
            Object(Matching, "MSL_C/wmem.c"),
            Object(Matching, "MSL_C/wprintf.c", mw_version="GC/3.0a5.2"),
            Object(Matching, "MSL_C/wstring.c"),
            Object(Matching, "MSL_C/wchar_io.c"),
            Object(Matching, "MSL_C/getenv.c"),
            Object(Matching, "MSL_C/time_zone.c"),
            Object(Matching, "MSL_C/uart_console_io_gcn.c"),
            Object(Matching, "MSL_C/abort_exit_ppc_eabi.c"),
            Object(Matching, "MSL_C/math_sun.c"),
            Object(Matching, "MSL_C/extras.c"),
        ]
    },
    {
        "lib": "MSL_C",
        "mw_version": "GC/3.0a3",
        "cflags": [*cflags_msl, "-Cpp_exceptions off"],
        "objects": [
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/e_acos.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/e_asin.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/e_atan2.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/e_exp.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/e_fmod.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/e_log.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/e_pow.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/e_rem_pio2.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/k_cos.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/k_rem_pio2.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/k_sin.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/k_tan.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/s_atan.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/s_ceil.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/s_copysign.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/s_cos.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/s_floor.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/s_frexp.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/s_ldexp.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/s_sin.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/s_tan.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/w_acos.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/w_asin.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/w_atan2.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/w_exp.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/w_fmod.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/w_log.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/w_pow.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/e_sqrt.c"),
            Object(Matching, "MSL_C/MSL_Common_Embedded/Math/Double_precision/w_sqrt.c"),
            Object(Matching, "MSL_C/PPC_EABI/SRC/math_ppc.c"),
        ],
    },
    {
        "lib": "MetroTRK",
        "mw_version": "GC/2.7",
        "cflags": cflags_trk,
        "progress_category": "metrotrk",
        "objects": [
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Portable/mainloop.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Portable/nubevent.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Portable/nubinit.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Portable/msg.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Portable/msgbuf.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Portable/serpoll.c", extra_cflags=["-sdata 8"]),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Os/dolphin/usr_put.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Portable/dispatch.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Portable/msghndlr.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Portable/support.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Portable/mutex_TRK.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Portable/notify.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Processor/ppc/Generic/flush_cache.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Portable/mem_TRK.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Portable/string_TRK.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Processor/ppc/Generic/targimpl.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Processor/ppc/Export/targsupp.s"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Processor/ppc/Generic/mpc_7xx_603e.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Export/mslsupp.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Processor/ppc/Generic/exception.s"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Os/dolphin/dolphin_trk.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Portable/main_TRK.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Os/dolphin/dolphin_trk_glue.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Os/dolphin/targcont.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Os/dolphin/target_options.c"),
            Object(Matching, "MetroTRK/debugger/embedded/MetroTRK/Os/dolphin/UDP_Stubs.c"),
            Object(Matching, "MetroTRK/gamedev/cust_connection/cc/exi2/GCN/EXI2_GDEV_GCN/main.c", extra_cflags=["-sdata 8"]),
            Object(Matching, "MetroTRK/gamedev/cust_connection/utils/common/CircleBuffer.c"),
            Object(Matching, "MetroTRK/gamedev/cust_connection/utils/gc/MWCriticalSection_gc.c"),
        ],
    },
    {
        "lib": "NW4R",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_nw4r,
        "progress_category": "nw4r",
        "objects": [
            # ef
            Object(Matching, "nw4r/ef/ef_draworder.cpp"),
            Object(Matching, "nw4r/ef/ef_effect.cpp"),
            Object(Matching, "nw4r/ef/ef_effectsystem.cpp"),
            Object(Matching, "nw4r/ef/ef_emitter.cpp"),
            Object(NonMatching, "nw4r/ef/ef_animcurve.cpp"),
            Object(Matching, "nw4r/ef/ef_particle.cpp"),
            Object(Matching, "nw4r/ef/ef_particlemanager.cpp"),
            Object(Matching, "nw4r/ef/ef_resource.cpp"),
            Object(Matching, "nw4r/ef/ef_util.cpp"),
            Object(Matching, "nw4r/ef/ef_memorymanager.cpp"),
            Object(Matching, "nw4r/ef/ef_emitterform.cpp"),
            Object(Matching, "nw4r/ef/ef_creationqueue.cpp"),
            Object(Matching, "nw4r/ef/emform/ef_emform.cpp"),
            Object(Matching, "nw4r/ef/emform/ef_point.cpp"),
            Object(Matching, "nw4r/ef/emform/ef_line.cpp"),
            Object(Matching, "nw4r/ef/emform/ef_disc.cpp"),
            Object(Matching, "nw4r/ef/emform/ef_sphere.cpp"),
            Object(Matching, "nw4r/ef/emform/ef_cylinder.cpp"),
            Object(Matching, "nw4r/ef/emform/ef_torus.cpp"),
            Object(Matching, "nw4r/ef/emform/ef_cube.cpp"),
            Object(Matching, "nw4r/ef/drawstrategy/ef_drawstrategybuilder.cpp"),
            Object(Matching, "nw4r/ef/drawstrategy/ef_drawstrategyimpl.cpp"),
            Object(Matching, "nw4r/ef/drawstrategy/ef_drawbillboardstrategy.cpp"),
            Object(Matching, "nw4r/ef/drawstrategy/ef_drawdirectionalstrategy.cpp"),
            Object(Matching, "nw4r/ef/drawstrategy/ef_drawfreestrategy.cpp"),
            Object(Matching, "nw4r/ef/drawstrategy/ef_drawlinestrategy.cpp"),
            Object(Matching, "nw4r/ef/drawstrategy/ef_drawpointstrategy.cpp"),
            Object(Matching, "nw4r/ef/drawstrategy/ef_drawstripestrategy.cpp"),
            # g3d
            Object(Matching, "nw4r/g3d/res/g3d_rescommon.cpp"),
            Object(Matching, "nw4r/g3d/res/g3d_resdict.cpp"),
            Object(Matching, "nw4r/g3d/res/g3d_resfile.cpp"),
            Object(Matching, "nw4r/g3d/res/g3d_resmdl.cpp"),
            Object(Matching, "nw4r/g3d/res/g3d_resshp.cpp"),
            Object(Matching, "nw4r/g3d/res/g3d_restev.cpp"),
            Object(Matching, "nw4r/g3d/res/g3d_resmat.cpp"),
            Object(Matching, "nw4r/g3d/res/g3d_resvtx.cpp"),
            Object(Matching, "nw4r/g3d/res/g3d_restex.cpp"),
            Object(Matching, "nw4r/g3d/res/g3d_resnode.cpp"),
            Object(Matching, "nw4r/g3d/res/g3d_resanmtexpat.cpp"),
            Object(Matching, "nw4r/g3d/g3d_anmvis.cpp"),
            Object(Matching, "nw4r/g3d/g3d_anmclr.cpp"),
            Object(Matching, "nw4r/g3d/g3d_anmtexpat.cpp"),
            Object(Matching, "nw4r/g3d/g3d_anmtexsrt.cpp"),
            Object(Matching, "nw4r/g3d/g3d_anmchr.cpp"),
            Object(Matching, "nw4r/g3d/g3d_anmscn.cpp"),
            Object(Matching, "nw4r/g3d/g3d_obj.cpp"),
            Object(Matching, "nw4r/g3d/g3d_anmobj.cpp"),
            Object(Matching, "nw4r/g3d/platform/g3d_gpu.cpp"),
            Object(Matching, "nw4r/g3d/platform/g3d_cpu.cpp"),
            Object(Matching, "nw4r/g3d/g3d_state.cpp"),
            Object(Matching, "nw4r/g3d/g3d_draw1mat1shp.cpp"),
            Object(Matching, "nw4r/g3d/g3d_calcview.cpp"),
            Object(Matching, "nw4r/g3d/g3d_dcc.cpp"),
            Object(Matching, "nw4r/g3d/g3d_workmem.cpp"),
            Object(Matching, "nw4r/g3d/g3d_calcworld.cpp"),
            Object(Matching, "nw4r/g3d/g3d_draw.cpp"),
            Object(Matching, "nw4r/g3d/g3d_camera.cpp"),
            Object(Matching, "nw4r/g3d/g3d_basic.cpp"),
            Object(Matching, "nw4r/g3d/g3d_maya.cpp"),
            Object(Matching, "nw4r/g3d/g3d_xsi.cpp"),
            Object(Matching, "nw4r/g3d/g3d_3dsmax.cpp"),
            Object(Matching, "nw4r/g3d/g3d_scnobj.cpp"),
            Object(Matching, "nw4r/g3d/g3d_scnroot.cpp"),
            Object(Matching, "nw4r/g3d/g3d_scnmdlsmpl.cpp"),
            Object(Matching, "nw4r/g3d/g3d_calcmaterial.cpp"),
            Object(Matching, "nw4r/g3d/g3d_init.cpp"),
            Object(Matching, "nw4r/g3d/g3d_fog.cpp"),
            Object(Matching, "nw4r/g3d/g3d_light.cpp"),
            # snd
            Object(Matching, "nw4r/snd/snd_AxManager.cpp"),
            Object(Matching, "nw4r/snd/snd_AxVoice.cpp"),
            Object(Matching, "nw4r/snd/snd_AxVoiceManager.cpp"),
            Object(Matching, "nw4r/snd/snd_AxfxImpl.cpp"),
            Object(Matching, "nw4r/snd/snd_Bank.cpp"),
            Object(Matching, "nw4r/snd/snd_BankFile.cpp"),
            Object(Matching, "nw4r/snd/snd_BasicPlayer.cpp"),
            Object(Matching, "nw4r/snd/snd_BasicSound.cpp"),
            Object(Matching, "nw4r/snd/snd_Channel.cpp"),
            Object(Matching, "nw4r/snd/snd_DisposeCallbackManager.cpp"),
            Object(Matching, "nw4r/snd/snd_DvdSoundArchive.cpp"),
            Object(Matching, "nw4r/snd/snd_EnvGenerator.cpp"),
            Object(Matching, "nw4r/snd/snd_ExternalSoundPlayer.cpp"),
            Object(Matching, "nw4r/snd/snd_FrameHeap.cpp"),
            Object(Matching, "nw4r/snd/snd_FxReverbHi.cpp"),
            Object(Matching, "nw4r/snd/snd_InstancePool.cpp"),
            Object(Matching, "nw4r/snd/snd_Lfo.cpp"),
            Object(Matching, "nw4r/snd/snd_MemorySoundArchive.cpp"),
            Object(Matching, "nw4r/snd/snd_MidiSeqPlayer.cpp"),
            Object(Matching, "nw4r/snd/snd_MmlParser.cpp"),
            Object(Matching, "nw4r/snd/snd_MmlSeqTrack.cpp"),
            Object(Matching, "nw4r/snd/snd_MmlSeqTrackAllocator.cpp"),
            Object(Matching, "nw4r/snd/snd_NandSoundArchive.cpp"),
            Object(Matching, "nw4r/snd/snd_RemoteSpeaker.cpp"),
            Object(Matching, "nw4r/snd/snd_RemoteSpeakerManager.cpp"),
            Object(Matching, "nw4r/snd/snd_SeqFile.cpp"),
            Object(Matching, "nw4r/snd/snd_SeqPlayer.cpp"),
            Object(Matching, "nw4r/snd/snd_SeqSound.cpp"),
            Object(Matching, "nw4r/snd/snd_SeqSoundHandle.cpp"),
            Object(Matching, "nw4r/snd/snd_SeqTrack.cpp"),
            Object(Matching, "nw4r/snd/snd_SoundArchive.cpp"),
            Object(Matching, "nw4r/snd/snd_SoundArchiveFile.cpp"),
            Object(Matching, "nw4r/snd/snd_SoundArchiveLoader.cpp"),
            Object(Matching, "nw4r/snd/snd_SoundArchivePlayer.cpp"),
            Object(Matching, "nw4r/snd/snd_SoundHandle.cpp"),
            Object(Matching, "nw4r/snd/snd_SoundHeap.cpp"),
            Object(Matching, "nw4r/snd/snd_SoundPlayer.cpp"),
            Object(Matching, "nw4r/snd/snd_SoundStartable.cpp"),
            Object(Matching, "nw4r/snd/snd_SoundSystem.cpp"),
            Object(Matching, "nw4r/snd/snd_SoundThread.cpp"),
            Object(Matching, "nw4r/snd/snd_StrmChannel.cpp"),
            Object(Matching, "nw4r/snd/snd_StrmFile.cpp"),
            Object(Matching, "nw4r/snd/snd_StrmPlayer.cpp"),
            Object(Matching, "nw4r/snd/snd_StrmSound.cpp"),
            Object(Matching, "nw4r/snd/snd_StrmSoundHandle.cpp"),
            Object(Matching, "nw4r/snd/snd_TaskManager.cpp"),
            Object(Matching, "nw4r/snd/snd_TaskThread.cpp"),
            Object(Matching, "nw4r/snd/snd_Voice.cpp"),
            Object(Matching, "nw4r/snd/snd_VoiceManager.cpp"),
            Object(Matching, "nw4r/snd/snd_Util.cpp"),
            Object(Matching, "nw4r/snd/snd_WaveFile.cpp"),
            Object(Matching, "nw4r/snd/snd_WavePlayer.cpp"),
            Object(Matching, "nw4r/snd/snd_WaveSound.cpp"),
            Object(Matching, "nw4r/snd/snd_WaveSoundHandle.cpp"),
            Object(Matching, "nw4r/snd/snd_WsdFile.cpp"),
            Object(Matching, "nw4r/snd/snd_WsdPlayer.cpp"),
            Object(Matching, "nw4r/snd/snd_WsdTrack.cpp"),
            # ut
            Object(Matching, "nw4r/ut/ut_list.cpp"),
            Object(Matching, "nw4r/ut/ut_LinkList.cpp"),
            Object(Matching, "nw4r/ut/ut_binaryFileFormat.cpp"),
            Object(Matching, "nw4r/ut/ut_CharStrmReader.cpp"),
            Object(Matching, "nw4r/ut/ut_TagProcessorBase.cpp"),
            Object(Matching, "nw4r/ut/ut_IOStream.cpp"),
            Object(Matching, "nw4r/ut/ut_FileStream.cpp"),
            Object(Matching, "nw4r/ut/ut_DvdFileStream.cpp"),
            Object(Matching, "nw4r/ut/ut_DvdLockedFileStream.cpp"),
            Object(Matching, "nw4r/ut/ut_LockedCache.cpp"),
            Object(Matching, "nw4r/ut/ut_Font.cpp"),
            Object(Matching, "nw4r/ut/ut_ResFontBase.cpp"),
            Object(Matching, "nw4r/ut/ut_ResFont.cpp"),
            Object(Matching, "nw4r/ut/ut_ArchiveFontBase.cpp"),
            Object(Matching, "nw4r/ut/ut_ArchiveFont.cpp"),
            Object(Matching, "nw4r/ut/ut_CharWriter.cpp"),
            Object(Matching, "nw4r/ut/ut_TextWriterBase.cpp"),
            # math
            Object(Matching, "nw4r/math/math_arithmetic.cpp"),
            Object(Matching, "nw4r/math/math_triangular.cpp"),
            Object(Matching, "nw4r/math/math_types.cpp"),
            Object(Matching, "nw4r/math/math_vec4.cpp"),
            # lyt
            Object(Matching, "nw4r/lyt/lyt_init.cpp"),
            Object(Matching, "nw4r/lyt/lyt_pane.cpp"),
            Object(Matching, "nw4r/lyt/lyt_group.cpp"),
            Object(Matching, "nw4r/lyt/lyt_layout.cpp"),
            Object(Matching, "nw4r/lyt/lyt_picture.cpp"),
            Object(Matching, "nw4r/lyt/lyt_textBox.cpp"),
            Object(Matching, "nw4r/lyt/lyt_window.cpp"),
            Object(Matching, "nw4r/lyt/lyt_bounding.cpp"),
            Object(Matching, "nw4r/lyt/lyt_material.cpp"),
            Object(Matching, "nw4r/lyt/lyt_drawInfo.cpp"),
            Object(Matching, "nw4r/lyt/lyt_animation.cpp"),
            Object(Matching, "nw4r/lyt/lyt_resourceAccessor.cpp"),
            Object(Matching, "nw4r/lyt/lyt_arcResourceAccessor.cpp"),
            Object(Matching, "nw4r/lyt/lyt_common.cpp"),
        ],
    },
    {
            "lib": "Channel",
            "mw_version": "GC/3.0a5.2",
            "cflags": cflags_channel,
            "progress_category": "channel",
            "objects": [
                Object(Matching, "CityLabel.cpp", extra_cflags=["-ipa file"]),
                Object(Matching, "Connect.cpp"),
                Object(Matching, "ErrorWindow.cpp"),
                Object(Matching, "ForecastCheck.cpp"),
                Object(Matching, "Weather.cpp"),
                Object(Matching, "WeatherOther.cpp", extra_cflags=["-ipa file"]),
                Object(Matching, "WeatherToday.cpp"),
                Object(Matching, "WeatherTomorrow.cpp"),
                Object(Matching, "WeatherWeek.cpp"),
                Object(Matching, "WeatherAddress.cpp", extra_cflags=["-ipa file"]),
                Object(Matching, "WeatherAround.cpp", extra_cflags=["-ipa file"]),
                Object(Matching, "WeatherBase.cpp"),
                Object(Matching, "WeatherBaseDay.cpp"),
                Object(Matching, "ForecastData.cpp", extra_cflags=["-ipa file"]),
                Object(Matching, "WeatherNormal.cpp", extra_cflags=["-ipa file"]),
                Object(Matching, "WeatherText.cpp"),
                Object(Matching, "WeatherTextUS.cpp"),
                Object(Matching, "WeatherScene.cpp", extra_cflags=["-ipa file"]),
                Object(Matching, "ForecastNow.cpp"),
                Object(Matching, "WeatherSetting.cpp"),
                Object(Matching, "ForecastSummary.cpp"),
                Object(Matching, "Region.cpp"),
                Object(Matching, "Scene.cpp"),
                Object(Matching, "SimpleModel.cpp"),
                Object(Matching, "HomeButton.cpp"),
                Object(Matching, "main.cpp"),
                Object(Matching, "DrawUtil.cpp"),
                Object(Matching, "GlobeDots.cpp"),
                Object(Matching, "WiiConnect24.cpp"),
                Object(Matching, "WeatherUtil.cpp"),
                Object(Matching, "ChannelMathData.cpp"),
                Object(Matching, "DragScroll.cpp"),
                Object(Matching, "MtxUtil.cpp"),
                Object(Matching, "System.cpp"),
                Object(Matching, "Tween.cpp"),
                Object(NonMatching, "LayoutButton.cpp"),
                Object(Matching, "ButtonGroup.cpp"),
                Object(Matching, "PointerHistory.cpp"),
                Object(Matching, "Fade.cpp"),
                Object(Matching, "WorkerThread.cpp"),
                Object(Matching, "LoopSound.cpp"),
                Object(Matching, "LayoutObj.cpp", extra_cflags=["-ipa file"]),
                Object(Matching, "GlobeView.cpp"),
                Object(Matching, "SceneBase.cpp", extra_cflags=["-ipa file"]),
                Object(Matching, "SimpleGlobe.cpp", extra_cflags=["-ipa file"]),
                Object(NonMatching, "GlobeDotsTable.cpp"),
            ],
    },
    {
        "lib": "vcmv",
        "mw_version": "GC/3.0a5.2",
        "cflags": cflags_vcmv,
        "progress_category": "vcmv",
        "objects": [
            Object(Matching, "vcmv/vcmv_cursor.cpp", extra_cflags=["-str reuse,readonly", "-use_lmw_stmw on", "-fp_contract on"]),
            Object(NonMatching, "vcmv/vcmv_draw.cpp", extra_cflags=["-str reuse,readonly", "-use_lmw_stmw on", "-fp_contract on"]),
            Object(Matching, "vcmv/vcmv_javascript.cpp", extra_cflags=["-str reuse,readonly"]),
            Object(NonMatching, "vcmv/vcmv_main.cpp", extra_cflags=["-str reuse,readonly", "-use_lmw_stmw on", "-fp_contract on"]),
            Object(Matching, "vcmv/vcmv_www.cpp", extra_cflags=["-str reuse,pool,readonly", "-fp_contract on"]),
            Object(Matching, "vcmv/vcmv_rso.cpp"),
        ],
    }
]


# Optional callback to adjust link order. This can be used to add, remove, or reorder objects.
# This is called once per module, with the module ID and the current link order.
#
# For example, this adds "dummy.c" to the end of the DOL link order if configured with --non-matching.
# "dummy.c" *must* be configured as a Matching (or Equivalent) object in order to be linked.
def link_order_callback(module_id: int, objects: List[str]) -> List[str]:
    # Don't modify the link order for matching builds
    if not config.non_matching:
        return objects
    if module_id == 0:  # DOL
        return objects + ["dummy.c"]
    return objects


# Uncomment to enable the link order callback.
# config.link_order_callback = link_order_callback


# Optional extra categories for progress tracking
# Adjust as desired for your project
config.progress_categories = [
    ProgressCategory("channel", "Channel Code"),
    ProgressCategory("sdk", "RVL_SDK"),
    ProgressCategory("metrotrk", "MetroTRK"),
    ProgressCategory("mslc", "MSL_C"),
    ProgressCategory("nw4r", "NW4R"),
    ProgressCategory("vcmv", "VCMV")
]
config.progress_each_module = args.verbose
# Optional extra arguments to `objdiff-cli report generate`
config.progress_report_args = [
    # Marks relocations as mismatching if the target value is different
    # Default is "functionRelocDiffs=none", which is most lenient
    # "--config functionRelocDiffs=data_value",
]

if args.mode == "configure":
    # Write build.ninja and objdiff.json
    generate_build(config)
elif args.mode == "progress":
    # Print progress information
    calculate_progress(config)
else:
    sys.exit("Unknown mode: " + args.mode)
