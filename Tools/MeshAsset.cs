using System;using System.IO;using System.Linq;using System.Collections.Generic;
public class MeshAsset {
 public class V {public double[] a=new double[9];public double[] normal;public V Copy(){var v=new V();a.CopyTo(v.a,0);v.normal=normal==null?null:(double[])normal.Clone();return v;}}
 public class Tri {public V a,b,c;public int mat,node;public Tri(V x,V y,V z,int m,int n){a=x;b=y;c=z;mat=m;node=n;}}
 public List<string> materials=new List<string>();public List<Tri> faces=new List<Tri>();public double scale=1; public bool hasTransformCommands=true;
 public MeshAsset(){}
 public MeshAsset(string path){var t=BinaryTokens.Read(path);hasTransformCommands=t.Any(x=>x.Type==50);int p=0;var verts=new List<V>();var indices=new List<int[]>();int mat=0,node=0;V[] cache=new V[64];
 while(p<t.Count){int kind=t[p++].Type;if(kind==51){scale=Convert.ToDouble(t[p++].Value);continue;}if(kind==0)continue;
 if(kind!=39&&kind!=41&&kind!=42&&kind!=45&&kind!=46)throw new Exception(path+" unsupported section "+kind);
 if(t[p++].Type!=7)throw new Exception("bracket");int count=Convert.ToInt32(t[p++].Value);p+=2;
 for(int i=0;i<count;i++){if(kind==39)materials.Add((string)t[p++].Value);
 else if(kind==41){var v=new V();for(int j=0;j<5;j++)v.a[j]=Convert.ToDouble(t[p++].Value);v.normal=new double[3];for(int j=0;j<3;j++)v.normal[j]=Convert.ToDouble(t[p++].Value);v.a[8]=255;verts.Add(v);}
 else if(kind==42){var v=new V();for(int j=0;j<9;j++)v.a[j]=Convert.ToDouble(t[p++].Value);verts.Add(v);}
 else if(kind==45){indices.Add(new int[]{Convert.ToInt32(t[p++].Value),Convert.ToInt32(t[p++].Value),Convert.ToInt32(t[p++].Value)});}
 else {int cmd=t[p++].Type;if(cmd==39)mat=Convert.ToInt32(t[p++].Value);else if(cmd==50)node=Convert.ToInt32(t[p++].Value);
 else if(cmd==49){int dest=Convert.ToInt32(t[p++].Value),first=Convert.ToInt32(t[p++].Value),n=Convert.ToInt32(t[p++].Value);for(int j=0;j<n;j++)cache[dest+j]=verts[first+j];}
 else if(cmd==45){int first=Convert.ToInt32(t[p++].Value),n=Convert.ToInt32(t[p++].Value);for(int j=0;j<n;j++){var q=indices[first+j];faces.Add(new Tri(cache[q[0]].Copy(),cache[q[1]].Copy(),cache[q[2]].Copy(),mat,node));}}
 else throw new Exception("unsupported group "+cmd);}}
 if(t[p++].Type!=6)throw new Exception("section end");}}
 static V Mid(V a,V b,bool round){var v=new V();for(int i=0;i<9;i++)v.a[i]=(a.a[i]+b.a[i])*.5;
 double ra=Math.Sqrt(a.a[1]*a.a[1]+a.a[2]*a.a[2]),rb=Math.Sqrt(b.a[1]*b.a[1]+b.a[2]*b.a[2]),r=Math.Sqrt(v.a[1]*v.a[1]+v.a[2]*v.a[2]);
 if(round&&ra>1&&Math.Abs(ra-rb)<ra*.01&&r>ra*.5){v.a[1]*=(ra+rb)*.5/r;v.a[2]*=(ra+rb)*.5/r;}return v;}
 public int RoundWheels(){int changed=0;for(int pass=0;pass<2;pass++){var output=new List<Tri>();foreach(var t in faces){if(t.node==0){output.Add(t);continue;}var ab=Mid(t.a,t.b,true);var bc=Mid(t.b,t.c,true);var ca=Mid(t.c,t.a,true);output.Add(new Tri(t.a,ab,ca,t.mat,t.node));output.Add(new Tri(ab,t.b,bc,t.mat,t.node));output.Add(new Tri(ca,bc,t.c,t.mat,t.node));output.Add(new Tri(ab,bc,ca,t.mat,t.node));changed++;}faces=output;}return changed;}
 public int AddStuds(){int count=0;var added=new List<Tri>();var seen=new HashSet<string>();foreach(var t in faces){if(materials[t.mat].IndexOf("nub",StringComparison.OrdinalIgnoreCase)<0)continue;
 var a=t.a.a;var b=t.b.a;var c=t.c.a;double nx=(b[1]-a[1])*(c[2]-a[2])-(b[2]-a[2])*(c[1]-a[1]),ny=(b[2]-a[2])*(c[0]-a[0])-(b[0]-a[0])*(c[2]-a[2]),nz=(b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]);if(Math.Abs(nz)<Math.Sqrt(nx*nx+ny*ny+nz*nz)*.98)continue;
 double det=(b[3]-a[3])*(c[4]-a[4])-(c[3]-a[3])*(b[4]-a[4]);if(Math.Abs(det)<1e-8)continue;
 // Both stud atlases contain four rows, including the lower half used by RR/VV.
 for(int y=0;y<4;y++)for(int x=0;x<4;x++){double u=.125+x*.25,v=.125+y*.25;double wb=((u-a[3])*(c[4]-a[4])-(c[3]-a[3])*(v-a[4]))/det,wc=((b[3]-a[3])*(v-a[4])-(u-a[3])*(b[4]-a[4]))/det,wa=1-wb-wc;if(wa<-.001||wb<-.001||wc<-.001)continue;
 var center=new V();for(int j=0;j<9;j++)center.a[j]=wa*a[j]+wb*b[j]+wc*c[j];string key=string.Format(System.Globalization.CultureInfo.InvariantCulture,"{0:F1},{1:F1},{2:F1}",center.a[0],center.a[1],center.a[2]);if(!seen.Add(key))continue;
 double size=Math.Sqrt(Math.Abs(nz/det))*.25;double radius=size*.30,height=size*.18;if(size<1||size>300)continue;count++;
 center.a[3]=.02;center.a[4]=.75;var top=center.Copy();top.a[2]+=height;top.normal=new double[]{0,0,1};
 for(int side=0;side<6;side++){double ang=side*Math.PI/3,ang2=(side+1)*Math.PI/3;var p=center.Copy();var q=center.Copy();p.a[0]+=radius*Math.Cos(ang);p.a[1]+=radius*Math.Sin(ang);q.a[0]+=radius*Math.Cos(ang2);q.a[1]+=radius*Math.Sin(ang2);var pt=p.Copy();var qt=q.Copy();pt.a[2]+=height;qt.a[2]+=height;
 if(side>=1&&side<=4){var cap=top.Copy();cap.a[0]+=radius;added.Add(new Tri(cap,pt.Copy(),qt.Copy(),t.mat,t.node));}// Bake a shared radial side normal at each endpoint; the cap copies above
 // retain their upward shading and stay separate across the hard rim.
 double shadeP=.72+.16*Math.Cos(ang+.6),shadeQ=.72+.16*Math.Cos(ang2+.6);
 p.normal=pt.normal=new double[]{Math.Cos(ang),Math.Sin(ang),0};q.normal=qt.normal=new double[]{Math.Cos(ang2),Math.Sin(ang2),0};
 for(int j=5;j<8;j++){p.a[j]*=shadeP;q.a[j]*=shadeQ;pt.a[j]*=shadeP;qt.a[j]*=shadeQ;}added.Add(new Tri(p,q,qt,t.mat,t.node));added.Add(new Tri(p,qt,pt,t.mat,t.node));}}
 // The physical studs replace the baked circles on their supporting faces.
 t.a.a[3]=t.b.a[3]=t.c.a[3]=.02;t.a.a[4]=t.b.a[4]=t.c.a[4]=.75;
 }faces.AddRange(added);return count;}
 public void Write(string path){if(faces.Count*3>65535)throw new Exception("vertex limit");var t=new List<BinaryToken>();Action<int,object> add=(k,v)=>t.Add(new BinaryToken(k,v));Action<int,int> section=(k,n)=>{add(k,null);add(7,null);add(4,n);add(8,null);add(5,null);};section(39,materials.Count);foreach(var m in materials)add(2,m);add(6,null);add(51,null);add(3,scale);bool normals=faces.Count>0&&faces[0].a.normal!=null;section(normals?41:42,faces.Count*3);foreach(var f in faces)foreach(var v in new[]{f.a,f.b,f.c}){if(normals){if(v.normal==null)throw new Exception("Missing vertex normal");for(int i=0;i<5;i++)add(3,v.a[i]);for(int i=0;i<3;i++)add(3,v.normal[i]);}else for(int i=0;i<9;i++)add(i<5?3:4,i<5?(object)v.a[i]:(object)(int)Math.Round(v.a[i]));}add(6,null);section(45,faces.Count);foreach(var f in faces){add(4,0);add(4,1);add(4,2);}add(6,null);section(46,faces.Count*(hasTransformCommands?4:3));for(int i=0;i<faces.Count;i++){add(39,null);add(4,faces[i].mat);if(hasTransformCommands){add(50,null);add(4,faces[i].node);}add(49,null);add(4,0);add(4,i*3);add(4,3);add(45,null);add(4,i);add(4,1);}add(6,null);Directory.CreateDirectory(Path.GetDirectoryName(path));BinaryTokens.Write(path,t);}
}
