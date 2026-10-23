from pathlib import Path
import struct,hashlib
p=Path(__file__).parent
exeNames=['BlazeUltimateOptimizer-x64.exe','BlazeUltimateOptimizer-arm64.exe']
s=(p/'BlazeUltimateOptimizer.c').read_text()
for name,machine in zip(exeNames,(0x8664,0xAA64)):
 data=(p/name).read_bytes()
 assert data[:2]==b'MZ',name
 pe=struct.unpack_from('<I',data,0x3C)[0]
 assert data[pe:pe+4]==b'PE\0\0'
 assert struct.unpack_from('<H',data,pe+4)[0]==machine
 assert b'.rsrc' in data[:4096] and b'advapi32.dll' in data
 for t in ['Loumer Cabaluna Manreal','CompanyName','Blaze Ultimate Optimizer']:
  assert t.encode('utf-16le') in data
 print(name,len(data),'bytes SHA-256',hashlib.sha256(data).hexdigest())
assert 'Sleep(1000)' in s and '3-second warning completed' in s
assert 'riskServices[2]={L"WSearch",L"SysMain"}' in s
assert 'ChangeServiceConfigW' in s and 'riskRegistry' in s
assert 'SERVICE_NO_CHANGE' in s
assert 'DeleteFileW(full)' in s and 'FILE_ATTRIBUTE_REPARSE_POINT' in s
assert 'group_process' in s and 'crash_insertions' in s
for banned in ['WinDefend', 'wuauserv', 'LanmanWorkstation', 'RpcSs', 'CryptSvc']:
 assert banned not in s
print('Static allowlist, branding, crash parser, and PE gates: PASS')

assert 'BS_OWNERDRAW' not in s and 'WM_DRAWITEM' not in s
assert 'WS_TABSTOP|BS_PUSHBUTTON' in s
assert 'int cols=(w>=520)?2:1' in s
assert '1.2.0.0' in (p/'resources.py').read_text()
print('Native label visibility/keyboard hotfix: PASS (static only; Windows runtime test pending)')

assert 'g_faults[i].exception' in s and 'TARGETED FAULT TRIAGE' in s
assert 'NO MEASURABLE RAM RELIEF' in s
assert 'provider[pn]=0' in s
print('v1.2 bounded crash parsing and honest safe-fix verdict: PASS (static; Windows test pending)')