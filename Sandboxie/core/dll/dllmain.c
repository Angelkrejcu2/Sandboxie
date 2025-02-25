/*
 * Copyright 2004-2020 Sandboxie Holdings, LLC 
 * Copyright 2020-2022 David Xanatos, xanasoft.com
 *
 * This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

//---------------------------------------------------------------------------
// Sandboxie DLL
//---------------------------------------------------------------------------


#include "dll.h"
#include "obj.h"
#include "trace.h"
#include "debug.h"
#include "dump.h"
#include "core/low/lowdata.h"
#include "common/my_version.h"


//---------------------------------------------------------------------------
// Functions
//---------------------------------------------------------------------------

static void Dll_InitGeneric(HINSTANCE hInstance);

static void Dll_InitInjected(void);

static void Dll_SelectImageType(void);

void Ldr_Inject_Init(BOOLEAN bHostInject);

//---------------------------------------------------------------------------
// Variables
//---------------------------------------------------------------------------


const ULONG tzuk = 'xobs';

SBIELOW_DATA* SbieApi_data = NULL;

HINSTANCE Dll_Instance = NULL;
HMODULE Dll_Ntdll = NULL;
HMODULE Dll_Kernel32 = NULL;
HMODULE Dll_KernelBase = NULL;
HMODULE Dll_DigitalGuardian = NULL;

const WCHAR *Dll_BoxName = NULL;
const WCHAR *Dll_ImageName = NULL;
const WCHAR *Dll_SidString = NULL;

const WCHAR *Dll_HomeNtPath = NULL;
ULONG Dll_HomeNtPathLen = 0;
const WCHAR *Dll_HomeDosPath = NULL;
//ULONG Dll_HomeDosPathLen = 0;

const WCHAR *Dll_BoxFilePath = NULL;
const WCHAR *Dll_BoxKeyPath = NULL;
const WCHAR *Dll_BoxIpcPath = NULL;

ULONG Dll_BoxFilePathLen = 0;
ULONG Dll_BoxKeyPathLen = 0;
ULONG Dll_BoxIpcPathLen = 0;
ULONG Dll_SidStringLen = 0;

ULONG Dll_ProcessId = 0;
ULONG Dll_SessionId = 0;

ULONG64 Dll_ProcessFlags = 0;

BOOLEAN Dll_IsWow64 = FALSE;
BOOLEAN Dll_IsSystemSid = FALSE;
BOOLEAN Dll_InitComplete = FALSE;
BOOLEAN Dll_RestrictedToken = FALSE;
BOOLEAN Dll_ChromeSandbox = FALSE;
BOOLEAN Dll_FirstProcessInBox = FALSE;
BOOLEAN Dll_CompartmentMode = FALSE;
//BOOLEAN Dll_AlernateIpcNaming = FALSE;

ULONG Dll_ImageType = DLL_IMAGE_UNSPECIFIED;

ULONG Dll_OsBuild = 0;  // initialized by Key module
ULONG Dll_Windows = 0;

const UCHAR *SbieDll_Version = MY_VERSION_COMPAT;

BOOLEAN Dll_SbieTrace = FALSE;

//extern ULONG64 __security_cookie = 0;


//---------------------------------------------------------------------------


static WCHAR *Dll_BoxNameSpace;
static WCHAR *Dll_ImageNameSpace;
static WCHAR *Dll_SidStringSpace;


//---------------------------------------------------------------------------


const WCHAR *DllName_advapi32   = L"advapi32.dll";
const WCHAR *DllName_combase    = L"combase.dll";
const WCHAR *DllName_kernel32   = L"kernel32.dll";
const WCHAR *DllName_kernelbase = L"kernelbase.dll";
const WCHAR *DllName_ole32      = L"ole32.dll";
const WCHAR *DllName_oleaut32   = L"oleaut32.dll";
const WCHAR *DllName_user32     = L"user32.dll";
const WCHAR *DllName_rpcrt4     = L"rpcrt4.dll";
const WCHAR *DllName_winnsi     = L"winnsi.dll";
const WCHAR *DllName_shell32    = L"shell32.dll";
const WCHAR *DllName_sechost    = L"sechost.dll";
const WCHAR *DllName_gdi32      = L"gdi32.dll";
const WCHAR *DllName_secur32    = L"secur32.dll";
const WCHAR *DllName_sspicli    = L"sspicli.dll";
const WCHAR *DllName_mscoree    = L"mscoree.dll";
const WCHAR *DllName_ntmarta    = L"ntmarta.dll";


//---------------------------------------------------------------------------
// DllMain
//---------------------------------------------------------------------------


_FX BOOL WINAPI DllMain(
    HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
    if (dwReason == DLL_THREAD_ATTACH) {

        if (Dll_BoxName) {
            Dll_FixWow64Syscall();
            Gui_ConnectToWindowStationAndDesktop(NULL);
        }

    } else if (dwReason == DLL_THREAD_DETACH) {

        Dll_FreeTlsData();

    } else if (dwReason == DLL_PROCESS_ATTACH) {
#ifdef _WIN64
        Dll_DigitalGuardian = GetModuleHandleA("DgApi64.dll");
#else
        Dll_DigitalGuardian = GetModuleHandleA("DgApi.dll");
#endif
        if (GetProcAddress(GetModuleHandleA("ntdll.dll"), "LdrFastFailInLoaderCallout")) {
            Dll_Windows = 10;
        }
        else {
            Dll_Windows = 8;
        }
        Dll_InitGeneric(hInstance);
        SbieDll_HookInit();

    } else if (dwReason == DLL_PROCESS_DETACH) {

        if (Dll_InitComplete && Dll_BoxName) {

            File_DoAutoRecover(TRUE);
            Gui_ResetClipCursor();
        }

    }

    return TRUE;
}


//---------------------------------------------------------------------------
// Dll_InitGeneric
//---------------------------------------------------------------------------


_FX void Dll_InitGeneric(HINSTANCE hInstance)
{
    //
    // Dll_InitGeneric initializes SbieDll in a general way, suitable
    // for a program which may or may not be in the sandbox
    //

    Dll_Instance = hInstance;

    Dll_Ntdll = GetModuleHandle(L"ntdll.dll");
    Dll_Kernel32 = GetModuleHandle(DllName_kernel32);
    Dll_KernelBase = GetModuleHandle(DllName_kernelbase);

	extern void InitMyNtDll(HMODULE Ntdll);
	InitMyNtDll(Dll_Ntdll);

	extern FARPROC __sys_GetModuleInformation;
	__sys_GetModuleInformation = GetProcAddress(LoadLibraryW(L"psapi.dll"), "GetModuleInformation");

    if (! Dll_InitMem()) {
        SbieApi_Log(2305, NULL);
        ExitProcess(-1);
    }
}


//---------------------------------------------------------------------------
// Dll_InitInjected
//---------------------------------------------------------------------------


_FX void Dll_InitInjected(void)
{
	//
	// Dll_InitInjected is executed by Dll_Ordinal1 in the context
	// of a program that is running in the sandbox
	//

	LONG status;
	BOOLEAN ok;
	ULONG BoxFilePathLen;
	ULONG BoxKeyPathLen;
	ULONG BoxIpcPathLen;

    Dll_SbieTrace = SbieApi_QueryConfBool(NULL, L"SbieTrace", FALSE);

	if (SbieApi_QueryConfBool(NULL, L"DebugTrace", FALSE)) {

		Trace_Init();

		OutputDebugString(L"SbieDll injected...");
	}

    //
    // confirm the process is sandboxed before going further
    //

    Dll_BoxNameSpace        = Dll_Alloc( 64 * sizeof(WCHAR));
    memzero(Dll_BoxNameSpace,            64 * sizeof(WCHAR));

    Dll_ImageNameSpace      = Dll_Alloc(256 * sizeof(WCHAR));
    memzero(Dll_ImageNameSpace,         256 * sizeof(WCHAR));

    Dll_SidStringSpace      = Dll_Alloc( 96 * sizeof(WCHAR));
    memzero(Dll_SidStringSpace,          96 * sizeof(WCHAR));

    Dll_ProcessId = (ULONG)(ULONG_PTR)GetCurrentProcessId();

    status = SbieApi_QueryProcessEx2( // sets proc->sbiedll_loaded = TRUE; in the driver
        (HANDLE)(ULONG_PTR)Dll_ProcessId, 255,
        Dll_BoxNameSpace, Dll_ImageNameSpace, Dll_SidStringSpace,
        &Dll_SessionId, NULL);

    if (status != 0) {
        SbieApi_Log(2304, Dll_ImageName);
        ExitProcess(-1);
    }

    Dll_BoxName = (const WCHAR *)Dll_BoxNameSpace;
    Dll_ImageName = (const WCHAR *)Dll_ImageNameSpace;
    Dll_SidString = (const WCHAR *)Dll_SidStringSpace;

    Dll_SidStringLen = wcslen(Dll_SidString);


    //
    // break for the debugger, as soon as we have Dll_ImageName
    //

    if (SbieDll_CheckStringInList(Dll_ImageName, NULL, L"WaitForDebugger")) {
    //if (SbieDll_GetSettingsForName_bool(NULL, Dll_ImageName, L"WaitForDebugger", FALSE)) {
    //if (SbieApi_QueryConfBool(NULL, L"WaitForDebuggerAll", FALSE)) {
        while (!IsDebuggerPresent()) {
            OutputDebugString(L"Waiting for Debugger\n");
            Sleep(500);
        } __debugbreak();
    }


    //
    // query Sandboxie home folder
    //

    Dll_HomeNtPath = Dll_AllocTemp(1024 * sizeof(WCHAR));
    Dll_HomeDosPath = Dll_AllocTemp(1024 * sizeof(WCHAR));

    SbieApi_GetHomePath((WCHAR*)Dll_HomeNtPath, 1020, (WCHAR*)Dll_HomeDosPath, 1020);

    Dll_HomeNtPathLen = wcslen(Dll_HomeNtPath);
    //Dll_HomeDosPathLen = wcslen(Dll_HomeDosPath);

    //
    // get process type and flags
    //

    Dll_ProcessFlags = SbieApi_QueryProcessInfo(0, 0);

    Dll_CompartmentMode = (Dll_ProcessFlags & SBIE_FLAG_APP_COMPARTMENT) != 0;

    Dll_SelectImageType();

    //
    // query the box paths
    //

    BoxFilePathLen = 0;
    BoxKeyPathLen = 0;
    BoxIpcPathLen = 0;

    status = SbieApi_QueryBoxPath(
        NULL, NULL, NULL, NULL,
        &BoxFilePathLen, &BoxKeyPathLen, &BoxIpcPathLen);
    if (status != 0) {
        SbieApi_Log(2304, Dll_ImageName);
        ExitProcess(-1);
    }

    Dll_BoxFilePath = Dll_Alloc(BoxFilePathLen);
    Dll_BoxKeyPath = Dll_Alloc(BoxKeyPathLen);
    Dll_BoxIpcPath = Dll_Alloc(BoxIpcPathLen);

    status = SbieApi_QueryBoxPath(
        NULL,
        (WCHAR *)Dll_BoxFilePath,
        (WCHAR *)Dll_BoxKeyPath,
        (WCHAR *)Dll_BoxIpcPath,
        &BoxFilePathLen, &BoxKeyPathLen, &BoxIpcPathLen);
    if (status != 0) {
        SbieApi_Log(2304, Dll_ImageName);
        ExitProcess(-1);
    }

    Dll_BoxFilePathLen = wcslen(Dll_BoxFilePath);
    Dll_BoxKeyPathLen = wcslen(Dll_BoxKeyPath);
    Dll_BoxIpcPathLen = wcslen(Dll_BoxIpcPath);

  //  Dll_AlernateIpcNaming = SbieApi_QueryConfBool(NULL, L"UseAlernateIpcNaming", FALSE);
  //  if (Dll_AlernateIpcNaming) {
  //
  //      //
  //      // instead of using a separate namespace
  //		// just replace all \ with _ and use it as a sufix rather then an actual path
  //      // similarly a its done for named pipes already
  //      // this approche can help to reduce teh footprint when running in portable mode
  //      // alternatively we could create volatile entries under AppContainerNamedObjects 
  //      //
  //
  //      WCHAR* ptr = (WCHAR*)Dll_BoxIpcPath;
  //      while (*ptr) {
  //          WCHAR *ptr2 = wcschr(ptr, L'\\');
  //          if (ptr2) {
  //              ptr = ptr2;
  //              *ptr = L'_';
  //          } else
  //              ptr += wcslen(ptr);
  //      }
  //  }

    //
    // check if process SID is LocalSystem
    //

    Dll_IsSystemSid = Secure_IsLocalSystemToken(FALSE);

    //
    // create a security descriptor granting access to everyone
    //

    Secure_InitSecurityDescriptors();

    //
    // initialize sandboxed process, first the basic NTDLL hooks
    //

    ok = Dll_InitPathList();

    if (ok)
        Dll_FixWow64Syscall();

    if (ok)
        ok = Handle_Init();

    if (ok)
        ok = Obj_Init();

    if (ok) {

        //
        // check if we are the first process in the sandbox
        // (for AutoExec function in custom module)
        //

        ULONG pid_count = 0;
        if (NT_SUCCESS(SbieApi_EnumProcessEx(NULL,FALSE,-1,NULL,&pid_count)) && pid_count == 1)
            Dll_FirstProcessInBox = TRUE;

        WCHAR str[32];
        if (NT_SUCCESS(SbieApi_QueryConfAsIs(NULL, L"ProcessLimit", 0, str, sizeof(str) - sizeof(WCHAR)))) {
            ULONG num = _wtoi(str);
            if (num > 0) {
                if (num < pid_count)
                    ExitProcess(-1);
                if ((num * 8 / 10) < pid_count)
                    Sleep(3000);
            }
        }
    }

    if (ok) {

        //
        // ipc must be initialized before anythign else to make delete v2 work
        //

        ok = Ipc_Init();
    }

    if (ok) {

        //
        // Key should be initialized first, to prevent key requests
        // with MAXIMUM_ALLOWED access from failing
        //

        ok = Key_Init();

        //
        // on Windows 8.1, we may get some crashes errors while Chrome
        // is shutting down, so just quit any WerFault.exe process
        //

        //if (ok && Dll_OsBuild >= 9600 &&
        //        _wcsicmp(Dll_ImageName, L"WerFault.exe") == 0) {

        //    ExitProcess(0);
        //}
    }

    if (ok)
        ok = File_Init();

    if (ok)
        ok = Secure_Init();

    if (ok)
        ok = SysInfo_Init();

    if (ok)
        ok = Sxs_InitKernel32();

    if (ok)
        ok = Proc_Init();

    if (ok)
        ok = Gui_InitConsole1();

    if (ok) // Note: Ldr_Init may cause rpcss to be started early
        ok = Ldr_Init();            // last to initialize

    //
    // finish
    //

#ifdef WITH_DEBUG
    if (ok) 
        ok = Debug_Init();
#endif WITH_DEBUG

    if (! ok) {
        SbieApi_Log(2304, Dll_ImageName);
        ExitProcess(-1);
    }

    Dll_InitComplete = TRUE;

    if (! Dll_RestrictedToken)
        CustomizeSandbox();
}


//---------------------------------------------------------------------------
// Dll_InitExeEntry
//---------------------------------------------------------------------------


_FX void Dll_InitExeEntry(void)
{
    //
    // Dll_InitInjected is executed by Ldr_Inject_Entry after NTDLL has
    // finished initializing the process (loading static import DLLs, etc)
    //

    //
    // on Windows 8, we can't load advapi32.dll during Scm_SecHostDll
    //
    //

    Scm_SecHostDll_W8();

    //
    // hook DefWindowProc on Windows 7, after USER32 has been initialized
    //

    Gui_InitWindows7();

    //
    // hook the console window, if applicable
    //

    Gui_InitConsole2();

    //
    // if we are SplWow64, register our pid with SbieSvc GUI Proxy
    //

    Gdi_SplWow64(TRUE);

    //
    // check if running as a forced COM server process
    // note:  it does not return if this is the case
    //

    Custom_ComServer();

    //
    // force load of UxTheme in a Google Chrome sandbox process
    //

    // Note: this does not seem to be needed anymore for modern Chrome builds, also it breaks Vivaldi browser

    //Custom_Load_UxTheme(); 

    UserEnv_InitVer(Dll_OsBuild >= 7600 ? Dll_KernelBase : Dll_Kernel32); // in KernelBase since Win 7

    //
    // Windows 8.1:  hook UserEnv-related entrypoint in KernelBase
    //

    if (Dll_OsBuild >= 9600)
        UserEnv_Init(Dll_KernelBase);

    //
    // start SandboxieRpcSs
    //

    SbieDll_StartCOM(TRUE);

    //
    // setup own top level exception handler
    //

    if(Config_GetSettingsForImageName_bool(L"EnableMiniDump", FALSE))
        Dump_Init();

    //
    // once we return here the process images entrypoint will be called
    //
}


//---------------------------------------------------------------------------
// Dll_GetImageType
//---------------------------------------------------------------------------


_FX ULONG Dll_GetImageType(const WCHAR *ImageName)
{
    ULONG ImageType = DLL_IMAGE_UNSPECIFIED;

    //
    // check for custom configured special images
    //

    ULONG index;
    NTSTATUS status;
    WCHAR wbuf[96];
    WCHAR* buf = wbuf;

    for (index = 0; ; ++index) {
        status = SbieApi_QueryConfAsIs(
            NULL, L"SpecialImage", index, buf, 90 * sizeof(WCHAR));
        if (!NT_SUCCESS(status))
            break;

        WCHAR* ptr = wcschr(buf, L',');
        if (!ptr) continue;

        *ptr++ = L'\0';

        if (_wcsicmp(ImageName, ptr) == 0) {

            if (_wcsicmp(L"chrome", buf) == 0)
                ImageType = DLL_IMAGE_GOOGLE_CHROME;
            else if (_wcsicmp(L"firefox", buf) == 0)
                ImageType = DLL_IMAGE_MOZILLA_FIREFOX;
            else if (_wcsicmp(L"thunderbird", buf) == 0)
                ImageType = DLL_IMAGE_MOZILLA_THUNDERBIRD;
            else if (_wcsicmp(L"browser", buf) == 0)
                ImageType = DLL_IMAGE_OTHER_WEB_BROWSER;
            else if (_wcsicmp(L"mail", buf) == 0)
                ImageType = DLL_IMAGE_OTHER_MAIL_CLIENT;
            else
                ImageType = DLL_IMAGE_LAST; // invalid type set place holder such that we keep this image uncustomized

            break;
        }
    }

    //
    // keep image names in sync with enum at top of dll.h
    //

    static const WCHAR *_ImageNames[] = {

        SANDBOXIE L"RpcSs.exe",     (WCHAR *)DLL_IMAGE_SANDBOXIE_RPCSS,
        SANDBOXIE L"DcomLaunch.exe",(WCHAR *)DLL_IMAGE_SANDBOXIE_DCOMLAUNCH,
        SANDBOXIE L"Crypto.exe",    (WCHAR *)DLL_IMAGE_SANDBOXIE_CRYPTO,
        SANDBOXIE L"WUAU.exe",      (WCHAR *)DLL_IMAGE_SANDBOXIE_WUAU,
        SANDBOXIE L"BITS.exe",      (WCHAR *)DLL_IMAGE_SANDBOXIE_BITS,
        SBIESVC_EXE,                (WCHAR *)DLL_IMAGE_SANDBOXIE_SBIESVC,

        L"msiexec.exe",             (WCHAR *)DLL_IMAGE_MSI_INSTALLER,
        L"TrustedInstaller.exe",    (WCHAR *)DLL_IMAGE_TRUSTED_INSTALLER,
        L"TiWorker.exe",            (WCHAR *)DLL_IMAGE_TRUSTED_INSTALLER,
        L"wuauclt.exe",             (WCHAR *)DLL_IMAGE_WUAUCLT,
        L"explorer.exe",            (WCHAR *)DLL_IMAGE_SHELL_EXPLORER,
        L"rundll32.exe",            (WCHAR *)DLL_IMAGE_RUNDLL32,
        L"dllhost.exe",             (WCHAR *)DLL_IMAGE_DLLHOST,
        L"ServiceModelReg.exe",     (WCHAR *)DLL_IMAGE_SERVICE_MODEL_REG,

        L"iexplore.exe",            (WCHAR *)DLL_IMAGE_INTERNET_EXPLORER,

        L"wmplayer.exe",            (WCHAR *)DLL_IMAGE_WINDOWS_MEDIA_PLAYER,
        L"winamp.exe",              (WCHAR *)DLL_IMAGE_NULLSOFT_WINAMP,
        L"kmplayer.exe",            (WCHAR *)DLL_IMAGE_PANDORA_KMPLAYER,
        L"wlmail.exe",              (WCHAR *)DLL_IMAGE_WINDOWS_LIVE_MAIL,
        L"wisptis.exe",             (WCHAR *)DLL_IMAGE_WISPTIS,

        L"GoogleUpdate.exe",        (WCHAR *)DLL_IMAGE_GOOGLE_UPDATE,

        L"AcroRd32.exe",            (WCHAR *)DLL_IMAGE_ACROBAT_READER,
        L"Acrobat.exe",             (WCHAR *)DLL_IMAGE_ACROBAT_READER,
        L"plugin-container.exe",    (WCHAR *)DLL_IMAGE_PLUGIN_CONTAINER,
        L"Outlook.exe",             (WCHAR *)DLL_IMAGE_OFFICE_OUTLOOK,
        L"Excel.exe",               (WCHAR *)DLL_IMAGE_OFFICE_EXCEL,

        NULL,                       NULL
    };

    if (ImageType == DLL_IMAGE_UNSPECIFIED) {

        for (int i = 0; _ImageNames[i]; i += 2) {
            if (_wcsicmp(ImageName, _ImageNames[i]) == 0) {
                ImageType = (ULONG)(ULONG_PTR)_ImageNames[i + 1];
                break;
            }
        }
    }

    return ImageType;
}

//---------------------------------------------------------------------------
// Dll_SelectImageType
//---------------------------------------------------------------------------


_FX void Dll_SelectImageType(void)
{
    Dll_ImageType = Dll_GetImageType(Dll_ImageName);

    if (Dll_ImageType == DLL_IMAGE_UNSPECIFIED &&
            _wcsnicmp(Dll_ImageName, L"FlashPlayerPlugin_", 18) == 0)
        Dll_ImageType = DLL_IMAGE_FLASH_PLAYER_SANDBOX;

    if (Dll_ImageType == DLL_IMAGE_DLLHOST) {

        const WCHAR *CmdLine = GetCommandLine();
        if (CmdLine) {
            if (wcsstr(CmdLine, L"{3EB3C877-1F16-487C-9050-104DBCD66683}"))
                Dll_ImageType = DLL_IMAGE_DLLHOST_WININET_CACHE;
        }
    }

    //
    // issue a warning for some known programs
    //

    if (Dll_ImageType == DLL_IMAGE_UNSPECIFIED && (
            _wcsicmp(Dll_ImageName, L"SchTasks.exe") == 0
        ||  _wcsicmp(Dll_ImageName, L"cvh.exe") == 0    // Office 2010 virt
        ||  0)) {

        SbieApi_Log(2205, Dll_ImageName);
    }

    if (Dll_ImageType == DLL_IMAGE_LAST)
        Dll_ImageType = DLL_IMAGE_UNSPECIFIED;

    SbieApi_QueryProcessInfoEx(0, 'spit', Dll_ImageType);

    //
    // we have some special cases for programs running under a restricted
    // token, such as a Chromium sandbox processes, or Microsoft Office 2010
    // programs running as embedded previewers within Outlook
    //

    Dll_RestrictedToken = Secure_IsRestrictedToken(FALSE);

    if (Dll_RestrictedToken) {

        if (Dll_ImageType == DLL_IMAGE_GOOGLE_CHROME ||
            Dll_ImageType == DLL_IMAGE_ACROBAT_READER ||
            Dll_ImageType == DLL_IMAGE_FLASH_PLAYER_SANDBOX) {

            Dll_ChromeSandbox = TRUE;
        }
    }

    Dll_SkipHook(NULL);
}


//---------------------------------------------------------------------------
// Dll_Ordinal1
//---------------------------------------------------------------------------


_FX ULONG_PTR Dll_Ordinal1(
    ULONG_PTR arg1, ULONG_PTR arg2, ULONG_PTR arg3,
    ULONG_PTR arg4, ULONG_PTR arg5)
{
    struct _INJECT_DATA {           // keep in sync with core/low/inject.c

        ULONG64 sbielow_data;               // syscall_data_len & extra_data_offset;
        ULONG64 RtlFindActCtx_SavedArg1;    // LdrLoadDll

        ULONG64 LdrGetProcAddr;
        ULONG64 NtRaiseHardError;
        ULONG64 RtlFindActCtx;
        ULONG   RtlFindActCtx_Protect;
        
        UCHAR   Reserved[188];              // the rest of _INJECT_DATA

    } *inject; // total size 232

    typedef ULONG_PTR (*P_RtlFindActivationContextSectionString)(
                    ULONG_PTR arg1, ULONG_PTR arg2, ULONG_PTR arg3,
                    ULONG_PTR arg4, ULONG_PTR arg5);
    P_RtlFindActivationContextSectionString RtlFindActCtx;

    SBIELOW_DATA *data;
    ULONG dummy_prot;
    BOOLEAN bHostInject = FALSE;

    extern HANDLE SbieApi_DeviceHandle;

    //
    // this code is invoked from our RtlFindActivationContextSectionString
    // hook in core/low/entry.asm, with a parameter that points to the
    // syscall/inject data area.  the first ULONG64 in this data area
    // includes a pointer to the SbieLow data area
    //

    inject = (struct _INJECT_DATA *)arg1;

    data = (SBIELOW_DATA *)inject->sbielow_data;

    SbieApi_data = data;

    VirtualProtect((void *)(ULONG_PTR)data, sizeof(SBIELOW_DATA),
                   PAGE_EXECUTE_READ, &dummy_prot);

    bHostInject = data->flags.bHostInject == 1;

    //
    // the SbieLow data area includes values that are useful to us
    //

    Dll_IsWow64 = data->flags.is_wow64 == 1;

    SbieApi_DeviceHandle = (HANDLE)data->api_device_handle;

    //
    // our RtlFindActivationContextSectionString hook already restored
    // the original bytes, but we should still restore the page protection
    //

    VirtualProtect((void *)(ULONG_PTR)inject->RtlFindActCtx, 5,
                   inject->RtlFindActCtx_Protect, &dummy_prot);

    arg1 = (ULONG_PTR)inject->RtlFindActCtx_SavedArg1;

    RtlFindActCtx = (P_RtlFindActivationContextSectionString)
                                                    inject->RtlFindActCtx;

    //
    // free the syscall/inject data area which is no longer needed
    //

    VirtualFree(inject, 0, MEM_RELEASE);

    if (!bHostInject)
    {
        //
        // SbieDll was already partially initialized in Dll_InitGeneric,
        // complete the initialization for a sandboxed process
        //
        HANDLE heventProcessStart = 0;

        Dll_InitInjected(); // install required hooks

        //
        // notify RPCSS that a new process was created in the current sandbox
        //

        if (Dll_ImageType != DLL_IMAGE_SANDBOXIE_RPCSS) {
            heventProcessStart = CreateEvent(0, FALSE, FALSE, SESSION_PROCESS);
            if (heventProcessStart) {
                SetEvent(heventProcessStart);
                CloseHandle(heventProcessStart);
            }
        }

        //
        // workaround for Program Compatibility Assistant (PCA), we have
        // to start a second instance of this process outside the PCA job,
        // see also Proc_RestartProcessOutOfPcaJob
        //

        int MustRestartProcess = 0;
        if (Dll_ProcessFlags & SBIE_FLAG_PROCESS_IN_PCA_JOB) {
            if (!SbieApi_QueryConfBool(NULL, L"NoRestartOnPAC", FALSE))
                MustRestartProcess = 1;
        }

        else if (Dll_ProcessFlags & SBIE_FLAG_FORCED_PROCESS) {
            if (SbieApi_QueryConfBool(NULL, L"ForceRestartAll", FALSE)
             || SbieDll_CheckStringInList(Dll_ImageName, NULL, L"ForceRestart"))
                MustRestartProcess = 2;
        }

        if (MustRestartProcess) {

            WCHAR text[128];
            Sbie_snwprintf(text, 128, L"Cleanly restarting forced process, reason %d", MustRestartProcess);
            SbieApi_MonitorPutMsg(MONITOR_OTHER, text);

            extern void Proc_RestartProcessOutOfPcaJob(void);
            Proc_RestartProcessOutOfPcaJob();
            // does not return
        }

        //
        // explorer needs sandboxed COM show warnign and terminate when COM is not sandboxies
        //

        if (Dll_ImageType == DLL_IMAGE_SHELL_EXPLORER && SbieDll_IsOpenCOM()) {

            SbieApi_Log(2195, NULL);
            ExitProcess(0);
        }
    }
    else
    {
        Ldr_Inject_Init(bHostInject);
    }
	
    //
    // conclude the detour by passing control back to the original
    // RtlFindActivationContextSectionString.  the detour code used
    // jump rather than call to invoke this function (see entry.asm)
    // so RtlFindActivationContextSectionString returns to its caller
    //

    return RtlFindActCtx(arg1, arg2, arg3, arg4, arg5);
}
