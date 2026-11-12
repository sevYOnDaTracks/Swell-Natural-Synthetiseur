#!/usr/bin/env python3
"""Parse Kontakt 5 .nki files of the PSR-S900 library into a Swell bank index (JSON).
Usage: nki_to_bank.py <PSR library root> <out bank.json>"""
import struct, re, sys, os, json, glob

def fastlz(src):
    level = (src[0] >> 5) + 1
    ip = 0; op = bytearray(); ctrl = src[ip] & 31; ip += 1
    while True:
        if ctrl >= 32:
            ln = (ctrl >> 5) - 1; ofs = (ctrl & 31) << 8
            ref = len(op) - ofs - 1
            if level == 1:
                if ln == 6: ln += src[ip]; ip += 1
                ref -= src[ip]; ip += 1
            else:
                if ln == 6:
                    while True:
                        c = src[ip]; ip += 1; ln += c
                        if c != 255: break
                c = src[ip]; ip += 1; ref -= c
                if c == 255 and ofs == (31 << 8):
                    ofs = (src[ip] << 8) | src[ip + 1]; ip += 2
                    ref = len(op) - ofs - 8191 - 1
            for k in range(ln + 3): op.append(op[ref + k])
        else:
            ctrl += 1; op += src[ip:ip + ctrl]; ip += ctrl
        if ip >= len(src): break
        ctrl = src[ip]; ip += 1
    return bytes(op)

def payload(d):
    # compressed patch: ... 01 | u32 usz | u32 csz | stream ; find a candidate that inflates to usz
    for m in re.finditer(rb'(?s)\x01(?=(....)(....))', d):
        usz, csz = struct.unpack('<II', m.group(1) + m.group(2))
        s = m.start() + 9
        if 1000 < usz < 10_000_000 and 100 < csz < len(d) and s + csz <= len(d) and csz < usz:
            try:
                out = fastlz(d[s:s + csz])
                if len(out) == usz: return out
            except Exception: pass
    return None

def ncw_info(path):
    h = open(path, 'rb').read(32)
    ch, bits, sr, n = struct.unpack('<HHII', h[8:20])
    return ch, bits, sr, n

def parse(nki, root):
    out = payload(open(nki, 'rb').read())
    if out is None: return None, 'no payload'
    strs = [(m.start(), m.group().decode('utf-16le')) for m in re.finditer(rb'(?:[\x20-\x7e]\x00){3,}', out)]
    files = []
    i = 0
    while i < len(strs):
        s = strs[i][1]
        if s.lower().endswith('.ncw') and not any('Wallpapers' in x[1] for x in strs[max(0,i-3):i]):
            files.append((strs[i-2][1] if i >= 2 else '', strs[i-1][1] if i >= 1 else '', s))
        i += 1
    zones = []
    for p in range(48, len(out) - 60):
        if out[p - 1] != 2: continue
        sr = struct.unpack_from('<I', out, p - 5)[0]
        if sr not in (44100, 48000, 22050, 32000, 96000): continue
        n = struct.unpack_from('<I', out, p)[0]
        if n < 16: continue
        lv, hv, lk, hk = struct.unpack_from('<HHHH', out, p - 43)
        rk = struct.unpack_from('<I', out, p - 27)[0]
        mode, ls, ll = struct.unpack_from('<Iii', out, p + 35)
        start = struct.unpack_from('<i', out, p - 55)[0]
        if not (lv <= hv <= 127 and lk <= hk <= 127 and rk <= 127): continue
        zones.append(dict(lv=lv, hv=hv, lk=lk, hk=hk, root=rk, n=n, sr=sr, mode=mode, ls=ls, ll=ll, start=start, p=p))
    MARK = b'\x01\x98\x00\x53'
    for i, z in enumerate(zones):
        lo = zones[i - 1]['p'] if i > 0 else max(0, z['p'] - 400)
        j = out.rfind(MARK, lo, z['p'])
        z['group'] = struct.unpack_from('<I', out, j - 4)[0] if j >= 4 else 0
    return (zones, files), None

PAT = re.compile(r'^(.*)-(\d+)-(\d+)(?:_\d+)?\.ncw$')

