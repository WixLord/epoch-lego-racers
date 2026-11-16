"""Append eight thin plates to both piece libraries; preserve existing piece IDs."""
import json,struct
from pathlib import Path
source=json.load(open('Build/plates-input.json',encoding='utf-8-sig'))
root=Path('Assets/PlatesBuild');Path('Assets/Plates').mkdir(exist_ok=True)
plates=[(w,h,f'ep{w}x{h}',6000+i) for i,(w,h) in enumerate((w,h) for w in (1,2) for h in range(1,5))]
for name,tokens in source.items():
 if not name.endswith(('LPIECEHI.LEB','LPIECELO.LEB')):continue
 sections={};order=[];p=0
 while p<len(tokens):
  k=tokens[p]['Type'];n=tokens[p+2]['Value'];q=p+5;e=q
  while tokens[e]['Type']!=6:e+=1
  sections[k]=[x['Value'] for x in tokens[q:e]];order.append(k);p=e+1
 records=sections[39];donor=next(i for i in range(0,len(records),5) if records[i].lower()=='l300400')
 _,_,shape,commands,offset=records[donor:donor+5]
 assert not any(x[2] in records for x in plates)
 for width,height,label,typ in plates:
  posmap={};stream=[];p=offset;slots=[];normal=False;texture=False
  for cmd in range(commands):
   code=sections[40][p];p+=1;stream.append(code);mode=code&0x3000
   if mode==0x2000:count=1
   else:count=3;normal=bool(code&0x4000);texture=mode==0x1000
   for v in range(count):
    idx=sections[40][p];p+=1
    if idx not in posmap:
     x,y,z=sections[41][idx*3:idx*3+3]
     posmap[idx]=len(sections[41])//3
     sections[41].extend([round(x*width),round(y*height/2),round(z/3)])
    stream.append(posmap[idx])
    extra=(int(normal)+int(texture)) if count==1 else (int(not(code&0x8000) and (normal or v==0))+int(texture))
    stream.extend(sections[40][p:p+extra]);p+=extra
  shape_index=len(sections[44])//2
  sections[44].extend([width,height]+[129,128]*(width*height)+[255,255])
  records.extend([label,typ,shape_index,commands,len(sections[40])]);sections[40].extend(stream)
  assert all(0<=x<65536 for x in stream)
 assert len(sections[41])//3<65536
 sizes={39:5,40:1,41:3,42:3,43:2,44:2}
 with (root/name).open('wb') as out:
  for k in order:
   values=sections[k];out.write(bytes([k,7,4])+struct.pack('<i',len(values)//sizes[k])+bytes([8,5]))
   for value in values:
    if isinstance(value,str):out.write(b'\x02'+value.encode('ascii')+b'\0')
    else:out.write(b'\x04'+struct.pack('<i',value))
   out.write(b'\x06')
 print(name,': eight plates, height 1, studs enabled')
# The first/basic brick theme offers every plate in all ten existing colors.
t=source['MENUDATA/PIECEDB/DP_CSET.LEB'];colors=[x['Value'] for x in source['MENUDATA/PIECEDB/L_COLORS.LEB'] if x['Type']==2]
i=next(i for i,x in enumerate(t) if x['Type']==48);t[i+2]['Value']+=len(plates)*len(colors)
added=[{'Type':2,'Value':value} for _,_,label,_ in plates for col in colors for value in (label,col)]
t[-1:-1]=added
with (root/'MENUDATA/PIECEDB/DP_CSET.LEB').open('wb') as f:
 for token in t:
  k,v=token['Type'],token['Value'];f.write(bytes([k]))
  if k==2:f.write(v.encode()+b'\0')
  elif k==4:f.write(struct.pack('<i',v))
print('Basic theme: added 80 colored plate choices')
