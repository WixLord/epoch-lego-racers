using System;
using System.IO;

// Decode every compression chunk and both palette depths used by racer parts.
public static class PackedBitmap {
    public static void Decode(string source, string target) {
        byte[] b = File.ReadAllBytes(source);
        int bits = b[0] & 60, colors = b[1] + 1;
        if (bits != 4 && bits != 8) throw new InvalidDataException("Expected palette bitmap");
        int w = BitConverter.ToUInt16(b, 2), h = BitConverter.ToUInt16(b, 4);
        int row = (w * bits + 7) / 8, s = 6 + colors * 3;
        byte[] pixels = new byte[row * h];
        int written = 0;
        while (written < pixels.Length) {
            int unpacked = BitConverter.ToUInt16(b, s), packed = BitConverter.ToUInt16(b, s + 2);
            s += 4;
            byte[] data = new byte[unpacked];
            if (packed >= unpacked) Buffer.BlockCopy(b, s, data, 0, unpacked);
            else {
                int p = s, d = 0;
                data[d++] = b[p++];
                bool end = false;
                while (!end) {
                    int ctrl = unchecked((b[p++] << 24) + 1);
                    while (true) {
                        if (ctrl >= 0) { ctrl = unchecked(ctrl * 2); data[d++] = b[p++]; }
                        else {
                            int token = b[p++], count = token & 15, back = ((token & 240) << 4) | b[p];
                            if (back == 0) { end = true; break; }
                            if (count == 0) count = -b[++p] - 18; else count -= 18;
                            p++;
                            do { count++; data[d] = data[d - back]; d++; } while (count != 0);
                            ctrl = unchecked(ctrl * 2);
                        }
                        if ((ctrl & 255) == 0) break;
                    }
                }
                if (d != unpacked) throw new InvalidDataException("Chunk size mismatch");
            }
            Buffer.BlockCopy(data, 0, pixels, written, unpacked);
            written += unpacked;
            s += packed;
        }
        int stride = (w * 3 + 3) & ~3;
        using (var o = new BinaryWriter(File.Create(target))) {
            o.Write((ushort)0x4d42); o.Write(54 + stride * h); o.Write(0); o.Write(54);
            o.Write(40); o.Write(w); o.Write(h); o.Write((ushort)1); o.Write((ushort)24);
            for (int i = 0; i < 6; i++) o.Write(0);
            for (int y = h - 1; y >= 0; y--) {
                for (int x = 0; x < w; x++) {
                    int index = pixels[y * row + (bits == 4 ? x / 2 : x)];
                    if (bits == 4) index = x % 2 == 0 ? index >> 4 : index & 15;
                    for (int k = 0; k < 3; k++) o.Write(b[6 + index * 3 + k]);
                }
                for (int pad = w * 3; pad < stride; pad++) o.Write((byte)0);
            }
        }
    }
}