def classify(sound_id, Z):
    """Role per zone: main | layer | slide | mw | fx.
    Kontakt S.Art!/MegaVoice instruments stack script-triggered groups (noises, slides,
    mod-wheel articulations) that must not play as plain notes."""
    is_kit = 'Kit' in sound_id
    slide_inst = 'Slide' in sound_id
    for z in Z:
        fn = os.path.basename(z['src']); folder = os.path.basename(os.path.dirname(z['src']))
        m = PAT.match(fn)
        prefix = m.group(1) if m else ''
        if is_kit:
            role = 'main'
        elif folder == 'Slide' or (not slide_inst and 'Slide' in prefix):
            role = 'slide'
        elif not m or folder == 'FX':
            role = 'fx'
        elif int(m.group(2)) != z['root']:
            role = 'layer' if ('Kit' in folder or 'Timpani' in folder) else 'fx'
        elif 'mw up' in prefix:
            role = 'mw'
        else:
            role = 'main'
        z['role'] = role; z['_prefix'] = prefix
    mains = [z for z in Z if z['role'] == 'main']
    if mains and not is_kit:
        # primary prefix = most common among the lowest group's mains
        g0 = min(z['group'] for z in mains)
        counts = {}
        for z in mains:
            if z['group'] == g0: counts[z['_prefix']] = counts.get(z['_prefix'], 0) + 1
        primary = max(counts, key=counts.get)
        for z in mains:
            if z['_prefix'] != primary: z['role'] = 'layer'
    for z in Z: z.pop('_prefix', None)
    if not any(z['role'] == 'main' for z in Z):
        for z in Z:
            if z['role'] in ('fx', 'layer', 'mw'): z['role'] = 'main'

def main():
    root, outp = sys.argv[1], sys.argv[2]
    inst_dir = os.path.join(root, 'Instruments'); smp_dir = os.path.join(root, 'Samples')
    bank = {'name': 'Swell Library', 'version': 2, 'categories': []}
    # Per-sound octave corrections measured by SwellRenderTest (pitch detection on the samples)
    fix_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'octave_fix.json')
    octave_fix = json.load(open(fix_path)) if os.path.exists(fix_path) else {}
    tune_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'tune_fix.json')
    tune_fix = json.load(open(tune_path)) if os.path.exists(tune_path) else {}
    stats = {'ok': 0, 'loops': 0, 'fail': [], 'mismatch': 0}
    for cat in sorted(os.listdir(inst_dir)):
        cpath = os.path.join(inst_dir, cat)
        if not os.path.isdir(cpath): continue
        catname = re.sub(r'^\d+_', '', cat)
        C = {'name': catname, 'folder': cat, 'sounds': []}
        for nki in sorted(glob.glob(os.path.join(cpath, '*.nki'))):
            name = os.path.basename(nki)[:-4]
            res, err = parse(nki, root)
            if res is None: stats['fail'].append((name, err)); continue
            zones, files = res
            if not zones or len(zones) != len(files):
                stats['fail'].append((name, f'zones {len(zones)} files {len(files)}')); continue
            Z = []
            for z, (_, folder, fn) in zip(zones, files):
                rel = os.path.join(cat, folder, fn)
                full = os.path.join(smp_dir, rel)
                if not os.path.exists(full):
                    hits = glob.glob(os.path.join(smp_dir, '*', '*', glob.escape(fn)))
                    if not hits: stats['fail'].append((name, 'missing ' + fn)); Z = None; break
                    full = hits[0]; rel = os.path.relpath(full, smp_dir)
                ch, bits, sr, n = ncw_info(full)
                if n != z['n']: stats['mismatch'] += 1
                loop = None
                if z['mode'] != 0 and z['ll'] > 32 and 0 <= z['ls'] and z['ls'] + z['ll'] <= n:
                    loop = [z['ls'], z['ll']]; stats['loops'] += 1
                start = z['start'] if 0 <= z['start'] < n // 2 else 0
                if loop and start >= loop[0]: start = 0
                Z.append({'src': rel, 'lk': z['lk'], 'hk': z['hk'], 'lv': z['lv'], 'hv': z['hv'],
                          'root': z['root'], 'sr': sr, 'ch': ch, 'n': n, 'loop': loop,
                          'start': start, 'group': z['group']})
            if Z is None: continue
            classify(name, Z)
            for r in ('main', 'layer', 'slide', 'mw', 'fx'):
                stats[r] = stats.get(r, 0) + sum(1 for q in Z if q['role'] == r)
            stats['ok'] += 1
            snd = {'name': re.sub(r'^\d+_', '', name), 'id': name, 'zones': Z}
            if octave_fix.get(name): snd['octave'] = int(octave_fix[name])
            if tune_fix.get(name): snd['tune'] = float(tune_fix[name])
            C['sounds'].append(snd)
        bank['categories'].append(C)
    json.dump(bank, open(outp, 'w'), indent=1)
    print('roles', {r: stats.get(r, 0) for r in ('main', 'layer', 'slide', 'mw', 'fx')})
    print('ok', stats['ok'], 'loops', stats['loops'], 'mismatch', stats['mismatch'], 'fail', len(stats['fail']))
    for f in stats['fail'][:30]: print('  FAIL', f)

if __name__ == '__main__': main()
