"""Offline updater boundary checks. No USB, physical target, bond or wallet access."""
import contextlib, hashlib, io, json, os, runpy, sys, tempfile, types
from pathlib import Path
from intelhex import IntelHex

root = Path(__file__).resolve().parents[1]
class Target:
    def __init__(self):
        self.memory = bytearray([0x5a]) * 0x30000
        self.halted = False
        self.programs = 0
    def get_memory_map(self): return self
    def get_region_for_address(self, address):
        assert address == 0
        return types.SimpleNamespace(blocksize=4096)
    def halt(self): self.halted = True
    def reset(self): self.halted = False
    def read_memory_block8(self, address, size):
        assert self.halted and 0 < size <= 128
        assert 0 <= address < address + size <= 0x30000
        return self.memory[address:address + size]

class Session:
    def __init__(self, target): self.target = target
    def __enter__(self): return self
    def __exit__(self, *_): pass

class Programmer:
    def __init__(self, session, **options):
        assert options == dict(chip_erase='sector', smart_flash=False, trust_crc=False, keep_unwritten=False)
        self.target = session.target
    def program(self, image, file_format):
        data = IntelHex(image)
        assert file_format == 'hex' and len(data.segments()) == 1
        start, end = data.segments()[0]
        assert start == 0 and end % 4096 == 0 and end <= 0x30000
        assert self.target.halted
        self.target.memory[:end] = bytes(data.tobinarray(start=0, end=end-1))
        self.target.programs += 1

def execute(args, target):
    modules = dict(sys.modules)
    argv = sys.argv[:]
    mask = os.umask(0o077)
    try:
        for name in ('pyocd','pyocd.core','pyocd.core.helpers','pyocd.flash','pyocd.flash.file_programmer'):
            sys.modules[name] = types.ModuleType(name)
        def connect(**options):
            assert options['options']['auto_unlock'] is False
            assert options['options']['cmsis_dap.limit_packets'] is True
            return Session(target)
        sys.modules['pyocd.core.helpers'].ConnectHelper = types.SimpleNamespace(session_with_chosen_probe=connect)
        sys.modules['pyocd.flash.file_programmer'].FileProgrammer = Programmer
        sys.argv = ['install-nu54-preview.py', *args]
        with contextlib.redirect_stdout(io.StringIO()):
            runpy.run_path(str(root/'tools/install-nu54-preview.py'), run_name='__main__')
    finally:
        os.umask(mask)
        sys.argv = argv
        for key in list(sys.modules):
            if key not in modules and key.startswith('pyocd'): del sys.modules[key]
        for key,value in modules.items():
            if key.startswith('pyocd'): sys.modules[key] = value

with tempfile.TemporaryDirectory() as folder:
    folder = Path(folder)
    old = IntelHex(); old.frombytes(bytes(range(128)),offset=0); old.write_hex_file(str(folder/'old.hex'))
    new = IntelHex(); new.frombytes(b'NEW TENNIS CODE!',offset=0); new.frombytes(b'END',offset=64); new.write_hex_file(str(folder/'new.hex'))
    digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    receipt = folder/'prior.json'; receipt.write_text(json.dumps({'writtenAndVerified':True,'imageSha256':digest(folder/'old.hex')}))
    args = ['--image',str(folder/'new.hex'),'--expected-sha256',digest(folder/'new.hex'),'--execute','--previous-image',str(folder/'old.hex'),'--previous-receipt',str(receipt)]
    target = Target(); target.memory[:128] = bytes(range(128)); before = bytes(target.memory)
    execute([*args,'--backup',str(folder/'success')],target)
    assert target.programs == 1 and not target.halted
    addresses=set(new.addresses())
    for i in range(0x30000):
        assert target.memory[i] == (new[i] if i in addresses else before[i]), i
    assert json.loads((folder/'success/result.json').read_text())['writtenAndVerified']
    target = Target()
    try: execute([*args,'--backup',str(folder/'mismatch')],target)
    except SystemExit as e: assert 'differs from previous Tennis image' in str(e)
    else: raise AssertionError('Changed code was accepted')
    assert target.programs == 0
    bad = IntelHex(); bad[0x30000] = 1; bad.write_hex_file(str(folder/'bad.hex'))
    try: execute(['--image',str(folder/'bad.hex'),'--expected-sha256',digest(folder/'bad.hex')],Target())
    except SystemExit as e: assert 'Only reviewed code range' in str(e)
    else: raise AssertionError('Forbidden region accepted')
print('PASS: bounded halted reads, complete-sector preservation, no auto-unlock, stale-image rejection, forbidden-region rejection; offline target only')
