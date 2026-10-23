/*
 * Blaze Ultimate Optimizer - low-overhead native Windows desktop utility.
 * Copyright (c) 2026. No third-party runtime. Works under a standard user.
 * Safe-by-default: no security-service shutdown, forced process termination,
 * pagefile removal, driver changes, or indiscriminate registry cleaning.
 */
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#define WIN32_LEAN_AND_MEAN 1
#define MAX_PATH 260
#define DLL __declspec(dllimport)
#define WINAPI __stdcall
#define NULL ((void*)0)
#define TRUE 1
#define FALSE 0
#define INVALID_HANDLE_VALUE ((HANDLE)(-1))

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int UINT;
typedef unsigned long DWORD;
typedef long LONG;
typedef unsigned long long ULONGLONG, ULONG_PTR, SIZE_T, DWORDLONG, WPARAM;
typedef long long LPARAM, LRESULT;
typedef int BOOL;
typedef void *HANDLE, *HWND, *HINSTANCE, *HMENU, *HICON, *HCURSOR, *HBRUSH, *HGDIOBJ, *HKEY;
typedef unsigned short WCHAR;
typedef const WCHAR *LPCWSTR;
typedef WCHAR *LPWSTR;
typedef const char *LPCSTR;
typedef void *LPVOID;
typedef const void *LPCVOID;
typedef LRESULT (WINAPI *WNDPROC)(HWND,UINT,WPARAM,LPARAM);
typedef struct { LONG x,y; } POINT;
typedef struct { DWORD dwLowDateTime,dwHighDateTime; } FILETIME;
typedef struct { DWORD dwLength,dwMemoryLoad; DWORDLONG ullTotalPhys,ullAvailPhys,ullTotalPageFile,ullAvailPageFile,ullTotalVirtual,ullAvailVirtual,ullAvailExtendedVirtual; } MEMORYSTATUSEX;
typedef struct { DWORD cbSize,style;WNDPROC lpfnWndProc;int cbClsExtra,cbWndExtra;HINSTANCE hInstance;HICON hIcon;HCURSOR hCursor;HBRUSH hbrBackground;LPCWSTR lpszMenuName,lpszClassName;HICON hIconSm; } WNDCLASSEXW;
typedef struct { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; POINT pt; DWORD lPrivate; } MSG;
typedef struct { WORD wYear,wMonth,wDayOfWeek,wDay,wHour,wMinute,wSecond,wMilliseconds; } SYSTEMTIME;
typedef struct { DWORD dwSize,cntUsage,th32ProcessID; ULONG_PTR th32DefaultHeapID; DWORD th32ModuleID,cntThreads,th32ParentProcessID;LONG pcPriClassBase;DWORD dwFlags;WCHAR szExeFile[MAX_PATH]; } PROCESSENTRY32W;
typedef struct { DWORD cb,PageFaultCount; SIZE_T PeakWorkingSetSize,WorkingSetSize,QuotaPeakPagedPoolUsage,QuotaPagedPoolUsage,QuotaPeakNonPagedPoolUsage,QuotaNonPagedPoolUsage,PagefileUsage,PeakPagefileUsage,PrivateUsage; } PROCESS_MEMORY_COUNTERS_EX;
typedef struct { DWORD dwFileAttributes;FILETIME ftCreationTime,ftLastAccessTime,ftLastWriteTime;DWORD nFileSizeHigh,nFileSizeLow,dwReserved0,dwReserved1;WCHAR cFileName[MAX_PATH],cAlternateFileName[14]; } WIN32_FIND_DATAW;
typedef struct { DWORD Length,Reserved,RecordNumber,TimeGenerated,TimeWritten,EventID;WORD EventType,NumStrings,EventCategory,ReservedFlags;DWORD ClosingRecordNumber,StringOffset,UserSidLength,UserSidOffset,DataLength,DataOffset; } EVENTLOGRECORD;

DLL HINSTANCE WINAPI GetModuleHandleW(LPCWSTR);
DLL DWORD WINAPI GetLastError(void);
DLL void WINAPI ExitProcess(UINT);
DLL void WINAPI Sleep(DWORD);
DLL ULONGLONG WINAPI GetTickCount64(void);
DLL BOOL WINAPI GlobalMemoryStatusEx(MEMORYSTATUSEX*);
DLL BOOL WINAPI GetDiskFreeSpaceExW(LPCWSTR,ULONGLONG*,ULONGLONG*,ULONGLONG*);
DLL DWORD WINAPI GetEnvironmentVariableW(LPCWSTR,LPWSTR,DWORD);
DLL DWORD WINAPI GetTempPathW(DWORD,LPWSTR);
DLL void WINAPI GetLocalTime(SYSTEMTIME*);
DLL void WINAPI GetSystemTimeAsFileTime(FILETIME*);
DLL BOOL WINAPI GetSystemTimes(FILETIME*,FILETIME*,FILETIME*);
DLL BOOL WINAPI GetProcessTimes(HANDLE,FILETIME*,FILETIME*,FILETIME*,FILETIME*);
DLL HANDLE WINAPI CreateToolhelp32Snapshot(DWORD,DWORD);
DLL BOOL WINAPI Process32FirstW(HANDLE,PROCESSENTRY32W*);
DLL BOOL WINAPI Process32NextW(HANDLE,PROCESSENTRY32W*);
DLL HANDLE WINAPI OpenProcess(DWORD,BOOL,DWORD);
DLL BOOL WINAPI K32GetProcessMemoryInfo(HANDLE,void*,DWORD);
DLL BOOL WINAPI CloseHandle(HANDLE);
DLL HANDLE WINAPI FindFirstFileW(LPCWSTR,WIN32_FIND_DATAW*);
DLL BOOL WINAPI FindNextFileW(HANDLE,WIN32_FIND_DATAW*);
DLL BOOL WINAPI FindClose(HANDLE);
DLL BOOL WINAPI DeleteFileW(LPCWSTR);
DLL DWORD WINAPI GetFileAttributesW(LPCWSTR);
DLL HANDLE WINAPI CreateFileW(LPCWSTR,DWORD,DWORD,void*,DWORD,DWORD,HANDLE);
DLL BOOL WINAPI WriteFile(HANDLE,LPCVOID,DWORD,DWORD*,void*);
DLL HANDLE WINAPI CreateThread(void*,SIZE_T,DWORD (WINAPI*)(void*),void*,DWORD,DWORD*);
DLL DWORD WINAPI GetCurrentThreadId(void);
DLL LONG WINAPI RegOpenKeyExW(HKEY,LPCWSTR,DWORD,DWORD,HKEY*);
DLL LONG WINAPI RegCreateKeyExW(HKEY,LPCWSTR,DWORD,LPWSTR,DWORD,DWORD,void*,HKEY*,DWORD*);
DLL LONG WINAPI RegQueryValueExW(HKEY,LPCWSTR,DWORD*,DWORD*,BYTE*,DWORD*);
DLL LONG WINAPI RegSetValueExW(HKEY,LPCWSTR,DWORD,DWORD,const BYTE*,DWORD);
DLL LONG WINAPI RegDeleteValueW(HKEY,LPCWSTR);
DLL LONG WINAPI RegEnumValueW(HKEY,DWORD,LPWSTR,DWORD*,DWORD*,DWORD*,BYTE*,DWORD*);
DLL LONG WINAPI RegCloseKey(HKEY);
DLL HANDLE WINAPI OpenEventLogW(LPCWSTR,LPCWSTR);
DLL BOOL WINAPI ReadEventLogW(HANDLE,DWORD,DWORD,LPVOID,DWORD,DWORD*,DWORD*);
DLL BOOL WINAPI CloseEventLog(HANDLE);
DLL BOOL WINAPI SystemParametersInfoW(UINT,UINT,void*,UINT);
DLL int WINAPI MessageBoxW(HWND,LPCWSTR,LPCWSTR,UINT);
DLL BOOL WINAPI PostMessageW(HWND,UINT,WPARAM,LPARAM);
DLL LRESULT WINAPI SendMessageW(HWND,UINT,WPARAM,LPARAM);
DLL LRESULT WINAPI DefWindowProcW(HWND,UINT,WPARAM,LPARAM);
DLL HCURSOR WINAPI LoadCursorW(HINSTANCE,LPCWSTR);
DLL BOOL WINAPI RegisterClassExW(const WNDCLASSEXW*);
DLL HWND WINAPI CreateWindowExW(DWORD,LPCWSTR,LPCWSTR,DWORD,int,int,int,int,HWND,HMENU,HINSTANCE,void*);
DLL BOOL WINAPI ShowWindow(HWND,int);
DLL BOOL WINAPI UpdateWindow(HWND);
DLL BOOL WINAPI SetWindowTextW(HWND,LPCWSTR);
DLL BOOL WINAPI MoveWindow(HWND,int,int,int,int,BOOL);
DLL BOOL WINAPI EnableWindow(HWND,BOOL);
DLL BOOL WINAPI DestroyWindow(HWND);
DLL void WINAPI PostQuitMessage(int);
DLL int WINAPI GetMessageW(MSG*,HWND,UINT,UINT);
DLL BOOL WINAPI TranslateMessage(const MSG*);
DLL LRESULT WINAPI DispatchMessageW(const MSG*);
DLL HGDIOBJ WINAPI GetStockObject(int);
DLL void* WINAPI ShellExecuteW(HWND,LPCWSTR,LPCWSTR,LPCWSTR,LPCWSTR,int);
DLL HANDLE WINAPI OpenSCManagerW(LPCWSTR,LPCWSTR,DWORD);
DLL HANDLE WINAPI OpenServiceW(HANDLE,LPCWSTR,DWORD);
DLL BOOL WINAPI QueryServiceConfigW(HANDLE,void*,DWORD,DWORD*);
DLL BOOL WINAPI ChangeServiceConfigW(HANDLE,DWORD,DWORD,DWORD,LPCWSTR,LPCWSTR,DWORD*,LPCWSTR,LPCWSTR,LPCWSTR,LPCWSTR);
DLL BOOL WINAPI CloseServiceHandle(HANDLE);
DLL HICON WINAPI LoadIconW(HINSTANCE,LPCWSTR);
DLL HBRUSH WINAPI CreateSolidBrush(DWORD);
DLL DWORD WINAPI SetTextColor(HANDLE,DWORD);
DLL int WINAPI SetBkMode(HANDLE,int);
DLL BOOL WINAPI DeleteObject(void*);
DLL HBRUSH WINAPI SetClassLongPtrW(HWND,int,HBRUSH); /* reserved; unused */


