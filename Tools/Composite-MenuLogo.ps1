Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Drawing.Drawing2D;
public class MenuLogoComposite {
 public static void Run(string folder) {
  using(var original=new Bitmap(folder+"/LEGO-Racers-Original.png"))
  using(var subtitle=new Bitmap(folder+"/Epoch-Subtitle.png")) {
   int l=subtitle.Width,t=subtitle.Height,r=0,b=0;
   for(int y=0;y<subtitle.Height;y++) for(int x=0;x<subtitle.Width;x++) if(subtitle.GetPixel(x,y).A>128) {l=Math.Min(l,x);t=Math.Min(t,y);r=Math.Max(r,x);b=Math.Max(b,y);}
   int width=original.Width-245;
   int height=(int)Math.Round((b-t+1)*(double)width/(r-l+1));
   using(var result=new Bitmap(original.Width,original.Height+height-10,PixelFormat.Format32bppArgb)) {
    using(var g=Graphics.FromImage(result)) {
     g.Clear(Color.Transparent);
     g.InterpolationMode=InterpolationMode.HighQualityBicubic;
     g.DrawImage(subtitle,new Rectangle(240,original.Height-16,width,height),new Rectangle(l,t,r-l+1,b-t+1),GraphicsUnit.Pixel);
    }
    // The protected artwork is copied verbatim, without resampling or blending.
    for(int y=0;y<original.Height;y++) for(int x=0;x<original.Width;x++) if(original.GetPixel(x,y).A>0) result.SetPixel(x,y,original.GetPixel(x,y));
    string path=folder+"/LEGO-Racers-Epoch-Remastered.png";
    result.Save(path,ImageFormat.Png);
    using(var saved=new Bitmap(path)) for(int y=0;y<original.Height;y++) for(int x=0;x<original.Width;x++) if(original.GetPixel(x,y).A>0 && saved.GetPixel(x,y).ToArgb()!=original.GetPixel(x,y).ToArgb()) throw new Exception("Protected pixel changed");
    Console.WriteLine("PASS: All "+original.Width*original.Height+" original artwork region checked; every nontransparent original pixel unchanged.");
   }
  }
 }
}
"@
[MenuLogoComposite]::Run((Join-Path (Get-Location) 'Assets/Branding'))


