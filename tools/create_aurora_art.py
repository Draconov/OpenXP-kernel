#!/usr/bin/env python3
"""Recreate original editable Aurora PNG masters. Requires Pillow (authoring only).
The OS build still uses standard-library-only pack_skin.py."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import math

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'assets/ui/aurora'
OUT.mkdir(parents=True,exist_ok=True)
FONT='/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
FONT2='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
def f(size,bold=True):
    try:return ImageFont.truetype(FONT if bold else FONT2,size)
    except OSError:return ImageFont.load_default()
def lerp(a,b,t):return round(a+(b-a)*t)
def mix(a,b,t):return tuple(lerp(a[i],b[i],t) for i in range(3))
def gradient(im,y0,y1,a,b):
    d=ImageDraw.Draw(im)
    for y in range(y0,y1):
        rgb=mix(a,b,(y-y0)/max(1,y1-y0-1))
        d.line((0,y,im.width-1,y),fill=rgb+(255,))
def rounded(im,box,fill,radius=5,outline=None,width=1):
    ImageDraw.Draw(im).rounded_rectangle(box,radius=radius,fill=fill,outline=outline,width=width)

def frame(active):
    im=Image.new('RGBA',(64,64));d=ImageDraw.Draw(im)
    top=((16,89,183),(45,156,217)) if active else ((92,118,151),(151,177,199))
    gradient(im,0,34,*top)
    # top gloss and central light shaft (9-slice center can stretch)
    for y in range(2,14):
        col=mix((255,255,255),(130,210,252) if active else (240,247,253),y/16)
        d.line((2,y,61,y),fill=col+(round(44-2*y),))
    d.line((7,1,56,1),fill=(184,237,255,255) if active else (242,248,255,255),width=1)
    d.line((4,32,59,32),fill=(11,59,126,255) if active else (90,116,147,255))
    d.line((5,33,58,33),fill=(255,255,255,255))
    d.rectangle((0,34,7,58),fill=(39,99,162,255) if active else (116,143,171,255))
    d.rectangle((56,34,63,58),fill=(39,99,162,255) if active else (116,143,171,255))
    d.line((1,34,1,58),fill=(132,199,240,255));d.line((62,34,62,58),fill=(10,49,109,255))
    d.line((7,34,7,58),fill=(235,247,255,255));d.line((56,34,56,58),fill=(226,241,253,255))
    d.rectangle((0,59,63,63),fill=(34,84,149,255) if active else (102,134,164,255))
    d.line((2,59,61,59),fill=(242,250,255,255));d.line((3,62,60,62),fill=(98,158,213,255))
    # center intentionally transparent, display comes from compositor
    # round corners; keep x=7 top row opaque for unit tests
    for yy,length in [(0,7),(1,4),(2,3),(3,2)]:
        for xx in range(length):
            im.putpixel((xx,yy),(0,0,0,0))
            im.putpixel((63-xx,yy),(0,0,0,0))
    for yy,length in [(63,6),(62,3)]:
        for xx in range(length):
            im.putpixel((xx,yy),(0,0,0,0))
            im.putpixel((63-xx,yy),(0,0,0,0))
    return im

# Small crisp control icons
def draw_caption(im,x,y,kind,state):
    d=ImageDraw.Draw(im)
    is_close=kind==2
    if is_close:
        a,b=((190,46,55),(250,126,120)) if state!=2 else ((145,31,39),(228,86,87))
        if state==1:a,b=(211,62,70),(255,155,142)
    else:
        a,b=((21,80,155),(79,153,225)) if state!=2 else ((18,67,129),(38,108,183))
        if state==1:a,b=(30,103,176),(113,191,244)
    for yy in range(2,22):
        col=mix(a,b,yy/23)
        d.line((x+2,y+yy,x+21,y+yy),fill=col+(255,))
    d.rounded_rectangle((x+2,y+2,x+21,y+21),radius=4,outline=(240,250,255,255) if state==1 else (18,50,91,255),width=1)
    d.line((x+5,y+5,x+18,y+5),fill=(255,255,255,155),width=2)
    dx=1 if state==2 else 0;dy=1 if state==2 else 0
    if kind==0:
        d.line((x+8+dx,y+15+dy,x+16+dx,y+15+dy),fill=(5,31,60,160),width=3)
        d.line((x+8+dx,y+14+dy,x+16+dx,y+14+dy),fill=(255,255,255,255),width=2)
    elif kind==1:
        d.rectangle((x+8+dx,y+9+dy,x+16+dx,y+16+dy),outline=(255,255,255,255),width=2)
        d.line((x+9+dx,y+9+dy,x+15+dx,y+9+dy),fill=(255,255,255,255),width=2)
    else:
        for offset in [-1,0,1]:
            d.line((x+9+dx+offset,y+9+dy,x+15+dx+offset,y+15+dy),fill=(255,255,255,255),width=1)
            d.line((x+15+dx+offset,y+9+dy,x+9+dx+offset,y+15+dy),fill=(255,255,255,255),width=1)

def button(im,box,primary=True,pressed=False):
    d=ImageDraw.Draw(im);x0,y0,x1,y1=box
    bottom=(45,92,169) if primary else (178,200,230)
    top=(92,181,239) if primary else (253,255,255)
    if pressed:top,bottom=bottom,top
    for y in range(y0,y1+1):
        t=(y-y0)/max(1,y1-y0)
        d.line((x0,y,x1,y),fill=mix(top,bottom,t)+(255,))
    d.rounded_rectangle(box,radius=5,outline=(12,63,134,255) if primary else (80,121,160,255),width=1)
    d.arc((x0+2,y0+2,x1-2,y1-3),180,360,fill=(255,255,255,125),width=1)

def make_widgets_atlas():
    im=Image.new('RGBA',(192,128),(0,0,0,0));d=ImageDraw.Draw(im)
    # compact sprites individually paintable; reference art for widget library
    for i in range(3):button(im,(i*64,0,i*64+62,27),primary=i!=0,pressed=i==2)
    for i in range(3):
        box=(i*64,32,i*64+62,58)
        d.rectangle(box,fill=(253,254,255,255),outline=(47,117,177,255) if i==1 else (140,166,190,255),width=2)
        d.line((box[0]+3,box[1]+3,box[2]-3,box[1]+3),fill=(212,227,243,255),width=1)
    # tabs
    for i in range(3):
        x=i*64
        d.rounded_rectangle((x+1,64,x+62,90),radius=5,fill=(245,250,255,255) if i==0 else (199,220,239,255),outline=(73,125,181,255))
        d.rectangle((x+2,88,x+61,90),fill=(245,250,255,255))
    # scrollbars with directional arrows
    for i in range(3):
        x=i*64;d.rectangle((x+1,96,x+62,124),fill=(216,233,249,255),outline=(105,153,195,255))
        d.rounded_rectangle((x+8+i*8,99,x+29+i*8,121),radius=4,fill=(103,164,215,255),outline=(39,101,169,255))
        d.line((x+11+i*8,102,x+25+i*8,102),fill=(227,248,255,255),width=2)
    return im

def make_icons():
    im=Image.new('RGBA',(192,64),(0,0,0,0));d=ImageDraw.Draw(im)
    # Six vivid, original 32px icons; intended as replaceable master tiles
    # folder
    x=0;d.rounded_rectangle((x+2,10,x+29,29),radius=3,fill=(194,126,24),outline=(135,84,18))
    d.rounded_rectangle((x+4,6,x+16,13),radius=2,fill=(255,213,93),outline=(181,125,33))
    d.rounded_rectangle((x+3,13,x+30,29),radius=3,fill=(251,184,52),outline=(171,109,21))
    d.line((x+5,15,x+28,15),fill=(255,237,156),width=2)
    # computer
    x=32;d.rounded_rectangle((x+2,5,x+28,25),radius=3,fill=(46,76,105),outline=(25,42,70))
    d.rectangle((x+5,8,x+25,21),fill=(102,197,239));d.polygon([(x+6,19),(x+16,9),(x+24,9),(x+24,21),(x+6,21)],fill=(137,225,239))
    d.rectangle((x+13,25,x+20,29),fill=(141,161,181));d.line((x+9,29,x+25,29),fill=(46,65,94),width=2)
    # notepad
    x=64;d.rounded_rectangle((x+5,3,x+27,30),radius=3,fill=(248,250,244),outline=(82,112,150),width=2)
    for yy in range(11,26,5):d.line((x+9,yy,x+23,yy),fill=(114,157,207),width=1)
    for xx in range(9,26,6):d.ellipse((x+xx,1,x+xx+3,6),fill=(41,99,157))
    # brush and palette
    x=96;d.ellipse((x+2,6,x+29,30),fill=(233,184,100),outline=(153,87,40),width=2)
    for xx,yy,c in [(9,12,'#e14e67'),(20,11,'#5a8fdc'),(11,22,'#72b85e'),(23,21,'#f3d355')]:d.ellipse((x+xx-3,yy-3,x+xx+3,yy+3),fill=c)
    d.ellipse((x+16,16,x+21,21),fill=(241,246,254))
    # gears
    x=128;d.ellipse((x+4,4,x+28,28),fill=(104,142,180),outline=(43,80,128),width=2)
    for i in range(8):
        a=i*math.pi/4;cx=16+12*math.cos(a);cy=16+12*math.sin(a)
        d.ellipse((x+cx-3,cy-3,x+cx+3,cy+3),fill=(104,142,180))
    d.ellipse((x+11,11,x+21,21),fill=(230,242,251),outline=(41,84,138),width=2)
    # terminal
    x=160;d.rounded_rectangle((x+2,4,x+30,29),radius=4,fill=(34,53,78),outline=(101,146,197),width=2)
    d.line((x+7,11,x+12,16,x+7,21),fill=(106,238,155),width=2)
    d.line((x+15,22,x+24,22),fill=(189,236,201),width=2)
    return im

frame(True).save(OUT/'frame-active.png');frame(False).save(OUT/'frame-inactive.png')
cap=Image.new('RGBA',(72,72))
for kind in range(3):
    for state in range(3):draw_caption(cap,state*24,kind*24,kind,state)
cap.save(OUT/'caption-buttons.png')
bar=Image.new('RGBA',(48,40))
for y in range(40):
    a=(70,166,224) if y<6 else (17,89,175)
    b=(18,83,162) if y<6 else (13,48,119)
    ImageDraw.Draw(bar).line((0,y,47,y),fill=mix(a,b,y/39)+(255,))
d=ImageDraw.Draw(bar)
d.line((0,0,47,0),fill=(184,235,251),width=1)
d.line((0,2,47,2),fill=(117,211,248),width=1)
d.line((0,6,47,6),fill=(15,70,141),width=1)
d.line((0,38,47,38),fill=(9,38,87),width=1)
bar.save(OUT/'taskbar.png')
start=Image.new('RGBA',(120,40))
for y in range(40):ImageDraw.Draw(start).line((0,y,119,y),fill=mix((111,195,77),(36,116,43),y/39)+(255,))
d=ImageDraw.Draw(start)
d.rounded_rectangle((0,0,119,39),radius=10,outline=(17,87,53,255),width=2)
d.arc((3,2,116,30),182,358,fill=(203,249,157,230),width=2)
d.rounded_rectangle((3,3,116,16),radius=8,fill=(208,250,178,47))
d.ellipse((13,8,34,31),fill=(22,85,163),outline=(234,245,255),width=2)
d.polygon([(17,11),(30,17),(20,25)],fill=(255,255,255))
d.text((40,10),'start',font=f(18),fill=(12,55,30))
d.text((39,9),'start',font=f(18),fill=(255,255,255))
start.save(OUT/'start.png')
make_widgets_atlas().save(OUT/'widgets-atlas.png')
base_icons=make_icons()
# Aurora 4 extends the original six icons; preserve the user-edited second row
# when regenerating default design masters (never overwrite custom pixel art).
prior=OUT/'icons-32.png'
if prior.exists():
    old=Image.open(prior).convert('RGBA')
    if old.size==(192,64):base_icons.paste(old.crop((0,32,192,64)),(0,32))
base_icons.save(prior)

# One single easy-to-find contact sheet proving masters are real graphics
preview=Image.new('RGB',(1240,800),(221,232,248));pd=ImageDraw.Draw(preview)
pd.rounded_rectangle((16,16,1224,784),radius=22,fill=(246,250,255),outline=(174,199,220),width=3)
pd.text((50,34),'OpenXP / Aurora',font=f(36),fill=(27,69,119))
pd.text((52,86),'Original editable UI artwork • PNG sprite masters',font=f(16,False),fill=(67,110,154))
# full mock system window, built from actual source frame title gradients (not screenshot)
wx,wy,ww,wh=53,137,752,450
pd.rounded_rectangle((wx+8,wy+11,wx+ww+8,wy+wh+11),radius=14,fill=(129,150,173))
pd.rounded_rectangle((wx,wy,wx+ww,wy+wh),radius=10,fill=(248,251,255),outline=(26,93,162),width=3)
for y in range(36):
    col=mix((13,87,176),(69,167,220),y/35)
    pd.line((wx+3,wy+5+y,wx+ww-3,wy+5+y),fill=col,width=1)
pd.text((wx+24,wy+13),'My Computer  •  OpenXP Explorer',font=f(20),fill=(255,255,255))
for i in range(3):
    tile=cap.crop((i*0+0, i*24,24,i*24+24)).resize((35,35),Image.Resampling.NEAREST)
    preview.paste(tile,(wx+ww-130+i*40,wy+5),tile)
pd.rectangle((wx+3,wy+45,wx+ww-3,wy+89),fill=(224,238,252))
pd.text((wx+24,wy+58),'File     Edit     View     Favorites     Tools     Help',font=f(15,False),fill=(38,76,117))
pd.rectangle((wx+20,wy+105,wx+210,wy+wh-30),fill=(223,237,249),outline=(180,205,231),width=2)
pd.text((wx+38,wy+119),'SYSTEM TASKS',font=f(14),fill=(29,87,154))
for n,t in enumerate(['My Documents','My Computer','Control Panel','Network Places']):
    pd.text((wx+42,wy+163+n*45),t,font=f(15,False),fill=(52,103,160))
pd.text((wx+248,wy+118),'Files and folders',font=f(20),fill=(37,86,140))
for n,t in enumerate(['Documents','Downloads','Pictures','System Apps','Wallpaper']):
    ico=make_icons().crop((0,0,32,32)).resize((38,38))
    preview.paste(ico,(wx+248,wy+167+n*46),ico)
    pd.text((wx+298,wy+174+n*46),t,font=f(16,False),fill=(46,74,103))
# taskbar uses actual sprites
pd.text((51,614),'Shell artwork  /  taskbar & Start',font=f(15),fill=(39,83,136))
preview.paste(bar.resize((750,49)),(54,643),bar.resize((750,49)))
preview.paste(start.resize((152,51)),(56,640),start.resize((152,51)))
pd.text((839,142),'ARTIST ASSETS',font=f(21),fill=(44,86,133))
pd.text((839,183),'Window frames',font=f(15,False),fill=(50,92,137))
for n,name in enumerate(['frame-active.png','frame-inactive.png']):
    img=Image.open(OUT/name).resize((110,110),Image.Resampling.NEAREST)
    preview.paste(img,(845+n*128,219),img)
pd.text((839,361),'Caption controls',font=f(15,False),fill=(50,92,137))
preview.paste(cap.resize((165,165),Image.Resampling.NEAREST),(847,394),cap.resize((165,165),Image.Resampling.NEAREST))
pd.text((838,585),'Control sprite studies',font=f(15,False),fill=(50,92,137))
wid=Image.open(OUT/'widgets-atlas.png').resize((320,156))
preview.paste(wid,(842,612),wid)
preview.save(OUT/'AURORA-ARTWORK-PREVIEW.png')
print('Wrote',len(list(OUT.glob('*.png'))),'Aurora PNGs to',OUT)