#define HKEY_CURRENT_USER ((HKEY)(LONG)0x80000001)
#define HKEY_LOCAL_MACHINE ((HKEY)(LONG)0x80000002)
#define KEY_READ 0x20019
#define KEY_WRITE 0x20006
#define REG_DWORD 4
#define REG_SZ 1
#define ERROR_SUCCESS 0
#define WM_CREATE 1
#define WM_DESTROY 2
#define WM_SIZE 5
#define WM_COMMAND 0x0111
#define WM_SETFONT 0x0030
#define WM_APP 0x8000
#define WS_OVERLAPPEDWINDOW 0x00CF0000
#define WS_CHILD 0x40000000
#define WS_VISIBLE 0x10000000
#define WS_VSCROLL 0x00200000
#define WS_BORDER 0x00800000
#define ES_MULTILINE 0x0004
#define ES_AUTOVSCROLL 0x0040
#define ES_READONLY 0x0800
#define ES_NOHIDESEL 0x0100
#define SW_SHOWNORMAL 1
#define MB_OK 0
#define MB_ICONWARNING 0x30
#define MB_ICONINFORMATION 0x40
#define MB_YESNO 4
#define IDYES 6
#define SPI_GETUIEFFECTS 0x103E
#define SPI_SETUIEFFECTS 0x103F
#define SPIF_UPDATEINIFILE 1
#define SPIF_SENDCHANGE 2
#define TH32CS_SNAPPROCESS 0x00000002
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000
#define PROCESS_VM_READ 0x0010
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define FILE_ATTRIBUTE_REPARSE_POINT 0x400
#define INVALID_FILE_ATTRIBUTES 0xFFFFFFFF
#define GENERIC_WRITE 0x40000000
#define FILE_SHARE_READ 1
#define CREATE_ALWAYS 2
#define FILE_ATTRIBUTE_NORMAL 0x80
#define EVENTLOG_SEQUENTIAL_READ 1
#define EVENTLOG_BACKWARDS_READ 8
#define EVENTLOG_ERROR_TYPE 1
#define EVENTLOG_WARNING_TYPE 2
#define DEFAULT_GUI_FONT 17
#define IDC_ARROW ((LPCWSTR)(ULONG_PTR)32512)
#define ID_DIAGNOSE 1001
#define ID_FIX 1002
#define ID_CLEAN 1003
#define ID_RESTORE 1004
#define ID_REPORT 1005
#define ID_STARTUP 1006
#define ID_ADVANCED 1007
#define ID_ROLLBACK 1008
#define WM_CTLCOLORSTATIC 0x0138
#define WM_CTLCOLOREDIT 0x0133
#define WM_CTLCOLORBTN 0x0135
#define BS_PUSHBUTTON 0x00000000
#define WS_TABSTOP 0x00010000
#define TRANSPARENT 1
#define DT_CENTER 1
#define DT_VCENTER 4
#define DT_SINGLELINE 0x20
#define SC_MANAGER_CONNECT 0x0001
#define SERVICE_QUERY_CONFIG 0x0001
#define SERVICE_CHANGE_CONFIG 0x0002
#define SERVICE_NO_CHANGE 0xFFFFFFFF
#define SERVICE_DISABLED 4
#define ICON_RESOURCE 101


static HINSTANCE g_inst;
static HWND g_main,g_editor,g_status,g_buttons[8],g_heading;
static HBRUSH g_darkBrush,g_panelBrush;
static volatile int g_busy=0;
static WCHAR g_report[24000];
static UINT g_pos=0;
static WCHAR g_reportPath[512];
static int g_events_ok=0,g_run_count=0,g_highmem=0,g_lowdisk=0,g_extremedisk=0,g_storage_faults=0,g_crashes=0,g_hangs=0,g_bsod=0,g_whea=0,g_sys_err=0,g_app_err=0;
static ULONGLONG g_ram=0,g_free=0,g_disk=0,g_uptime=0;
static unsigned g_memload=0;
static unsigned g_cpu=0,g_cpuTopPct=0;
static WCHAR g_cpuTopName[MAX_PATH];
static int g_cpu_ok=0;
static unsigned g_deleted=0;
static ULONGLONG g_freed=0;
static int g_opt_ok=0;
static int g_mode=0;
static WCHAR g_top1[MAX_PATH];
static int g_app_names_count=0;
static ULONGLONG g_current_event_epoch=0;
typedef struct {WCHAR name[120]; WCHAR module[120]; WCHAR exception[32]; WCHAR appPath[260]; unsigned count; DWORD newest;} FAULT;
static FAULT g_faults[12];
typedef struct {WCHAR name[MAX_PATH];ULONGLONG priv,working;unsigned processes;} GROUP;
static GROUP g_group[64];
static int g_group_count=0;
typedef struct {DWORD dwServiceType,dwStartType,dwErrorControl;WCHAR *lpBinaryPathName,*lpLoadOrderGroup;DWORD dwTagId;WCHAR *lpDependencies,*lpServiceStartName,*lpDisplayName;} QSVC;


typedef struct { WCHAR name[MAX_PATH]; DWORD pid;ULONGLONG usage; } TOP;
static TOP g_top[8];
typedef struct {HANDLE h; DWORD pid; WCHAR name[MAX_PATH]; ULONGLONG ticks;} CPU_ENTRY;
static CPU_ENTRY g_cpu_samples[320];

