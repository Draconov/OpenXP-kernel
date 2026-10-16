#!/usr/bin/env python3
"""Build original editable PNG masters into fixed-size RGB565/A8 OXUA.
Requires only the Python standard library; no runtime PNG decoder.
"""
import struct
from pathlib import Path
from pack_skin import png_rgba

SOURCES=(("widgets-atlas.png",192,128,"aurora-widgets.oxua"),
         ("icons-32.png",192,64,"aurora-icons.oxua"),
         ("chrome-strip.png",192,64,"aurora-chrome.oxua"),
         ("menu-atlas.png",192,128,"aurora-menu.oxua"),
         ("toolbar-atlas.png",192,64,"aurora-toolbar.oxua"))
def encode(path,width,height):
    w,h,pixels=png_rgba(path)
    if (w,h)!=(width,height):raise ValueError(f"{path}: expected {width}x{height}, got {w}x{h}")
    out=bytearray(struct.pack('<4sHHHHI',b'OXUA',1,w,h,0,w*h*3))
    for i in range(0,len(pixels),4):
        r,g,b,a=pixels[i:i+4]
        out.extend(struct.pack('<HB',((r>>3)<<11)|((g>>2)<<5)|(b>>3),a))
    return bytes(out)
def build(root,output,prefix="aurora"):
    output.mkdir(parents=True,exist_ok=True)
    for name,w,h,target in SOURCES:
        data=encode(root/name,w,h)
        target=target.replace("aurora-",prefix+"-",1)
        (output/target).write_bytes(data)
        print(f'packed {name} -> {target}: {len(data)} bytes')
if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser()
    parser.add_argument('--source',type=Path,default=Path('assets/ui/aurora'))
    parser.add_argument('--output',type=Path,default=Path('build/skin'))
    parser.add_argument('--prefix',default='aurora',choices=('aurora','olive','midnight'))
    args=parser.parse_args();build(args.source,args.output,args.prefix)
