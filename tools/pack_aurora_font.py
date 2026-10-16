#!/usr/bin/env python3
"""Pack an editable RGBA glyph atlas into a compact validated OXFT resource.
Only uses the existing standard-library PNG decoder; Pillow is authoring-only.
"""
import struct
from pathlib import Path
from pack_skin import png_rgba
CW,CH,COLS,ROWS,STYLES=11,13,16,6,2
COUNT=95

def encode(path):
    w,h,rgba=png_rgba(path)
    if (w,h)!=(CW*COLS,CH*ROWS*STYLES):
        raise ValueError(f'font PNG must be {CW*COLS}x{CH*ROWS*STYLES}')
    alpha=bytes(rgba[3::4])
    advances=bytearray()
    for style in range(STYLES):
        for idx in range(COUNT):
            x=(idx%COLS)*CW;y=(idx//COLS+style*ROWS)*CH
            right=-1
            for yy in range(CH):
                for xx in range(CW):
                    if alpha[(y+yy)*w+x+xx]>20:
                        right=max(right,xx)
            # Keep an extra inter-glyph pixel and a readable word space.
            advance=4 if idx==0 else max(3,min(CW,right+2))
            advances.append(advance)
    return (struct.pack('<4sBBBBHHI',b'OXFT',1,CW,CH,STYLES,32,COUNT,len(alpha))+
            advances+alpha)

if __name__=='__main__':
    import argparse
    pa=argparse.ArgumentParser()
    pa.add_argument('source',type=Path)
    pa.add_argument('output',type=Path)
    a=pa.parse_args();a.output.parent.mkdir(parents=True,exist_ok=True)
    data=encode(a.source);a.output.write_bytes(data)
    print(f'packed OXFT: {a.output} ({len(data)} bytes)')