__attribute__((optnone)) void *memcpy(void *dst,const void *src,SIZE_T n){BYTE*d=(BYTE*)dst;const BYTE*s=(const BYTE*)src;for(SIZE_T i=0;i<n;i++)d[i]=s[i];return dst;}
static UINT wlen(const WCHAR*s){UINT i=0;if(!s)return 0;while(s[i])i++;return i;}
static void wcopy(WCHAR*d,const WCHAR*s,UINT cap){UINT i=0;if(!cap)return;while(s&&s[i]&&i+1<cap){d[i]=s[i];i++;}d[i]=0;}
static void wcat(WCHAR*d,const WCHAR*s,UINT cap){UINT n=wlen(d);if(n<cap)wcopy(d+n,s,cap-n);}
static UINT ascii_upper(UINT c){return (c>=97&&c<=122)?c-32:c;}
static BOOL same_ci(LPCWSTR a,LPCWSTR b){while(*a&&*b){if(ascii_upper(*a)!=ascii_upper(*b))return 0;a++;b++;}return *a==*b;}
static BOOL contains_ci(LPCWSTR a,LPCWSTR b){UINT bl=wlen(b);if(!bl)return 1;for(;*a;a++){UINT i=0;while(i<bl&&a[i]&&ascii_upper(a[i])==ascii_upper(b[i]))i++;if(i==bl)return 1;}return 0;}
static ULONGLONG ft64(FILETIME a){return ((ULONGLONG)a.dwHighDateTime<<32)|a.dwLowDateTime;}
static void append(LPCWSTR s){while(*s&&g_pos+1<sizeof(g_report)/sizeof(g_report[0]))g_report[g_pos++]=*s++;g_report[g_pos]=0;}
static void num(ULONGLONG n){WCHAR a[32];UINT i=0;do{a[i++]=(WCHAR)(L'0'+n%10);n/=10;}while(n&&i<31);while(i&&g_pos+1<sizeof(g_report)/sizeof(g_report[0]))g_report[g_pos++]=a[--i];g_report[g_pos]=0;}
static void nl(void){append(L"\r\n");}
static void line(LPCWSTR s){append(s);nl();}
static void fmt_size(ULONGLONG n){num(n/1048576);append(L" MB");}
static void heading(LPCWSTR s){nl();line(L"------------------------------------------------------------");line(s);line(L"------------------------------------------------------------");}
static void reset(void){g_report[0]=0;g_pos=0;g_events_ok=0;g_run_count=0;g_highmem=0;g_lowdisk=0;g_extremedisk=0;g_storage_faults=0;g_crashes=0;g_hangs=0;g_bsod=0;g_whea=0;g_sys_err=0;g_app_err=0;g_cpu_ok=0;g_cpu=0;g_cpuTopPct=0;g_cpuTopName[0]=0;g_top1[0]=0;for(int i=0;i<8;i++)g_top[i].usage=0;g_group_count=0;g_app_names_count=0;for(int i=0;i<12;i++)g_faults[i].count=0;}
static void reg_count(HKEY root,LPCWSTR key,const WCHAR*title){HKEY h=0;if(RegOpenKeyExW(root,key,0,KEY_READ,&h)!=0)return;DWORD idx=0;WCHAR name[256];WCHAR data[1024];DWORD namelen,type,datalen;int c=0;for(;idx<120;idx++){namelen=255;datalen=sizeof(data);type=0;if(RegEnumValueW(h,idx,name,&namelen,0,&type,(BYTE*)data,&datalen)!=0)break;name[namelen]=0;if(c<20){append(L"  ");append(title);append(L": ");append(name);nl();}c++;}g_run_count+=c;RegCloseKey(h);}
static void group_process(LPCWSTR name,ULONGLONG priv,ULONGLONG ws){
    int k=0;for(;k<g_group_count;k++)if(same_ci(g_group[k].name,name))break;
    if(k==g_group_count){if(k>=64)return;wcopy(g_group[k].name,name,MAX_PATH);g_group[k].priv=0;g_group[k].working=0;g_group[k].processes=0;g_group_count++;}
    g_group[k].priv+=priv;g_group[k].working+=ws;g_group[k].processes++;
}
static void store_crash(LPCWSTR app,LPCWSTR mod,LPCWSTR exception,LPCWSTR path,DWORD eventEpoch){
    if(!app||!app[0])app=L"(name not available)";
    if(!mod)mod=L"";if(!exception)exception=L"";
    int k=0;
    for(;k<g_app_names_count;k++)if(same_ci(g_faults[k].name,app)&&same_ci(g_faults[k].module,mod)&&same_ci(g_faults[k].exception,exception))break;
    if(k>=12)return;
    if(k==g_app_names_count){
        g_app_names_count++;
        wcopy(g_faults[k].name,app,120);wcopy(g_faults[k].module,mod,120);
        wcopy(g_faults[k].exception,exception,32);wcopy(g_faults[k].appPath,path?path:L"",260);
        g_faults[k].count=0;g_faults[k].newest=0;
    }
    g_faults[k].count++;
    if(eventEpoch>g_faults[k].newest)g_faults[k].newest=eventEpoch;
}
static void crash_insertions(const EVENTLOGRECORD*r){
    /* Application Error event 1000 insertion order is commonly:
       1 app; 4 module; 7 exception code; 11 application path.
       Read only NUL-terminated UTF-16 strings within the original record. */
    if(r->NumStrings<1||r->StringOffset>=r->Length||r->StringOffset<sizeof(EVENTLOGRECORD)||(r->StringOffset&1))return;
    DWORD byteOff=r->StringOffset;
    WCHAR app[120],mod[120],exception[32],path[260];
    app[0]=mod[0]=exception[0]=path[0]=0;
    for(unsigned k=0;k<r->NumStrings&&k<11;k++){
        WCHAR tmp[260];UINT j=0;int terminated=0;
        while(byteOff+2<=r->Length){
            WCHAR wc=*(const WCHAR*)((const BYTE*)r+byteOff);byteOff+=2;
            if(!wc){terminated=1;break;}
            if(j+1<260)tmp[j++]=wc;
        }
        if(!terminated)return;
        tmp[j]=0;
        if(k==0)wcopy(app,tmp,120);
        if(k==3)wcopy(mod,tmp,120);
        if(k==6)wcopy(exception,tmp,32);
        if(k==10)wcopy(path,tmp,260);
    }
    store_crash(app,mod,exception,path,r->TimeGenerated);
}
static void process_snapshot(void){HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snap==INVALID_HANDLE_VALUE)return;PROCESSENTRY32W p;p.dwSize=sizeof(p);if(Process32FirstW(snap,&p)){do{HANDLE h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_VM_READ,0,p.th32ProcessID);if(!h)h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,0,p.th32ProcessID);if(h){PROCESS_MEMORY_COUNTERS_EX pm;pm.cb=sizeof(pm);if(K32GetProcessMemoryInfo(h,&pm,sizeof(pm))){ULONGLONG m=(ULONGLONG)pm.WorkingSetSize;group_process(p.szExeFile,(ULONGLONG)pm.PrivateUsage,m);for(int k=0;k<8;k++){if(m>g_top[k].usage){for(int q=7;q>k;q--)g_top[q]=g_top[q-1];g_top[k].pid=p.th32ProcessID;g_top[k].usage=m;wcopy(g_top[k].name,p.szExeFile,MAX_PATH);break;}}}CloseHandle(h);}}while(Process32NextW(snap,&p));}CloseHandle(snap);}
static DWORD scan_events(LPCWSTR logname){HANDLE h=OpenEventLogW(0,logname);if(!h)return 0;static BYTE b[65536];DWORD count=0,read=0,needed=0,loops=0;FILETIME ft;GetSystemTimeAsFileTime(&ft);ULONGLONG cur=ft64(ft)/10000000ULL-11644473600ULL;g_current_event_epoch=cur;DWORD limit=(cur>7ULL*86400)?(DWORD)(cur-7ULL*86400):0;int isSystem=same_ci(logname,L"System");while(loops++<60){if(!ReadEventLogW(h,EVENTLOG_SEQUENTIAL_READ|EVENTLOG_BACKWARDS_READ,0,b,sizeof(b),&read,&needed))break;DWORD off=0;while(off+sizeof(EVENTLOGRECORD)<=read){EVENTLOGRECORD*r=(EVENTLOGRECORD*)(b+off);if(r->Length<sizeof(EVENTLOGRECORD)||r->Length>read-off){off=read;break;}if(r->TimeGenerated<limit){CloseEventLog(h);return count;}int id=(int)(r->EventID&0xFFFF);WCHAR provider[120];UINT pn=0;DWORD po=sizeof(EVENTLOGRECORD);int foundEnd=0;
while(po+2<=r->Length){WCHAR wc=*(const WCHAR*)((const BYTE*)r+po);po+=2;if(!wc){foundEnd=1;break;}if(pn+1<120)provider[pn++]=wc;}
provider[pn]=0;
if(!foundEnd){off+=r->Length;continue;}
if(r->EventType==EVENTLOG_ERROR_TYPE){if(isSystem)g_sys_err++;else g_app_err++;}if(isSystem){if((id==41&&contains_ci(provider,L"Kernel-Power"))||(id==1001&&contains_ci(provider,L"BugCheck")))g_bsod++;
/* Storage IDs are correlated with source/provider only where possible. */
if((id==7||id==51||id==55||id==129||id==153||id==157)&&(contains_ci(provider,L"Disk")||contains_ci(provider,L"stor")||contains_ci(provider,L"Ntfs")||contains_ci(provider,L"volmgr")||contains_ci(provider,L"iaStor")))g_storage_faults++;
if((id==17||id==18||id==19)&&contains_ci(provider,L"WHEA"))g_whea++;
}else{if(id==1000&&contains_ci(provider,L"Application Error")){g_crashes++;crash_insertions(r);}if(id==1002&&contains_ci(provider,L"Application Hang"))g_hangs++;}count++;off+=r->Length;if(count>=3500){CloseEventLog(h);return count;}}}CloseEventLog(h);return count;}
static void metric_cpu(void){
    FILETIME i0,k0,u0,i1,k1,u1,cr,ex,pk,pu;
    DWORD count=0;
    HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snap!=INVALID_HANDLE_VALUE){
        PROCESSENTRY32W p; p.dwSize=sizeof(p);
        if(Process32FirstW(snap,&p))do{
            if(count>=320)break;
            HANDLE h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,p.th32ProcessID);
            if(!h)continue;
            if(GetProcessTimes(h,&cr,&ex,&pk,&pu)){
                g_cpu_samples[count].h=h;
                g_cpu_samples[count].pid=p.th32ProcessID;
                wcopy(g_cpu_samples[count].name,p.szExeFile,MAX_PATH);
                g_cpu_samples[count].ticks=ft64(pk)+ft64(pu);
                count++;
            }else CloseHandle(h);
        }while(Process32NextW(snap,&p));
        CloseHandle(snap);
    }
    int ok=GetSystemTimes(&i0,&k0,&u0);
    Sleep(1500);
    if(ok&&GetSystemTimes(&i1,&k1,&u1)){
        ULONGLONG idle=ft64(i1)-ft64(i0),kernel=ft64(k1)-ft64(k0),usr=ft64(u1)-ft64(u0),total=kernel+usr;
        if(total){
            g_cpu=(unsigned)((total>idle)?((total-idle)*100ULL/total):0);
            g_cpu_ok=1;
            for(DWORD i=0;i<count;i++){
                if(GetProcessTimes(g_cpu_samples[i].h,&cr,&ex,&pk,&pu)){
                    ULONGLONG ticks=ft64(pk)+ft64(pu);
                    ULONGLONG used=ticks>=g_cpu_samples[i].ticks?ticks-g_cpu_samples[i].ticks:0;
                    unsigned pct=(unsigned)((used>total)?100:(used*100ULL/total));
                    if(pct>g_cpuTopPct){g_cpuTopPct=pct;wcopy(g_cpuTopName,g_cpu_samples[i].name,MAX_PATH);}
                }
            }
        }
    }
    for(DWORD i=0;i<count;i++)CloseHandle(g_cpu_samples[i].h);
}
static void stats(void){MEMORYSTATUSEX mem;mem.dwLength=sizeof(mem);if(GlobalMemoryStatusEx(&mem)){g_ram=mem.ullTotalPhys;g_memload=mem.dwMemoryLoad;g_highmem=mem.dwMemoryLoad>=83;append(L"Physical RAM: ");fmt_size(mem.ullTotalPhys);append(L" | Available: ");fmt_size(mem.ullAvailPhys);append(L" | Memory load: ");num(mem.dwMemoryLoad);line(L"%");append(L"Virtual/commit capacity: ");fmt_size(mem.ullTotalPageFile);append(L" | Available commit: ");fmt_size(mem.ullAvailPageFile);nl();if(g_ram<=2147483648ULL)line(L"DEVICE PROFILE: EXTREME 1-2 GB RAM. Prioritize one active application; pagefile must remain enabled.");
else if(g_ram<=4294967296ULL)line(L"DEVICE PROFILE: LIGHT 4 GB RAM. High-impact work is grouping/closing unused app sessions.");
if(mem.ullTotalPageFile<mem.ullTotalPhys+268435456ULL){line(L"NOTICE: Commit capacity is unusually small. Verify the pagefile is SYSTEM-MANAGED.");}}
WCHAR drive[32];if(!GetEnvironmentVariableW(L"SystemDrive",drive,32))wcopy(drive,L"C:",32);wcat(drive,L"\\",32);ULONGLONG avail,total,freeb;if(GetDiskFreeSpaceExW(drive,&avail,&total,&freeb)){g_disk=total;g_free=avail;g_extremedisk=(avail<1073741824ULL);g_lowdisk=(avail<3ULL*1073741824ULL || (total&&avail*100ULL/total<12ULL));append(L"System drive ");append(drive);append(L": ");fmt_size(total);append(L" total, ");fmt_size(avail);line(L" available");}else line(L"Disk capacity check unavailable.");
if(g_disk<=20ULL*1073741824ULL)line(L"STORAGE PROFILE: TINY 16-20 GB. Avoid extra applications, temporary copies, and large Windows repair caches.");
else if(g_disk<=40ULL*1073741824ULL)line(L"STORAGE PROFILE: COMPACT 32-40 GB. Monitor free space during Windows updates and repair operations.");
g_uptime=GetTickCount64()/3600000ULL;append(L"Uptime: ");num(g_uptime);line(L" hours");metric_cpu();if(g_cpu_ok){append(L"Sample CPU utilization (1500ms): ");num(g_cpu);line(L"% (a momentary sample, not a long-term average)");if(g_cpuTopPct){append(L"Top sampled process CPU share: ");append(g_cpuTopName);append(L" - ");num(g_cpuTopPct);line(L"% of total machine CPU capacity");}}}
static void top_procs(void){process_snapshot();
line(L"Grouped process private committed bytes (NOT physical RAM saved by closure):");
for(int i=0;i<g_group_count;i++)for(int j=i+1;j<g_group_count;j++)if(g_group[j].priv>g_group[i].priv){GROUP tmp=g_group[i];g_group[i]=g_group[j];g_group[j]=tmp;}
for(int i=0;i<g_group_count&&i<8;i++){append(L"  ");append(g_group[i].name);append(L" (processes ");num(g_group[i].processes);append(L"): private commit ");fmt_size(g_group[i].priv);append(L", working-set sum ");fmt_size(g_group[i].working);nl();}
line(L"Working sets below can double-count shared pages across processes:");line(L"Largest visible process working sets (shared pages included):");for(int i=0;i<8;i++){if(!g_top[i].usage)break;if(i==0)wcopy(g_top1,g_top[i].name,MAX_PATH);append(L"  ");append(g_top[i].name);append(L" [PID ");num(g_top[i].pid);append(L"]: ");fmt_size(g_top[i].usage);nl();}}
static void make_recommendations(void){heading(L"FINAL VERDICT - EVIDENCE-BASED PRIORITIES");int findings=0;if(g_storage_faults){line(L"PRIORITY 1 - POSSIBLE DISK/CONTROLLER ERRORS");append(L"Found ");num(g_storage_faults);line(L" storage-related System Event IDs during the past 7 days.");line(L"ACTION: Back up important files immediately; inspect Event Viewer event source and storage cabling/health. Use `chkdsk C: /scan` when you have time. Do not repeatedly write to a failing SSD.");findings++;}
if(g_whea){line(L"HARDWARE/DRIVER WARNINGS - WHEA-like event IDs found; inspect source/provider in Event Viewer. Check heat, power, and driver stability.");findings++;}
if(g_bsod){line(L"UNEXPECTED SHUTDOWN/BUGCHECK INDICATORS - inspect Kernel-Power 41 and BugCheck events. Event 41 alone does NOT prove a BSOD; it also happens after forced power-off.");findings++;}
if(g_extremedisk){line(L"CRITICAL FREE SPACE - below 1 GB. Windows may freeze, fail updates, and page heavily.");line(L"ACTION: Run the safe cleanup, then Disk Cleanup; move large downloads off the system drive. Keep the pagefile enabled.");findings++;}
else if(g_lowdisk){line(L"LOW SYSTEM-DRIVE FREE SPACE - performance and reliability can degrade.");line(L"ACTION: Use cleanup, review Storage Settings / Temporary files, and remove unused app installations selectively.");findings++;}
if(g_highmem){line(L"HIGH LIVE RAM PRESSURE - memory is heavily occupied now.");
line(L"ACTION: End only unused app sessions from their own menus; do not force-close unsaved developer tools, Windows services, or the browser that holds active work. Keep the pagefile SYSTEM-MANAGED.");
if(g_group_count){append(L"Largest private-commit application group: ");append(g_group[0].name);append(L" (multiple processes may be one app session). Private commit is NOT the memory reclaimed by closing it.");nl();}
findings++;}
if(g_cpu_ok&&g_cpu>=85){line(L"HIGH INSTANTANEOUS CPU USE - CPU was busy during the sample; recheck when idle. Do not stop Windows security or core system processes.");if(g_cpuTopPct>=15){append(L"Top sampled CPU user: ");append(g_cpuTopName);append(L" (approx ");num(g_cpuTopPct);line(L"% overall). Investigate it before changing startup preferences.");}findings++;}
if(g_run_count>=8){line(L"MANY LEGACY RUN STARTUP ENTRIES - check Windows Settings > Apps > Startup, disable only programs you recognize and do not need at boot.");findings++;}
if(g_crashes||g_hangs){append(L"APPLICATION INSTABILITY - ");num(g_crashes);append(L" crash-ID and ");num(g_hangs);line(L" hang-ID records. Consult TARGETED FAULT TRIAGE; SAFE FIX does not repair drivers, applications or these crash records.");findings++;}
if(g_sys_err>=12||g_app_err>=12){line(L"REPEATED LOGGED ERRORS - investigate the event SOURCE/DETAILS before trying broad Windows repairs.");findings++;}
if(!g_events_ok){line(L"EVENT LOG ACCESS INCOMPLETE - this report cannot exclude crashes or hardware errors. Run again as administrator for broader visibility.");findings++;}
if(!findings)line(L"No major pressure flags from sampled metrics and accessible log records. Intermittent freezes still require a longer capture; check temperatures, drivers, and Event Viewer around the exact freeze time.");
heading(L"LOW-COST REPAIRS TO CONSIDER");line(L"1. Use this tool's SAFE FIX: reversible Windows animations/transparency and old top-level user-temp files.");line(L"2. Open Startup Apps; turn off only unneeded startup programs (leave security software alone).");line(L"3. Windows Security: run a built-in Quick scan if unexplained CPU/disk usage persists.");line(L"4. For corruption: run Command Prompt as admin, `sfc /verifyonly` first. If faults are detected, plan an `sfc /scannow`; DISM may need extra disk space.");line(L"5. For disk errors: `chkdsk C: /scan`. For repeated storage events, back up first and check SSD condition.");line(L"6. Set a lightweight wallpaper/theme, keep simultaneous browser tabs minimal, restart after updates.");line(L"Optional Advanced Risk Mode: explicit 3-second countdown; affects only whitelisted search/indexing/prefetch services, never essential OS/security services. Requires administrator rights and can be rolled back.");
line(L"NOT recommended: random service disabling, disabling Defender, registry cleaners, forced RAM flushing, fixed tiny pagefiles, or blindly deleting WinSxS/Windows Update files.");
heading(L"INTERPRETATION AND LIMITS");line(L"Crash clusters are grouped by application, module and exception code. Records and path insertion fields depend on the event provider. Historical crash counts persist after repairs until records age out of the 7-day window.");line(L"A one-click scan cannot prove every root cause. CPU use is one 1.5-second sample. Private commit is NOT total physically occupied RAM. This tool does not measure sustained disk latency, SSD SMART, malware infection, pagefile hard-fault rates, or full component integrity.");}
static void report_title(void){SYSTEMTIME t;GetLocalTime(&t);line(L"BLAZE ULTIMATE OPTIMIZER - Windows 10/11 ultra-low-spec diagnostic v1.2");line(L"Native Windows x64/ARM64 / user-mode / offline / zero telemetry");append(L"Scan date: ");num(t.wYear);append(L"-");num(t.wMonth);append(L"-");num(t.wDay);append(L" ");num(t.wHour);append(L":");if(t.wMinute<10)append(L"0");num(t.wMinute);nl();}
static void collect(void){reset();report_title();heading(L"LIVE SYSTEM METRICS");stats();heading(L"TOP MEMORY USERS");top_procs();heading(L"STARTUP ENTRIES (REGISTRY RUN KEYS)");reg_count(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",L"User");reg_count(HKEY_LOCAL_MACHINE,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",L"Machine");reg_count(HKEY_LOCAL_MACHINE,L"Software\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Run",L"Machine x86");append(L"Visible registry Run entries: ");num(g_run_count);line(L". Other startup sources (tasks/services/startup folder) are not counted.");heading(L"WINDOWS EVENT LOGS (PAST 7 DAYS)");DWORD sys=scan_events(L"System"),app=scan_events(L"Application");g_events_ok=(sys||app)?1:0;append(L"Records read: System ");num(sys);append(L", Application ");num(app);nl();append(L"Errors: System ");num(g_sys_err);append(L", Application ");num(g_app_err);nl();append(L"Storage-related IDs: ");num(g_storage_faults);append(L" | WHEA-like IDs: ");num(g_whea);append(L" | power/bugcheck IDs: ");num(g_bsod);nl();append(L"App crash (1000): ");num(g_crashes);append(L" | app hang (1002): ");num(g_hangs);nl();line(L"Counts are clues, not confirmed diagnoses. Read the source/provider and message in Event Viewer. (Max 3500 recent records per log.)");
heading(L"APPLICATION CRASH CLUSTERS - EVENT 1000 DETAILS");
if(!g_app_names_count)line(L"No parseable Application Error 1000 insertion strings in sampled records.");
for(int i=0;i<g_app_names_count;i++)for(int j=i+1;j<g_app_names_count;j++)if(g_faults[j].count>g_faults[i].count){FAULT tmp=g_faults[i];g_faults[i]=g_faults[j];g_faults[j]=tmp;}
for(int i=0;i<g_app_names_count;i++){
    append(L"  ");append(g_faults[i].name);append(L" - ");num(g_faults[i].count);append(L" crash record(s)");
    if(g_faults[i].module[0]){append(L" | module: ");append(g_faults[i].module);}
    if(g_faults[i].exception[0]){append(L" | exception: ");append(g_faults[i].exception);}
    if(g_faults[i].newest&&g_current_event_epoch>=g_faults[i].newest){append(L" | latest approximately ");num((g_current_event_epoch-g_faults[i].newest)/3600ULL);append(L"h ago");}
    nl();
    if(g_faults[i].appPath[0]){append(L"    app path: ");append(g_faults[i].appPath);nl();}
}
line(L"Event 1000 inserted fields are evidence, not proof that the named module caused the fault. Records may repeat or be truncated.");
heading(L"TARGETED FAULT TRIAGE - NO AUTOMATIC DRIVER CHANGES");
int matched=0;
for(int i=0;i<g_app_names_count;i++){
  if(same_ci(g_faults[i].name,L"AUDIODG.EXE")&&contains_ci(g_faults[i].module,L"CX64")){
    line(L"AUDIO: Windows audio engine crashes with a Conexant-family CX64 module. Test disabling Audio Enhancements on the output device, then check your device maker's matching driver. Do not terminate audiodg.exe or disable audio services.");matched=1;
  }else if(contains_ci(g_faults[i].name,L"E_YAR")||contains_ci(g_faults[i].module,L"E_YAP")){
    line(L"EPSON-RELATED: Possible Epson printer status-monitor component. If printing works but the monitor crashes, repair that matching Epson utility; do not disable the Windows Print Spooler blindly.");matched=1;
  }else if(same_ci(g_faults[i].module,L"KERNELBASE.dll")){
    line(L"GENERIC WINDOWS MODULE: KERNELBASE.dll identifies where an exception surfaced, not necessarily the cause. Use the app path and exception code to investigate this specific program.");matched=1;
  }
}
if(!matched)line(L"No safe vendor-specific repair pattern matched. Inspect full Event Viewer messages before changing drivers or services.");
make_recommendations();}
static BOOL reg_read_dword(HKEY h,LPCWSTR name,DWORD*out){DWORD t=0,n=4;return RegQueryValueExW(h,name,0,&t,(BYTE*)out,&n)==0&&t==REG_DWORD&&n==4;}
static BOOL backup_reg(DWORD effect){HKEY h=0;DWORD disposition=0;if(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\BlazeSystems\\UltimateOptimizer",0,0,0,KEY_READ|KEY_WRITE,0,&h,&disposition)!=0)return FALSE;DWORD marker=0;if(!reg_read_dword(h,L"BackupExists",&marker)||!marker){RegSetValueExW(h,L"OriginalUIEffects",0,REG_DWORD,(const BYTE*)&effect,4);DWORD empty=0;RegSetValueExW(h,L"TransparencyExisted",0,REG_DWORD,(const BYTE*)&empty,4);HKEY hp;if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",0,KEY_READ,&hp)==0){DWORD transp=0;if(reg_read_dword(hp,L"EnableTransparency",&transp)){RegSetValueExW(h,L"OriginalTransparency",0,REG_DWORD,(const BYTE*)&transp,4);DWORD exists=1;RegSetValueExW(h,L"TransparencyExisted",0,REG_DWORD,(const BYTE*)&exists,4);}RegCloseKey(hp);}marker=1;RegSetValueExW(h,L"BackupExists",0,REG_DWORD,(const BYTE*)&marker,4);}RegCloseKey(h);return TRUE;}
static int set_transparency(DWORD value){HKEY hp=0;if(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",0,0,0,KEY_WRITE,0,&hp,0)!=0)return 0;int ok=RegSetValueExW(hp,L"EnableTransparency",0,REG_DWORD,(BYTE*)&value,4)==0;RegCloseKey(hp);return ok;}
static void visual_optimize(void){g_opt_ok=0;BOOL current=1;if(!SystemParametersInfoW(SPI_GETUIEFFECTS,0,&current,0)){line(L"Visual effect API not available; no visual modifications performed.");return;}if(!backup_reg((DWORD)current)){line(L"Backup registry inaccessible; visual optimizations skipped to preserve reversibility.");return;}BOOL off=FALSE;int a=SystemParametersInfoW(SPI_SETUIEFFECTS,0,&off,SPIF_UPDATEINIFILE|SPIF_SENDCHANGE);int b=set_transparency(0);g_opt_ok=a||b;line(a?L"Visual animation effects: disabled, previous state backed up.":L"Visual effect change refused by Windows.");line(b?L"Transparency effects: set to off (sign-out may be needed).":L"Transparency change not applied.");}
static void visual_restore(void){HKEY h=0;DWORD exists=0,orig=0,transp=0,tExists=0;if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\BlazeSystems\\UltimateOptimizer",0,KEY_READ|KEY_WRITE,&h)!=0){MessageBoxW(g_main,L"No saved visual preferences found.",L"Restore",MB_OK|MB_ICONINFORMATION);return;}if(!reg_read_dword(h,L"BackupExists",&exists)||!exists){RegCloseKey(h);MessageBoxW(g_main,L"No saved visual preferences found.",L"Restore",MB_OK|MB_ICONINFORMATION);return;}if(reg_read_dword(h,L"OriginalUIEffects",&orig)){BOOL set=orig?TRUE:FALSE;SystemParametersInfoW(SPI_SETUIEFFECTS,0,&set,SPIF_UPDATEINIFILE|SPIF_SENDCHANGE);}if(reg_read_dword(h,L"TransparencyExisted",&tExists)&&tExists&&reg_read_dword(h,L"OriginalTransparency",&transp))set_transparency(transp);else {HKEY hp=0;if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",0,KEY_WRITE,&hp)==0){RegDeleteValueW(hp,L"EnableTransparency");RegCloseKey(hp);}}RegDeleteValueW(h,L"BackupExists");RegCloseKey(h);MessageBoxW(g_main,L"Saved animation and transparency preferences restored. Removed temporary files cannot be restored.",L"Preferences restored",MB_OK|MB_ICONINFORMATION);}
static void clean_user_temp(void){g_deleted=0;g_freed=0;WCHAR dir[512],pat[520],full[650];DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",dir,480);if(!n||n>=480){line(L"Personal LOCALAPPDATA unavailable. Cleanup skipped to avoid shared/system temp folders.");return;}wcat(dir,L"\\Temp\\",512);static WCHAR check[512];wcopy(check,dir,512);UINT checklen=wlen(check);if(checklen&&check[checklen-1]==L'\\')check[checklen-1]=0;DWORD attr=GetFileAttributesW(check);if(attr==INVALID_FILE_ATTRIBUTES||!(attr&FILE_ATTRIBUTE_DIRECTORY)||(attr&FILE_ATTRIBUTE_REPARSE_POINT)){line(L"Personal TEMP folder missing or a directory link; cleanup skipped to protect other locations.");return;}wcopy(pat,dir,520);wcat(pat,L"*",520);WIN32_FIND_DATAW f;HANDLE h=FindFirstFileW(pat,&f);if(h==INVALID_HANDLE_VALUE){line(L"Temp listing failed. Cleanup skipped.");return;}FILETIME now;GetSystemTimeAsFileTime(&now);ULONGLONG cutoff=ft64(now)-7ULL*86400ULL*10000000ULL;do{if(f.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))continue;if(f.cFileName[0]==L'.')continue;if(ft64(f.ftLastWriteTime)>cutoff)continue;wcopy(full,dir,650);wcat(full,f.cFileName,650);if(DeleteFileW(full)){g_deleted++;g_freed+=((ULONGLONG)f.nFileSizeHigh<<32)|f.nFileSizeLow;}}while(g_deleted<25000&&FindNextFileW(h,&f));FindClose(h);append(L"Old top-level temp files removed (>7 days old): ");num(g_deleted);append(L"; apparent freed size: ");fmt_size(g_freed);nl();line(L"Safety limits: no subfolders, shortcuts/reparse points, recent files, or Windows system directories touched. Locked files were left alone.");}
static const WCHAR* riskServices[2]={L"WSearch",L"SysMain"};
static const WCHAR* riskRegistry[2]={L"ServiceBackup_WSearch",L"ServiceBackup_SysMain"};
static void risky_services(int restore){
    heading(restore?L"ADVANCED RISK MODE - SERVICE ROLLBACK":L"ADVANCED RISK MODE - OPT-IN SERVICE STARTUP CHANGES");
    HANDLE scm=OpenSCManagerW(0,0,SC_MANAGER_CONNECT);
    if(!scm){line(L"Could not open Service Control Manager (permissions denied or unsupported Windows installation). No changes made.");return;}
    for(int i=0;i<2;i++){
        HKEY key=0;DWORD disp=0,prev=0;
        if(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\BlazeSystems\\UltimateOptimizer",0,0,0,KEY_READ|KEY_WRITE,0,&key,&disp)!=0){line(L"Could not open rollback registry store. Skipped service.");continue;}
        int haveBackup=reg_read_dword(key,riskRegistry[i],&prev);
        HANDLE svc=OpenServiceW(scm,riskServices[i],SERVICE_QUERY_CONFIG|SERVICE_CHANGE_CONFIG);
        if(!svc){append(L"  ");append(riskServices[i]);line(L": unavailable or insufficient rights; skipped.");RegCloseKey(key);continue;}
        static BYTE b[8192]; QSVC *cfg=(QSVC*)b;DWORD needed=0;
        if(!QueryServiceConfigW(svc,b,sizeof(b),&needed)){append(riskServices[i]);line(L": config read failed; skipped.");CloseServiceHandle(svc);RegCloseKey(key);continue;}
        DWORD from=cfg->dwStartType,to=restore?prev:SERVICE_DISABLED;
        if(restore&&!haveBackup){append(riskServices[i]);line(L": no backup; skipped.");CloseServiceHandle(svc);RegCloseKey(key);continue;}
        if(!restore&&!haveBackup){
            /* The backup MUST persist before any write to the service configuration. */
            if(RegSetValueExW(key,riskRegistry[i],0,REG_DWORD,(BYTE*)&from,4)!=ERROR_SUCCESS){append(riskServices[i]);line(L": rollback backup failed; refused change.");CloseServiceHandle(svc);RegCloseKey(key);continue;}
        }
        BOOL ok=(from==to)||ChangeServiceConfigW(svc,SERVICE_NO_CHANGE,to,SERVICE_NO_CHANGE,0,0,0,0,0,0,0);
        append(L"  ");append(riskServices[i]);append(ok?(restore?L": restored original startup configuration":L": startup changed to DISABLED (restart needed)"):L": change rejected by Windows");nl();
        if(ok&&restore)RegDeleteValueW(key,riskRegistry[i]);
        CloseServiceHandle(svc);RegCloseKey(key);
    }
    CloseServiceHandle(scm);
    line(L"Existing running services are NOT forcibly stopped. Windows Search and SysMain features may behave differently after restart.");
    line(L"Core OS services, Defender, pagefile, Windows Update and boot drivers were not altered.");
}
static void save_report(void){WCHAR base[512];DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",base,512);if(!n||n>=512){n=GetTempPathW(512,base);if(!n||n>=512)return;}else wcat(base,L"\\",512);wcopy(g_reportPath,base,512);wcat(g_reportPath,L"BlazeUltimateOptimizer-Report.txt",512);HANDLE h=CreateFileW(g_reportPath,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);if(h==INVALID_HANDLE_VALUE)return;DWORD wrote;WORD bom=0xFEFF;WriteFile(h,&bom,2,&wrote,0);WriteFile(h,g_report,g_pos*2,&wrote,0);CloseHandle(h);}
static DWORD WINAPI worker(LPVOID ignored){
    (void)ignored;
    collect();
    if(g_mode==1){
        ULONGLONG beforeDisk=g_free;
        unsigned beforeMem=g_memload;
        /* Safe actions are performed only after explicit confirmation on the UI thread. */
        visual_optimize();
        clean_user_temp();
        ULONGLONG deletedBytes=g_freed;
        unsigned deletedFiles=g_deleted;
        int optApplied=g_opt_ok;
        collect(); /* Refresh the verdict based on the actual post-fix condition. */
        heading(L"ONE-CLICK FIX RECEIPT AND RECHECK");
        append(L"Visual effects/transparency update: ");line(optApplied?L"attempted and at least one setting applied; reversible via RESTORE PREFERENCES":L"not applied; consult Windows display settings");
        append(L"Old personal temp files deleted: ");num(deletedFiles);append(L" (approx ");fmt_size(deletedBytes);line(L" apparent file size)");
        append(L"Free disk: before ");fmt_size(beforeDisk);append(L", after ");fmt_size(g_free);nl();
        append(L"Memory load: before ");num(beforeMem);append(L"%, after ");num(g_memload);line(L"% (naturally fluctuates; not a RAM boost)");
if(g_memload+4<=beforeMem)line(L"OUTCOME: lower observed RAM load, but this snapshot cannot attribute the change to the repair.");
else line(L"OUTCOME: NO MEASURABLE RAM RELIEF from safe fix. Focus on unused application sessions or the specific crash clusters above.");
if(g_crashes)line(L"UNRESOLVED: historical application crashes remain. SAFE FIX does not repair printer/audio drivers or third-party apps.");
        line(L"Deleted temp files cannot be restored. No system services, pagefile, or security settings were modified.");
    }
    if(g_mode==2){heading(L"TEMP CLEANUP ACTION");clean_user_temp();line(L"Rerun DIAGNOSE to refresh the verdict.");}
    if(g_mode==3)risky_services(0);
    if(g_mode==4)risky_services(1);
    save_report();
    PostMessageW(g_main,WM_APP+1,0,0);
    return 0;
}
static void start_worker(int mode){if(g_busy)return;g_mode=mode;g_busy=1;for(int i=0;i<8;i++)EnableWindow(g_buttons[i],FALSE);SetWindowTextW(g_status,mode==0?L"Scanning resource pressure, processes, startup items and 7-day event logs...":L"Scanning and running safe low-spec optimizations...");DWORD tid=0;HANDLE t=CreateThread(0,0,worker,0,0,&tid);if(t)CloseHandle(t);else{g_busy=0;for(int i=0;i<8;i++)EnableWindow(g_buttons[i],TRUE);SetWindowTextW(g_status,L"Could not create diagnostic worker thread.");}}
static void risk_start(void){
   if(g_busy)return;
   if(MessageBoxW(g_main,L"ADVANCED RISK MODE - HIGH RISK\r\n\r\nOnly optional Windows Search (WSearch) and SysMain startup configurations are eligible. Disabling them can break search, indexing, prefetch functionality or worsen performance. This does not improve every PC. Administrator rights required.\r\n\r\nYou can restore the saved startup types with RESTORE SERVICES. A failed write or external registry cleanup can still prevent rollback.\r\n\r\nContinue to the mandatory 3-second warning countdown?",L"Blaze Systems - Advanced Risk Mode",MB_YESNO|MB_ICONWARNING)!=IDYES)return;
   const WCHAR*steps[3]={L"HIGH-RISK WARNING - 3 seconds until final confirmation...",L"HIGH-RISK WARNING - 2 seconds until final confirmation...",L"HIGH-RISK WARNING - 1 second until final confirmation..."};
   for(int i=0;i<3;i++){SetWindowTextW(g_status,steps[i]);UpdateWindow(g_status);Sleep(1000);}
   if(MessageBoxW(g_main,L"3-second warning completed. Last confirmation: disable startup for WSearch and SysMain only? This may make search or loading slower. Save your work and ensure you can restore the services. No essential security/boot services will be disabled.",L"CONFIRM - High-risk change",MB_YESNO|MB_ICONWARNING)==IDYES)start_worker(3);
   else SetWindowTextW(g_status,L"Advanced changes canceled; nothing altered.");
}
static void layout(int w,int h){
    /* Phone/tablet conversions: two spacious columns down to 520px client width.
       Keep each label legible at 100-150% UI scaling and let Windows paint the text. */
    if(w<460)w=460;
    if(h<350)h=350;
    MoveWindow(g_heading,14,10,w-28,26,TRUE);
    MoveWindow(g_status,14,36,w-28,28,TRUE);
    int top=70;
    int gap=8;
    int cols=(w>=520)?2:1;
    int rows=(8+cols-1)/cols;
    int bw=(w-28-(cols-1)*gap)/cols;
    int bh=(w>=520)?35:32;
    for(int i=0;i<8;i++){
        int r=i/cols,c=i%cols;
        MoveWindow(g_buttons[i],14+c*(bw+gap),top+r*(bh+gap),bw,bh,TRUE);
    }
    int etop=top+rows*(bh+gap)+3;
    int eh=h-etop-14;
    if(eh<35)eh=35;
    MoveWindow(g_editor,14,etop,w-28,eh,TRUE);
}
static LRESULT WINAPI window_proc(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam){if(msg==WM_CREATE){g_heading=CreateWindowExW(0,L"STATIC",L"BLAZE ULTIMATE OPTIMIZER  |  BLAZE SYSTEMS  |  v1.2",WS_CHILD|WS_VISIBLE,15,12,700,22,hwnd,0,g_inst,0);g_status=CreateWindowExW(0,L"STATIC",L"Ready. Start with DIAGNOSE (read-only). No changes without permission.",WS_CHILD|WS_VISIBLE,15,38,700,24,hwnd,0,g_inst,0);
LPCWSTR labels[8]={L"1-CLICK DIAGNOSE",L"DIAGNOSE + SAFE FIX",L"OLD TEMP CLEANUP",L"RESTORE PREFERENCES",L"OPEN REPORT FILE",L"STARTUP APPS",L"ADVANCED RISK MODE",L"RESTORE SERVICES"};for(int i=0;i<8;i++){g_buttons[i]=CreateWindowExW(0,L"BUTTON",labels[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,0,0,150,32,hwnd,(HMENU)(ULONG_PTR)(ID_DIAGNOSE+i),g_inst,0);SendMessageW(g_buttons[i],WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),1);}g_editor=CreateWindowExW(0,L"EDIT",L"BLAZE SYSTEMS | Loumer Cabaluna Manreal\r\nUltra-low-spec Windows diagnostics + opt-in controls\r\n\r\nPress 1-CLICK DIAGNOSE for a read-only assessment, or DIAGNOSE + SAFE FIX to also apply reversible UI tweaks and remove >7-day-old top-level user-temp files.\r\n\r\nNothing is sent online. Reports are kept on this PC.\r\n\r\nNo optimizer can make 1 GB behave exactly like 8 GB; this app aims to reduce avoidable load and pinpoint actionable problems.",WS_CHILD|WS_VISIBLE|WS_BORDER|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|ES_NOHIDESEL,15,147,690,360,hwnd,0,g_inst,0);SendMessageW(g_editor,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),1);SendMessageW(g_status,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),1);SendMessageW(g_heading,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),1);return 0;}
if(msg==WM_SIZE){layout((int)(short)(lparam&0xFFFF),(int)(short)((lparam>>16)&0xFFFF));return 0;}
if(msg==WM_COMMAND){int id=(int)(wparam&0xFFFF);if(g_busy)return 0;if(id==ID_DIAGNOSE)start_worker(0);else if(id==ID_FIX){if(MessageBoxW(hwnd,L"Run diagnosis, disable optional UI animation/transparency, and delete ONLY top-level files in your personal TEMP directory that have not changed in over 7 days?\r\n\r\nRegistry visual preferences will be backed up. TEMP deletion is irreversible. Core services, Defender, and pagefile are NOT touched.",L"Confirm safe optimization",MB_YESNO|MB_ICONWARNING)==IDYES)start_worker(1);}else if(id==ID_CLEAN){if(MessageBoxW(hwnd,L"Delete only top-level files older than 7 days from your personal LOCALAPPDATA\\Temp folder? This deletion cannot be undone.",L"Confirm cleanup",MB_YESNO|MB_ICONWARNING)==IDYES)start_worker(2);}else if(id==ID_REPORT){if(g_reportPath[0])ShellExecuteW(hwnd,L"open",g_reportPath,0,0,SW_SHOWNORMAL);else MessageBoxW(hwnd,L"Run a scan first; your local report will be saved automatically.",L"Report",MB_OK);}else if(id==ID_STARTUP)ShellExecuteW(hwnd,L"open",L"ms-settings:startupapps",0,0,SW_SHOWNORMAL);else if(id==ID_RESTORE)visual_restore();else if(id==ID_ADVANCED)risk_start();else if(id==ID_ROLLBACK){if(MessageBoxW(hwnd,L"Restore recorded startup types for WSearch and SysMain? Requires administrator rights. Changes may need a restart.",L"Restore advanced settings",MB_YESNO|MB_ICONWARNING)==IDYES)start_worker(4);}return 0;}
if(msg==WM_APP+1){g_busy=0;for(int i=0;i<8;i++)EnableWindow(g_buttons[i],TRUE);SetWindowTextW(g_editor,g_report);SetWindowTextW(g_status,L"Analysis complete. A UTF-16 report was saved locally; OPEN REPORT FILE to view/copy it.");return 0;}
if(msg==WM_CTLCOLOREDIT){SetBkMode((HANDLE)wparam,1);SetTextColor((HANDLE)wparam,0x00FFFFFF);return (LRESULT)g_panelBrush;}
if(msg==WM_CTLCOLORSTATIC){SetBkMode((HANDLE)wparam,TRANSPARENT);SetTextColor((HANDLE)wparam,0x00FFFFFF);return (LRESULT)((HWND)lparam==g_editor?g_panelBrush:g_darkBrush);}
if(msg==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcW(hwnd,msg,wparam,lparam);}
void mainCRTStartup(void){g_inst=GetModuleHandleW(NULL);g_darkBrush=CreateSolidBrush(0x00200E05);g_panelBrush=CreateSolidBrush(0x002D180C);WNDCLASSEXW cls;/* no C runtime: initialize field by field */
cls.cbSize=sizeof(cls);cls.style=0;cls.lpfnWndProc=window_proc;cls.cbClsExtra=0;cls.cbWndExtra=0;cls.hInstance=g_inst;cls.hIcon=LoadIconW(g_inst,(LPCWSTR)(ULONG_PTR)ICON_RESOURCE);cls.hCursor=LoadCursorW(0,IDC_ARROW);cls.hbrBackground=g_darkBrush;cls.lpszMenuName=0;cls.lpszClassName=L"BlazeUltimateOptimizerWindowV1";cls.hIconSm=cls.hIcon;
if(!RegisterClassExW(&cls)){MessageBoxW(0,L"Failed to initialize window class.",L"Blaze Ultimate Optimizer",MB_OK);ExitProcess(2);}g_main=CreateWindowExW(0,cls.lpszClassName,L"Blaze Ultimate Optimizer 1.2 Preview  |  Blaze Systems",WS_OVERLAPPEDWINDOW|WS_VISIBLE,80,60,835,630,0,0,g_inst,0);if(!g_main){MessageBoxW(0,L"Failed to create main window.",L"Blaze Ultimate Optimizer",MB_OK);ExitProcess(3);}ShowWindow(g_main,SW_SHOWNORMAL);UpdateWindow(g_main);MSG m;while(GetMessageW(&m,0,0,0)>0){TranslateMessage(&m);DispatchMessageW(&m);}ExitProcess(0);}