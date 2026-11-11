"""Retarget a boss (Bs13) clip onto a player clip with world-space bind deltas."""
import sys, struct, math
sys.path.insert(0, '/var/home/user/Projects/mn9-patcher/analysis/tools')
import ue3_defaults as U
import skmesh as S
import patch_boss_mesh as PB
from import_megaman import read_bones

import os as _os
SRC = _os.environ.get('SRC_PKG','/var/home/user/.local/share/Steam/steamapps/common/Mighty No. 9/MN9Game/CookedLinux/Map15_COLO12.upk')  # SRC_ENV
TGT = __import__('os').environ.get('GEN_BASE','/tmp/opencode/MN9Game_trinity.upk')
TMESH = 'Spw09_Body1'


def qmul(a, b):
    ax, ay, az, aw = a; bx, by, bz, bw = b
    return (aw*bx+ax*bw+ay*bz-az*by, aw*by-ax*bz+ay*bw+az*bx,
            aw*bz+ax*by-ay*bx+az*bw, aw*bw-ax*bx-ay*by-az*bz)


def qinv(q): return (-q[0], -q[1], -q[2], q[3])


def qn(q):
    n = math.sqrt(sum(x*x for x in q))
    if n < 1e-9: return (0.0, 0.0, 0.0, 1.0)
    q = tuple(x/n for x in q)
    return tuple(-x for x in q) if q[3] < 0 else q


def mq(M):
    m = M[:3, :3]
    tr = m[0, 0]+m[1, 1]+m[2, 2]
    if tr > 0:
        s = math.sqrt(tr+1.0)*2; w = .25*s
        x = (m[2, 1]-m[1, 2])/s; y = (m[0, 2]-m[2, 0])/s; z = (m[1, 0]-m[0, 1])/s
    elif m[0, 0] > m[1, 1] and m[0, 0] > m[2, 2]:
        s = math.sqrt(1+m[0, 0]-m[1, 1]-m[2, 2])*2
        w = (m[2, 1]-m[1, 2])/s; x = .25*s; y = (m[0, 1]+m[1, 0])/s; z = (m[0, 2]+m[2, 0])/s
    elif m[1, 1] > m[2, 2]:
        s = math.sqrt(1+m[1, 1]-m[0, 0]-m[2, 2])*2
        w = (m[0, 2]-m[2, 0])/s; x = (m[0, 1]+m[1, 0])/s; y = .25*s; z = (m[1, 2]+m[2, 1])/s
    else:
        s = math.sqrt(1+m[2, 2]-m[0, 0]-m[1, 1])*2
        w = (m[1, 0]-m[0, 1])/s; x = (m[0, 2]+m[2, 0])/s; y = (m[1, 2]+m[2, 1])/s; z = .25*s
    return qn((x, y, z, w))


def find_seq(p, name):
    hits = []
    for e in p.exports:
        if p.class_name_of(e) != 'AnimSequence':
            continue
        try:
            pr = p.read_properties(e)
        except Exception:
            continue
        if pr.get('SequenceName', (0, ''))[1] == name:
            hits.append((e, pr))
    return hits


