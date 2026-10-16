#!/usr/bin/env python3
"""OpenXP 1.8 OXSK skin packer: small non-interlaced RGB/RGBA PNG -> RGB565+A8.

Requires only Python standard library. Strict enough for build-time authoring;
PNG masters remain easy to edit in Aseprite, Krita, GIMP and other editors.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import struct
import zlib

ASSETS = (
    (0, 'frame-active.png', (8, 8, 34, 6)),
    (1, 'frame-inactive.png', (8, 8, 34, 6)),
    (2, 'caption-buttons.png', (0, 0, 0, 0)),
    (3, 'taskbar.png', (8, 8, 2, 2)),
    (4, 'start.png', (12, 12, 4, 4)),
)
MAX_RESOURCE = 65536

def png_rgba(path: Path) -> tuple[int, int, bytes]:
    blob = path.read_bytes()
    if not blob.startswith(b'\x89PNG\r\n\x1a\n'):
        raise ValueError(f'{path}: not PNG')
    pos, width, height, kind, depth = 8, 0, 0, 0, 0
    chunks, have_header, ended = [], False, False
    while pos + 12 <= len(blob):
        n = struct.unpack_from('>I', blob, pos)[0]
        end = pos + 12 + n
        if n > MAX_RESOURCE * 16 or end > len(blob):
            raise ValueError(f'{path}: invalid chunk bounds')
        tag = blob[pos+4:pos+8]
        data = blob[pos+8:pos+8+n]
        if zlib.crc32(tag + data) != struct.unpack_from('>I', blob, pos+8+n)[0]:
            raise ValueError(f'{path}: PNG CRC error')
        pos = end
        if tag == b'IHDR':
            if have_header or n != 13:
                raise ValueError(f'{path}: bad IHDR')
            width,height,depth,kind,comp,filter_method,interlace = struct.unpack('>IIBBBBB',data)
            if not 1 <= width <= 256 or not 1 <= height <= 256 or depth != 8 or \
               kind not in (2, 6) or comp or filter_method or interlace:
                raise ValueError(f'{path}: only non-interlaced RGB/RGBA 8-bit PNG <=256x256')
            have_header = True
        elif tag == b'IDAT':
            chunks.append(data)
        elif tag == b'IEND':
            ended = True
            break
        elif tag[0] & 32 == 0 and tag != b'IHDR':
            raise ValueError(f'{path}: unsupported critical PNG chunk {tag!r}')
    if not have_header or not chunks or not ended:
        raise ValueError(f'{path}: incomplete PNG')
    cpp = 4 if kind == 6 else 3
    row_bytes = width*cpp
    raw = zlib.decompress(b''.join(chunks))
    if len(raw) != height*(row_bytes+1):
        raise ValueError(f'{path}: invalid decompressed size')
    rgba = bytearray()
    prev = bytearray(row_bytes)
    for y in range(height):
        begin=y*(row_bytes+1)
        filt=raw[begin]
        cur=bytearray(raw[begin+1:begin+1+row_bytes])
        if filt>4:
            raise ValueError(f'{path}: invalid scanline filter')
        for x in range(row_bytes):
            a=cur[x-cpp] if x>=cpp else 0
            b=prev[x]
            c=prev[x-cpp] if x>=cpp else 0
            if filt==1: cur[x]=(cur[x]+a)&255
            elif filt==2: cur[x]=(cur[x]+b)&255
            elif filt==3: cur[x]=(cur[x]+((a+b)//2))&255
            elif filt==4:
                p=a+b-c
                d=(abs(p-a),abs(p-b),abs(p-c))
                pick=a if d[0]<=d[1] and d[0]<=d[2] else b if d[1]<=d[2] else c
                cur[x]=(cur[x]+pick)&255
        for x in range(width):
            pix=cur[cpp*x:cpp*(x+1)]
            rgba.extend((pix[0],pix[1],pix[2],pix[3] if cpp==4 else 255))
        prev=cur
    return width,height,bytes(rgba)

def build(source: Path) -> bytes:
    records=[]
    pixels=bytearray()
    start=16+20*len(ASSETS)
    for sid,name,border in ASSETS:
        w,h,rgba=png_rgba(source/name)
        left,right,top,bottom=border
        if left+right>=w or top+bottom>=h:
            raise ValueError(f'{name}: invalid nine-slice borders')
        if sid==2 and (w,h)!=(72,72):
            raise ValueError('caption-buttons.png must be a 3x3 grid of 24x24 tiles')
        records.append(struct.pack('<8HI',sid,w,h,left,right,top,bottom,0,start+len(pixels)))
        for i in range(0,len(rgba),4):
            r,g,b,a=rgba[i:i+4]
            c=((r>>3)<<11)|((g>>2)<<5)|(b>>3)
            pixels.extend(struct.pack('<HB',c,a))
    total=start+len(pixels)
    if total>MAX_RESOURCE:
        raise ValueError(f'skin ({total} bytes) exceeds {MAX_RESOURCE}-byte runtime limit')
    return struct.pack('<4sHHII',b'OXSK',1,len(records),total,0)+b''.join(records)+pixels

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('output', type=Path)
    p.add_argument('--source',type=Path,default=Path('assets/ui/aurora'))
    args=p.parse_args()
    encoded=build(args.source)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_bytes(encoded)
    print(f'Packed {len(ASSETS)} skin sprites -> {args.output} ({len(encoded)} bytes)')

if __name__=='__main__':
    main()
