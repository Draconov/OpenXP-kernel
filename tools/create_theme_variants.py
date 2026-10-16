#!/usr/bin/env python3
"""One-time editable palette variations from the original Aurora master PNGs.
Do not run this script as part of the build: artists own the resulting files.
Pillow is an authoring-only dependency, not required to pack or boot OpenXP.
"""
from pathlib import Path
from PIL import Image
import colorsys
BASE=Path(__file__).resolve().parents[1]/'assets/ui/aurora'
FILES=('frame-active.png','frame-inactive.png','caption-buttons.png','taskbar.png','start.png','widgets-atlas.png','icons-32.png')

def recolor(pixel,kind,filename):
    r,g,b,a=pixel
    if a==0:return pixel
    h,s,v=colorsys.rgb_to_hsv(r/255,g/255,b/255)
    if filename=='icons-32.png':
        # Leave distinctive application identities recognizable: recolor only
        # blue/cyan interface surfaces, not yellow folder or paint colors.
        if kind=='olive' and 0.48<h<0.72 and s>0.23:
            h=0.30+(h-0.6)*0.12;s=min(0.85,s*0.9)
        elif kind=='midnight' and 0.48<h<0.72 and s>0.2:
            h=0.66+(h-0.6)*0.22;v=v*0.82
        return tuple(round(max(0,min(1,x))*255) for x in (*colorsys.hsv_to_rgb(h,s,v),))+(a,)
    if kind=='olive':
        if (0.48<h<0.75 and s>0.20):
            h=0.27+(h-0.59)*0.20;s=min(0.8,s*0.93);v=v*0.89+0.04
        elif filename=='start.png' and .19<h<.47 and s>.2:
            h=.17+(h-.31)*.10;v=min(1,v*1.03)
        elif s<.20:
            h=.21;s=min(.23,s+.09);v=v*.97
    else:  # midnight: violet-blue chrome and deep charcoal interiors
        if .48<h<.75 and s>.19:
            h=.655+(h-.60)*.22;s=min(.75,s*.80+.09);v=v*.70+.02
        elif filename=='start.png' and .19<h<.47 and s>.2:
            h=.65;s=min(.80,s*.8);v=v*.68
        elif s<.25:
            h=.64;s=.18 if v>.25 else .25;v=.20+.55*v
    r1,g1,b1=colorsys.hsv_to_rgb(h,s,v)
    return (round(r1*255),round(g1*255),round(b1*255),a)

def make():
    for theme in ('olive','midnight'):
        out=BASE/theme;out.mkdir(exist_ok=True)
        for file in FILES:
            im=Image.open(BASE/file).convert('RGBA')
            im.putdata([recolor(v,theme,file) for v in list(im.getdata())])
            im.save(out/file,optimize=True)

if __name__=='__main__':make()
