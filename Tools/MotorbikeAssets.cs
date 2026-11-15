using System;using System.IO;using System.Collections.Generic;using System.Linq;
public static class MotorbikeAssets {
 static BinaryToken T(int k,object v=null){return new BinaryToken(k,v);}
 static List<BinaryToken> Read(string p){return BinaryTokens.Read(p);}
 static void Write(string p,List<BinaryToken> t){BinaryTokens.Write(p,t);}
 static List<BinaryToken> Header(int token,int count){return new List<BinaryToken>{T(token),T(7),T(4,count),T(8),T(5)};}
 public static void Build(string root,string meshFile){
  string menu=Path.Combine(root,"MENUDATA","PIECEDB"),race=Path.Combine(root,"GAMEDATA","COMMON");
  foreach(string dir in new[]{menu,race}){
   File.Copy(meshFile,Path.Combine(dir,"BIKE.GDB"),true);
   var mat=Header(39,5);string[] names={"bike","biketyre","bikehub","bikein","biketred"};int[] colors={0xb8bbb5,0x202020,0xa41e20,0x555952,0xffffff};
   for(int m=0;m<5;m++){int r=(colors[m]>>16)&255,g=(colors[m]>>8)&255,b=colors[m]&255;mat.AddRange(new[]{T(39),T(2,names[m]),T(5),T(41),T(4,r),T(4,g),T(4,b),T(4,255),T(40),T(4,r),T(4,g),T(4,b),T(4,255),T(43)});if(m==4)mat.AddRange(new[]{T(44),T(2,"bb_tire"),T(45)});mat.Add(T(6));}mat.Add(T(6));Write(Path.Combine(dir,"BIKE.MDB"),mat);
   File.Copy(Path.Combine(menu,"BBJMW.TDB"),Path.Combine(dir,"BIKE.TDB"),true);if(dir!=menu)File.Copy(Path.Combine(menu,"BB_TIRE.BMP"),Path.Combine(dir,"BB_TIRE.BMP"),true);
   var nodes=Header(39,3);
   for(int i=0;i<3;i++){nodes.AddRange(new[]{T(39),T(2,i==0?"bike":i==1?"da_bt":"da_ft"),T(5),T(40),T(3,i==0?0:i==1?-172.8:172.8),T(3,0),T(3,i==0?0:83.2),T(41),T(3,0),T(3,0),T(3,0),T(3,1)});if(i>0)nodes.AddRange(new[]{T(42),T(2,"bike")});nodes.Add(T(6));}nodes.Add(T(6));Write(Path.Combine(dir,"BIKE.SDB"),nodes);
   var anim=Read(Path.Combine(menu,"DAJMW.ADB"));int qi=anim.FindIndex(x=>x.Type==41);int count=Convert.ToInt32(anim[qi+2].Value);for(int i=0;i<count;i++){double angle=(i%9)*Math.PI/4*(i<9?1:-1);int j=qi+5+i*4;anim[j].Value=0f;anim[j+1].Value=(float)Math.Sin(angle/2);anim[j+2].Value=0f;anim[j+3].Value=(float)Math.Cos(angle/2);}Write(Path.Combine(dir,"BIKE.ADB"),anim);
   var chassis=Read(Path.Combine(dir,"CHASSIS.CMB"));var entry=new List<BinaryToken>();int begin=5,depth=0,end=begin;do{if(chassis[end].Type==5)depth++;if(chassis[end].Type==6)depth--;end++;}while(end<begin+3||depth>0);
   for(int i=begin;i<end;i++)entry.Add(T(chassis[i].Type,chassis[i].Value));entry[1].Value="bike";
   for(int i=0;i<entry.Count;i++){
    if(entry[i].Type==2 && i>0 && entry[i-1].Type>=52 && entry[i-1].Type<=56)entry[i].Value="bike";
    int k=entry[i].Type;
    if(k==43){entry[i+1].Value=-.8f;entry[i+2].Value=0f;entry[i+3].Value=2.7f;}
    // Keep the donor's collision dimensions and four-wheel contact footprint.
    // Shadow dimensions also determine body collision points in Racer::Initialize.
    if(k==47)entry[i+1].Value=.92f;
    // Token 49 remains unchanged; the two visual wheels are independent of physics.
    if(k==48){entry[i+2].Value=.25f;entry[i+3].Value=.25f;for(int w=0;w<4;w++){entry[i+4+w*3].Value=w<2?2.7f:-2.7f;entry[i+5+w*3].Value=w%2==0?-.12f:.12f;entry[i+6+w*3].Value=-.66f;}}
   }
   chassis[2].Value=Convert.ToInt32(chassis[2].Value)+1;chassis.InsertRange(chassis.Count-1,entry);Write(Path.Combine(dir,"CHASSIS.CMB"),chassis);
  }
  foreach(string path in Directory.GetFiles(root,"LPIECE*.LEB",SearchOption.AllDirectories))AddPiece(path);
  var manager=Read(Path.Combine(menu,"CRSTMGR.LEB"));manager[2].Value=13;manager.Insert(manager.Count-1,T(2,"bike_set.leg"));Write(Path.Combine(menu,"CRSTMGR.LEB"),manager);
  var set=Read(Path.Combine(menu,"DA_CSET.LEB"));set[1].Value=100;set[3].Value="bike";Write(Path.Combine(menu,"BIKE_SET.LEB"),set);
 }
 static void AddPiece(string path){
  var t=Read(path);int pos=t.FindIndex(x=>x.Type==41);int posCount=Convert.ToInt32(t[pos+2].Value);
  if(posCount>=65535)throw new Exception("Position table overflow");
  t[pos+2].Value=posCount+1;
  t.InsertRange(pos+5+posCount*3,new[]{T(4,985),T(4,128),T(4,32)});
  int shapes=t.FindIndex(x=>x.Type==44),pieces=t.FindIndex(x=>x.Type==39);
  int n=Convert.ToInt32(t[pieces+2].Value),shapeCount=Convert.ToInt32(t[shapes+2].Value);
  for(int i=0;i<n;i++)if(Convert.ToInt32(t[pieces+6+i*5].Value)==100)throw new Exception("Bike already present");
  t[pieces+2].Value=n+1;
  t.InsertRange(pieces+5+n*5,new[]{T(2,"bike"),T(4,100),T(4,shapeCount),T(4,0),T(4,0)});
  var shape=new List<BinaryToken>{T(4,8),T(4,1)};
  for(int x=0;x<8;x++){shape.Add(T(4,x<2?137:4));shape.Add(T(4,0));}
  shape.Add(T(4,posCount&255));shape.Add(T(4,posCount>>8));
  shapes=t.FindIndex(x=>x.Type==44);t[shapes+2].Value=shapeCount+10;
  t.InsertRange(shapes+5+shapeCount*2,shape);Write(path,t);
 }
}

