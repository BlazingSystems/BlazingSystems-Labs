from pathlib import Path
import sys
p=Path(sys.argv[1])
src=p/'BlazeUltimateOptimizer.c'
s=src.read_text(encoding='utf-8')
assert 'WS_CHILD|WS_VISIBLE|BS_OWNERDRAW' in s, 'Expected original v1.1 source'
s=s.replace('#define BS_OWNERDRAW 0x0000000B','#define BS_PUSHBUTTON 0x00000000\n#define WS_TABSTOP 0x00010000')
s=s.replace('WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,0,0,150,32','WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,0,0,150,32')
a=s.index('if(msg==WM_DRAWITEM){DRAWITEM *d=')
b=s.index('if(msg==WM_CTLCOLOREDIT)',a)
s=s[:a]+s[b:]
s=s.replace('L"BLAZE ULTIMATE OPTIMIZER  |  BLAZE SYSTEMS  |  v1.1 PREVIEW"','L"BLAZE ULTIMATE OPTIMIZER  |  BLAZE SYSTEMS  |  v1.1.1"')
s=s.replace('L"Blaze Ultimate Optimizer 1.1 Preview  |  Blaze Systems"','L"Blaze Ultimate Optimizer 1.1.1 Preview  |  Blaze Systems"')
a=s.index('static void layout(int w,int h){')
b=s.index('static LRESULT WINAPI window_proc',a)
s=s[:a]+'''static void layout(int w,int h){
    if(w<460)w=460;
    if(h<350)h=350;
    MoveWindow(g_heading,14,10,w-28,26,TRUE);
    MoveWindow(g_status,14,36,w-28,28,TRUE);
    int top=70,gap=8,cols=(w>=520)?2:1;
    int rows=(8+cols-1)/cols;
    int bw=(w-28-(cols-1)*gap)/cols;
    int bh=(w>=520)?35:32;
    for(int i=0;i<8;i++){
        int r=i/cols,c=i%cols;
        MoveWindow(g_buttons[i],14+c*(bw+gap),top+r*(bh+gap),bw,bh,TRUE);
    }
    int etop=top+rows*(bh+gap)+3,eh=h-etop-14;
    if(eh<35)eh=35;
    MoveWindow(g_editor,14,etop,w-28,eh,TRUE);
}
''' + s[b:]
s=s.replace('#define WM_DRAWITEM 0x002B\n','')
s=s.replace('DLL int WINAPI DrawTextW(HANDLE,LPCWSTR,int,void*,UINT);\n','')
s=s.replace('DLL int WINAPI FillRect(HANDLE,const void*,HBRUSH);\n','')
s=s.replace('typedef struct {LONG l,t,r,b;} BRECT;\n','')
s=s.replace('typedef struct {UINT CtlType,CtlID;HWND hwndItem;HANDLE hDC;UINT itemAction,itemState;HWND hwndOwner; BRECT rcItem; ULONG_PTR itemData;} DRAWITEM;\n','')
src.write_text(s,encoding='utf-8')
r=p/'resources.py'
v=r.read_text(encoding='utf-8')
for key in ('FileVersion','ProductVersion'):
    assert f"('{key}','1.1.0.0')" in v
    v=v.replace(f"('{key}','1.1.0.0')",f"('{key}','1.1.1.0')")
r.write_text(v,encoding='utf-8')
verify=p/'verify.py'
v=verify.read_text(encoding='utf-8')
v+='''
assert 'BS_OWNERDRAW' not in s and 'WM_DRAWITEM' not in s
assert 'WS_TABSTOP|BS_PUSHBUTTON' in s
assert 'cols=(w>=520)?2:1' in s
assert '1.1.1.0' in (p/'resources.py').read_text()
print('Native label visibility / keyboard hotfix: PASS (static; real Windows tests required)')
'''
verify.write_text(v,encoding='utf-8')
(p/'HOTFIX-README.md').write_text('''# v1.1.1 Preview — Button Visibility Hotfix

- Uses native Windows push buttons instead of custom owner-drawn controls to restore readable button captions in Windows themes and contrast modes.
- Adds keyboard Tab navigation and two wider columns of buttons on common laptop/tablet screens.
- Keeps approved Blaze Systems Logo V2 icon and Loumer Cabaluna Manreal publisher metadata.
- Retains v1.1 memory/fault diagnostics and optional advanced mode with 3-second warning.
- No background telemetry. No optimizer/risk settings changed by this hotfix.

**Status: compiled preview.** Actual Windows x64 and ARM64 runtime button and service rollback testing is still required. The EXE is not Authenticode-signed, so Windows may display an Unknown Publisher warning.

Windows x64 and Windows ARM64 have separate binaries. Linux Orange Pi cannot run either natively.
''',encoding='utf-8')
print('Applied native-button hotfix to',p)