def keys(p, e, pr):
    b = p.buf
    offs = pr['CompressedTrackOffsets'][1]
    end = S.tags_end(p, e)
    base = end + 20
    out = []
    for t in range(len(offs)//6):
        ro, rn = offs[t*6], offs[t*6+1]
        ks = []
        for k in range(rn):
            x, y, z = struct.unpack_from('<3f', b, base+ro+k*12)
            w2 = 1-x*x-y*y-z*z
            ks.append((x, y, z, math.sqrt(w2) if w2 > 0 else 0.0))
        out.append(ks)
    return out, base, offs


def main():
    tname, sname, out = sys.argv[1], sys.argv[2], sys.argv[3]
    tp = U.Package(TGT); sp = U.Package(SRC)
    te, tpr = find_seq(tp, tname)[0]
    se, spr = find_seq(sp, sname)[0]
    tmesh = S.find_export(tp, TMESH)
    tent, tw = read_bones(tp, tmesh)
    tbind = [mq(tw[b['name']]['M']) for b in tent]
    tparent = [b['parent'] for b in tent]
    tnames = [b['name'] for b in tent]
    tindex = {n: i for i, n in enumerate(tnames)}

    sset = sp.exports[se.outer_index-1].name
    smesh_name = 'Bs13B_Head1' if 'Bs13B' in sset else ('Bs13A_body1' if 'Bs13A' in sset else 'B13C_body1')
    smesh = S.find_export(sp, smesh_name)
    sent, sw = read_bones(sp, smesh)
    sbind = [mq(sw[b['name']]['M']) for b in sent]
    sloc_bind = [b['q'] for b in sent]
    sparent = [b['parent'] for b in sent]
    snames = [b['name'] for b in sent]

    skeys, sbase, soffs = keys(sp, se, spr)
    tkeys, tbase, toffs = keys(tp, te, tpr)
    nframes = max(1, spr['NumFrames'][1])

    ALIAS = {
        'M_Center': 'M_Center', 'M_Head': 'M_Head', 'M_Body': 'M_Spine2',
        'M_CollarD': 'M_Spine1', 'M_CollarU': 'M_Spine2',
        'M_UpperJaw': 'M_Head', 'M_LowerJaw': 'M_Hip', 'M_Core': 'M_Spine2',
        'M_Core2': 'M_Spine2', 'M_Xel': 'M_Spine2', 'M_PanelsRoot': 'M_Spine2',
        'M_WingRoot': 'M_Spine2', 'M_WarmRoot': 'M_Spine2',
        'L_Collar1': 'L_shoulder', 'L_Collar2': 'L_Arm1', 'L_CollarU1': 'L_Arm2',
        'R_Collar1': 'R_shoulder', 'R_Collar2': 'R_Arm1', 'R_CollarU1': 'R_Arm2',
        'L_WarmA1': 'L_Arm1', 'L_WarmA2': 'L_Arm2', 'L_WarmA3': 'L_Hand',
        'L_WarmB1': 'L_Arm1', 'L_WarmB2': 'L_Arm2', 'L_WarmB3': 'L_Hand',
        'L_WarmC1': 'L_Arm1', 'L_WarmC2': 'L_Arm2', 'L_WarmC3': 'L_Hand',
        'R_WarmA1': 'R_Arm1', 'R_WarmA2': 'R_Arm2', 'R_WarmA3': 'R_Hand',
        'R_WarmB1': 'R_Arm1', 'R_WarmB2': 'R_Arm2', 'R_WarmB3': 'R_Hand',
        'R_WarmC1': 'R_Arm1', 'R_WarmC2': 'R_Arm2', 'R_WarmC3': 'R_Hand',
    }
    s2t = {}
    for i, n in enumerate(snames):
        tn = ALIAS.get(n, n)
        if tn in tindex and tn not in s2t and tn not in ('Global_SRT','M_Move'):
            s2t[tn] = i
    print('bones mapped', len(s2t), sorted(s2t))

    def sample(track, frac):
        if not track: return None
        return track[min(len(track)-1, int(round(frac*(len(track)-1))))]

    # bind-time check: with source at bind, result must equal target bind local
    if 'M_Head' in s2t:
        si = s2t['M_Head']; ti = tindex['M_Head']
        sw_ = [None]*len(sent)
        for i in range(len(sent)):
            p_ = sparent[i]
            sw_[i] = qmul(sw_[p_], sloc_bind[i]) if 0 <= p_ < i else sloc_bind[i]
        desired = qmul(qmul(sw_[si], qinv(sbind[si])), tbind[ti])
        pi = tparent[ti]
        par = tbind[pi] if 0 <= pi < len(tbind) and pi != ti else (0,0,0,1)
        loc = qn(qmul(qinv(par), desired))
        tb = tent[ti]['q']
        err = max(abs(loc[j]-tb[j]) for j in range(4))
        print('BIND CHECK M_Head local=%s target=%s max_err=%.5f' % (
            tuple(round(x,4) for x in loc), tuple(round(x,4) for x in tb), err))

    b = bytearray(tp.buf)
    edits = 0
    # target key times (per track) -> sample source once per distinct frac
    cache = {}
    for t, ks in enumerate(tkeys):
        if t >= len(tnames): break
        tn = tnames[t]
        if tn not in s2t: continue
        si = s2t[tn]
        ro, rn = toffs[t*6], toffs[t*6+1]
        swb = sbind[si]
        for k in range(rn):
            frac = k/max(1, rn-1)
            key = (frac,)
            if key not in cache:
                sloc = []
                for i in range(len(sent)):
                    q = sample(skeys[i], frac) if i < len(skeys) else None
                    sloc.append(q if q else sloc_bind[i])
                sworld = [None]*len(sent)
                for i in range(len(sent)):
                    p_ = sparent[i]
                    sworld[i] = qmul(sworld[p_], sloc[i]) if 0 <= p_ < i else sloc[i]
                cache[key] = (sloc, sworld)
            _, sworld = cache[key]
            delta = qmul(sworld[si], qinv(swb))
            desired = qmul(delta, tbind[t])
            pi = tparent[t]
            par = tbind[pi] if 0 <= pi < len(tbind) and pi != t else (0, 0, 0, 1)
            local = qn(qmul(qinv(par), desired))
            struct.pack_into('<3f', b, tbase+ro+k*12, local[0], local[1], local[2])
            edits += 1
    print('rot keys written', edits)
    print('rebuild', PB.rebuild(tp, b, out))


main()
