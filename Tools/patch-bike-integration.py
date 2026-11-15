from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=root/'Tools/MotorbikeAssets.cs'
s=p.read_text().replace('double angle=(i%9)*Math.PI/4','double angle=(i%9)*Math.PI/4*(i<9?1:-1)')
s=s.replace('if(entry[i].Type==2 && i!=1)entry[i].Value="bike";','if(entry[i].Type==2 && i>0 && entry[i-1].Type>=52 && entry[i-1].Type<=56)entry[i].Value="bike";')
a=s.index(' static void AddPiece(')
s=s[:a]+''' static void AddPiece(string path){
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
'''
p.write_text(s)
for name,count in [('editcarscreen',13),('carmodelscreenbase',12)]:
 p=root/f'Source/racers/LEGORacers/src/menu/screens/{name}.cpp';s=p.read_text();s=s.replace('m_partCategoryAvailable[index] = TRUE;',f'if (index >= 0 && index < {count}) m_partCategoryAvailable[index] = TRUE;');s=s.replace('if (m_partCategoryAvailable[index]) {',f'if (index >= 0 && index < {count} && m_partCategoryAvailable[index]) {{');p.write_text(s)
p=root/'Source/racers/LEGORacers/src/menu/screens/carbuildscreen.cpp';s=p.read_text().replace('m_categoryCarousel.SelectChild(&m_categoryIcons[m_context->m_partSet.GetSelectedEntry()->GetIndex()]);','LegoS32 categoryIndex = m_context->m_partSet.GetSelectedEntry()->GetIndex();\n\t\t\tif (categoryIndex >= 0 && categoryIndex < 12) m_categoryCarousel.SelectChild(&m_categoryIcons[categoryIndex]);');p.write_text(s)
p=root/'Source/racers/LEGORacers/src/model/carbuildpiecegrid.cpp';s=p.read_text().replace('m_cellCallback(p_x + x, p_y + y, oldHeight, colorRecordIndex, delta);','// The motorcycle mesh already includes its studs.\n\t\t\t\t\t\tif (oldRecord->m_pieceType != 100) m_cellCallback(p_x + x, p_y + y, oldHeight, colorRecordIndex, delta);').replace('p_callback(x, y, m_entries[index].m_height, colorRecordIndex, delta);','if (oldRecord->m_pieceType != 100) p_callback(x, y, m_entries[index].m_height, colorRecordIndex, delta);');p.write_text(s)
