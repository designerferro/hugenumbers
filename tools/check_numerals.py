"""Validate checked-in mask records and render a layout/seconds contact sheet."""
import struct
from pathlib import Path
from PIL import Image, ImageDraw
ROOT = Path(__file__).resolve().parents[1]

def read_masks(name):
    data = (ROOT / f'resources/numerals/{name}.bin').read_bytes()
    offsets = struct.unpack_from('<161I', data)
    assert offsets[0] == 644 and offsets[-1] == len(data)
    masks = []
    for start, end in zip(offsets, offsets[1:]):
        assert start < end
        w, h = data[start:start+2]
        im = Image.new('1', (w,h))
        draw = ImageDraw.Draw(im)
        pos, y = start+2, 0
        while pos < end:
            repeat, count = data[pos:pos+2]; pos += 2
            assert repeat and y+repeat <= h
            last = 0
            for _ in range(count):
                x, length = data[pos:pos+2]; pos += 2
                assert length and x >= last and x+length <= w
                draw.rectangle((x,y,x+length-1,y+repeat-1),fill=1)
                last=x+length
            y += repeat
        assert pos == end and y == h and im.getbbox()
        masks.append(im)
    return masks

def frames(w,h,minute,round_screen=False):
    mx,my=(w*15//100,h*15//100) if round_screen else (2,2)
    w,h=w-2*mx,h-2*my
    left=(w-3)*(22+(minute*7%16)*56//15)//100
    result=[None]*4
    for col,width in enumerate([left,w-3-left]):
        split=(h-3)*(28+((minute*5+col*7)%16)*44//15)//100
        x=mx+(left+3 if col else 0)
        result[col]=(x,my,width,split)
        result[col+2]=(x,my+split+3,width,h-3-split)
    return result

def main():
    masks={s:read_masks(s) for s in ['western','eastern','devanagari']}
    total=sum(p.stat().st_size for p in (ROOT/'resources/numerals').glob('*.bin'))
    assert total+4096 < 128*1024, 'Aplite resource budget'
    for w,h,rounded in [(144,168,False),(180,180,True),(200,228,False),(260,260,True)]:
        for minute in range(1440):
            f=frames(w,h,minute,rounded)
            for x,y,fw,fh in f:
                assert fw>0 and fh>0 and 0<=x<x+fw<=w and 0<=y<y+fh<=h
            assert f != frames(w,h,(minute+1)%1440,rounded)
        assert h-h*0//60==h and 0<h-h*59//60<=5
    sheet=Image.new('RGB',(6*200,3*252),'#222222')
    for row,(script,assets) in enumerate(masks.items()):
        for col,(minute,seconds) in enumerate([(9*60+41,0),(9*60+41,30),(9*60+41,59),(9*60+42,0),(9*60+43,30),(9*60+44,45)]):
            im=Image.new('RGB',(200,228),'#55ffaa')
            boundary=228-228*seconds//60
            ImageDraw.Draw(im).rectangle((0,boundary,200,228),fill='#aa55ff')
            digits=[minute//60//10,minute//60%10,minute%60//10,minute%10]
            for digit,(x,y,w,h) in zip(digits,frames(200,228,minute)):
                state=max(0,min(15,int((w*80//h-18)*15/90)))
                mask=assets[digit*16+state].resize((w,h),Image.Resampling.NEAREST)
                ink=Image.new('RGB',(w,h),'black')
                if boundary-y < h:
                    ImageDraw.Draw(ink).rectangle((0,max(0,boundary-y),w,h),fill='white')
                im.paste(ink,(x,y),mask)
            sheet.paste(im,(col*200,row*252))
            ImageDraw.Draw(sheet).text((col*200+3,row*252+230),f'{script} {minute//60:02}:{minute%60:02}:{seconds:02}',fill='white')
    out=ROOT/'build/previews';out.mkdir(parents=True,exist_ok=True)
    sheet.save(out/'layouts.png')
    print(f'Validated 480 masks, {total} resource bytes, all 1440 minute layouts and seconds endpoints.')
if __name__=='__main__': main()
