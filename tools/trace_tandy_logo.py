"""Four fixed 4bpp masks with palette-only stroke tracing.

Reads the supplied logo; all visible pixels are clipped to its silhouettes.
No Sonic artwork, ROM data, audio changes, or hardware access.
"""
from pathlib import Path
import argparse, base64, hashlib, io, json, math
import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
FINAL_TICK = 481
TRACE_END_TICK = 421
HANDOFF_GAP_TICKS = 16
RATE = 1193182 / 4972
GROUPS = 13
PHASES = 4
STEPS = GROUPS * PHASES

# Pen routes in the supplied 288x136 image coordinates. Disjoint subpaths
# represent pen lifts, not connecting strokes. They guide arrival time only;
# no path geometry is rendered into the final logo.
ROUTES = [
    ('T', [[(6,29),(58,29)],[(39,39),(39,79)],[(29,79),(29,39),(7,39)]]),
    ('A', [[(51,79),(61,57),(73,32),(78,29),(83,30),(87,37),
            (103,77),(78,77)],[(79,66),(92,66)]]),
    ('N', [[(118,79),(118,36),(121,30),(125,30),(129,34),
            (154,73),(160,77),(165,73),(166,26)]]),
    ('D', [[(175,29),(203,29),(215,33),(224,41),(227,51),
            (225,63),(218,72),(208,77),(178,77),(178,39),
            (201,39),(210,42),(214,49),(214,57),(209,65),
            (190,65),(190,47)]]),
    ('Y', [[(227,29),(244,60),(246,64),(246,79)],
           [(278,29),(257,63),(256,69),(256,79)]]),
]

COLORS = {0:(0,0,170),1:(0,0,170),3:(0,170,170),
          4:(170,0,0),9:(85,85,255),15:(255,255,255)}

def source_masks():
    p = ROOT / 'examples/smsboot/logo.png'
    im = Image.open(p).convert('RGBA')
    bg = Image.new('RGBA',im.size,'white'); bg.alpha_composite(im)
    im = bg.convert('RGB').resize((288,136),Image.Resampling.LANCZOS)
    a = np.array(im,dtype=np.float64);r,g,b = a[:,:,0],a[:,:,1],a[:,:,2]
    blue = (b>r*1.3)&(b>90)&(r<180)
    red = (r>100)&(r>b*1.5)&(r>g*1.5)&~blue
    return im, blue, red, hashlib.sha256(p.read_bytes()).hexdigest()

def arrival_map(blue):
    ys,xs = np.nonzero(blue)
    pts = np.column_stack((xs*5/6,ys))
    distances=[];progress=[]
    for _,subpaths in ROUTES:
        segments=[];total=0.
        for path in subpaths:
            for a,b in zip(path,path[1:]):
                a=np.array((a[0]*5/6,a[1]),float)
                b=np.array((b[0]*5/6,b[1]),float)
                length=np.linalg.norm(b-a)
                segments.append((a,b,total,length));total+=length
        best=np.full(len(pts),1e20);where=np.zeros(len(pts))
        for a,b,start,length in segments:
            t=np.clip(((pts-a)@(b-a))/(length*length),0,1)
            d=np.sum((pts-(a[None,:]+t[:,None]*(b-a)))**2,axis=1)
            choose=d<best
            best[choose]=d[choose]
            where[choose]=(start+t[choose]*length)/total
        distances.append(best);progress.append(where)
    distances=np.array(distances);progress=np.array(progress)
    letter=np.argmin(distances,axis=0)
    birth=np.zeros(len(pts),dtype=np.int16)
    for k in range(5):
        sel=letter==k
        # Let the stroke's core lead its edges; every pixel still comes from
        # the supplied mask. Small letter-specific delay avoids a uniform beat.
        local=progress[k,sel]*.9+np.sqrt(distances[k,sel])*.012
        local=(local-local.min())/(local.max()-local.min())
        delay=[0,2,4,1,3][k]
        birth[sel]=np.clip(np.floor(delay+local*(STEPS-1-delay)),0,STEPS-1)
    result=np.full(blue.shape,-1,dtype=np.int16);result[ys,xs]=birth
    letters=np.full(blue.shape,-1,dtype=np.int8);letters[ys,xs]=letter
    return result,letters

