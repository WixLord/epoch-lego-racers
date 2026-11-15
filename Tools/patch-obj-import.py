from pathlib import Path
p=Path('Tools/ObjMeshImporter.cs');s=p.read_text();s=s.replace('if(p.Length!=4)throw new Exception("Export triangulated faces for reliable import");','')
s=s.replace('var points=new double[3][];for(int j=0;j<3;j++)','var points=new double[p.Length-1][];for(int j=0;j<points.Length;j++)')
s=s.replace('builder.Triangle(points[0],points[1],points[2],color,node);','Triangulate(builder,points,color,node);')
i=s.rfind('\n}')
s=s[:i]+'''
 static void Triangulate(MeshBuilder builder,double[][] points,int color,int node){
  double[] normal=new double[3];for(int i=0;i<points.Length;i++){var a=points[i];var b=points[(i+1)%points.Length];normal[0]+=(a[1]-b[1])*(a[2]+b[2]);normal[1]+=(a[2]-b[2])*(a[0]+b[0]);normal[2]+=(a[0]-b[0])*(a[1]+b[1]);}
  int drop=0;for(int i=1;i<3;i++)if(Math.Abs(normal[i])>Math.Abs(normal[drop]))drop=i;
  int u=(drop+1)%3,v=(drop+2)%3;double sign=normal[drop]>=0?1:-1;
  Func<int,int,int,double> cross=(a,b,c)=>sign*((points[b][u]-points[a][u])*(points[c][v]-points[a][v])-(points[b][v]-points[a][v])*(points[c][u]-points[a][u]));
  var remaining=new List<int>();for(int i=0;i<points.Length;i++)remaining.Add(i);
  while(remaining.Count>3){bool found=false;for(int i=0;i<remaining.Count;i++){int a=remaining[(i+remaining.Count-1)%remaining.Count],b=remaining[i],c=remaining[(i+1)%remaining.Count];if(cross(a,b,c)<=1e-9)continue;bool inside=false;foreach(int q in remaining)if(q!=a&&q!=b&&q!=c&&cross(a,b,q)>=-1e-9&&cross(b,c,q)>=-1e-9&&cross(c,a,q)>=-1e-9){inside=true;break;}if(inside)continue;builder.Triangle(points[a],points[b],points[c],color,node);remaining.RemoveAt(i);found=true;break;}if(!found)throw new Exception("Cannot triangulate polygon");}
  builder.Triangle(points[remaining[0]],points[remaining[1]],points[remaining[2]],color,node);
 }
'''+s[i:];p.write_text(s)
