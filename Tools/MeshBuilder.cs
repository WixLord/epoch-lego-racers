using System;
// Small reusable faceted-mesh builder. Coordinates are in game units.
public class MeshBuilder {
 public MeshAsset mesh=new MeshAsset();
 public MeshBuilder(){mesh.scale=1.0/64;mesh.materials.Add("bike");}
 public void Triangle(double[] a,double[] b,double[] c,int color,int node=0){
  double nx=(b[1]-a[1])*(c[2]-a[2])-(b[2]-a[2])*(c[1]-a[1]),ny=(b[2]-a[2])*(c[0]-a[0])-(b[0]-a[0])*(c[2]-a[2]),nz=(b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]);
  double len=Math.Sqrt(nx*nx+ny*ny+nz*nz);if(len<1e-8)return;
  double shade=.72+.28*Math.Max(0,(-nx*.3-ny*.4+nz*.85)/len);
  var vs=new MeshAsset.V[3];int k=0;foreach(var p in new[]{a,b,c}){var v=new MeshAsset.V();for(int i=0;i<3;i++)v.a[i]=p[i]*64;v.a[5]=((color>>16)&255)*shade;v.a[6]=((color>>8)&255)*shade;v.a[7]=(color&255)*shade;v.a[8]=255;vs[k++]=v;}mesh.faces.Add(new MeshAsset.Tri(vs[0],vs[1],vs[2],0,node));
 }
 public void Quad(double[] a,double[] b,double[] c,double[] d,int color,int node=0){Triangle(a,b,c,color,node);Triangle(a,c,d,color,node);}
 // Counter-clockwise profile in X/Z, extruded across Y.
 public void Extrude(double[][] profile,double halfWidth,int color){
  int n=profile.Length;
  var remaining=new System.Collections.Generic.List<int>();for(int i=0;i<n;i++)remaining.Add(i);
  while(remaining.Count>2){bool found=false;for(int j=0;j<remaining.Count;j++){
   int ia=remaining[(j+remaining.Count-1)%remaining.Count],ib=remaining[j],ic=remaining[(j+1)%remaining.Count];var a=profile[ia];var b=profile[ib];var c=profile[ic];
   if(Cross(a,b,c)<=1e-8)continue;bool inside=false;foreach(int k in remaining)if(k!=ia&&k!=ib&&k!=ic&&Cross(a,b,profile[k])>=-1e-8&&Cross(b,c,profile[k])>=-1e-8&&Cross(c,a,profile[k])>=-1e-8){inside=true;break;}if(inside)continue;
   Triangle(new[]{a[0],-halfWidth,a[1]},new[]{b[0],-halfWidth,b[1]},new[]{c[0],-halfWidth,c[1]},color);
   Triangle(new[]{c[0],halfWidth,c[1]},new[]{b[0],halfWidth,b[1]},new[]{a[0],halfWidth,a[1]},color);
   remaining.RemoveAt(j);found=true;break;
  }if(!found)throw new Exception("Invalid extrusion polygon");}
  for(int i=0;i<n;i++){var a=profile[i];var b=profile[(i+1)%n];
   double[] l={a[0],-halfWidth,a[1]},r={a[0],halfWidth,a[1]},ln={b[0],-halfWidth,b[1]},rn={b[0],halfWidth,b[1]};
   Quad(l,r,rn,ln,color);
  }
 }
 static double Cross(double[] a,double[] b,double[] c){return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]);}
 public void Cylinder(double[] a,double[] b,double radius,int sides,int color,int node=0){
  var d=new[]{b[0]-a[0],b[1]-a[1],b[2]-a[2]};double len=Math.Sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);for(int j=0;j<3;j++)d[j]/=len;
  var u=Math.Abs(d[2])<.9?new[]{-d[1],d[0],0.0}:new[]{0.0,-d[2],d[1]};double ul=Math.Sqrt(u[0]*u[0]+u[1]*u[1]+u[2]*u[2]);for(int j=0;j<3;j++)u[j]/=ul;
  var v=new[]{d[1]*u[2]-d[2]*u[1],d[2]*u[0]-d[0]*u[2],d[0]*u[1]-d[1]*u[0]};
  for(int i=0;i<sides;i++){var p=new double[3];var q=new double[3];var r=new double[3];var s=new double[3];double t=i*2*Math.PI/sides,t2=(i+1)*2*Math.PI/sides;for(int j=0;j<3;j++){p[j]=a[j]+radius*(u[j]*Math.Cos(t)+v[j]*Math.Sin(t));q[j]=a[j]+radius*(u[j]*Math.Cos(t2)+v[j]*Math.Sin(t2));r[j]=q[j]+b[j]-a[j];s[j]=p[j]+b[j]-a[j];}Quad(p,q,r,s,color,node);Triangle(a,q,p,color,node);Triangle(b,s,r,color,node);}
 }
}
