"""Import the NPC Trinity mesh (B13C_body1) into Beck's Spw09_Body1.

Source: Map15_COLO12.upk :: B13C_body1 (82-bone humanoid, 4 weights/vertex).
Target: MN9Game.upk :: Spw09_Body1 (68-bone player rig, 5120 tri / 4040 vert).

Both rigs share MN9's bone naming (M_Center/M_Spine/L_Arm/...), so vertices are
re-based bone by bone: local offset in Trinity's bind pose -> the same-named
bone in Beck's bind pose.  Trinity-only bones (hair, skirt) ride their parent,
and influences are folded onto the 20 bones the target chunk can address.
"""
import sys, os, struct, math
import numpy as np

TOOLS = '/var/home/user/Projects/mn9-patcher/analysis/tools'
sys.path.insert(0, TOOLS)
import ue3_defaults as U
import skmesh as S
import lod
import patch_boss_mesh as PB
from import_megaman import read_bones, collapse

COOKED = '/var/home/user/.local/share/Steam/steamapps/common/Mighty No. 9/MN9Game/CookedLinux/'
SRC_PKG = COOKED + 'Map15_COLO12.upk'
TGT_PKG = COOKED + 'MN9Game.upk'
SRC_MESH = 'B13C_body1'
MESH = 'Spw09_Body1'

EXTRA = {}
for side in ('L', 'R'):
    for i in (1, 2, 3):
        EXTRA[f'{side}_hair{i}'] = 'M_Head'
    for band in ('F', 'S', 'B'):
        for i in (1, 2, 3):
            EXTRA[f'{side}_{band}Skirt{i}'] = 'M_Hip'


def chunk_maps(p, e):
    d = lod.parse(p, e, verbose=False)
    rem, q = d['rem'], d['next']
    nab = struct.unpack_from('<i', rem, q)[0]; q += 4 + 2 * nab
    nch = struct.unpack_from('<i', rem, q)[0]; q += 4
    maps, info = [], []
    for _ in range(nch):
        q += 4
        nr = struct.unpack_from('<i', rem, q)[0]; q += 4 + nr * 12
        ns = struct.unpack_from('<i', rem, q)[0]; q += 4 + ns * 32
        nb = struct.unpack_from('<i', rem, q)[0]; q += 4
        map_off = q
        maps.append(struct.unpack_from('<%dH' % nb, rem, q))
        q += 2 * nb + 12
        info.append((nb, map_off, q - 12))
    return d, maps, info



