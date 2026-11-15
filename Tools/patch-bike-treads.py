from pathlib import Path
p=Path('Tools/ObjMeshImporter.cs');s=p.read_text().replace('"bikehub","bikein"','"bikehub","bikein","biketred"');s=s.replace('  return builder.mesh;','''  foreach(var face in builder.mesh.faces){
   if(face.node==0||face.mat!=1)continue;
   var vs=new[]{face.a,face.b,face.c};double minY=double.MaxValue,maxY=double.MinValue,minR=double.MaxValue,maxR=0;
   foreach(var v in vs){minY=Math.Min(minY,v.a[1]);maxY=Math.Max(maxY,v.a[1]);double r=Math.Sqrt(v.a[0]*v.a[0]+v.a[2]*v.a[2]);minR=Math.Min(minR,r);maxR=Math.Max(maxR,r);}
   if(maxY-minY<1||minR<64||maxR-minR>.05)continue;
   double[] angles=new double[3];double lo=double.MaxValue,hi=double.MinValue;for(int i=0;i<3;i++){angles[i]=Math.Atan2(vs[i].a[2],vs[i].a[0]);if(i>0){while(angles[i]-angles[0]>Math.PI)angles[i]-=2*Math.PI;while(angles[i]-angles[0]<-Math.PI)angles[i]+=2*Math.PI;}lo=Math.Min(lo,angles[i]);hi=Math.Max(hi,angles[i]);}
   if(hi-lo<1e-6)continue;face.mat=4;
   for(int i=0;i<3;i++){vs[i].a[3]=.52+.46*(vs[i].a[1]-minY)/(maxY-minY);vs[i].a[4]=.51+.47*(angles[i]-lo)/(hi-lo);}
  }
  return builder.mesh;''');p.write_text(s)
p=Path('Tools/MotorbikeAssets.cs');s=p.read_text().replace('var mat=Header(39,4);','var mat=Header(39,5);').replace('"bikehub","bikein"','"bikehub","bikein","biketred"').replace('0xa41e20,0x555952};','0xa41e20,0x555952,0xffffff};').replace('for(int m=0;m<4;m++)','for(int m=0;m<5;m++)');s=s.replace('T(4,b),T(4,255),T(43),T(6)});}','T(4,b),T(4,255),T(43)});if(m==4)mat.AddRange(new[]{T(44),T(2,"bb_tire"),T(45)});mat.Add(T(6));}');s=s.replace('var tex=Header(39,0);tex.Add(T(6));Write(Path.Combine(dir,"BIKE.TDB"),tex);','File.Copy(Path.Combine(menu,"BBJMW.TDB"),Path.Combine(dir,"BIKE.TDB"),true);');p.write_text(s)
