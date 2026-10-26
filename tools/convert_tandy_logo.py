"""Developer-only palette/RLE conversion of the supplied Tandy PNG."""
from pathlib import Path
import hashlib
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
source = ROOT / 'examples/smsboot/logo.png'
im = Image.open(source).convert('RGBA')
bg = Image.new('RGBA', im.size, 'white'); bg.alpha_composite(im)
# Mode 9 on a 4:3 display: pixel width/height = 5/6. Preserve source aspect.
im = bg.convert('RGB').resize((288, 136), Image.Resampling.LANCZOS)
pixels = []
for y in range(136):
    for x in range(288):
        r,g,b = im.getpixel((x,y))
        if b > r * 1.3 and b > 90 and r < 180:
            pixels.append(1 + min(12, x * 13 // 288))
        elif r > 100 and r > b * 1.5 and r > g * 1.5:
            pixels.append(14)
        else: pixels.append(15)
packed = [(pixels[i]<<4)|pixels[i+1] for i in range(0,len(pixels),2)]
runs = []
for v in packed:
    if runs and runs[-1] == v and runs[-2] < 255: runs[-2] += 1
    else: runs += [1,v]
text = '/* Supplied Tandy logo, palette labels/RLE. Not redrawn lettering. */\n'
text += '#define LOGO_WIDTH 288U\n#define LOGO_HEIGHT 136U\n#define LOGO_X 16U\n#define LOGO_Y 32U\n'
text += 'static const unsigned char logo_runs[] = {\n'
text += ''.join('    '+','.join('0x%02XU'%v for v in runs[i:i+16])+',\n'
                for i in range(0,len(runs),16))+'};\n'
(ROOT/'examples/smsboot/logo.h').write_text(text,encoding='ascii')
print('Source SHA256',hashlib.sha256(source.read_bytes()).hexdigest())
print('Logo RLE bytes',len(runs),'unpacked bytes',len(packed))