def grid_merge(P, F, part, max_verts):
    """Cluster vertices on a grid (per bone part) until the count fits."""
    lo = P.min(0); hi = P.max(0); span = float(np.max(hi - lo)) or 1.0
    cell = span / 400.0
    for _ in range(80):
        keys = {}
        idx = np.zeros(len(P), dtype=np.int64)
        for i, p in enumerate(P):
            k = (int(part[i]), int((p[0]-lo[0])//cell), int((p[1]-lo[1])//cell), int((p[2]-lo[2])//cell))
            if k not in keys:
                keys[k] = len(keys)
            idx[i] = keys[k]
        if len(keys) <= max_verts:
            break
        cell *= 1.05
    n = len(keys)
    acc = np.zeros((n, 3)); cnt = np.zeros(n)
    for i, p in enumerate(P):
        acc[idx[i]] += p; cnt[idx[i]] += 1
    Q = acc / np.maximum(cnt, 1)[:, None]
    F2 = idx[F]
    keep = (F2[:, 0] != F2[:, 1]) & (F2[:, 1] != F2[:, 2]) & (F2[:, 0] != F2[:, 2])
    F2 = F2[keep]
    used = np.unique(F2)
    remap = -np.ones(n, dtype=np.int64); remap[used] = np.arange(len(used))
    return Q[used], remap[F2]


def main():
    out_path = sys.argv[1]
    sp = U.Package(SRC_PKG)
    se = S.find_export(sp, SRC_MESH)
    sd, smaps, _ = chunk_maps(sp, se)
    ext = lod.extract(sp, se)
    sverts, sidx, ssecs = ext['vertices'], np.array(ext['indices'], dtype=np.int64), ext['sections']

    tp = U.Package(TGT_PKG)
    te = S.find_export(tp, MESH)
    td = lod.parse(tp, te, verbose=False)
    tent, tgt = read_bones(tp, te)
    _, src = read_bones(sp, se)
    sname = [b['name'] for b in sd['bones']]

    # bone map: same name, else parent fallback
    bmap = {}
    for i, nm in enumerate(sname):
        if nm in tgt:
            bmap[i] = nm
        elif nm in EXTRA:
            bmap[i] = EXTRA[nm]
        else:
            bmap[i] = None

    def dist(m, a, b):
        return np.linalg.norm(m[a]['M'][:3, 3] - m[b]['M'][:3, 3])
    scale = dist(tgt, 'M_Head', 'M_Hip') / (dist(src, 'M_Head', 'M_Hip') or 1.0)
    print('scale %.4f' % scale)
    print('unmapped bones:', sorted({n for i, n in enumerate(sname) if bmap[i] is None}))

    # chunk per vertex (from section ranges)
    vchunk = np.full(len(sverts), -1, dtype=np.int64)
    for si, sec in enumerate(ssecs):
        a, n = sec['first'], sec['tris'] * 3
        vchunk[sidx[a:a + n]] = si

    P = np.array([v['p'] for v in sverts], dtype=np.float64)
    UV = np.array([(v['uv'][0] / 65535.0, 1.0 - v['uv'][1] / 65535.0) for v in sverts])
    F = np.array(sidx, dtype=np.int64).reshape(-1, 3)

    # per-vertex influences in Trinity's bone ids
    infl = []
    for vi, v in enumerate(sverts):
        cm = smaps[vchunk[vi]] if 0 <= vchunk[vi] < len(smaps) else ()
        acc = {}
        for bi, w in zip(v['bones'], v['weights']):
            if not w: continue
            g = cm[bi] if bi < len(cm) else -1
            tn = bmap.get(g)
            if not tn: continue
            acc[tn] = acc.get(tn, 0.0) + w
        infl.append(acc)

    # total weight per target bone -> keep what the chunk map can address
    nb_slots = 20
    total = {}
    for acc in infl:
        for nm, w in acc.items():
            total[nm] = total.get(nm, 0.0) + w
    keep = [nm for nm, _ in sorted(total.items(), key=lambda kv: -kv[1])[:nb_slots]]
    keeps = set(keep)

    parent = {}
    for i, b in enumerate(tent):
        parent[b['name']] = tent[b['parent']]['name'] if b['parent'] != i else None

    def fold(nm):
        seen = 0
        while nm and nm not in keeps and seen < 32:
            nm = parent.get(nm)
            seen += 1
        return nm or 'M_Hip'

    P2 = np.zeros_like(P)
    BONE = np.zeros((len(P), 4), dtype=np.int64)
    WT = np.zeros((len(P), 4), dtype=np.float64)
    for vi, v in enumerate(sverts):
        cm = smaps[vchunk[vi]] if 0 <= vchunk[vi] < len(smaps) else ()
        acc = {}
        for nm, w in infl[vi].items():
            fn = fold(nm)
            acc[fn] = acc.get(fn, 0.0) + w
        top = sorted(acc.items(), key=lambda kv: -kv[1])[:4]
        tot = sum(w for _, w in top) or 1.0
        p = np.zeros(3)
        for k, (tn, w) in enumerate(top):
            # source bone with the largest weight that folds to tn
            best, bw = None, -1
            for bi, sw in zip(v['bones'], v['weights']):
                if not sw: continue
                g = cm[bi] if bi < len(cm) else -1
                sn = sname[g] if 0 <= g < len(sname) else 'M_Hip'
                if fold(bmap.get(g) or 'M_Hip') == tn and sw > bw:
                    bw, best = sw, sn
            if best is None: best = 'M_Hip'
            if best not in src: best = 'M_Hip'
            if tn not in tgt: tn = 'M_Hip'
            loc = np.linalg.inv(src[best]['M']) @ np.append(P[vi], 1.0)
            loc[:3] *= scale
            p += (w / tot) * (tgt[tn]['M'] @ loc)[:3]
            BONE[vi, k] = tgt[tn]['index']
            WT[vi, k] = w / tot
        P2[vi] = p
    print('bbox dst', P2.min(0).round(1), P2.max(0).round(1))

    # target budgets
    rem = td['rem']; q = td['next']
    nab = struct.unpack_from('<i', rem, q)[0]; q += 4 + 2 * nab
    nch = struct.unpack_from('<i', rem, q)[0]; q += 4
    chinfo = []
    for _ in range(nch):
        q += 4
        nr = struct.unpack_from('<i', rem, q)[0]; q += 4 + nr * 12
        ns = struct.unpack_from('<i', rem, q)[0]; q += 4 + ns * 32
        nb = struct.unpack_from('<i', rem, q)[0]; q += 4
        map_off = q; q += 2 * nb
        nri_off = q; q += 12
        chinfo.append(dict(map_off=map_off, nri_off=nri_off, nb=nb))
    _sz, numverts = struct.unpack_from('<2i', rem, q)
    stride, voff = lod.find_vertex_buffer(rem, td['next'], numverts)
    tris_budget = len(td['indices']) // 3
    print('target verts', numverts, 'tris', tris_budget, 'stride', stride)
    assert stride == 32

    P3, F3, _ = collapse(P2, F, BONE[:, 0].copy(), tris_budget, numverts)
    if len(P3) > numverts:
        P3, F3 = grid_merge(P3, F3, np.zeros(len(P3), dtype=np.int64), numverts)
        print('grid merged verts', len(P3), 'tris', len(F3))
    uv3 = np.zeros((len(P3), 2)); B3 = np.zeros((len(P3), 4), dtype=np.int64)
    W3 = np.zeros((len(P3), 4))
    for i, p in enumerate(P3):
        j = int(np.argmin(((P2 - p) ** 2).sum(1)))
        uv3[i] = UV[j]; B3[i] = BONE[j]; W3[i] = WT[j]
    print('after collapse verts', len(P3), 'tris', len(F3))

    used = []
    for row, wrow in zip(B3, W3):
        for b, w in zip(row, wrow):
            if w > 0 and int(b) not in used:
                used.append(int(b))
    slot = {b: i for i, b in enumerate(used)}
    print('bones used', len(used), used)
    assert len(used) <= chinfo[0]['nb'], (len(used), chinfo[0]['nb'])

    bones, at, after = S.read_skeleton(tp, te)
    logical = bytearray(tp.buf)
    vbase = after + voff
    c0 = chinfo[0]
    q = td['next']
    nab2 = struct.unpack_from('<i', rem, q)[0]; q += 4
    for i in range(nab2):
        struct.pack_into('<H', logical, after + q + i * 2, used[i % len(used)])
    for i in range(c0['nb']):
        struct.pack_into('<H', logical, after + c0['map_off'] + i * 2,
                         used[i] if i < len(used) else used[0])

    fn = np.cross(P3[F3[:, 1]] - P3[F3[:, 0]], P3[F3[:, 2]] - P3[F3[:, 0]])
    N = np.zeros_like(P3)
    for k in range(3):
        np.add.at(N, F3[:, k], fn)
    ln = np.linalg.norm(N, axis=1); ln[ln == 0] = 1; N /= ln[:, None]
    Nb = np.round((np.clip(N, -1, 1) * 0.5 + 0.5) * 255).astype(np.uint8)

    for k in range(numverts):
        o = vbase + k * stride
        j = k if k < len(P3) else len(P3) - 1
        p = P3[j]; u, v = uv3[j]
        logical[o:o+4] = bytes(Nb[j]) + b'\x80'
        logical[o+4:o+8] = bytes([255, 0, 0, 255])
        logical[o+8:o+12] = bytes([slot.get(int(B3[j, 0]), 0), slot.get(int(B3[j, 1]), 0),
                                   slot.get(int(B3[j, 2]), 0), slot.get(int(B3[j, 3]), 0)])
        wq = np.clip(np.round(W3[j] * 255), 0, 255).astype(int)
        logical[o+12:o+16] = bytes([int(x) for x in wq])
        struct.pack_into('<3f', logical, o + 16, float(p[0]), float(p[1]), float(p[2]))
        struct.pack_into('<2e', logical, o + 28, float(u), 0.5 * (1.0 - float(v)))

    iabs = after + td['next'] - len(td['indices']) * 2
    for t in range(len(td['indices']) // 3):
        a, b, c = F3[t] if t < len(F3) else (0, 0, 0)
        struct.pack_into('<3H', logical, iabs + t * 6, int(a), int(b), int(c))
    for si in range(len(td['sections'])):
        cnt = len(F3) if si == 0 else 0
        struct.pack_into('<iiiB', logical, after + 12 + si * 13, si << 16, 0, cnt, 0)
    for ci, c in enumerate(chinfo):
        if ci == 0:
            struct.pack_into('<2i', logical, after + c['nri_off'], numverts, 0)
            struct.pack_into('<i', logical, after + c['nri_off'] + 8, 1)
        else:
            struct.pack_into('<2i', logical, after + c['nri_off'], 0, 0)
            struct.pack_into('<i', logical, after + c['nri_off'] + 8, 1)

    print('rebuild', PB.rebuild(tp, logical, out_path), 'verts', len(P3), 'tris', len(F3))


if __name__ == '__main__':
    main()
