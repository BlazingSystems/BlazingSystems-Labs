import re, pathlib, subprocess, sys
p=pathlib.Path(__file__).parent
arch='arm64' if '--arm64' in sys.argv else 'x64'
target='aarch64-windows-msvc' if arch=='arm64' else 'x86_64-windows-msvc'
subprocess.run([sys.executable,str(p/'resources.py')],check=True)
s=(p/'BlazeUltimateOptimizer.c').read_text()
all_functions=re.findall(r'^DLL\s+[^\n]+?\s+WINAPI\s+(\w+)\s*\(',s,re.M)
libs={"kernel32":[],"user32":[],"gdi32":[],"advapi32":[],"shell32":[]}
user=set('DrawTextW FillRect LoadIconW MessageBoxW PostMessageW SendMessageW DefWindowProcW LoadCursorW RegisterClassExW CreateWindowExW ShowWindow UpdateWindow SetWindowTextW MoveWindow EnableWindow DestroyWindow PostQuitMessage GetMessageW TranslateMessage DispatchMessageW SystemParametersInfoW'.split())
gdi=set('GetStockObject CreateSolidBrush SetTextColor SetBkMode DeleteObject'.split())
adv=set('OpenSCManagerW OpenServiceW QueryServiceConfigW ChangeServiceConfigW CloseServiceHandle RegOpenKeyExW RegCreateKeyExW RegQueryValueExW RegSetValueExW RegDeleteValueW RegEnumValueW RegCloseKey OpenEventLogW ReadEventLogW CloseEventLog'.split())
shell={'ShellExecuteW'}
for a in all_functions:
    x='user32' if a in user else 'gdi32' if a in gdi else 'advapi32' if a in adv else 'shell32' if a in shell else 'kernel32'
    libs[x].append(a)
for lib,names in libs.items():
    (p/(lib+'.def')).write_text('LIBRARY '+lib+'.dll\nEXPORTS\n'+''.join(' '+name+'\n' for name in sorted(set(names))))
    subprocess.run(['lld-link','/dll','/noentry','/machine:'+arch,'/def:'+str(p/(lib+'.def')),'/out:'+str(p/(lib+'.dll')),'/implib:'+str(p/(lib+'.lib'))],check=True,stdout=subprocess.PIPE)
subprocess.run(['clang','--target='+target,'-ffreestanding','-fno-stack-protector','-fno-builtin','-O2','-Wall','-Wextra','-Wno-unused-function','-c',str(p/'BlazeUltimateOptimizer.c'),'-o',str(p/'BlazeUltimateOptimizer.obj')],check=True)
subprocess.run(['lld-link','/subsystem:windows','/entry:mainCRTStartup','/machine:'+arch,'/opt:ref','/opt:icf','/out:'+str(p/('BlazeUltimateOptimizer-'+arch+'.exe')),str(p/'BlazeUltimateOptimizer.obj'),str(p/'BlazeUltimateOptimizer.res')]+[str(p/(l+'.lib')) for l in libs],check=True)
print('Built',p/('BlazeUltimateOptimizer-'+arch+'.exe'))