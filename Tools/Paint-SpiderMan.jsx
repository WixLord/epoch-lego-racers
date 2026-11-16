#target photoshop
// Hand-authored pixel geometry. No generated artwork or photo projection.
(function () {
    var root = new File($.fileName).parent.parent;
    var out = new Folder(root + '/Assets/SpiderMan'); out.create();
    var oldUnits = app.preferences.rulerUnits, oldDialogs = app.displayDialogs;
    app.preferences.rulerUnits = Units.PIXELS;
    app.displayDialogs = DialogModes.NO;
    var red = 'CE1726', blue = '1664BE', ink = '171822', web = '84232C', white = 'F7F6F0';
    var doc;
    function color(hex) { var c = new SolidColor(); c.rgb.hexValue = hex; return c; }
    function layer(name) { var l = doc.artLayers.add(); l.name = name; return l; }
    function polygon(points, hex) {
        doc.selection.select(points, SelectionType.REPLACE, 0, true);
        doc.selection.fill(color(hex), ColorBlendMode.NORMAL, 100, false);
        doc.selection.deselect();
    }
    function rect(x, y, w, h, hex) { polygon([[x,y],[x+w,y],[x+w,y+h],[x,y+h]],hex); }
    function stroke(points, hex, width) {
        for (var i = 1; i < points.length; i++) {
            var a=points[i-1], b=points[i], dx=b[0]-a[0], dy=b[1]-a[1];
            var len=Math.sqrt(dx*dx+dy*dy); if (!len) continue;
            var nx=-dy/len*width/2, ny=dx/len*width/2;
            polygon([[a[0]+nx,a[1]+ny],[b[0]+nx,b[1]+ny],[b[0]-nx,b[1]-ny],[a[0]-nx,a[1]-ny]],hex);
        }
    }
    function ellipse(cx, cy, rx, ry, hex) {
        var p=[]; for(var i=0;i<24;i++){var a=i*Math.PI/12;p.push([cx+Math.cos(a)*rx,cy+Math.sin(a)*ry]);} polygon(p,hex);
    }
    function start(name, size, base) {
        doc=app.documents.add(size,size,72,name,NewDocumentMode.RGB,DocumentFill.TRANSPARENT);
        doc.activeLayer.name='Plastic base'; rect(0,0,size,size,base);
    }
    function save(name) {
        var psd=new PhotoshopSaveOptions(); psd.layers=true;
        doc.saveAs(new File(out+'/'+name+'.psd'),psd,true,Extension.LOWERCASE);
        doc.saveAs(new File(out+'/'+name+'.png'),new PNGSaveOptions(),true,Extension.LOWERCASE);
    }
    function bodyWeb() {
        layer('Fine web print');
        for(var x=8;x<=120;x+=16) stroke([[x,0],[64+(x-64)*0.67,64],[x,128]],web,1.25);
        for(var y=10;y<128;y+=15) stroke([[0,y-6],[32,y],[64,y+7],[96,y],[128,y-6]],web,1.25);
    }
    function spider(cx,cy,scale,hex,round) {
        ellipse(cx,cy+4*scale,3.5*scale,6*scale,hex);
        ellipse(cx,cy-3*scale,2.6*scale,3*scale,hex);
        for(var side=-1;side<=1;side+=2) {
            var legs=[[[2,-3],[7,-8],[8,-14],[12,-17]],[[2,-1],[10,-4],[15,-5],[19,-8]],[[3,2],[9,1],[16,5],[20,6]],[[2,5],[8,8],[10,14],[14,17]]];
            for(var j=0;j<legs.length;j++) {
                var p=[];for(var k=0;k<legs[j].length;k++)p.push([cx+legs[j][k][0]*scale*side,cy+legs[j][k][1]*scale]);
                stroke(p,hex,round?4:2);
            }
        }
    }
    try {
        start('Spider-Man mask - 64px',64,red);
        layer('Mask web');
        var center=[32,29];
        var ends=[[0,0],[16,0],[32,0],[48,0],[64,0],[64,20],[64,43],[64,64],[48,64],[32,64],[16,64],[0,64],[0,43],[0,20]];
        for(var i=0;i<ends.length;i++)stroke([center,ends[i]],web,1);
        for(var r=8;r<48;r+=9) {
            var ring=[];for(var a=0;a<=16;a++){var t=a*Math.PI/8;ring.push([32+Math.cos(t)*r,29+Math.sin(t)*r*0.93]);}stroke(ring,web,1);
        }
        layer('Black eye outlines');
        var eye=[[13,18],[19,21],[28,28],[30,33],[27,39],[22,40],[16,37],[13,31]];
        for(var i=0;i<eye.length;i++){eye[i][0]=21.5+(eye[i][0]-21.5)*1.18;eye[i][1]=29+(eye[i][1]-29)*1.18;}
        polygon(eye,ink); var mirror=[];for(var i=0;i<eye.length;i++)mirror.push([64-eye[i][0],eye[i][1]]);polygon(mirror,ink);
        layer('White lenses');
        var lens=[[16,23],[21,26],[27,31],[27,34],[24,37],[20,36],[17,33]];
        for(var i=0;i<lens.length;i++){lens[i][0]=21.5+(lens[i][0]-21.5)*1.18;lens[i][1]=29+(lens[i][1]-29)*1.18;}
        polygon(lens,white);mirror=[];for(var i=0;i<lens.length;i++)mirror.push([64-lens[i][0],lens[i][1]]);polygon(mirror,white);
        // Head/neck geometry samples UV (0.023,0.023), the bottom-left of this
        // top-down image. Leave a margin for bilinear/anisotropic filtering.
        layer('Solid plastic color sample');rect(0,0,6,6,red);rect(0,58,6,6,red);
        save('mask');

        start('Spider-Man torso front - 128px',128,red); bodyWeb();
        layer('Blue suit panels');
        var panel=[[0,24],[14,32],[29,58],[36,88],[0,78]];
        polygon(panel,ink);var right=[];for(var i=0;i<panel.length;i++)right.push([128-panel[i][0],panel[i][1]]);polygon(right,ink);
        panel=[[0,29],[12,35],[26,59],[32,83],[0,75]];
        polygon(panel,blue);right=[];for(var i=0;i<panel.length;i++)right.push([128-panel[i][0],panel[i][1]]);polygon(right,blue);
        polygon([[0,104],[64,119],[128,104],[128,128],[0,128]],ink);
        polygon([[0,108],[64,123],[128,108],[128,128],[0,128]],blue);
        layer('Panel seams');
        stroke([[0,55],[13,60],[25,61]],ink,2);stroke([[0,68],[18,73],[28,72]],ink,2);
        stroke([[128,55],[115,60],[103,61]],ink,2);stroke([[128,68],[110,73],[100,72]],ink,2);
        layer('Chest spider emblem');spider(64,41,1,ink,false);save('torso-front');

        start('Spider-Man torso back - 128px',128,red); bodyWeb();
        layer('Blue back panel');
        polygon([[5,21],[123,21],[119,94],[64,110],[9,94]],ink);
        polygon([[8,24],[120,24],[116,91],[64,107],[12,91]],blue);
        polygon([[0,112],[64,124],[128,112],[128,128],[0,128]],blue);
        layer('Back emblem outline');spider(64,61,1.35,ink,true);ellipse(64,63,15,21,ink);
        layer('Red back emblem');spider(64,61,1.1,red,true);ellipse(64,63,12,18,red);
        save('torso-back');
        var done=new File(out+'/paint-complete.txt');done.open('w');done.write('Painted mask and torso front/back as layered PSD and PNG files.');done.close();
    } catch(e) {
        var log=new File(out+'/paint-error.txt');log.open('w');log.write(e.toString()+' at line '+e.line);log.close();
    } finally { app.preferences.rulerUnits=oldUnits;app.displayDialogs=oldDialogs; }
})();