def make_masks(blue,red,birth):
    masks=[]
    for phase in range(PHASES):
        m=np.full(blue.shape,15,dtype=np.uint8)
        m[red]=14
        m[blue&(birth<phase*GROUPS)]=0
        sel=blue&(birth>=phase*GROUPS)&(birth<(phase+1)*GROUPS)
        m[sel]=(birth[sel]%GROUPS)+1
        assert set(np.unique(m))<=set(range(16))
        masks.append(m)
    return masks

def palette_for(phase,step,final=False):
    p=[COLORS[15]]*16;p[0]=COLORS[0]
    settled = step >= STEPS-1 or step % GROUPS == GROUPS-1
    for i in range(1,14):
        born=phase*GROUPS+i-1
        age=step-born
        if age<0:p[i]=COLORS[15]
        elif age==0 and not settled:p[i]=COLORS[3]
        elif age==1 and not settled:p[i]=COLORS[9]
        else:p[i]=COLORS[1]
    p[14]=COLORS[4] if final else COLORS[15]
    return np.array(p,dtype=np.uint8)

def render(masks,step,final=False):
    phase=min(PHASES-1,max(0,step//GROUPS))
    rgb=palette_for(phase,step,final)[masks[phase]]
    canvas=np.full((200,320,3),255,dtype=np.uint8)
    canvas[32:168,16:304]=rgb
    return Image.fromarray(canvas).resize((640,480),Image.Resampling.NEAREST)

def rle(m):
    a=m.flatten();packed=(a[::2]<<4)|a[1::2]
    out=[]
    for v in packed:
        v=int(v)
        if out and out[-1]==v and out[-2]<255:out[-2]+=1
        else:out.extend((1,v))
    return out

def verify(masks,blue,red,birth):
    report=[]
    for phase,m in enumerate(masks):
        # Palette-only reconstruction of every intended subframe must be exact.
        histories=set(); index_histories={}
        for y,x in np.ndindex(m.shape):
            history=[]
            for sub in range(GROUPS):
                s=phase*GROUPS+sub;final=s==STEPS-1
                if red[y,x]:c=COLORS[4] if final else COLORS[15]
                elif not blue[y,x] or birth[y,x]>s:c=COLORS[15]
                elif birth[y,x]<phase*GROUPS:c=COLORS[1]
                else:
                    age=s-int(birth[y,x])
                    settled = s >= STEPS-1 or s % GROUPS == GROUPS-1
                    c=COLORS[3] if age==0 and not settled else COLORS[9] if age==1 and not settled else COLORS[1]
                history.append(c)
                actual=tuple(palette_for(phase,s,final)[int(m[y,x])])
                assert actual==c
            history=tuple(history)
            index=int(m[y,x])
            if index in index_histories:assert index_histories[index]==history
            else:index_histories[index]=history
            histories.add(history)
        assert len(histories)<=16
        report.append({'phase':phase,'distinct_color_histories':len(histories),
                       'used_indices':sorted(int(v) for v in np.unique(m)),
                       'rle_bytes':len(rle(m))})
    # At a handoff, commit all old active indices to permanent blue before
    # resetting them. Then install the next encoded mask while new indices
    # remain white. Both bitmap writes are visually invisible.
    for phase in range(3):
        old=masks[phase];new=masks[phase+1]
        p=np.full((16,3),255,dtype=np.uint8);p[:14]=COLORS[1]
        before=p[old]
        committed=old.copy();committed[(old>=1)&(old<=13)]=0
        assert np.array_equal(before,p[committed])
        p[1:14]=255
        assert np.array_equal(before,p[committed])
        assert np.array_equal(before,p[new])
    return report

def deadline(step):
    available = TRACE_END_TICK - (PHASES-1)*HANDOFF_GAP_TICKS
    return ((step+1)*available+STEPS-1)//STEPS + (step//GROUPS)*HANDOFF_GAP_TICKS

def runtime_header(masks):
    def packed(m):
        a=m.flatten();return ((a[::2]<<4)|a[1::2]).tolist()
    def emit_array(name,data,typ='unsigned char'):
        h='static const '+typ+' '+name+'[] = {\n'
        h+=''.join('    '+','.join(str(int(v))+'U' for v in data[i:i+16])+',\n' for i in range(0,len(data),16))+'};\n'
        return h
    initial=rle(masks[0]);patch=[];offsets=[];counts=[]
    for phase in range(3):
        old=masks[phase].copy();committed=old.copy()
        committed[(old>=1)&(old<=13)]=0
        for a,b in [(old,committed),(committed,masks[phase+1])]:
            offsets.append(len(patch));count=0
            for i,(before,after) in enumerate(zip(packed(a),packed(b))):
                if before!=after:
                    y,x=divmod(i,144)
                    y+=32
                    addr=(y&3)*8192+(y>>2)*160+8+x
                    patch.extend((addr&255,addr>>8,after));count+=1
            counts.append(count)
    h='/* Source-derived palette stroke assets; no Sonic ROM bytes. */\n'
    h+='#define TRACE_END_TICK 421U\n#define TRACE_FINAL_TICK 481U\n'
    h+='#define TRACE_PATCH_PASSES 6U\n'
    h+=emit_array('trace_deadlines',[deadline(s) for s in range(STEPS)],'unsigned short')
    h+=emit_array('trace_initial',initial)
    h+=emit_array('trace_patch_offsets',offsets,'unsigned short')
    h+=emit_array('trace_patch_counts',counts,'unsigned short')
    h+=emit_array('trace_patches',patch)
    budgets=[deadline((p+1)*13)-deadline(p*13+12) for p in range(3)]
    return h,{'initial_rle_bytes':len(initial),'patch_data_bytes':len(patch),'patch_counts':counts,'deadline_table_bytes':104,'descriptor_bytes':24,'total_constant_bytes':len(initial)+len(patch)+128,'handoff_available_ticks':budgets}

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--out',type=Path,required=True)
    a=ap.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
    original,blue,red,sha=source_masks()
    birth,letters=arrival_map(blue);masks=make_masks(blue,red,birth)
    checks=verify(masks,blue,red,birth)
    # Count actual whole-sequence color histories, not a theoretical claim.
    all_histories=[]
    for step in range(STEPS):
        phase=step//GROUPS
        rgb=palette_for(phase,step,step==STEPS-1)[masks[phase]]
        codes=np.zeros(blue.shape,dtype=np.uint8)
        for code,color in enumerate([COLORS[15],COLORS[1],COLORS[3],COLORS[9],COLORS[4]]):
            codes[np.all(rgb==color,axis=2)]=code
        all_histories.append(codes.flatten())
    whole_history_count=len(np.unique(np.stack(all_histories,axis=1),axis=0))
    assert whole_history_count>16
    header='/* Source-derived inspection masks; the DOS runtime uses TRACE_RT.H. */\n'
    offsets=[];data=[]
    for m in masks:offsets.append(len(data));data.extend(rle(m))
    header+='#define TRACE_PHASES 4U\n#define TRACE_GROUPS 13U\n'
    header+='static const unsigned short trace_offsets[4] = {'+','.join(str(x)+'U' for x in offsets)+'};\n'
    header+='static const unsigned char trace_runs[] = {\n'
    header+=''.join('    '+','.join('0x%02XU'%v for v in data[i:i+16])+',\n' for i in range(0,len(data),16))+'};\n'
    (out/'TRACE.H').write_text(header,encoding='ascii')
    runtime = runtime_header(masks)
    (out/'TRACE_RT.H').write_text(runtime[0],encoding='ascii')
    images=[]
    for s in range(-1,STEPS):
        im=render(masks,s,False)
        b=io.BytesIO();im.save(b,format='PNG');images.append('data:image/png;base64,'+base64.b64encode(b.getvalue()).decode())
    im=render(masks,STEPS-1,True)
    b=io.BytesIO();im.save(b,format='PNG');images.append('data:image/png;base64,'+base64.b64encode(b.getvalue()).decode())
    deadlines=[0]+[deadline(s)/RATE for s in range(STEPS)]+[FINAL_TICK/RATE]
    frames=[render(masks,s,False) for s in range(-1,STEPS)]+[im]
    duration=805/RATE+.15
    delays=[round((deadlines[i+1]-deadlines[i])*1000) for i in range(STEPS+1)]+[round((duration-deadlines[-1])*1000)]
    frames[0].save(out/'TANDY-STROKE.gif',save_all=True,append_images=frames[1:],duration=delays,loop=0,optimize=False)
    frames[-1].save(out/'TANDY-STROKE-FINAL.png')
    # A contact sheet makes route shape, not only the final logo, reviewable.
    picks=[-1,8,20,30,39,46,51,51,52]
    sheet=Image.new('RGB',(960,780),(232,232,232));d=ImageDraw.Draw(sheet)
    for j,s in enumerate(picks):
        x=(j%3)*320;y=(j//3)*260
        im=render(masks,min(s,51),j==8).resize((320,240),Image.Resampling.NEAREST)
        sheet.paste(im,(x,y));label='white start' if s<0 else 'TANDY settled: 1.754 s' if j==6 else 'blue hold before hit' if j==7 else 'tagline hit: 2.004 s' if j==8 else 'trace step %d/52'%(s+1)
        d.text((x+6,y+242),label,fill=(20,20,20))
    sheet.save(out/'TANDY-STROKE-CONTACT.png')
    debug=np.zeros((136,288,3),dtype=np.uint8);debug[:]=255
    wheel=[(255,0,0),(255,128,0),(200,200,0),(0,180,0),(0,180,150),(0,170,255),(0,80,255),(80,0,255),(160,0,255),(220,0,160),(180,80,80),(80,100,80),(60,60,60)]
    for i in range(13):debug[blue&(birth%13==i)]=wheel[i]
    debug[red]=(170,0,0)
    Image.fromarray(debug).resize((864,408),Image.Resampling.NEAREST).save(out/'STROKE-PHASE-DEBUG.png')
    report={'type':'stroke-logo assets and software proof; see NATIVE.json for runtime qualification',
        'source_logo_sha256':sha,'sound_changed':False,'final_tick':FINAL_TICK,
        'trace_end_tick':TRACE_END_TICK,'trace_end_seconds':TRACE_END_TICK/RATE,'hold_ticks':FINAL_TICK-TRACE_END_TICK,'handoff_gap_ticks':HANDOFF_GAP_TICKS,'runtime_data':runtime[1],
        'final_seconds':FINAL_TICK/RATE,'phases':PHASES,'steps':STEPS,
        'indices':{'finished_blue':0,'active_stroke_groups':'1..13','tagline':14,'white_background':15},
        'phase_checks':checks,'whole_sequence_distinct_histories':whole_history_count,'total_rle_bytes':len(data),
        'bitmap_handoffs':3,'max_patch_writes_per_pass':max(runtime[1]['patch_counts']),'total_patch_writes':sum(runtime[1]['patch_counts']),
        'handoff':'commit active indices to blue0, reset now-unused active palette to white, install next mask invisibly',
        'native_limit':'Software proof covers pixel/index/patch semantics. Runtime measurements belong in NATIVE.json.'}
    (out/'TRACE-CHECKS.json').write_text(json.dumps(report,indent=2))
    (out/'PREVIEW-FRAMES.json').write_text(json.dumps({'images':images,'times':deadlines}))
    print(json.dumps(report,indent=2))

if __name__=='__main__':main()
