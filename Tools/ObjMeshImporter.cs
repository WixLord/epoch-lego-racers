using System;using System.IO;using System.Collections.Generic;using System.Globalization;
public static class ObjMeshImporter {
 public static MeshAsset Read(string path, Dictionary<string,int> nodes, double[][] pivots) {
  var vertices=new List<double[]>();var normals=new List<double[]>();var colors=new Dictionary<string,int>();
  string mat=null;foreach(string line in File.ReadAllLines(Path.ChangeExtension(path,".mtl"))){var p=line.Trim().Split(new[]{' ','\t'},StringSplitOptions.RemoveEmptyEntries);if(p.Length==0)continue;if(p[0]=="newmtl")mat=p[1];if(p[0]=="Kd")colors[mat]=((int)Math.Round(double.Parse(p[1],CultureInfo.InvariantCulture)*255)<<16)|((int)Math.Round(double.Parse(p[2],CultureInfo.InvariantCulture)*255)<<8)|(int)Math.Round(double.Parse(p[3],CultureInfo.InvariantCulture)*255);}
  var builder=new MeshBuilder();int node=-1;int color=0xffffff;
  builder.mesh.materials.Clear();builder.mesh.materials.AddRange(new[]{"bike","biketyre","bikehub","bikein","biketred"});
  foreach(string line in File.ReadAllLines(path)){var p=line.Trim().Split(new[]{' ','\t'},StringSplitOptions.RemoveEmptyEntries);if(p.Length==0)continue;
   if(p[0]=="v"){vertices.Add(new[]{double.Parse(p[1],CultureInfo.InvariantCulture),double.Parse(p[2],CultureInfo.InvariantCulture),double.Parse(p[3],CultureInfo.InvariantCulture)});}
   else if(p[0]=="vn"){normals.Add(new[]{double.Parse(p[1],CultureInfo.InvariantCulture),double.Parse(p[2],CultureInfo.InvariantCulture),double.Parse(p[3],CultureInfo.InvariantCulture)});}
   else if(p[0]=="g"||p[0]=="o"){if(!nodes.TryGetValue(p[1],out node))throw new Exception("Unknown mesh group: "+p[1]);}
   else if(p[0]=="usemtl"){color=colors[p[1]];}
   else if(p[0]=="f"){
    if(node<0)throw new Exception("Face without object group");
    var pointNormals=new double[p.Length-1][];var points=new double[p.Length-1][];for(int j=0;j<points.Length;j++){var indices=p[j+1].Split('/');if(indices.Length<3||indices[2].Length==0)throw new Exception("Export OBJ vertex normals from Max");int ni=int.Parse(indices[2]);pointNormals[j]=normals[ni>0?ni-1:normals.Count+ni];int index=int.Parse(indices[0]);index=index>0?index-1:vertices.Count+index;var v=vertices[index];points[j]=new[]{v[0]-pivots[node][0],v[1]-pivots[node][1],v[2]-pivots[node][2]};}
    int first=builder.mesh.faces.Count;Triangulate(builder,points,color,node);
    int material=color==0x202020?1:color==0xa41e20?2:color==0x555952?3:0;
    for(int f=first;f<builder.mesh.faces.Count;f++){var face=builder.mesh.faces[f];face.mat=material;foreach(var vertex in new[]{face.a,face.b,face.c}){int match=-1;for(int j=0;j<points.Length;j++)if(Math.Abs(vertex.a[0]/64-points[j][0])<1e-8&&Math.Abs(vertex.a[1]/64-points[j][1])<1e-8&&Math.Abs(vertex.a[2]/64-points[j][2])<1e-8){match=j;break;}if(match<0)throw new Exception("Missing corner normal");vertex.normal=(double[])pointNormals[match].Clone();}}
   }
  }
  foreach(var face in builder.mesh.faces){
   if(face.node==0||face.mat!=1)continue;
   var vs=new[]{face.a,face.b,face.c};double minY=double.MaxValue,maxY=double.MinValue,minR=double.MaxValue,maxR=0;
   foreach(var v in vs){minY=Math.Min(minY,v.a[1]);maxY=Math.Max(maxY,v.a[1]);double r=Math.Sqrt(v.a[0]*v.a[0]+v.a[2]*v.a[2]);minR=Math.Min(minR,r);maxR=Math.Max(maxR,r);}
   if(maxY-minY<1||minR<64||maxR-minR>.05)continue;
   double[] angles=new double[3];double lo=double.MaxValue,hi=double.MinValue;for(int i=0;i<3;i++){angles[i]=Math.Atan2(vs[i].a[2],vs[i].a[0]);if(i>0){while(angles[i]-angles[0]>Math.PI)angles[i]-=2*Math.PI;while(angles[i]-angles[0]<-Math.PI)angles[i]+=2*Math.PI;}lo=Math.Min(lo,angles[i]);hi=Math.Max(hi,angles[i]);}
   if(hi-lo<1e-6)continue;face.mat=4;
   for(int i=0;i<3;i++){vs[i].a[3]=.52+.46*(vs[i].a[1]-minY)/(maxY-minY);vs[i].a[4]=.51+.47*(angles[i]-lo)/(hi-lo);}
  }
  return builder.mesh;
 }
 static void Triangulate(MeshBuilder builder,double[][] points,int color,int node){
  double[] normal=new double[3];for(int i=0;i<points.Length;i++){var a=points[i];var b=points[(i+1)%points.Length];normal[0]+=(a[1]-b[1])*(a[2]+b[2]);normal[1]+=(a[2]-b[2])*(a[0]+b[0]);normal[2]+=(a[0]-b[0])*(a[1]+b[1]);}
  int drop=0;for(int i=1;i<3;i++)if(Math.Abs(normal[i])>Math.Abs(normal[drop]))drop=i;
  int u=(drop+1)%3,v=(drop+2)%3;double sign=normal[drop]>=0?1:-1;
  Func<int,int,int,double> cross=(a,b,c)=>sign*((points[b][u]-points[a][u])*(points[c][v]-points[a][v])-(points[b][v]-points[a][v])*(points[c][u]-points[a][u]));
  var remaining=new List<int>();for(int i=0;i<points.Length;i++)remaining.Add(i);
  while(remaining.Count>3){bool found=false;for(int i=0;i<remaining.Count;i++){int a=remaining[(i+remaining.Count-1)%remaining.Count],b=remaining[i],c=remaining[(i+1)%remaining.Count];if(cross(a,b,c)<=1e-9)continue;bool inside=false;foreach(int q in remaining)if(q!=a&&q!=b&&q!=c&&cross(a,b,q)>=-1e-9&&cross(b,c,q)>=-1e-9&&cross(c,a,q)>=-1e-9){inside=true;break;}if(inside)continue;builder.Triangle(points[a],points[b],points[c],color,node);remaining.RemoveAt(i);found=true;break;}if(!found)throw new Exception("Cannot triangulate polygon");}
  builder.Triangle(points[remaining[0]],points[remaining[1]],points[remaining[2]],color,node);
 }

}
